#include "stdafx.h"
#include "WizardMachine.h"
#include "Interpret.h"
#include "WizardUI.h"
#include "ImgDlg.h"
#include "SumDlg.h"
#include "NewDialog.h"
#include <direct.h>

// for CopyDir
#include "winbase.h"  

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CWizardMachineApp theApp;
extern NODE *CurrentNode;
extern HBITMAP hBitmap;
extern CString Path;
extern char iniFilePath[MAX_SIZE];

extern BOOL inNext;
extern BOOL inPrev;
extern NODE* WizardTree;
extern WIDGET GlobalWidgetArray[1000];
extern int GlobalArrayIndex;
extern char currDirPath[MAX_SIZE];
extern char customizationPath[MAX_SIZE];
extern BOOL IsSameCache;
extern CString CacheFile;
extern CString CachePath;
extern char asePath[MAX_SIZE];
extern char nciPath[MAX_SIZE];
extern char tmpPath[MAX_SIZE];

extern _declspec (dllimport) WIDGET ptr_ga[1000];

CInterpret::CInterpret()
{
	// Init linked list to avoid messy operations on the linked list
	m_DLLs.dllName = "";
	m_DLLs.procName = "";
	m_DLLs.next = NULL;
}

CInterpret::~CInterpret()
{
	DLLINFO *dllp = m_DLLs.next;

	while (dllp)
	{
		FreeLibrary(dllp->hDLL);
		dllp = dllp->next;
	}
}

BOOL CInterpret::InitInstance()
{
	return FALSE;
}

//----------------------------------------------------------

void CInterpret::CopyDir(CString from, CString to)
{
	WIN32_FIND_DATA data;
	HANDLE d;
	CString dot = ".";
	CString dotdot = "..";
	CString fchild, tchild;
	CString pattern = from + "\\*.*";
	int		found;
	DWORD	tmp;


	d = FindFirstFile((const char *) to, &data);
	if (d == INVALID_HANDLE_VALUE)
		mkdir(to);

	d = FindFirstFile((const char *) pattern, &data);
	found = (d != INVALID_HANDLE_VALUE);

	while (found)
	{
		if (data.cFileName != dot && data.cFileName != dotdot)
		{
			fchild = from + "\\" + data.cFileName;
			tchild = to + "\\" + data.cFileName;
			tmp = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			if (tmp == FILE_ATTRIBUTE_DIRECTORY)
				CopyDir(fchild, tchild);
			else
				CopyFile((const char *) fchild, (const char *) tchild, FALSE);
		}

		found = FindNextFile(d, &data);
	}

	FindClose(d);
}

void CInterpret::ExecuteCommand(char *command, int showflag)
{
	STARTUPINFO	startupInfo; 
	PROCESS_INFORMATION	processInfo; 

	memset(&startupInfo, 0, sizeof(startupInfo));
	memset(&processInfo, 0, sizeof(processInfo));

	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	//startupInfo.wShowWindow = SW_SHOW;
	startupInfo.wShowWindow = showflag;

	BOOL executionSuccessful = CreateProcess(NULL, command, NULL, NULL, TRUE, 
												NORMAL_PRIORITY_CLASS, NULL, NULL, 
												&startupInfo, &processInfo); 
	DWORD error = GetLastError();
	WaitForSingleObject(processInfo.hProcess, INFINITE);
}

