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


	_mkdir (HelpPath);
	CString HelpMenuFile = HelpPath +"helpMenu.rdf";
	CString HelpMenuName = GetGlobal("HelpMenuCommandName");
	CString HelpMenuUrl = GetGlobal("HelpMenuCommandURL");
	
	if ( !(HelpMenuName.IsEmpty()) && !(HelpMenuUrl.IsEmpty()) )
	{
		ifstream help1(file1);
		ifstream help2(file2);
		ofstream Hlp(HelpMenuFile);
		char jsprefname[200];

		if(!help1) 
		{
			cout << "cannot open the file \n";
		}
		while (!help1.eof()) 
		{
			help1.getline(jsprefname,200);
			Hlp <<jsprefname<<"\n";
		}

		Hlp << "	<menuitem label=\"" << HelpMenuName << "\"\n"; 
		Hlp << "	 position=\"6\"\n"; 
		Hlp << "	 oncommand=\"openTopWin('" << HelpMenuUrl << "');\" />\n";

	//	Hlp <<"<menuitem position=\"7\" value=\""<<HelpMenuName<<"\"\n\t";
	//	Hlp <<"oncommand=\"openTopWin('"<<HelpMenuUrl<<"')\" /> \n\t";
	//	Hlp <<"<menuseparator position=\"9\" /> \n";

		if(!help2) 
		{
			cout << "cannot open the file \n";
		}
		while (!help2.eof()) 
		{
			help2.getline(jsprefname,200);
			Hlp <<jsprefname<<"\n";
		}

		Hlp.close();
	}
}

