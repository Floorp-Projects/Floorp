#include "stdafx.h"
#include "WizardTypes.h"
#include "winbase.h"  // for CopyDir
#include <direct.h>

__declspec(dllexport) WIDGET GlobalWidgetArray[1000];
__declspec(dllexport) int GlobalArrayIndex=0;
__declspec(dllexport) BOOL IsSameCache = TRUE;

void ExtractContents(CString rootPath, CString instblobPath, 
					 CString instFilename, CString platformInfo, 
					 CString platformPath, CString extractPath);
void PopulateNscpxpi(CString rootpath, CString platformInfo, 
					 CString nscpxpiPath, CString extractPath);

extern "C" __declspec(dllexport)
int GetAttrib(CString theValue, char* attribArray[MAX_SIZE])
{
	//	attribArray= (char**) malloc(MIN_SIZE*MINSIZE);
	int j = 0;
	for (int i = 0; i < GlobalArrayIndex; i++)
	{
		if (GlobalWidgetArray[i].attrib == theValue) 
		{
			attribArray[j] = (char*)(LPCTSTR)(GlobalWidgetArray[i].name);
			j++;
		}
	}
	return j;
}


extern "C" __declspec(dllexport)
WIDGET* findWidget(CString theName)
{
	
	for (int i = 0; i < GlobalArrayIndex; i++)
	{
		if (GlobalWidgetArray[i].name == theName) {
			return (&GlobalWidgetArray[i]);
		}
	}

	return NULL;
}

extern "C" __declspec(dllexport)
WIDGET* SetGlobal(CString theName, CString theValue)
{
	WIDGET* w = findWidget(theName);
	if (w == NULL)
	{
		// Make sure we can add this value
		if (GlobalArrayIndex >= sizeof(GlobalWidgetArray))
			exit(11);

		GlobalWidgetArray[GlobalArrayIndex].name  = theName;
		GlobalWidgetArray[GlobalArrayIndex].value = theValue;
		w = &GlobalWidgetArray[GlobalArrayIndex];
		GlobalArrayIndex++;
	}
	else 
		w->value = theValue;

	return w;
}

__declspec(dllexport)
CString GetGlobal(CString theName)
{
	WIDGET *w = findWidget(theName);

	if (w)
		return (w->value);

	return "";
}

extern "C" __declspec(dllexport)
void CopyDir(CString from, CString to, LPCTSTR extension, int overwrite)
{
	WIN32_FIND_DATA data;
	HANDLE d;
	CString dot = ".";
	CString dotdot = "..";
	CString fchild, tchild;
	CString pattern = from + "\\*.*";
	int		found;
	DWORD	tmp;


	d = FindFirstFile((const char *) to, &data);
	if (d == INVALID_HANDLE_VALUE)
		mkdir(to);

	d = FindFirstFile((const char *) pattern, &data);
	found = (d != INVALID_HANDLE_VALUE);

	while (found)
	{
		if (data.cFileName != dot && data.cFileName != dotdot)
		{
			fchild = from + "\\" + data.cFileName;
			tchild = to + "\\" + data.cFileName;
			tmp = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			if (tmp == FILE_ATTRIBUTE_DIRECTORY)
				CopyDir(fchild, tchild, NULL, overwrite);
			else
			{
				CString spot=fchild;
				int loc = fchild.Find('.');
				if (loc)
					spot.Delete(0,loc+1);
				if (!extension || (spot.CompareNoCase((CString)extension)==0))
					CopyFile((const char *) fchild, (const char *) tchild, !overwrite);
			}									
		}

		found = FindNextFile(d, &data);
	}

	FindClose(d);
}

extern "C" __declspec(dllexport)
void ExecuteCommand(char *command, int showflag, DWORD wait)
{
	STARTUPINFO	startupInfo; 
	PROCESS_INFORMATION	processInfo; 

	memset(&startupInfo, 0, sizeof(startupInfo));
	memset(&processInfo, 0, sizeof(processInfo));

	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	//startupInfo.wShowWindow = SW_SHOW;
	startupInfo.wShowWindow = showflag;

	BOOL executionSuccessful = CreateProcess(NULL, command, NULL, NULL, TRUE, 
												NORMAL_PRIORITY_CLASS, NULL, NULL, 
												&startupInfo, &processInfo); 
	DWORD error = GetLastError();
	WaitForSingleObject(processInfo.hProcess, wait);
}

