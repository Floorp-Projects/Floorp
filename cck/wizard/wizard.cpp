#include "stdafx.h"
#include <Winbase.h>
#include <direct.h>
#include "globals.h"
#include "NewConfigDialog.h"
#include "NewDialog.h"
//#include <stdio.h>
//#include <string.h>
//#include <stdlib.h>
#include <iostream.h>
#include <fstream.h>
//#include <windows.h>
//#include <ctype.h>

CString DlgTitle = "";
CString rootpath = GetGlobal("Root");
CString Clist = GetGlobal("CustomizationList");

extern "C" __declspec(dllexport)
void NewNCIDialog(CString parms, WIDGET* curWidget)
{

	CString entryName;
	CNewDialog newDlg;
	newDlg.DoModal();
	entryName = newDlg.GetData();
	SetGlobal("_NewNCIFileName", entryName);

}


extern "C" __declspec(dllexport)
BOOL Config(CString globalsName, CString DialogTitle, WIDGET* curWidget) 
{
	// This doesn't really belong here...
	WIN32_FIND_DATA data;
	HANDLE d;

	CNewConfigDialog newDlg;
	if (!DialogTitle.IsEmpty())
		DlgTitle = "Create Copy";
	else 
		DlgTitle = "";
	newDlg.DoModal();
	CString configField = newDlg.GetConfigName();
	CString newDir = rootpath + "Configs\\" ;
	newDir += configField;
	CString Template = rootpath + "WSTemplate" ;
	CString FooCopy = rootpath + "Configs\\";
	FooCopy += Clist ;
	d = FindFirstFile((const char *) newDir, &data);
	if (configField.IsEmpty())
		return FALSE;
	if (d == INVALID_HANDLE_VALUE)
	{
		if (!DialogTitle.IsEmpty())
			CopyDir(FooCopy,newDir,NULL, FALSE);
		else 
			CopyDir(Template,newDir,NULL, FALSE);
	}
	else
	{
		CWnd myWnd;
		myWnd.MessageBox("That configuration already exists.", "Error", MB_OK);
		return FALSE;
	}

	CString targetWid = curWidget->target;
	WIDGET* tmpWidget = findWidget((char*) (LPCTSTR)curWidget->target);
	if (!tmpWidget)
		return FALSE;

	SetGlobal(globalsName, configField);
	SetGlobal(targetWid, configField);

	if(DialogTitle == "Create a Copy")
	{
		CString CurAnimPath = GetGlobal("LargeAnimPath");
		CString CurStillPath = GetGlobal("LargeStillPath");
		CString CurReadMePath = GetGlobal("ReadMeFile");
		CString CurBitmapPath = GetGlobal("ShellBgBitmap");
		CString CurInstallFilePath = GetGlobal("ShellInstallTextFile");

		CString OrigConfigName = GetGlobal("_FromConfigName");
		CString OrigConfigDir = rootpath + "Configs\\" + OrigConfigName;

		if(CurAnimPath.Find(OrigConfigDir) != -1)
		{
			//the file is located in the orig config dir that will be copied
			CurAnimPath.Replace(OrigConfigDir, newDir);
			//replace the old config's path with the new one
			SetGlobal("LargeAnimPath", CurAnimPath);
		}
		if(CurStillPath.Find(OrigConfigDir) != -1)
		{
			CurStillPath.Replace(OrigConfigDir, newDir);
			SetGlobal("LargeStillPath", CurStillPath);
		}
		if(CurReadMePath.Find(OrigConfigDir) != -1)
		{
			CurReadMePath.Replace(OrigConfigDir, newDir);
			SetGlobal("ReadMeFile", CurReadMePath);
		}
		if(CurBitmapPath.Find(OrigConfigDir) != -1)
		{
			CurBitmapPath.Replace(OrigConfigDir, newDir);
			SetGlobal("ShellBgBitmap", CurBitmapPath);
		}
		if(CurInstallFilePath.Find(OrigConfigDir) != -1)
		{
			CurInstallFilePath.Replace(OrigConfigDir, newDir);
			SetGlobal("ShellInstallTextFile", CurInstallFilePath);
		}

	}
	else
	{ 	
		// Making custom config point to files in its own workspace
		SetGlobal("LargeAnimPath", newDir + "\\Workspace\\AnimLogo\\animlogo32.gif");
		SetGlobal("LargeStillPath", newDir + "\\Workspace\\AnimLogo\\staticlogo32.gif");
		SetGlobal("ReadMeFile", newDir + "\\Workspace\\readme.txt");
		SetGlobal("ShellBgBitmap", newDir + "\\Workspace\\Autorun\\Shell\\bmps\\install.bmp");
		SetGlobal("ShellInstallTextFile", newDir + "\\Workspace\\Autorun\\install.txt");
	}

	IsSameCache = FALSE;

	return TRUE;
}

extern "C" __declspec(dllexport)
BOOL NewConfig(CString configname, WIDGET* curWidget)
{
	if (!Config(configname,"",curWidget))
		return FALSE;
	else
		return TRUE;
}
			
extern "C" __declspec(dllexport)
BOOL CopyConfig(CString configname, WIDGET* curWidget)
{
	if(!Config(configname,"Create a Copy",curWidget))
		return FALSE;
	else
		return TRUE;
}
extern "C" __declspec(dllexport)
BOOL CreateJSFile(void)
{
	CString JSFile = rootpath + "Configs\\" + Clist +"\\Temp\\prefs.jsc";

	ofstream myout(JSFile);

	if(!myout) 
	{
		cout << "cannot open the file \n";
	}
	
	myout<< "with (PrefConfig) { \n";
	char* attribArray[MAX_SIZE];
	int k =GetAttrib("Pref", attribArray);
	int g= 0;
	while(g<k)
	{
		myout<< "defaultPref(\"" << attribArray[g] << "\", " <<GetGlobal(attribArray[g])<<");\n";
		g++;
	}

	myout<<"}";
	myout.close();
	return TRUE;
}