// This function creates the file "mailaccount.rdf" to customize the Mail account
void CreateIspMenu(void)
{
	CString root   = GetGlobal("Root");
	CString config = GetGlobal("CustomizationList");
	CString IspPath = root + "\\Configs\\" + config + "\\Temp\\";//ISpPath=CCKTool\Configs\configname\Temp

	_mkdir (IspPath);
	CString ispDomainName = GetGlobal("DomainName");
	CString ispPrettyName = GetGlobal("PrettyName");
	CString ispLongName = GetGlobal("LongName");
	CString ispInServer = GetGlobal("IncomingServer");
	CString ispOutServer = GetGlobal("OutgoingServer");
	CString ispPortNumber = GetGlobal("PortNumber");
	CString serverType; 
	// Determine whether the server type is POP or IMAP
	CString pop = GetGlobal("pop");
	if (pop == "1")
		serverType = "pop3";
	else
		serverType = "imap";
	CString popMessage = GetGlobal("PopMessages");
	// check if "leave pop messages on server" option is set
	if (popMessage == "0")	
		popMessage = "false";
	else if (popMessage == "1")
		popMessage = "true";
	
	// mailaccount.rdf file is created only if values are entered for all the fields in the CCK mail UI
	if (!( (ispDomainName.IsEmpty()) || (ispPrettyName.IsEmpty()) || (ispLongName.IsEmpty()) || (ispInServer.IsEmpty()) || (ispOutServer.IsEmpty()) || (ispPortNumber.IsEmpty()) ))
	{
		CString IspFile = IspPath +"mailaccount.rdf";
		ofstream Isp(IspFile);

		char *shortname;
		char tempdomain[25];
		strcpy(tempdomain,ispDomainName);
		shortname = strtok(tempdomain,".");

		if (!Isp) 
		{
			cout << "The file cannot be opened \n";
		}
		else
		{
	
			Isp <<"<\?xml version=\"1.0\"\?>\n";
			Isp <<"<RDF:RDF\n";
			Isp <<" xmlns:NC=\"http://home.netscape.com/NC-rdf#\"\n";
			Isp <<" xmlns:RDF=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n\n";
			Isp <<"<RDF:Description about=\"NC:ispinfo\">\n";
			Isp <<" <NC:providers>\n";
			Isp <<"  <NC:nsIMsgAccount about=\"domain:" << ispDomainName << "\">\n\n";

			Isp <<"   <!-- server info -->\n";
			Isp <<"   <NC:incomingServer>\n";
			Isp <<"    <NC:nsIMsgIncomingServer>\n";
			Isp <<"     <NC:prettyName>" << ispPrettyName << "</NC:prettyName>\n";
			Isp <<"     <NC:hostName>" << ispInServer << "</NC:hostName>\n";
			Isp <<"     <NC:type>" << serverType << "</NC:type>\n";
			Isp <<"     <NC:port>" << ispPortNumber << "</NC:port>\n";
			Isp <<"     <NC:rememberPassword>false</NC:rememberPassword>\n";
			if (serverType == "pop3")
			{
				Isp <<"     <NC:ServerType-pop3>\n";
				Isp <<"      <NC:nsIPopIncomingServer>\n";
				Isp <<"       <NC:leaveMessagesOnServer>" << popMessage << "</NC:leaveMessagesOnServer>\n";
				Isp <<"       <NC:deleteMailLeftOnServer>false</NC:deleteMailLeftOnServer>\n";
				Isp <<"      </NC:nsIPopIncomingServer>\n";
				Isp <<"     </NC:ServerType-pop3>\n";
			}
			Isp <<"    </NC:nsIMsgIncomingServer>\n";
			Isp <<"   </NC:incomingServer>\n\n";

			Isp <<"   <!-- identity defaults -->\n";
			Isp <<"   <NC:identity>\n";
			Isp <<"    <NC:nsIMsgIdentity>\n";
			Isp <<"     <NC:composeHtml>false</NC:composeHtml>\n";
			Isp <<"     <NC:bccSelf>false</NC:bccSelf>\n";
			Isp <<"    </NC:nsIMsgIdentity>\n";
			Isp <<"   </NC:identity>\n\n";

			Isp <<"   <NC:smtp>\n";
			Isp <<"    <NC:nsISmtpServer>\n";
			Isp <<"     <NC:hostname>" << ispOutServer << "</NC:hostname>\n";
			Isp <<"    </NC:nsISmtpServer>\n";
			Isp <<"   </NC:smtp>\n\n";

			Isp <<"   <NC:smtpRequiresUsername>true</NC:smtpRequiresUsername>\n";
			Isp <<"   <NC:smtpCreateNewServer>ture</NC:smtpCreateNewServer>\n";
			Isp <<"   <NC:smtpUsePreferredServer>true</NC:smtpUsePreferredServer>\n\n";

			Isp <<"   <NC:wizardSkipPanels>true</NC:wizardSkipPanels>\n";
			Isp <<"   <NC:wizardShortName>" << shortname << "</NC:wizardShortName>\n";
			Isp <<"   <NC:wizardLongName>" << ispLongName << "</NC:wizardLongName>\n";
			Isp <<"   <NC:wizardShow>true</NC:wizardShow>\n";
			Isp <<"   <NC:wizardPromote>true</NC:wizardPromote>\n";
			Isp <<"   <NC:emailProviderName>" << ispDomainName << "</NC:emailProviderName>\n";
			Isp <<"   <NC:sampleEmail>username@" << ispDomainName << "</NC:sampleEmail>\n";
			Isp <<"   <NC:sampleUserName>username</NC:sampleUserName>\n";
			Isp <<"   <NC:emailIDDescription>user name</NC:emailIDDescription>\n";
			Isp <<"   <NC:emailIDFieldTitle>User name:</NC:emailIDFieldTitle>\n";
			Isp <<"   <NC:showServerDetailsOnWizardSummary>false</NC:showServerDetailsOnWizardSummary>\n";
			Isp <<"  </NC:nsIMsgAccount>\n";
			Isp <<" </NC:providers>\n";
			Isp <<"</RDF:Description>\n\n";
			Isp <<"</RDF:RDF>\n";
		}
	Isp.close();
	}
}