extern "C" __declspec(dllexport)
void CopyDirectory(CString source, CString dest, BOOL subdir)
// Copy files in subdirectories if the subdir flag is set (equal to 1).
{
	CFileFind finder;
	CString sFileToFind = source + "\\*.*";
	BOOL bWorking = finder.FindFile(sFileToFind);
	while (bWorking) 
	{
		bWorking = finder.FindNextFile();
		CString newPath=dest + "\\";

		if (finder.IsDots()) continue;
		if (finder.IsDirectory()) 
		{
			CString dirPath = finder.GetFilePath();
			newPath += finder.GetFileName();
			_mkdir(newPath);
			if (subdir == TRUE)
				CopyDirectory(dirPath, newPath, TRUE);
			if (!CopyFile(dirPath,newPath,0))
				DWORD e = GetLastError();
			continue; 
		}

		newPath += finder.GetFileName();
		CString source = finder.GetFilePath();
		if (!CopyFile(source,newPath,0))
			DWORD e = GetLastError();
	}
}

extern "C" __declspec(dllexport)
void EraseDirectory(CString sPath)
{
        CFileFind finder;
	CString  sFullPath = sPath + "\\*.*";

        BOOL bWorking = finder.FindFile(sFullPath);
        while (bWorking)
        {
        	bWorking = finder.FindNextFile();
        	if (finder.IsDots()) continue;
        	if (finder.IsDirectory())
        	{
            		CString dirPath = finder.GetFilePath();
            		EraseDirectory(dirPath);
            		_rmdir(finder.GetFilePath());
            		continue;
         	}
         	_unlink( finder.GetFilePath() );
     	}
}

__declspec(dllexport)
CString SearchDirectory(CString dirPath, BOOL subDir, CString serachStr)
// This function searches all the files in the directory dirPath,
// for the file whose name contains the search string serachStr,
// searching recursively if subDir is TRUE 
{
	CFileFind finder;
	CString filePath, fileName, retval, sFileToFind;
	sFileToFind = dirPath + "\\*.*";
	BOOL found = finder.FindFile(sFileToFind);
	while (found) 
	{
		found = finder.FindNextFile();

		if (finder.IsDots()) continue;

		filePath = finder.GetFilePath();
		fileName = finder.GetFileName();
		fileName.MakeLower();
		if (fileName.Find(serachStr) != -1)
			return fileName;

		if (finder.IsDirectory()) 
		{
			if (subDir == TRUE)
			retval = SearchDirectory(filePath, TRUE, serachStr);
			return retval;
		}		
	}
	return fileName;
}

extern "C" __declspec(dllexport)
void CreateDirectories(CString instblobPath)
// Create appropriate platform and language directories
{
	CString rootPath, curVersion, instDirname, instFilename, fileExtension, 
			platformInfo, platformPath, extractPath, languageInfo, 
			languagePath, nscpxpiPath;
	int blobPathlen, findfilePos, finddirPos;
	char oldDir[MAX_SIZE];

	rootPath        = GetGlobal("Root");
	curVersion      = GetGlobal("Version");
	blobPathlen     = instblobPath.GetLength();
	finddirPos      = instblobPath.ReverseFind('\\');
	instDirname     = instblobPath.Left(finddirPos);
	instFilename    = instblobPath.Right(blobPathlen - finddirPos - 1);
	findfilePos     = instFilename.Find('.');
	fileExtension   = instFilename.Right(instFilename.GetLength()- findfilePos - 1);

	if (fileExtension == "tar.gz")
		platformInfo = "Linux";
	else if (fileExtension == "exe")
		platformInfo = "Windows";
	else if (fileExtension == "zip")
		platformInfo = "Mac OS";

	platformPath = rootPath + "Version\\" + curVersion + "\\" + platformInfo;
	extractPath  = platformPath + "\\" + "temp";

	if (GetFileAttributes(platformPath) == -1)
	// platform directory does not exist
		_mkdir(platformPath);
	_mkdir(extractPath);
	GetCurrentDirectory(sizeof(oldDir), oldDir);
	SetCurrentDirectory((char *)(LPCTSTR) instDirname);
	ExtractContents(rootPath, instblobPath, instFilename, platformInfo, 
		platformPath, extractPath);
	CString searchStr = "defl";
	searchStr = SearchDirectory(extractPath, TRUE, searchStr);

	if (searchStr != "")
	{
		languageInfo   = searchStr.Mid(4,4);
		languagePath   = rootPath + "Version\\" + curVersion + "\\" + 
			platformInfo + "\\" + languageInfo;
		nscpxpiPath    = languagePath + "\\Nscpxpi";
		_mkdir(languagePath);
		_mkdir(nscpxpiPath);
		PopulateNscpxpi(rootPath, platformInfo, nscpxpiPath, extractPath);
	}		
	EraseDirectory(extractPath);
	RemoveDirectory(extractPath);
	SetCurrentDirectory(oldDir);
}

