#include "stdafx.h"
#include <Winbase.h>
#include <direct.h>
#include "globals.h"
#include "comp.h"
#include "ib.h"

#define MAX_SIZE 1024

int interpret(char *cmd);

CString rootPath;
CString configName;
CString configPath;
CString workspacePath;
CString cdPath;
CString networkPath;
CString tempPath;
CString iniDstPath;
CString iniSrcPath;
CString scriptPath;
CString nscpxpiPath;

char buffer[50000];
XPI	xpiList[100];
int xpiLen = -1;

COMPONENT Components[100];
int		numComponents;

int findXPI(CString xpiname, CString filename)
{
	int found = FALSE;

	for (int i=0; !found && i<=xpiLen; i++)
		if (xpiList[i].xpiname == xpiname && 
			xpiList[i].filename == filename)
				found = TRUE;

	if (!found)
	{
		xpiLen++;
		xpiList[xpiLen].xpiname  = xpiname;
		xpiList[xpiLen].filename = filename;
	}

	return found;
}

int ExtractXPIFile(CString xpiname, CString xpifile)
{
	CString command;

	if (findXPI(xpiname, xpifile))
		return TRUE;

	// Can use -d instead of change CWD???
	CString xpiArchive = nscpxpiPath + "\\" + xpiname;
	command = rootPath + "unzip.exe -o" + " " + xpiArchive + " " + xpifile;
	ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);

	return TRUE;
}

int ReplaceXPIFiles()
{
	CString command;
	CString xpiArchive;
	CString xpiArcDest;

	// Go through the whole list putting them into the archives
	for (int i=0; i<=xpiLen; i++)
	{
		// This copy preserves the existing archive if it exists - do we
		// need to delete it the first time through?
		xpiArchive = nscpxpiPath + "\\" + xpiList[i].xpiname;
		xpiArcDest = cdPath + "\\" + xpiList[i].xpiname;
		if (!CopyFile(xpiArchive, xpiArcDest, TRUE))
			DWORD e = GetLastError();

		command = rootPath + "zip.exe -m " + xpiArcDest + " " + xpiList[i].filename;
		ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
	}

	return TRUE;
}

int ReplaceINIFile()
{
	CString command;
	CString exeName("Seamonkey32e.exe");
	char	olddir[1024];

	GetCurrentDirectory(sizeof(olddir), olddir);

	if(SetCurrentDirectory((char *)(LPCTSTR) cdPath) == FALSE)
		return FALSE;

	CString Src = nscpxpiPath + "\\" +exeName;
	CString Dst = cdPath + "\\" + exeName;
	if (!CopyFile(Src, Dst, TRUE))
		DWORD e = GetLastError();

	command = rootPath + "nszip.exe " + exeName +  " config.ini";
	ExecuteCommand((char *)(LPCTSTR) command, SW_SHOW, INFINITE);

	SetCurrentDirectory(olddir);

	return TRUE;
}

void ModifyEntity(char *buffer, CString entity, CString newvalue)
{
	CString buf(buffer);
	entity = entity + " ";  // Ensure we don't get false matches

	int i = buf.Find(entity);
	if (i == -1) return;

	i = buf.Find('"', i+1);
	if (i == -1) return;
	i++;

	int j = buf.Find('"', i);
	if (j == -1) return;
	j--;

	buf.Delete(i, j-i+1);
	buf.Insert(i, newvalue);

	strcpy(buffer, (char *)(LPCTSTR) buf);
}

int ModifyDTD(CString xpifile, CString entity, CString newvalue)
{
	CString newfile = xpifile + ".new";
	int rv = TRUE;
	char *fgetsrv;

	// Read in DTD file and make substitutions
	FILE *srcf = fopen(xpifile, "r");
	FILE *dstf = fopen(newfile, "w");
	if (!srcf)
		rv = FALSE;
	else
	{
		int done = FALSE;
		while (!done)
		{
			fgetsrv = fgets(buffer, sizeof(buffer), srcf);
			done = feof(srcf);
			if (!done)
			{
				if (!fgetsrv || ferror(srcf))
				{
					rv = FALSE;
					break;
				}
				ModifyEntity(buffer, entity, newvalue);
				fputs(buffer, dstf);
			}
		}

		fclose(srcf);
		fclose(dstf);
	}

	remove(xpifile);
	rename(newfile, xpifile);

	return TRUE;
}

int interpret(char *cmd)
{
	char *cmdname = strtok(cmd, "(");

	if (strcmp(cmdname, "configure") == 0)
	{
		char temp[1024];
		char *section	= strtok(NULL, ",)");
		char *key 		= strtok(NULL, ",)");
		char *value 	= strtok(NULL, ",)");
		char *newvalue	= value;
		if (value[0] == '%')
		{
			value++;
			char *t = strchr(value, '%');
			if (!t)
				return FALSE;
			*t = '\0';
			newvalue = (char *)(LPCTSTR) GetGlobal(value);
		}
		if (!section || !key || !newvalue)
			return FALSE;
		if (!CopyFile(iniSrcPath, iniDstPath, TRUE))
			DWORD e = GetLastError();
		if (strcmp(key, "Program Folder Name") == 0)
		{
			strcpy(temp, "Netscape by ");
			strcat(temp, newvalue);
			newvalue = temp;
		}
		WritePrivateProfileString(section, key, newvalue, iniDstPath);
	}
	else if (strcmp(cmdname, "replaceXPI") == 0)
	{
		char *xpiname	= strtok(NULL, ",)");
		char *xpifile	= strtok(NULL, ",)");
		char *value 	= strtok(NULL, ",)");
		char *newvalue	= value;
		if (value[0] == '%')
		{
			value++;
			char *t = strchr(value, '%');
			if (!t)
				return FALSE;
			*t = '\0';
			newvalue = (char *)(LPCTSTR) GetGlobal(value);
		}
		if (!xpiname || !xpifile || !newvalue)
			return FALSE;
		ExtractXPIFile(xpiname, xpifile);
		if (!CopyFile(newvalue, xpifile, FALSE))
		{
			DWORD e = GetLastError();
			return FALSE;
		}
	}
	else if (strcmp(cmdname, "modifyDTD") == 0)
	{
		char *xpiname	= strtok(NULL, ",)");
		char *xpifile	= strtok(NULL, ",)");
		char *entity	= strtok(NULL, ",)");
		char *value 	= strtok(NULL, ",)");
		char *newvalue	= value;
		if (value[0] == '%')
		{
			value++;
			char *t = strchr(value, '%');
			if (!t)
				return FALSE;
			*t = '\0';
			newvalue = (char *)(LPCTSTR) GetGlobal(value);
		}
		if (!xpiname || !xpifile || !entity || !newvalue)
			return FALSE;
		ExtractXPIFile(xpiname, xpifile);
		ModifyDTD(xpifile, entity, newvalue);
	}
	else if (strcmp(cmdname, "wrapXPI") == 0)
	{
	}
	else
		return FALSE;

	return TRUE;
}

