#include "stdafx.h"
#include <Winbase.h>
#include <direct.h>
#include "globals.h"
#include "NewConfigDialog.h"
#include "NewDialog.h"

CString DlgTitle = "";

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

	CString rootpath = GetGlobal("Root");

	CString newDir = rootpath + "Configs\\" ;
	CString Clist = GetGlobal("CustomizationList");
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
	
	IsSameCache = FALSE;

	return TRUE;
}

extern "C" __declspec(dllexport)
void NewConfig(CString configname, WIDGET* curWidget)
{
	Config(configname,"",curWidget);
}
			
extern "C" __declspec(dllexport)
void CopyConfig(CString configname, WIDGET* curWidget)
{
	Config(configname,"Create a Copy",curWidget);
}
