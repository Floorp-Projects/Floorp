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
extern int		numComponents;

extern "C" __declspec(dllexport)
int BuildComponentList(COMPONENT *comps, int *compNum, CString iniSrcPath,int invisibleCount)
{
	*compNum = 0;
	int invNum = *compNum;

	// Get all the component info from each component section
	CString component;
	char archive[MAX_SIZE];
	char name[MAX_SIZE];
	char desc[MAX_SIZE];
	char attr[MAX_SIZE];
	component.Format("Component%d", *compNum);
	GetPrivateProfileString(component, "Archive", "", archive, MAX_SIZE, iniSrcPath);
	while (*archive)
	{
		GetPrivateProfileString(component, "Description Short", "", 
			name, MAX_SIZE, iniSrcPath);
		GetPrivateProfileString(component, "Description Long", "", 
			desc, MAX_SIZE, iniSrcPath);
		GetPrivateProfileString(component, "Attributes", "", 
			attr, MAX_SIZE, iniSrcPath);

		comps[*compNum].archive  = CString(archive);
		comps[*compNum].compname = component;
		comps[*compNum].name 	 = CString(name);
		comps[*compNum].desc 	 = CString(desc);
		comps[*compNum].selected = (strstr(attr, "SELECTED") != NULL);
		comps[*compNum].invisible = (strstr(attr, "INVISIBLE") != NULL);
		comps[*compNum].launchapp = (strstr(attr, "LAUNCHAPP") != NULL);
		comps[*compNum].additional = (strstr(attr, "ADDITIONAL") != NULL);

		
		if (!(comps[*compNum].selected && comps[*compNum].invisible && invisibleCount))
		{
			(*compNum)++;
			invNum++;
			component.Format("Component%d", invNum);
		}
		else
		{
			invNum++;
			component.Format("Component%d", invNum);
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

	BuildComponentList(Components, &numComponents, iniSrcPath, 1);

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