void ExtractContents(CString rootPath, CString instblobPath, 
					 CString instFilename, CString platformInfo, 
					 CString platformPath, CString extractPath)
// Extract contents of blob installer
{
	CString command, quotes = "\"", spaces = " ";
	char originalDir[MAX_SIZE];

	if (platformInfo == "Linux")
	{
		extractPath.Replace("\\","/");
		extractPath.Replace(":","");
		extractPath.Insert(0,"/cygdrive/");
		command = "tar -zxvf " + instFilename + " -C " + quotes + 
			extractPath + quotes;
		ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
	}
	else if (platformInfo == "Windows")
	{
		GetCurrentDirectory(sizeof(originalDir), originalDir);
		CopyFile(instFilename, extractPath+"\\"+instFilename, FALSE);
		SetCurrentDirectory((char *)(LPCTSTR) extractPath);
		command = instFilename + " -u";
		ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
		DeleteFile(extractPath+"\\"+instFilename);
		SetCurrentDirectory(originalDir);
	}
	else if (platformInfo == "Mac OS")
	{
		command = quotes + rootPath + "unzip.exe"+ quotes + "-od" + spaces 
			+ quotes + platformPath + quotes + spaces + quotes + instblobPath
			+ quotes; 
		ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
	}

}

void PopulateNscpxpi(CString rootPath, CString platformInfo, 
					 CString nscpxpiPath, CString extractPath)
// Populate Nscpxpi directory with appropriate installer files
{
	CString command, 
			quotes = "\"", 
			spaces = " ",
			nscpzipFile = nscpxpiPath + "\\N6Setup.zip";
	if (platformInfo == "Linux")
	{
		CString strNscpInstaller = "\\netscape-installer";
		_mkdir(nscpxpiPath+strNscpInstaller);
		CopyDirectory(extractPath+strNscpInstaller+"\\xpi", nscpxpiPath, TRUE);
		CopyDirectory(extractPath+strNscpInstaller, nscpxpiPath+strNscpInstaller, 
			FALSE);
		CopyFile(nscpxpiPath+strNscpInstaller+"\\Config.ini", 
			nscpxpiPath+"\\Config.ini", FALSE);
	}
	else if (platformInfo == "Windows")
	{
		CopyDirectory(extractPath, nscpxpiPath, TRUE);
		command = quotes + rootPath + "zip.exe" + quotes + " -jm " + 
			quotes + nscpzipFile + quotes + spaces + quotes + 
			nscpxpiPath + "\\*.exe" + quotes + spaces + quotes + 
			nscpxpiPath + "\\*.txt" + quotes + spaces + quotes +
			nscpxpiPath + "\\*.dll" + quotes + spaces + quotes +
			nscpxpiPath + "\\install.ini" + quotes;
		ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
		command = quotes + rootPath + "zip.exe" + quotes + " -j " + 
			quotes + nscpzipFile + quotes + spaces + quotes + 
			nscpxpiPath + "\\config.ini" + quotes;
		ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
	}
}

__declspec(dllexport)
CString GetModulePath()
{
        char currPath[MID_SIZE];
        int     i,numBytes;

        // Get the path of the file that was executed
        numBytes = GetModuleFileName(NULL, currPath, MIN_SIZE);

        // get the cmd path
        // Remove the filename from the path
        for (i=numBytes-1;i >= 0 && currPath[i] != '\\';i--);
        // Terminate command line with 0
        if (i >= 0)
                currPath[i+1]= '\0';

        return CString(currPath);
}
