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

void CreateHelpMenu (void)
{
	CString root   = GetGlobal("Root");
	CString config = GetGlobal("CustomizationList");
	CString file1 = root + "\\help1.txt";
	CString file2 = root + "\\help2.txt";
	CString HelpPath = root + "\\Configs\\" + config + "\\Temp\\";

	ifstream help1(file1);
	ifstream help2(file2);
	_mkdir (HelpPath);
	CString HelpMenuFile = HelpPath +"helpMenu.rdf";

	ofstream Hlp(HelpMenuFile);
	CString HelpMenuName = GetGlobal("HelpMenuCommandName");
	CString HelpMenuUrl = GetGlobal("HelpMenuCommandURL");
	
	if (HelpMenuName.IsEmpty())
		HelpMenuName = config + "-Help";

	char jsprefname[200];

	if(!help1) {
		cout << "cannot open the file \n";
		}
	while (!help1.eof()) {
	
		help1.getline(jsprefname,200);

		Hlp <<jsprefname<<"\n";
	}
	Hlp <<"     <RDF:li resource=\"urn:helpmenu:customizedHelp\" />\n";

//	Hlp <<"<menuitem position=\"7\" value=\""<<HelpMenuName<<"\"\n\t";
//	Hlp <<"oncommand=\"openTopWin('"<<HelpMenuUrl<<"')\" /> \n\t";
//	Hlp <<"<menuseparator position=\"9\" /> \n";

	if(!help2) {
		cout << "cannot open the file \n";
		}
	while (!help2.eof()) {
	
		help2.getline(jsprefname,200);
		Hlp <<jsprefname<<"\n";
	}

	Hlp <<"<RDF:Description about=\"urn:helpmenu:customizedHelp\">\n";
	Hlp	<<"	<NC:level>1</NC:level> \n";
	Hlp <<"	<NC:title>"<<HelpMenuName<<"</NC:title> \n";
	Hlp <<"	<NC:content>openTopWin('"<<HelpMenuUrl<<"')</NC:content>\n";
	Hlp <<"</RDF:Description>\n";
	Hlp <<"</RDF:RDF>\n";


	Hlp.close();
}