BOOL CInterpret::IterateListBox(char *parms)
{
	char *target = strtok(parms, ",");
	char *showstr = strtok(NULL, ",");
	char *cmd	 = strtok(NULL, "");
	WIDGET *w 	 = theApp.findWidget(target);
	char indices[MAX_SIZE];
	int  showflag;

	if (!w || !(w->type == "ListBox" || w->type == "CheckListBox"))
		return FALSE;

	if (w->control && w->control->m_hWnd)
	{
		// Widget is still visible on screen, get temporary copy of current value
		// (from here, we depend on UpdataData() having been called somewhere upstream)
		CString v = CWizardUI::GetScreenValue(w);
		strcpy(indices, (char *) (LPCSTR) v);
	}
	else
		strcpy(indices, w->value); 

	if (strcmp(showstr, "SHOW") == 0)
		showflag = SW_SHOWDEFAULT;
	else if (strcmp(showstr, "HIDE") == 0)
		showflag = SW_HIDE;
	else
		showflag = SW_SHOWDEFAULT;

	char *s = strtok(indices, ",");
	for (; s; s=strtok(NULL, ","))
	{
		char *cmd_copy = strdup(cmd);
		CString command = replaceVars(cmd_copy, s);
		free(cmd_copy);
		ExecuteCommand((char *) (LPCSTR) command, showflag);
	}

	return TRUE;
}

CString CInterpret::replaceVars(CString str, char *listval)
{
	char *theStr = (char *) (LPCTSTR) str;
	return replaceVars(theStr, listval);
}

CString CInterpret::replaceVars(char *str, char *listval)
{
	char buf[MIN_SIZE];
	char *b = buf;
	char *v;
	CString v1;
	char x[MIN_SIZE];

	while (*str)
	{
		if (*str == '%')
		{
			char *n = ++str;
			while (*str && *str != '%')
				str++;
			if (*str == '%')
			{
//				*str = '\0';

				int len = (strchr(n,'%') - n);
				strncpy(x,n,len);
				x[len]=NULL;
				
				if (listval && strlen(x) <= 0)
					v = listval;
				else
				{
					WIDGET *w = theApp.findWidget(x);
					if (w)
					{
						if (w->control && w->control->m_hWnd)
						{
							v1 = CWizardUI::GetScreenValue(w);
							v = (char *) (LPCSTR) v1;
						}
						else
							v = (char *) (LPCSTR) w->value;
					}
					else
						v = "";
				}
				strcpy(b, v);
				b += strlen(v);
			}
			else
			{
				CWnd myWnd;
				myWnd.MessageBox("Improperly terminated globals reference", "Error", MB_OK);
				exit(12);
			}
			str++;
		}
		else
			*b++ = *str++;
	}
	*b = '\0';

	return CString(buf);
}

BOOL CInterpret::CallDLL(char *dll, char *proc, char *parms)
{
	// When searching for DLL info, the m_DLLs is a dummy object
	// that only exists to hold the head of the list.  This simplifies 
	// the handling of the linked list by allowing us to otherwise
	// ignore the difference in the first node.
	
	DLLINFO *last = &m_DLLs;
	DLLINFO *dllp = m_DLLs.next;
	int found = FALSE;
	while (!found && dllp)
	{
		last = dllp;
		if (strcmp(dllp->dllName, dll) == 0 && strcmp(dllp->procName, proc) == 0)
			found = TRUE;
		else
			dllp = dllp->next;
	}

	// If we didn't find a match, then create a new entry in the list
	// and load the library and procedure
	if (!found)
	{
		dllp = (DLLINFO *) GlobalAlloc(0, sizeof(DLLINFO));
		dllp->dllName = CString(dll);
		dllp->procName = CString(proc);
		VERIFY(dllp->hDLL = ::LoadLibrary(dll));
		if (!dllp->hDLL)
			return FALSE;
		VERIFY(dllp->procAddr = (DLLPROC *) ::GetProcAddress(dllp->hDLL, proc));
		if (!dllp->procAddr)
			return FALSE;
		dllp->next = NULL;

		last->next = dllp;
	}

	// OK, if we get this far we've got a valid DLLINFO struct so call the procedure
	if (!(*dllp->procAddr)(CString(parms)))
		return FALSE;

	return TRUE;
}

BOOL CInterpret::interpret(char *cmds, WIDGET *curWidget)
{
	CString commands(cmds);
	return interpret(commands, curWidget);
}

