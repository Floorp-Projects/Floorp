#include "stdafx.h"
#include "WizardTypes.h"
#include "winbase.h"  // for CopyDir
#include <direct.h>

__declspec(dllexport) WIDGET GlobalWidgetArray[1000];
__declspec(dllexport) int GlobalArrayIndex=0;
__declspec(dllexport) BOOL IsSameCache = TRUE;

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
	CString filePath;
	CString fileName;
	CString retval;
	CString sFileToFind = dirPath + "\\*.*";
	BOOL found = finder.FindFile(sFileToFind);
	while (found) 
	{
		found = finder.FindNextFile();

		if (finder.IsDots()) continue;

		filePath = finder.GetFilePath();
		fileName = finder.GetFileName();
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
	CString quotes          = "\"";
	CString rootPath        = GetGlobal("Root");
	CString curVersion      = GetGlobal("Version");
	int instblobPathlen     = instblobPath.GetLength();
	int findfilePos         = instblobPath.Find('.');
	int finddirPos          = instblobPath.ReverseFind('\\');
	CString instDirname     = instblobPath.Left(finddirPos);
	CString instFilename    = instblobPath.Right(instblobPathlen - finddirPos -1);
	CString fileExtension   = instblobPath.Right(instblobPathlen - findfilePos -1);

	// Is the blob path a Linux blob installer
	if (fileExtension == "tar.gz")
	{
		char oldDir[MAX_SIZE];
		CString platformInfo = "Linux";
		CString platformPath = rootPath + "Version\\" + curVersion + "\\" + platformInfo;
		CString extractPath  = platformPath + "\\" + "temp";
		CString tempPath     = extractPath;

		if (GetFileAttributes(platformPath) == -1)
		// platform directory does not exist
			_mkdir(platformPath);

		// extract contents of Linux blob installer
		_mkdir(extractPath);
		GetCurrentDirectory(sizeof(oldDir), oldDir);
		SetCurrentDirectory((char *)(LPCTSTR) instDirname);
		tempPath.Replace("\\","/");
		tempPath.Replace(":","");
		tempPath.Insert(0,"/cygdrive/");
		CString command = "tar -zxvf " + instFilename + " -C " + quotes + tempPath + quotes;
		ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
		CString searchStr = "defl";
		searchStr = SearchDirectory(extractPath, TRUE, searchStr);

		if (searchStr != "")
		{
			CString languageInfo = searchStr.Mid(4,4);
			CString languagePath = rootPath + "Version\\" + curVersion + "\\" + platformInfo + "\\" + languageInfo;
			CString nscpxpiPath  = languagePath + "\\Nscpxpi";
						
			_mkdir(languagePath);
			_mkdir(nscpxpiPath);

			// checking if nscpxpi directory is non-empty  
			// to avoid the steps of populating installer files 
			BOOL empty = TRUE;
			CFileFind fn;
			if (fn.FindFile(nscpxpiPath+"\\*.*") != 0)
			{
				while (TRUE)
				{
					int fileExist = fn.FindNextFile();
					if (!fn.IsDots()) 
					{
						empty = FALSE;
						break;
					}
					if (fileExist == 0) break;
				}
			}
			if (empty)
			{
				CString nsinstallerStr = "\\netscape-installer";
				_mkdir(nscpxpiPath+nsinstallerStr);
				CopyDirectory(extractPath+nsinstallerStr+"\\xpi",
					nscpxpiPath, TRUE);
				CopyDirectory(extractPath+nsinstallerStr, 
					nscpxpiPath+nsinstallerStr, FALSE);
				CopyFile(nscpxpiPath+nsinstallerStr+"\\Config.ini", 
					nscpxpiPath+"\\Config.ini", FALSE);
				CopyFile(rootPath+"script_linux.ib", 
					languagePath+"\\script.ib", FALSE);
			}		
		}
		EraseDirectory(extractPath);
		RemoveDirectory(extractPath);
		SetCurrentDirectory(oldDir);
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
