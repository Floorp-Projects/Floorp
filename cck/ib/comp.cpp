#include "stdafx.h"
#include "globals.h"
#include "ib.h"
#include "comp.h"

#define MAX_SIZE 1024

extern CString rootPath;
extern CString configName;
extern CString configPath;
extern CString workspacePath;
extern CString cdPath;
extern CString tempPath;
extern CString iniSrcPath;
extern CString iniDstPath;
extern CString scriptPath;
extern CString nscpxpiPath;

extern COMPONENT Components[100];
extern int numComponents;
extern CString compString;

extern "C" __declspec(dllexport)
int BuildComponentList(COMPONENT *comps, CString compString, int& compNum, CString iniSrcPath,int invisibleCount)
{
	CString configcompString[50];
	compNum=0;

	configcompString[0]="Marker Recommended Start";
	configcompString[1]="Marker Full Start";
	configcompString[2]="Marker Custom Start";
	configcompString[3]="XPCOM";
	configcompString[4]="Navigator";
	configcompString[5]="Java";
	configcompString[6]="MailNews";
	configcompString[7]="Instant Messenger";
	configcompString[8]="QFA";
	configcompString[9]="PSM";
	configcompString[10]="Spell Checker";
	configcompString[11]="AOL Art";
	configcompString[12]="Net2Phone";
	configcompString[13]="Flash";
	configcompString[14]="Uninstaller";
	configcompString[15]="AOD";
	configcompString[16]="RealPlayer";
	configcompString[17]="Winamp";
	configcompString[18]="US English Profile Defaults";
	configcompString[19]="HP Printer Plugin";
	configcompString[20]="Classic Skin";
	configcompString[21]="En US lang pack";
	configcompString[22]="US region pack";
	configcompString[23]="Marker Recommended";
	configcompString[24]="Marker Full End";
	configcompString[25]="Marker Custom End";
	configcompString[26]=" ";

	int arrayindex=compNum;
	compString = configcompString[compNum];
	CString invNum = compString;

	// Get all the component info from each component section
	CString component;
	char archive[MAX_SIZE];
	char name[MAX_SIZE];
	char desc[MAX_SIZE];
	char attr[MAX_SIZE];
	
	component.Format("Component %s", compString);

	GetPrivateProfileString(component, "Archive", "", archive, MAX_SIZE, iniSrcPath);
	while (*archive)
	{
		GetPrivateProfileString(component, "Description Short", "", 
			name, MAX_SIZE, iniSrcPath);
		GetPrivateProfileString(component, "Description Long", "", 
			desc, MAX_SIZE, iniSrcPath);
		GetPrivateProfileString(component, "Attributes", "", 
			attr, MAX_SIZE, iniSrcPath);

		comps[compNum].archive  = CString(archive);
		comps[compNum].compname = component;
		comps[compNum].name 	 = CString(name);
		comps[compNum].desc 	 = CString(desc);
		comps[compNum].selected = (strstr(attr, "SELECTED") != NULL);
		comps[compNum].invisible = (strstr(attr, "INVISIBLE") != NULL);
		comps[compNum].launchapp = (strstr(attr, "LAUNCHAPP") != NULL);
		comps[compNum].additional = (strstr(attr, "ADDITIONAL") != NULL);
		comps[compNum].disabled = (strstr(attr, "DISABLED") != NULL);
		comps[compNum].forceupgrade = (strstr(attr, "FORCE_UPGRADE") != NULL);
		
		if (!(comps[compNum].selected && comps[compNum].invisible && invisibleCount))
		{
			compNum++;
			compString = configcompString[compNum];
			arrayindex++;
			invNum = configcompString[arrayindex];
			component.Format("Component %s", invNum);
		}
		else
		{
			arrayindex++;
			invNum=configcompString[arrayindex];
			component.Format("Component %s", invNum);
		}
		GetPrivateProfileString(component, "Archive", "", archive, MAX_SIZE, iniSrcPath);
	}

	return TRUE;
}

extern "C" __declspec(dllexport)
int GenerateComponentList(CString parms, WIDGET *curWidget)
{
	rootPath	= GetGlobal("Root");
	configName	= GetGlobal("CustomizationList");
	configPath  = rootPath + "Configs\\" + configName;
	workspacePath = configPath + "\\Workspace";
	nscpxpiPath;
	if (SearchPath(workspacePath, "NSCPXPI", NULL, 0, NULL, NULL))
		nscpxpiPath = workspacePath + "\\NSCPXPI";
	else
		nscpxpiPath = rootPath + "NSCPXPI";
	iniSrcPath		= nscpxpiPath + "\\config.ini";

	BuildComponentList(Components, compString, numComponents, iniSrcPath, 1);
	
	int i;
	CString WidgetValue("");
	for (i=0; i<numComponents; i++)
	{
		curWidget->options.name[i] = new char[strlen(Components[i].compname)+1];
		strcpy(curWidget->options.name[i], Components[i].compname);

		curWidget->options.value[i] = new char[strlen(Components[i].name)+1];
		strcpy(curWidget->options.value[i], Components[i].name);

		curWidget->optDesc.name[i]  = new char[strlen(Components[i].compname)+1];
		strcpy(curWidget->optDesc.name[i], Components[i].compname);

		curWidget->optDesc.value[i] = new char[strlen(Components[i].desc)+1];
		strcpy(curWidget->optDesc.value[i], Components[i].desc);

		// INVISIBLE just means not selected, let user decide whether to
		// include them again.  SELECTED components get checkmarks.
		if (Components[i].selected)
		{
			WidgetValue += ",";
			WidgetValue += Components[i].name;
		}
	}

	curWidget->numOfOptions = numComponents;
	curWidget->numOfOptDesc = numComponents;

	if (curWidget->value.IsEmpty())
		curWidget->value = WidgetValue;

	return TRUE;
}