// This function creates the file "newsaccount.rdf" to customize the News account
void CreateNewsMenu(void)
{
	CString root   = GetGlobal("Root");
	CString config = GetGlobal("CustomizationList");
	CString NewsPath = root + "\\Configs\\" + config + "\\Temp\\";//NewsPath=CCKTool\Configs\configname\Temp

	_mkdir (NewsPath);
	CString newsDomainName = GetGlobal("nDomainName");
	CString newsPrettyName = GetGlobal("nPrettyName");
	CString newsLongName = GetGlobal("nLongName");
	CString newsServer = GetGlobal("nServer");
	CString newsPortNumber = GetGlobal("nPortNumber");

	// newsaccount.rdf file is created only if values are entered for all the fields in the CCK News UI		
	if (!( (newsDomainName.IsEmpty()) || (newsPrettyName.IsEmpty()) || (newsLongName.IsEmpty()) || (newsServer.IsEmpty()) || (newsPortNumber.IsEmpty()) ))
	{
		CString NewsFile = NewsPath +"newsaccount.rdf";
		ofstream News(NewsFile);

		char *shortname;
		char tempdomain[25];
		strcpy(tempdomain,newsDomainName);
		shortname = strtok(tempdomain,".");

		if (!News) 
		{
			cout << "The file cannot be opened \n";
		}
		else
		{
			News <<"<\?xml version=\"1.0\"\?>\n";
			News <<"<RDF:RDF\n";
			News <<" xmlns:NC=\"http://home.netscape.com/NC-rdf#\"\n";
			News <<" xmlns:RDF=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n\n";
			News <<"<RDF:Description about=\"NC:ispinfo\">\n";
			News <<" <NC:providers>\n";
			News <<"  <NC:nsIMsgAccount about=\"domain:" << newsDomainName << "\">\n\n";

			News <<"   <!-- server info -->\n";
			News <<"   <NC:incomingServer>\n";
			News <<"    <NC:nsIMsgIncomingServer>\n";
			News <<"     <NC:prettyName>" << newsPrettyName << "</NC:prettyName>\n";
			News <<"     <NC:hostName>" << newsServer << "</NC:hostName>\n";
			News <<"     <NC:type>nntp</NC:type>\n";
			News <<"     <NC:port>" << newsPortNumber << "</NC:port>\n";
			News <<"     <NC:rememberPassword>false</NC:rememberPassword>\n";
			News <<"    </NC:nsIMsgIncomingServer>\n";
			News <<"   </NC:incomingServer>\n\n";

			News <<"   <!-- identity defaults -->\n";
			News <<"   <NC:identity>\n";
			News <<"    <NC:nsIMsgIdentity>\n";
			News <<"     <NC:composeHtml>false</NC:composeHtml>\n";
			News <<"     <NC:bccSelf>false</NC:bccSelf>\n";
			News <<"    </NC:nsIMsgIdentity>\n";
			News <<"   </NC:identity>\n\n";

			News <<"   <NC:wizardSkipPanels>true</NC:wizardSkipPanels>\n";
			News <<"   <NC:wizardShortName>" << shortname << "</NC:wizardShortName>\n";
			News <<"   <NC:wizardLongName>" << newsLongName << "</NC:wizardLongName>\n";
			News <<"   <NC:wizardShow>true</NC:wizardShow>\n";
			News <<"   <NC:wizardPromote>true</NC:wizardPromote>\n";
			News <<"   <NC:emailProviderName>" << newsDomainName << "</NC:emailProviderName>\n";
			News <<"   <NC:sampleEmail>username@" << newsDomainName << "</NC:sampleEmail>\n";
			News <<"   <NC:sampleUserName>username</NC:sampleUserName>\n";
			News <<"   <NC:emailIDDescription>user name</NC:emailIDDescription>\n";
			News <<"   <NC:emailIDFieldTitle>User name:</NC:emailIDFieldTitle>\n";
			News <<"   <NC:showServerDetailsOnWizardSummary>false</NC:showServerDetailsOnWizardSummary>\n";
			News <<"  </NC:nsIMsgAccount>\n";
			News <<" </NC:providers>\n";
			News <<"</RDF:Description>\n\n";
			News <<"</RDF:RDF>\n";
		}
	News.close();
	}
}
