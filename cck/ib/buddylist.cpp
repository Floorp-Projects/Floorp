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

void CreateBuddyList(void)
{
	CString root   = GetGlobal("Root");
	CString config = GetGlobal("CustomizationList");
	CString BLTPath = root + "\\Configs\\" + config + "\\Temp";

	_mkdir (BLTPath);
	CString BLTFile = BLTPath +"buddy_list.blt";

	ofstream Blt(BLTFile);

	CString BuddyGroup=GetGlobal("BuddyGroup");
	CString Buddy1=GetGlobal("Buddy1");
	CString Buddy2=GetGlobal("Buddy2");
	CString Buddy3=GetGlobal("Buddy3");
	CString Buddy4=GetGlobal("Buddy4");
	CString Buddy5=GetGlobal("Buddy5");

	Blt <<"Config { \n version 1 \n} \nUser { \n screenName newuser \n} \nBuddy { \n list {\n";
	Blt <<"  "<<BuddyGroup<<" {\n";
	Blt <<"   \""<<Buddy1<<"\"\n";
	Blt <<"   \""<<Buddy2<<"\"\n";
	Blt <<"   \""<<Buddy3<<"\"\n";
	Blt <<"   \""<<Buddy4<<"\"\n";
	Blt <<"   \""<<Buddy5<<"\"\n";
	Blt <<"  }\n }\n}";

	Blt.close();
}
