#include "stdafx.h"
#include "globals.h"
#include "fstream.h"
#include "direct.h"
#include <Winbase.h>
#include <direct.h>
#include "comp.h"
#include "ib.h"

extern CString rootPath;//	= GetGlobal("Root");
extern CString configName;//	= GetGlobal("CustomizationList");
extern CString configPath;//  = rootPath + "Configs\\" + configName;
extern CString cdPath ;//		= configPath + "\\CD";
extern CString workspacePath;// = configPath + "\\Workspace";
extern CString cdshellPath;
//CString CDout="CD_output";

void CreateRshell (void)
{
	CString root   = GetGlobal("Root");
	CString config = GetGlobal("CustomizationList");
	CString file1 = root + "\\part1.ini";
	CString file2 = root + "\\part2.ini";
	CString rshellPath = root + "\\Configs\\" + config + "\\Output\\Shell\\Nsetup32\\";

	CWnd Mywnd;

//		Mywnd.MessageBox(CString(iniFilePath),iniFilePath,MB_OK);

//		Mywnd.MessageBox(CString(customizationPath),customizationPath,MB_OK);

	ifstream part1(file1);
	ifstream part2(file2);
	_mkdir (rshellPath);
	CString Rsh = rshellPath +"rshell.ini";
//	FILE* rshell = theApp.OpenAFile(CDdir +"rshell.ini", "w");

	ofstream rshell(Rsh);
	CString captionValue = "Netscape 6.2" ;
	CString netscapeValue = "Netscape by ";
	CString fvalue1=GetGlobal("ShellTitleText");
	CString fvalue2=GetGlobal("ShellBgBitmap");
	CString fvalue3=GetGlobal("ShellBelowTitleText");
	CString fvalue4=GetGlobal("ShellInstallTextFile");
	
	if (fvalue1.IsEmpty())
		;
	else 
		captionValue = netscapeValue + fvalue1;
	char jsprefname[200];

	if(!part1) {
		cout << "cannot open the file \n";
		}
	while (!part1.eof()) {
	
		part1.getline(jsprefname,200);
//		fprintf(globs, jsprefname);
//		fprintf(globs, "\n");

		rshell <<jsprefname<<"\n";
	}
	rshell <<"caption="<<captionValue<<"\n";
//	rshell <<"bk_bitmap="<<fvalue2<<"\n";
//	rshell <<"button2_cmdline=exe,"<<fvalue4<<"\n";

	rshell <<"dialog_title_text="<<fvalue3<<"\n";
	if(!part2) {
		cout << "cannot open the file \n";
		}
	while (!part2.eof()) {
	
		part2.getline(jsprefname,200);
		rshell <<jsprefname<<"\n";
	}
	rshell.close();
	CString bmpdest = cdshellPath + "\\bmps\\Install.bmp";
	CString txtdest = configPath + "\\Output\\install.txt";
	CopyFile(fvalue2,bmpdest,FALSE);
	CopyFile(fvalue4,txtdest,FALSE);
	// Deleting the part1.ini and part2.ini files. 
	DeleteFile(rshellPath + "part1.ini");
	DeleteFile(rshellPath + "part2.ini");
}