BOOL CInterpret::interpret(CString cmds, WIDGET *curWidget)
{
		// Make modifiable copy of string's buffer
		char buf[MAX_SIZE];
		strcpy(buf, (char *)(LPCTSTR) cmds);

		// Format of commandList is:  <command1>;<command2>;...
		// Format of command is:   <command>(<parm1>,<parm2>,...)
		int numCmds = 0;
		char *cmdList[100];
		char *pcmdL = strtok(buf, ";");
		while (pcmdL)
		{
			cmdList[numCmds] = strdup(pcmdL);
			pcmdL = strtok(NULL, ";");
			numCmds++;
			if (numCmds >= 100)
			{
				//fprintf(out, "Skipping command interpretation, too many commands.\n");
				return FALSE;
			}
		}

		int i;
		for (i=0; i<numCmds; i++)
		{
			char *pcmd = strtok(cmdList[i], "(");
			char *parms = strtok(NULL, ")");

			if (pcmd)
			{
				// Only do this AFTER we're done with previous set
				char *dll   = strtok(pcmd, ".");
				char *proc = strtok(NULL, "(");  // guaranteed no "(" will match
				if (proc)
				{
					if (!proc)
						return FALSE;

					if (!CallDLL(dll, proc, parms))
						return FALSE;
				}
				else if (strcmp(pcmd, "command") == 0)
				{
					CString p = replaceVars(parms, NULL);
					ExecuteCommand((char *) (LPCSTR) p, SW_SHOWDEFAULT);
				}
				else if (strcmp(pcmd, "IterateListBox") == 0)
				{
					if (!IterateListBox(parms))
						return FALSE;
				}
				else if (strcmp(pcmd, "VerifySet") == 0)
				{
					// VerifySet checks to see if the first parameter has any value
					//   If (p1) then continue else show error dialog and return FALSE

					char *p2 = strchr(parms, ',');

					if (p2)
						*p2++ = '\0';
					else
						p2 = "A message belongs here.";
						
					CString value = replaceVars(parms, NULL);

					if (!value || value.IsEmpty())
					{
						CWnd myWnd;
						myWnd.MessageBox(p2, "Error", MB_OK|MB_SYSTEMMODAL);
						return FALSE;
					}
				}
				else if (strcmp(pcmd, "Reload") == 0)
				{
					// Enforce the rule that Reload() cannot be followed by any 
					// further commands to interpret
					if (i < numCmds-1)
					{
						CWnd myWnd;
						myWnd.MessageBox(
							"No commands allowed after a Reload().  File = " + CachePath, 
							"Information", MB_OK);
						exit(-30);
					}
					
					
					// Write out the current cache
					if (!IsSameCache)
						theApp.CreateNewCache();

					// Reload sets the CachePath and reloads the cache from the new file
					CString newDir = replaceVars(parms, NULL);
					CachePath = newDir + "\\" + CacheFile;
					theApp.FillGlobalWidgetArray(CachePath);  // Ignore failure, we'll write one out later
					IsSameCache = FALSE;
				}
				else if (strcmp(pcmd, "VerifyVal") == 0)
				{
					char *p2 = strchr(parms, ',');

					if (p2)
						*p2++ = '\0';
					else
					{
						AfxMessageBox("Please Enter Verification Value for" + CString(parms),MB_OK);
						exit(12);
					}

					CString Getval = replaceVars(parms, NULL);
					
					if (strcmp(Getval,p2) ==0)
						return FALSE;
				}
				else if (strcmp(pcmd, "VerifyDir") == 0)
				{
					CFileFind tmpDir;
					BOOL dirFound = tmpDir.FindFile(CString(nciPath)+"*.*");
					if (!dirFound)
					{
						strcpy(tmpPath,asePath);
						strcat(tmpPath,"NCIFiles");
						_mkdir (tmpPath);
					}
				}
				else if (strcmp(pcmd, "Message") ==0)
				{
					int rv = AfxMessageBox(parms,MB_YESNO);
					if (rv == IDYES)
						return TRUE;
					if (rv == IDNO)
						return FALSE;
				}

				else if (strcmp(pcmd, "DisplayImage") == 0)
				{
					// This is to dsiplay an image in a separate dialog
					CImgDlg imgDlg(parms);
					int retVal = imgDlg.DoModal();
				}
				else if (strcmp(pcmd, "ShowSum") == 0)
				{
					CSumDlg sumdlg;
					int retVal = sumdlg.DoModal();
				}
				else if (strcmp(pcmd, "NewNCIDialog") == 0)
				{
					CString entryName;
					CNewDialog newDlg;
					newDlg.DoModal();
					entryName = newDlg.GetData();
					theApp.SetGlobal(parms, entryName);
				}
				else if (strcmp(pcmd, "inform") == 0)
				{
					char *p2 = strchr(parms, ',');

					if (p2)
						*p2++ = '\0';
					else
						p2 = "Specify Directory here";
						
					CString entryName;
					CWnd myWnd;
					entryName = theApp.GetGlobal(parms);
					CString p2path = replaceVars(p2,NULL);
					if (entryName != "") {
						myWnd.MessageBox( entryName + " is saved in " + p2path, "Information", MB_OK);
					}
					// Delete the global var now...
				}	
				else if (strcmp(pcmd, "GenerateFileList") == 0 || strcmp(pcmd, "GenerateDirList") == 0)
				{
					char *p2 = strchr(parms, ',');

					if (p2)
						*p2++ = '\0';
					CString value = replaceVars(parms, NULL);

					WIDGET *w;
					if (strcmp(parms, "self") == 0)
						w = curWidget;
					else
						w = theApp.findWidget(parms);

					if (w)
					{
						CString p2path = replaceVars(p2,NULL);
						theApp.GenerateList(pcmd, w, p2path);
					}
				}
				else if (strcmp(pcmd, "BrowseFile") == 0)
				{
					if (curWidget)
						CWizardUI::BrowseFile(curWidget);
				}
			
				else if (strcmp(pcmd, "BrowseDir") == 0)
				{
					if (curWidget)
						CWizardUI::BrowseDir(curWidget);
				}
			
				else if (strcmp(pcmd, "NewConfigDialog") == 0)
				{
					if (curWidget)
						CWizardUI::NewConfig(curWidget, CString(parms));
				}

				else if (strcmp(pcmd, "CopyDir") == 0)
				{
					char *p2 = strchr(parms, ',');
					if (p2)
					{
						*p2++ = '\0';
						CString from = replaceVars(parms, NULL);
						CString to = replaceVars(p2, NULL);
						CopyDir(from, to);
					}
				}
				else if (strcmp(pcmd, "SetGlobal") == 0)
				{
					char *p2 = strchr(parms, ',');
					if (p2)
					{
						*p2++ = '\0';
						CString name = replaceVars(parms, NULL);
						CString value = replaceVars(p2, NULL);
						value.TrimRight();
						theApp.SetGlobal(name, value);
					}
				}
				else if (strcmp(pcmd, "ShowDescription") == 0)
				{
					if (curWidget)
					{
						int i = ((CCheckListBox*)curWidget->control)->GetCurSel();
						WIDGET *t = theApp.findWidget((char *)(LPCSTR) curWidget->target);
						CString msg(i);
						((CEdit*)t->control)->SetWindowText(msg);
					}
				}
				else if (strcmp(pcmd, "toggleEnabled") == 0)
				{
					// convert first parm into boolean...
					char *p2 = strchr(parms, ',');
					if (p2)
					{
						*p2++ = '\0';
						CString value = replaceVars(parms, NULL);
						int newval = (value == "1");
						parms = p2;
						p2 = strchr(parms, ',');
						while (parms)
						{
							if (p2)
								*p2++ = '\0';
							WIDGET *w = theApp.findWidget(parms);
							if (w)
								w->control->EnableWindow(newval);
							parms = p2;
							if (parms)
								p2 = strchr(parms, ',');
						}
					}
				}
			}
			// This is an extra free...
			//free(pcmd);
		}

	return TRUE;
}


