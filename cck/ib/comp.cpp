#include "stdafx.h"
#include "globals.h"
#include "ib.h"
#include "comp.h"
#include <direct.h>

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

extern CString linuxOption;
extern CString linuxblobPath;
extern CString linuxDir;
extern CString nsinstPath;
extern CString xpiDir;
extern CString tarfile;
extern CString tarfile1;
extern CString quotes;

extern COMPONENT Components[100];
extern int numComponents;

extern "C" __declspec(dllexport)
int BuildComponentList(COMPONENT *comps, int *compNum, CString iniSrcPath,
					   int invisibleCount)
{
	*compNum = 0;
	int invNum = *compNum;
	CString componentstr;

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

		
		if (!(comps[*compNum].selected && comps[*compNum].invisible && 
			invisibleCount))
		{
			(*compNum)++;
			invNum++;
			componentstr.Format("C%d", invNum);
			strcpy(tempcomponentstr, componentstr);
			GetPrivateProfileString("Setup Type2", tempcomponentstr, "", 
				component, MAX_SIZE, iniSrcPath);
		}
		else
		{
			invNum++;
			componentstr.Format("C%d", invNum);
			strcpy(tempcomponentstr, componentstr);
			GetPrivateProfileString("Setup Type2", tempcomponentstr, "", 
				component, MAX_SIZE, iniSrcPath);
		}
		GetPrivateProfileString(component, "Archive", "", archive, MAX_SIZE, 
			iniSrcPath);
	}


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

	linuxOption = GetGlobal("lPlatform");
	if (linuxOption == "Linux")
	{
		linuxblobPath = GetGlobal("LinuxPath");
		linuxDir = "nscpxpiLinux";
		nsinstPath = "\\netscape-installer\\xpi";
		xpiDir = "\\xpi";
		CString tnscpxpilinuxPath = rootPath + linuxDir;
		CString nscpxpilinuxPath = tnscpxpilinuxPath;
		int pathlen = linuxblobPath.GetLength();
		int pos = linuxblobPath.ReverseFind('\\');
		pos += 1;
		CString linuxinstDirPath = linuxblobPath.Left(pos);
		tarfile = linuxblobPath.Right(pathlen-pos);
		pos = tarfile.ReverseFind('.');
		tarfile1 = tarfile.Left(pos);

		int direxist = GetFileAttributes(nscpxpilinuxPath);
		if ((direxist == -1) && (linuxblobPath != "")) 
		// nscpxpiLinux directory does not exist	
		{
			quotes = "\"";
			char currentdir[_MAX_PATH];
			_getcwd(currentdir, _MAX_PATH);
			_chdir(rootPath);
			_mkdir(linuxDir);
			_chdir(linuxinstDirPath);
			tnscpxpilinuxPath.Replace("\\","/");
			tnscpxpilinuxPath.Replace(":","");
			tnscpxpilinuxPath.Insert(0,"/cygdrive/");
			CString command = "gzip -d " + tarfile;
			ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
			command = "tar -xvf " + tarfile1 + " -C " + quotes + tnscpxpilinuxPath + quotes;
			ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
			command = "gzip " + tarfile1;
			ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
			
			nscpxpiPath = nscpxpilinuxPath + nsinstPath;
			CString tempxpiPath = nscpxpiPath;
			tempxpiPath.Replace(xpiDir,"");
			CopyFile(tempxpiPath+"\\Config.ini", nscpxpiPath+"\\Config.ini", 
				FALSE);
			_chdir(currentdir);
		}
		nscpxpiPath = nscpxpilinuxPath + nsinstPath;
	}
	else
	{
	if (SearchPath(workspacePath, "NSCPXPI", NULL, 0, NULL, NULL))
		nscpxpiPath = workspacePath + "\\NSCPXPI";
	else
		nscpxpiPath = rootPath + "NSCPXPI";
	}
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
