#include <experimental/filesystem>
#include <string>
#include <iostream>
#include <regex>
#include <fstream>
#include <set>
#include <vector>
#include <chrono>

#include <Windows.h>
#include <TlHelp32.h>

namespace fs = std::experimental::filesystem;

std::set<std::string> settingsFiles;
std::set<std::string> objFiles;
PROCESS_INFORMATION info;
STARTUPINFO sinfo = {sizeof(sinfo)};
using hclock = std::chrono::high_resolution_clock;

std::string settingsToLaunchArgs(const std::string& path)
{
	std::ifstream in(path);
	
	std::regex settingsRe("(?:\\s+|)(\\w+)(?:\\s+|)=(?:\\s+|)(.*)(?:\\s+|\\;)");

	std::string ret("--type tileMesh");
	if (!in.good())
	{
		std::cout << "\tUnable to load " << path << " navmesh settings file, attempting to load default.settings\n";
		in.open("default.settings");
	}

	if (in.good())
	{
		std::string line;
		while (std::getline(in, line))
		{
			//std::cout << line << "\n";
			line = line.empty() ? line : line.substr(line.find_first_not_of(' '));
			if (line.empty() || line[0] == '#' || line[0] == '/')
				continue;

			std::smatch match;
			if (std::regex_match(line, match, settingsRe))
			{
				ret += " --" + std::string(match[1].str().c_str()) + std::string(" ") + match[2].str();
			}
		}
		in.close();
	}
	else
	{
		static std::string defaultStr(
			" \t--tileSize 160.0 --cellSize 0.2 --cellHeight 0.2"
			" \t--agentHeight 2.0 --agentRadius 0.5 --agentMaxClimb 0.6 --agentMaxSlope 56"
			" \t--regionMinSize 8.0 --regionMergeSize 20.0 --edgeMaxLen 12.0 --edgeMaxError 1.4"
			" \t--vertsPerPoly 6.0 --detailSampleDist 6.0 --detailSampleMaxError 1.0 --partitionType 0");

		ret += defaultStr;
		std::cout << "\tUnable to load default navmesh settings file. Using hardcoded defaults instead.\n";
	}
	return ret;
}

void cleanup()
{
	std::cout << "\n\nCleaning up Process " << info.dwProcessId << "\n";
	system(std::string("taskkill /PID" + std::to_string(info.dwProcessId) + std::string(" /F")).c_str());
}


int main(int argc, char* argv[])
{
	std::atexit(cleanup);
	std::set_terminate(cleanup);
	auto start = hclock::now();
	for (const auto& path : fs::recursive_directory_iterator("./"))
	{
		std::string strPath(path.path().string());

		if (path.path().extension() == ".obj")
			objFiles.emplace(strPath);
		else if (path.path().extension() == ".settings")
			settingsFiles.emplace(strPath);
	}
	
	int exportCount = 0;
	for (const auto& file : objFiles)
	{
		std::cout << "\n\n";
		auto entryExportTime = std::chrono::high_resolution_clock::now();
		std::string fileName(file.substr(0, file.find_last_of('.')));
		std::string settingsFile(fileName + ".settings");
		static std::string launchStr("RecastDemo.exe ");
		std::string launchArg;
		auto it = settingsFiles.find(settingsFile);
		
		std::cout << "Exporting " << fileName << "\n";
		launchArg += settingsToLaunchArgs(it != settingsFiles.end() ? *it : fileName);


		launchArg += " --obj \"" + file + "\"";

		std::cout << "\t" << launchArg << "\n";

		if (CreateProcess("./RecastDemo.exe", &launchArg[0], 0, 0, true, 0, 0, 0, &sinfo, &info))
		{
			WaitForSingleObject(info.hProcess, INFINITE);
			
			DWORD exitCode;
			GetExitCodeProcess(info.hProcess, &exitCode);
			if (exitCode == EXIT_SUCCESS)
			{
				exportCount++;
				std::cout << "\n\tExported navmesh for " << fileName << " in " <<
					std::to_string(std::chrono::duration_cast<std::chrono::seconds>(hclock::now() - entryExportTime).count()) << " seconds.\n";
			}
			else
			{
				std::cout << "\n\tUnable to export navmesh for " << fileName << "\n";
			}
			CloseHandle(info.hProcess);
			CloseHandle(info.hThread);
		}
		else
		{
			std::cout << "\n\tUnable to export navmesh for " << fileName << "\n";
		}
	}
	std::cout << "Exported " << exportCount << " files in " << std::chrono::duration_cast<std::chrono::seconds>(hclock::now() - start).count() << " seconds.\n";
}