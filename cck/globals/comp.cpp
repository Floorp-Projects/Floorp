#include "stdafx.h"
#include "globals.h"
#include "comp.h"
#include <direct.h>

#define MAX_SIZE 1024

CString rootPath;
CString configName;
CString configPath;
CString workspacePath;
CString cdPath;
CString tempPath;
CString iniSrcPath;
CString iniDstPath;
CString scriptPath;
CString nscpxpiPath;

COMPONENT Components[100];
int numComponents;

extern "C" __declspec(dllexport)
int BuildComponentList(COMPONENT *comps, int *compNum, CString iniSrcPath,
					   int invisibleCount)
{
	*compNum = 0;
	int invNum = *compNum;
	CString componentstr;
  bool bCalendarExists = FALSE;
  bool bCalendarInserted = FALSE;

  CString strCMRE = "Component Marker Recommended End";
  CString strCMFE = "Component Marker Full End";
  CString strCMCE = "Component Marker Custom End";

  
  // see if Calendar exists

  int iLastSlash = iniSrcPath.ReverseFind(_T('\\'));

  if (iLastSlash)
  {
    CString strCalendarXPIPath = iniSrcPath.Left(iLastSlash);
    CFileStatus fs;

    strCalendarXPIPath += "\\calendar.xpi";

    if (CFile::GetStatus(strCalendarXPIPath,fs)) // exists
      bCalendarExists = TRUE;
  }



	// Get all the component info from each component section
	char component[MAX_SIZE];
	char archive[MAX_SIZE];
	char name[MAX_SIZE];
	char desc[MAX_SIZE];
	char attr[MAX_SIZE];
	char tempcomponentstr[MAX_SIZE];
	
	componentstr.Format("C%d", *compNum);
	strcpy(tempcomponentstr, componentstr);

	GetPrivateProfileString("Setup Type2", tempcomponentstr, "", component, 
		MAX_SIZE, iniSrcPath);

	GetPrivateProfileString(component, "Archive", "", archive, MAX_SIZE, 
		iniSrcPath);

	while (*archive)
	{
		GetPrivateProfileString(component, "Description Short", "", 
			name, MAX_SIZE, iniSrcPath);
		GetPrivateProfileString(component, "Description Long", "", 
			desc, MAX_SIZE, iniSrcPath);
		GetPrivateProfileString(component, "Attributes", "", 
			attr, MAX_SIZE, iniSrcPath);


    if (!bCalendarInserted && bCalendarExists)
    {
      CString str = component;

      if (   str.Find(strCMRE) != -1
          || str.Find(strCMFE) != -1
          || str.Find(strCMCE) != -1) // found CMRE, CMFE, or CMCE
      {

        comps[*compNum].archive       = CString("calendar.xpi");
	      comps[*compNum].compname      = CString("Component Calendar");
	      comps[*compNum].name 	        = CString("Calendar");
	      comps[*compNum].desc 	        = CString("Calendar Client");

	      comps[*compNum].selected      = 
	      comps[*compNum].forceupgrade  = 
	      comps[*compNum].empty	        = 1;

    	  comps[*compNum].invisible     = 
	      comps[*compNum].launchapp     = 
	      comps[*compNum].additional    = 
	      comps[*compNum].disabled      = 
	      comps[*compNum].uncompress    = 
	      comps[*compNum].downloadonly  = 
	      comps[*compNum].unselected    = 0;

		    if (!(comps[*compNum].selected && comps[*compNum].invisible && 
			    invisibleCount))
        {
			    (*compNum)++;
        }

        bCalendarInserted = TRUE;
      }
    }
    

    
    if (strcmp(component, "Component AOD") == 0)
			strcpy(attr, "SELECTED");

		comps[*compNum].archive  = CString(archive);
		comps[*compNum].compname = component;
		comps[*compNum].name 	 = CString(name);
		comps[*compNum].desc 	 = CString(desc);
		comps[*compNum].selected = (strstr(attr, "SELECTED") != NULL);
		comps[*compNum].invisible = (strstr(attr, "INVISIBLE") != NULL);
		comps[*compNum].launchapp = (strstr(attr, "LAUNCHAPP") != NULL);
		comps[*compNum].additional = (strstr(attr, "ADDITIONAL") != NULL);
		comps[*compNum].disabled = (strstr(attr, "DISABLED") != NULL);
		comps[*compNum].forceupgrade = (strstr(attr, "FORCE_UPGRADE") != NULL);
		comps[*compNum].uncompress = (strstr(attr, "UNCOMPRESS") != NULL);
		comps[*compNum].downloadonly = (strstr(attr, "DOWNLOAD_ONLY") != NULL);
		comps[*compNum].supersede = (strstr(attr, "SUPERSEDE") != NULL);
		comps[*compNum].ignoreerror = (strstr(attr, "IGNORE_DOWNLOAD_ERROR") != NULL);
		comps[*compNum].unselected = (strstr(attr, "UNSELECTED") != NULL);
		// using strcmp for VISIBLE attrib instead of strstr since ststr returns 
		// true for INVISIBLE attribs also (VISIBLE is a part of INVISIBLE)
		comps[*compNum].visible = (strcmp(attr, "VISIBLE") == 0);
		comps[*compNum].empty	 = strcmp(attr, "");
		
		if (!(comps[*compNum].selected && comps[*compNum].invisible && 
			invisibleCount))
		{
			(*compNum)++;
    }

    invNum++;
		componentstr.Format("C%d", invNum);
		strcpy(tempcomponentstr, componentstr);

		GetPrivateProfileString("Setup Type2", tempcomponentstr, "", 
				                    component, MAX_SIZE, iniSrcPath);

		GetPrivateProfileString(component, "Archive", "", archive, MAX_SIZE, 
			iniSrcPath);
	}

/*
  // add in Calendar if it exists

  int iLastSlash = iniSrcPath.ReverseFind(_T('\\'));

  if (iLastSlash)
  {
    CString strCalendarXPIPath = iniSrcPath.Left(iLastSlash);
    CFileStatus fs;

    strCalendarXPIPath += "\\calendar.xpi";

    if (CFile::GetStatus(strCalendarXPIPath,fs)) // exists
    {
      comps[*compNum].archive       = CString("calendar.xpi");
	    comps[*compNum].compname      = CString("Component Calendar");
	    comps[*compNum].name 	        = CString("Calendar");
	    comps[*compNum].desc 	        = CString("Calendar Client");

	    comps[*compNum].selected      = 
	    comps[*compNum].forceupgrade  = 
	    comps[*compNum].empty	        = 1;

    	comps[*compNum].invisible     = 
	    comps[*compNum].launchapp     = 
	    comps[*compNum].additional    = 
	    comps[*compNum].disabled      = 
	    comps[*compNum].uncompress    = 
	    comps[*compNum].downloadonly  = 
	    comps[*compNum].unselected    = 0;

    	(*compNum)++;
	    invNum++;

    }  // file exists

  } // slash found
*/

	return TRUE;
}

extern "C" __declspec(dllexport)
int GenerateComponentList(CString parms, WIDGET *curWidget)
{
	rootPath = GetGlobal("Root");
	configName = GetGlobal("CustomizationList");
	configPath = rootPath + "Configs\\" + configName;
	workspacePath = configPath + "\\Workspace";
	nscpxpiPath;
	CString curVersion  = GetGlobal("Version");
	CString curPlatform = GetGlobal("lPlatform");
	CString curLanguage = GetLocaleCode(GetGlobal("Language"));
	CString localePath  = rootPath+"Version\\"+curVersion+"\\"+curPlatform+"\\"+curLanguage;

	if (SearchPath(workspacePath, "NSCPXPI", NULL, 0, NULL, NULL))
		nscpxpiPath = workspacePath + "\\NSCPXPI";
	else
		nscpxpiPath = localePath + "\\Nscpxpi";

	iniSrcPath = nscpxpiPath + "\\config.ini";

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