void init_components()
{
	int i;
	WIDGET *w = findWidget("SelectedComponents");
	BuildComponentList(Components, &numComponents, iniSrcPath);

	// Turn off components that aren't selected
	for (i=0; i<numComponents; i++)
		if (strstr(w->value, Components[i].name) == NULL)
			Components[i].selected = FALSE;

}

void invisible()
{
	WIDGET *tempWidget;
	tempWidget = findWidget("SelectedComponents");
	CString cdConfigPath = cdPath + "\\config.ini";
	CString component;
	{
		int count = (((CCheckListBox *)tempWidget->control))->GetCount();
			
		for (int i=0; i < count; i++)
		{
			if (((CCheckListBox *)tempWidget->control)->GetCheck(i) != 1)
			{
			component = Components[i].compname;	
			WritePrivateProfileString(Components[i].compname, "Attributes", "INVISIBLE", cdConfigPath);
				
			}
		}
	}
}
extern "C" __declspec(dllexport)
int StartIB(CString parms, WIDGET *curWidget)
{
	char *fgetsrv;
	int rv = TRUE;
	char	olddir[1024];

	rootPath	= GetGlobal("Root");
	configName	= GetGlobal("CustomizationList");
	configPath  = rootPath + "Configs\\" + configName;
	cdPath 		= configPath + "\\CD";
	networkPath = configPath + "\\Network";
	tempPath 	= configPath + "\\Temp";
	iniDstPath	= cdPath + "\\config.ini";
	scriptPath	= rootPath + "\\script.ib";
	workspacePath = configPath + "\\Workspace";
	if (SearchPath(workspacePath, "NSCPXPI", NULL, 0, NULL, NULL))
		nscpxpiPath = workspacePath + "\\NSCPXPI";
	else
		nscpxpiPath = rootPath + "NSCPXPI";
	iniSrcPath	= nscpxpiPath + "\\config.ini";

	init_components();

	_mkdir((char *)(LPCTSTR) cdPath);
	_mkdir((char *)(LPCTSTR) tempPath);
	_mkdir((char *)(LPCTSTR) workspacePath);

	GetCurrentDirectory(sizeof(olddir), olddir);

	if(SetCurrentDirectory((char *)(LPCTSTR) tempPath) == FALSE)
		return FALSE;

	// Read in script file and interpret commands from it
	FILE *f = fopen(scriptPath, "r");
	if (!f)
		rv = FALSE;
	else
	{
		int done = FALSE;
		while (!done)
		{
			fgetsrv = fgets(buffer, sizeof(buffer), f);
			done = feof(f);
			if (!done)
			{
				if (!fgetsrv || ferror(f))
				{
					rv = FALSE;
					break;
				}

				buffer[strlen(buffer)] = '\0';  // Eliminate the trailing newline

				if (!interpret(buffer))
				{
					rv = FALSE;
					break;
				}
			}
		}

		fclose(f);
	}

	// Put all the extracted files back into their new XPI homes
	ReplaceXPIFiles();

	// Copy remaining default installer files into config
	// preserving any existing files that we created already
	// in previous steps
	/* -- Need to be more selective than this
	CopyDir(nscpxpiPath, cdPath, NULL, FALSE);
	*/
	CString cdDir= GetGlobal("CD image");
	CString networkDir = GetGlobal("Network");
	CString ftpLocation = GetGlobal("FTPLocation");

	if (cdDir.Compare("1") ==0)
	{
		for (int i=0; i<numComponents; i++)
		{
			if (Components[i].selected)
				CopyFile(nscpxpiPath + "\\" + Components[i].archive, 
						 cdPath + "\\" + Components[i].archive, TRUE);
		}
	}

	CString component;
	if (networkDir.Compare("1") ==0)
	{
		for (int i=0; i<numComponents; i++)
		{
			if (Components[i].selected)
				CopyFile(nscpxpiPath + "\\" + Components[i].archive, 
						 networkPath + "\\" + Components[i].archive, TRUE);
			
			WritePrivateProfileString(Components[i].compname, "url0", ftpLocation, iniSrcPath);
		}
		
	}

	// Didn't work...
	CreateRshell ();
	CString shellPath = workspacePath + "\\Autorun\\Shell\\";
	CopyDir(shellPath, cdPath, NULL, FALSE);

	invisible();

	ReplaceINIFile();

	SetCurrentDirectory(olddir);


	return rv;
}

