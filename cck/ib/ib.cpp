#include "stdafx.h"
#include <Winbase.h>
#include "globals.h"

#define MAX_SIZE 1024

extern "C" __declspec(dllexport)
int StartIB(CString parms)
{
	CString root = GetGlobal("Root");

	return TRUE;
}

#if 0
extern "C" __declspec(dllexport)
int StartIB_old(CString parms)
{
	char installPath[MAX_SIZE];
	char destPath[MAX_SIZE];
	char curSrcFile[MAX_SIZE];
	char curDestFile[MAX_SIZE];

	// Added because wizmach had these global
	CString CachePath;
	CString Path;

	// Create config.ini here
	// Read from cck.che and write the relavant stuff to config.ini

	char inst_text1[MAX_SIZE] = { '\0' };
	char path[MAX_SIZE]= { '\0' };
	char config[MAX_SIZE]= { '\0' };

	/* Get globals??? */
	GetPrivateProfileString("data", "InstallerScreenText1", "", inst_text1, MAX_SIZE, (LPCTSTR) CachePath);
	GetPrivateProfileString("data", "Root", "", path, MAX_SIZE, (LPCTSTR) CachePath);
	GetPrivateProfileString("data", "CustomizationList", "", config, MAX_SIZE, (LPCTSTR) CachePath);


	int extractPosition = CachePath.ReverseFind('\\');
	extractPosition++;
	CString configPath = CachePath.Left(extractPosition);;

	//windows path--Path
	//dest path--configPath

	// Setting up the install path
	strcpy(installPath, (char *) (LPCTSTR) Path);
	strcat(installPath, "install\\");

	// Setting up the destination Path
	strcpy(destPath, (char *) (LPCTSTR) configPath);


	/* CopyDir??? */
	strcpy(curSrcFile, installPath);
	strcat(curSrcFile, "config.ini");
	strcpy(curDestFile, destPath);
	strcat(curDestFile, "config.ini");
	CopyFile(curSrcFile, curDestFile, FALSE);

	strcpy(curSrcFile, installPath);
	strcat(curSrcFile, "setup.exe");
	strcpy(curDestFile, destPath);
	strcat(curDestFile, "setup.exe");
	CopyFile(curSrcFile, curDestFile, FALSE);

	strcpy(curSrcFile, installPath);
	strcat(curSrcFile, "setuprsc.dll");
	strcpy(curDestFile, destPath);
	strcat(curDestFile, "setuprsc.dll");
	CopyFile(curSrcFile, curDestFile, FALSE);

			
	/* Update config.ini with new content */
	WritePrivateProfileString( "General", "Product Name", inst_text1, configPath + "\\config.ini");
	WritePrivateProfileString( "General", "Path", path, configPath + "\\config.ini");
	WritePrivateProfileString( "General", "Program Folder Name", config, configPath + "\\config.ini");

	return TRUE;
}
#endif
