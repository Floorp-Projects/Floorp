#include "stdafx.h"
/*
#include <Winbase.h>
#include <direct.h>
*/
#include "globals.h"
#include "ib.h"

#define MAX_SIZE 1024

extern "C" __declspec(dllexport)
int GenerateComponentList(CString parms, WIDGET *curWidget)
{
	CString rootPath	= GetGlobal("Root");
	CString configName	= GetGlobal("CustomizationList");
	CString configPath  = rootPath + "Configs\\" + configName;
	CString workspacePath = configPath + "\\Workspace";
	CString nscpxpiPath;
	if (SearchPath(workspacePath, "NSCPXPI", NULL, 0, NULL, NULL))
		nscpxpiPath = workspacePath + "\\NSCPXPI";
	else
		nscpxpiPath = rootPath + "NSCPXPI";
	CString iniPath		= nscpxpiPath + "\\config.ini";

	// Get all the component info from each component section
	int i=0;
	int optNum = 0;
	CString component;
	char archive[MAX_SIZE];
	char name[MAX_SIZE];
	char desc[MAX_SIZE];
	char attr[MAX_SIZE];
	CString WidgetValue("");
	component.Format("Component%d", i);
	GetPrivateProfileString(component, "Archive", "", archive, MAX_SIZE, iniPath);
	while (*archive)
	{
		// Organize all the data into the checklistbox widget data sections
		//   - be sure to look at selected and visible attributes
		GetPrivateProfileString(component, "Description Short", "", name, MAX_SIZE, iniPath);
		GetPrivateProfileString(component, "Description Long", "", desc, MAX_SIZE, iniPath);
		GetPrivateProfileString(component, "Attributes", "", attr, MAX_SIZE, iniPath);

		curWidget->options.name[optNum] = new char[strlen(component)+1];
		strcpy(curWidget->options.name[optNum], component);

		curWidget->options.value[optNum] = new char[strlen(name)+1];
		strcpy(curWidget->options.value[optNum], name);

		curWidget->optDesc.name[optNum]  = new char[strlen(component)+1];
		strcpy(curWidget->optDesc.name[optNum], component);

		curWidget->optDesc.value[optNum] = new char[strlen(desc)+1];
		strcpy(curWidget->optDesc.value[optNum], desc);

		// INVISIBLE just means not selected, let user decide whether to
		// include them again.  SELECTED components get checkmarks.
		if (strstr(attr, "SELECTED"))
		{
			WidgetValue += ",";
			WidgetValue += name;
		}

		optNum++;

		component.Format("Component%d", ++i);
		GetPrivateProfileString(component, "Archive", "", archive, MAX_SIZE, iniPath);
	}

	curWidget->numOfOptions = optNum;
	curWidget->numOfOptDesc = optNum;

	if (curWidget->value.IsEmpty())
		curWidget->value = WidgetValue;

	// Force it to redraw somehow???

	return TRUE;
}

