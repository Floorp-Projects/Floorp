#include "stdafx.h"
#include <Winbase.h>
#include "globals.h"

#define MAX_SIZE 1024

extern "C" __declspec(dllexport)
int StartIB(CString parms)
{
	CString root   = GetGlobal("Root");
	CString config = GetGlobal("CustomizationList");

	CString fromPath = root + "\\Installer";
	CString destPath = root + "\\Configs\\" + config + "\\Output";

	// Copy default installer files into config
	CopyDir(fromPath, destPath,"NULL");

	// Update config.ini with new content
	CString inst_text1 = GetGlobal("InstallerScreenText1");
	CString programPath = CString("C:\\Program Files\\Netscape\\Communicator");
	CString configINI	= CString(destPath + "\\config.ini");

	WritePrivateProfileString( "General", "Product Name", inst_text1, configINI);
	WritePrivateProfileString( "General", "Path", programPath, configINI);
	WritePrivateProfileString( "General", "Program Folder Name", config, configINI);

	return TRUE;
}
