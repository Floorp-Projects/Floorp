#include <direct.h>
#include "stdafx.h"
#include "globals.h"
#include "WizardMachine.h"
#include "Interpret.h"
#include "WizardUI.h"
#include "ImgDlg.h"
#include "SumDlg.h"
#include "NewDialog.h"
#include "NewConfigDialog.h"

// The following is included to make 
// the browse for a dir code compile
#include <shlobj.h>
#include <ctype.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#define FILEPATHLENGTH 47
extern CWizardMachineApp theApp;
extern NODE *CurrentNode;
extern HBITMAP hBitmap;
extern CString Path;
extern char iniFilePath[MAX_SIZE];
extern BOOL Validate;
extern CString numericMessage;
extern BOOL inNext;
extern BOOL inPrev;
extern NODE* WizardTree;
extern char currDirPath[MAX_SIZE];
extern char customizationPath[MAX_SIZE];
extern CString CacheFile;
extern CString CachePath;
extern char asePath[MAX_SIZE];
extern char nciPath[MAX_SIZE];
extern char tmpPath[MAX_SIZE];
extern UINT nID;
extern UINT wNotifyCode;


extern _declspec (dllimport) WIDGET ptr_ga[1000];

CInterpret::CInterpret()
{
	// Init linked list to avoid messy operations on the linked list
	m_DLLs.dllName = NULL;
	m_DLLs.procName = NULL;
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

BOOL CInterpret::NewConfig(WIDGET *curWidget, CString globalsName, CString DialogTitle) 
{
	// This doesn't really belong here...
	WIN32_FIND_DATA data;
	HANDLE d;

	CNewConfigDialog newDlg;
	if (!DialogTitle.IsEmpty())
		SetGlobal("DialogTitle", "CreateCopy");
	newDlg.DoModal();
	CString configField = newDlg.GetConfigName();

	CString newDir = CString(replaceVars("%Root%Configs\\",NULL)); 
	
	newDir += configField;
	CString Template = CString(replaceVars("%Root%WSTemplate",NULL));
	CString FooCopy = CString(replaceVars("%Root%Configs\\%CustomizationList%",NULL));
	d = FindFirstFile((const char *) newDir, &data);
	if (d == INVALID_HANDLE_VALUE)
	{
		if (!DialogTitle.IsEmpty())
			CopyDir(FooCopy,newDir,NULL, FALSE);
		else 
			CopyDir(Template,newDir,NULL, FALSE);
	}
	else
	{
		CWnd myWnd;
		myWnd.MessageBox("That configuration already exists.", "Error", MB_OK);
		return FALSE;
	}

					
	WIDGET* tmpWidget = findWidget((char*) (LPCTSTR)curWidget->target);
	if (!tmpWidget)
		return FALSE;

	if (!tmpWidget->action.onInit.IsEmpty())
		interpret(tmpWidget->action.onInit, tmpWidget);
					
	if (!configField.IsEmpty())
		((CComboBox*)tmpWidget->control)->SelectString(0, configField);
	
	SetGlobal(globalsName, configField);
	SetGlobal("CustomizationList", configField);
	
	IsSameCache = FALSE;

	return TRUE;
}

BOOL CInterpret::BrowseFile(WIDGET *curWidget) 
{
	// This is to browse to a file
	WIDGET* tmpWidget = findWidget((char*) (LPCTSTR)curWidget->target);

	CString filePath = tmpWidget->value;
	int fileLength = filePath.GetLength();
	int dotPlace = filePath.ReverseFind('.');
	CString fileExt = filePath.Right(fileLength - dotPlace - 1);

	CFileDialog fileDlg(TRUE, NULL, filePath, OFN_OVERWRITEPROMPT|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_LONGNAMES, NULL, NULL);
	int retVal = fileDlg.DoModal();
	CString fullFileName="";


	//Checking to see if the open file dialog did get a value or was merely cancelled.
	//If it was cancelled then the value of the edit box is not changed.
	if (retVal == IDOK)
	{	
		fullFileName = fileDlg.GetPathName();
//		if ((fullFileName.Right(fileLength - dotPlace -1) == fileExt) && (tmpWidget && (CEdit*)tmpWidget->control))
		if (tmpWidget && (CEdit*)tmpWidget->control)
		{
			tmpWidget->value = fullFileName;
			if (tmpWidget->name == "LinuxPath")
				tmpWidget->display = fullFileName;
			else
				tmpWidget->display = GetTrimFile(fullFileName);
		}
	}
	((CEdit*)tmpWidget->control)->SetWindowText(tmpWidget->display);
		
	return TRUE;

}

CString CInterpret::BrowseDir(WIDGET *curWidget) 
{
	// The following code is used to browse to a dir
	// CFileDialog does not allow this
	CString returnDir("");
	BROWSEINFO bi;
	char szPath[MAX_PATH];
	char szTitle[] = "Select Directory";
	bi.hwndOwner = AfxGetMainWnd()->m_hWnd;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = (char*)malloc(MAX_PATH);
	bi.lpszTitle = szTitle;

	// Enable this line to browse for a directory
	bi.ulFlags = BIF_RETURNONLYFSDIRS;
				
	// Enable this line to browse for a computer
	bi.lpfn = NULL;
	bi.lParam = NULL;
	LPITEMIDLIST pidl= SHBrowseForFolder(&bi);
	
	if(pidl != NULL)
	{
		SHGetPathFromIDList(pidl,szPath);
		if( bi.ulFlags & BIF_BROWSEFORCOMPUTER )
		{
			// bi.pszDisplayName variable contains the computer name
		}
		else if( bi.ulFlags & BIF_RETURNONLYFSDIRS )
		{
			// szPath variable contains the path
			WIDGET* tmpWidget = findWidget((char*) (LPCTSTR)curWidget->target);
			if (tmpWidget)
				((CEdit*)tmpWidget->control)->SetWindowText(szPath);

		}
	 }
	returnDir = szPath;
	free( bi.pszDisplayName );
	return returnDir;
}

BOOL CInterpret::Progress() 
{
#ifdef SUPPORTINGIBPROGRESS
	CProgressDialog progressDlg(this);
	progressDlg.Create(IDD_PROGRESS_DLG);
	CProgressDialog *pProgressDlg = &progressDlg;
				
	//CRuntimeClass *pProgDlgThread = RUNTIME_CLASS(CProgDlgThread);	//This is the multi-threading stuff for the progress dialog
	//AfxBeginThread(pProgDlgThread);

	pProgressDlg->m_ProgressText.SetWindowText("Creating a CD Layout...");
	pProgressDlg->m_ProgressBar.SetPos(0);
	pProgressDlg->m_ProgressBar.SetRange(0,4);
	pProgressDlg->m_ProgressBar.SetStep(1);
				
	if (curWidget->action.dll == "IBEngine.dll") {
		VERIFY(hModule = ::LoadLibrary("IBEngine.dll"));

		VERIFY(
			pMyDllPath =
			(MYDLLPATH*)::GetProcAddress(
			(HMODULE) hModule, "SetPath")
		);

		(*pMyDllPath)((char*)(LPCTSTR)Path);

		pProgressDlg->m_ProgressText.SetWindowText("Loading Globals...");
		LoadGlobals();
		pProgressDlg->m_ProgressBar.StepIt();
		pProgressDlg->UpdateWindow();

		pProgressDlg->m_ProgressText.SetWindowText("Reading files...");
		ReadIniFile();
		pProgressDlg->m_ProgressBar.StepIt();
		pProgressDlg->UpdateWindow();

		pProgressDlg->m_ProgressText.SetWindowText("Merging files...");
		MergeFiles();
		pProgressDlg->m_ProgressBar.StepIt();
		pProgressDlg->UpdateWindow();

		pProgressDlg->m_ProgressText.SetWindowText("Creating CD Layout...");
		CreateMedia();
		pProgressDlg->m_ProgressBar.StepIt();
		pProgressDlg->UpdateWindow();

		MessageBox("CD Directory created", "OK", MB_OK);
	}
#endif

	return TRUE;
}

BOOL CInterpret::IterateListBox(char *parms)
{
	char *target = strtok(parms, ",");
	char *showstr = strtok(NULL, ",");
	char *cmd	 = strtok(NULL, "");
	WIDGET *w 	 = findWidget(target);
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
		ExecuteCommand((char *) (LPCSTR) command, showflag,INFINITE);
	}

	return TRUE;
}

BOOL CInterpret::GetRegistryKey( HKEY key, const char *subkey, char *retdata )
{ 
	long retval;
	HKEY hkey;

	retval = RegOpenKeyEx(key, subkey, 0, KEY_QUERY_VALUE, &hkey);

	if(retval == ERROR_SUCCESS)
	{
		long datasize = MAX_SIZE;
		char data[MAX_SIZE];

		RegQueryValue(hkey,NULL,(LPSTR)data,&datasize);

		lstrcpy(retdata,data);
		RegCloseKey(hkey);
	}
	return retval;
}

BOOL CInterpret::OpenBrowser(const char *url)
{
	char key[MAX_SIZE + MAX_SIZE];
	BOOL retflag = FALSE;

	/* get the .htm regkey and lookup the program */
	if(GetRegistryKey(HKEY_CLASSES_ROOT,".htm",key) == ERROR_SUCCESS)
	{
		lstrcat(key,"\\shell\\open\\command");
		if(GetRegistryKey(HKEY_CLASSES_ROOT,key,key) == ERROR_SUCCESS)
		{
			char *pos;
			pos = strstr(key,"\"%1\"");
			if(pos == NULL)     /* if no quotes */
			{
				/* now check for %1, without the quotes */
				pos = strstr(key,"%1");
				if(pos == NULL) /* if no parameter */
				pos = key+lstrlen(key)-1;
				else
				*pos = '\0';    /* remove the parameter */
			}
			else
			*pos = '\0';        /* remove the parameter */

		lstrcat(pos," ");
		lstrcat(pos,"\"");
		lstrcat(pos,url);
		lstrcat(pos,"\"");
		ExecuteCommand(key,SW_SHOW,50); 
		retflag = TRUE;
		}
	}

	return retflag;
}

BOOL CInterpret::OpenViewer(const char *url)
{
	CString file2Open = url;
	int filelength = file2Open.GetLength();
	int findext = file2Open.ReverseFind('.');
	int extlength = filelength - findext;
	CString fileExtension = file2Open.Right(extlength);

	char key[MAX_SIZE + MAX_SIZE];
	BOOL retflag = FALSE;

	/* get the .htm regkey and lookup the program */
	if(GetRegistryKey(HKEY_CLASSES_ROOT,(LPCTSTR)fileExtension,key) == ERROR_SUCCESS)
	{
		lstrcat(key,"\\shell\\open\\command");
		if(GetRegistryKey(HKEY_CLASSES_ROOT,key,key) == ERROR_SUCCESS)
		{
			char *pos;
			pos = strstr(key,"\"%1\"");
			if(pos == NULL)     /* if no quotes */
			{
				/* now check for %1, without the quotes */
				pos = strstr(key,"%1");
				if(pos == NULL) /* if no parameter */
				pos = key+lstrlen(key)-1;
				else
				*pos = '\0';    /* remove the parameter */
			}
			else
			*pos = '\0';        /* remove the parameter */

		lstrcat(pos," ");
		lstrcat(pos,"\"");
		lstrcat(pos,url);
		lstrcat(pos,"\"");
		ExecuteCommand(key,SW_SHOW,50); 
		retflag = TRUE;
		}
	}

	return retflag;
}

void CInterpret::GenerateList(CString action, WIDGET* curWidget, CString parentDirPath)
{
	CFileFind fileList;
	BOOL dirFound = fileList.FindFile(parentDirPath);
	CString tmpFile;
	int i = 0;

	while (dirFound)
	{
	    dirFound = fileList.FindNextFile();
		tmpFile  = fileList.GetFileName();

		if (action == "GenerateFileList" && !fileList.IsDirectory() ||
		    action == "GenerateDirList" && fileList.IsDirectory() && 
			 !(tmpFile == "." || tmpFile == "..")) 
		{
			curWidget->options.value[i] = new char[tmpFile.GetLength()+1];
			strcpy(curWidget->options.value[i], (char *)(LPCTSTR) tmpFile);
			i++;
		}
	}
	curWidget->numOfOptions = i;

	fileList.Close();
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
					WIDGET *w = findWidget(x);
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

BOOL CInterpret::CallDLL(char *dll, char *proc, char *parms, WIDGET *curWidget)
{
	// When searching for DLL info, the m_DLLs is a dummy object
	// that only exists to hold the head of the list.  This simplifies 
	// the handling of the linked list by allowing us to otherwise
	// ignore the difference in the first node.
	
	CString dllStr = CString(dll);
	CString procStr = CString(proc);
	DLLINFO *last = &m_DLLs;
	DLLINFO *dllp = m_DLLs.next;
	int found = FALSE;
	while (!found && dllp)
	{
		last = dllp;
		if (*dllp->dllName == dllStr && *dllp->procName == procStr)
			found = TRUE;
		else
			dllp = dllp->next;
	}

	// If we didn't find a match, then create a new entry in the list
	// and load the library and procedure
	if (!found)
	{
		dllp = (DLLINFO *) GlobalAlloc(0, sizeof(DLLINFO));
		dllp->dllName = new CString(dll);
		dllp->procName = new CString(proc);
		VERIFY(dllp->hDLL = ::LoadLibrary(dll));
		if (!dllp->hDLL)
		{
			DWORD e = GetLastError();
			return FALSE;
		}
		VERIFY(dllp->procAddr = (DLLPROC *) ::GetProcAddress(dllp->hDLL, proc));
		if (!dllp->procAddr)
		{
			DWORD e = GetLastError();
			return FALSE;
		}
		dllp->next = NULL;

		last->next = dllp;
	}

	// OK, if we get this far we've got a valid DLLINFO struct so call the procedure
	if (!(*dllp->procAddr)(CString(parms), curWidget))
		return FALSE;

	return TRUE;
}
CString CInterpret::GetTrimFile(CString filePath)
{
	int fileLength=0;
	int findSlash=0;
	fileLength = filePath.GetLength();
	if (fileLength > FILEPATHLENGTH)
	{
		findSlash = filePath.Find('\\');
		filePath.Delete(findSlash+1,(fileLength - FILEPATHLENGTH));
		filePath.Insert(findSlash+1,"...");
	}
	return filePath;
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

					if (!CallDLL(dll, proc, parms, curWidget))
						return FALSE;
				}
				else if (strcmp(pcmd, "command") == 0)
				{
					CString p = replaceVars(parms, NULL);
					ExecuteCommand((char *) (LPCSTR) p, SW_SHOWDEFAULT,INFINITE);
				}
				else if (strcmp(pcmd, "IterateListBox") == 0)
				{
					if (!IterateListBox(parms))
						return FALSE;
				}
				else if(strcmp(pcmd, "IsMailfieldempty") ==0)
				{
					CString ispDomainName = GetGlobal("DomainName");
					CString ispPrettyName = GetGlobal("PrettyName");
					CString ispLongName = GetGlobal("LongName");
					CString ispInServer = GetGlobal("IncomingServer");
					CString ispOutServer = GetGlobal("OutgoingServer");
					CString ispPortNumber = GetGlobal("PortNumber");
					if ((ispDomainName.IsEmpty()) || 
						(ispPrettyName.IsEmpty()) || 
						(ispLongName.IsEmpty()) || 
						(ispInServer.IsEmpty())	|| 
						(ispOutServer.IsEmpty()) || 
						(ispPortNumber.IsEmpty()))
					{
						if (!((ispDomainName.IsEmpty()) && 
							(ispPrettyName.IsEmpty()) && 
							(ispLongName.IsEmpty()) && 
							(ispInServer.IsEmpty())	&& 
							(ispOutServer.IsEmpty()) && 
							(ispPortNumber.IsEmpty())))
						{
							AfxMessageBox("All fields must be filled to create a customized mail account", MB_OK);
							return FALSE;
						}
					}
				}
				else if(strcmp(pcmd, "IsNewsfieldempty") ==0)
				{
					CString newsDomainName = GetGlobal("nDomainName");
					CString newsPrettyName = GetGlobal("nPrettyName");
					CString newsLongName = GetGlobal("nLongName");
					CString newsServer = GetGlobal("nServer");
					CString newsPortNumber = GetGlobal("nPortNumber");
					if ((newsDomainName.IsEmpty()) || 
						(newsPrettyName.IsEmpty()) || 
						(newsLongName.IsEmpty()) || 
						(newsServer.IsEmpty()) || 
						(newsPortNumber.IsEmpty()))
					{
						if (!((newsDomainName.IsEmpty()) && 
							(newsPrettyName.IsEmpty()) && 
							(newsLongName.IsEmpty()) && 
							(newsServer.IsEmpty())	&& 
							(newsPortNumber.IsEmpty())))
						{
							AfxMessageBox("All fields must be filled to create a customized news account", MB_OK);
							return FALSE;
						}
					}
				}
				else if (strcmp(pcmd, "IsAutourlempty") == 0)
				{
					CString proxyConfigoption = GetGlobal("radioGroup2");
					if (proxyConfigoption == "3")
					{
						WIDGET *wid;
						wid = findWidget("autoproxyurl");
						CString autourl = CWizardUI::GetScreenValue(wid);
						if (autourl == "")
						{
							AfxMessageBox("Please enter a URL for automatic proxy configuration", MB_OK);
							return FALSE;
						}
					}
				}
				else if (strcmp(pcmd, "IsSamedomain") == 0)
				{
					CString ispDomainName = GetGlobal("DomainName");
					CString newsDomainName;
					newsDomainName = CWizardUI::GetScreenValue(curWidget);
					if (newsDomainName == ispDomainName)
					{
						AfxMessageBox("The domain name for News must be different from the domain name used for Mail", MB_OK);
						return FALSE;
					}
				}
				else if(strcmp(pcmd, "IsNumeric") ==0)
				{
					WIDGET *wid;
					wid = curWidget;
					if (wid)
					{
						CString retval = CWizardUI::GetScreenValue(curWidget);
						curWidget->value = retval;
						int len = retval.GetLength();
						char* intval = (char*)(LPCTSTR)(retval);
						intval[len] = '\0';
						int pCount =0;
						while (intval[0] != '\0')
						{
							if (!isdigit(intval[0]))
								pCount++;

							intval++;
							
						}
						if (pCount > 0)
						{
							numericMessage = replaceVars(parms,NULL);
							AfxMessageBox(numericMessage,MB_OK);
							Validate = FALSE;
						}
						else
							Validate = TRUE;
					}
					
				}
 				else if(strcmp(pcmd, "IsPortnumEmpty") ==0)
				{
					WIDGET *wid;
					wid = curWidget;
					if (wid)
					{
						CString retval = CWizardUI::GetScreenValue(curWidget);
						if (retval == "")
						{
							SetGlobal(curWidget->value,"0");
							((CEdit*)wid->control)->SetWindowText("0");
						}
					}
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
				else if (strcmp(pcmd, "CopyFile") == 0)
				{
					// VerifySet checks to see if the first parameter has any value
					//   If (p1) then continue else show error dialog and return FALSE

					char *p2 = strchr(parms, ',');

					if (p2)
						*p2++ = '\0';
					else
						p2 = "You must specify a second file to copy";
					CString fchild =replaceVars(parms,NULL);
					CString tchild = replaceVars(p2,NULL);
					if (!CopyFile((LPCTSTR) fchild, (LPCTSTR) tchild, FALSE))
					{
						DWORD copyerror = GetLastError();
						AfxMessageBox("Error - File couldnt be copied", MB_OK|MB_SYSTEMMODAL);
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
				else if (strcmp(pcmd, "WriteCache") ==0)
				{
					WIDGET *w = findWidget(parms);
					if (w)
						if (wNotifyCode == CBN_SELCHANGE)
						{
							IsSameCache = FALSE;
						}
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
				else if (strcmp(pcmd, "Depend") == 0)
				{
					int numParam = 0;
					char *dParamList[5];
					char *dParam = strtok(parms, ",");
					while (dParam)
					{
						dParamList[numParam] = strdup(dParam);
						dParam = strtok(NULL, ",");
						numParam++;
					}
					CString var1 = replaceVars(dParamList[0],NULL);
					CString value1 = replaceVars(dParamList[1],NULL);
					CString var2 = replaceVars(dParamList[2],NULL);
					CString value2;
					if (strcmp(dParamList[3],"NULL") !=0)
						value2 = replaceVars(dParamList[3],NULL);
					else 
						value2.Empty();
					CString message = replaceVars(dParamList[4],NULL);
					if (!message)
					{
						AfxMessageBox("Error - provide a message to show when depend is false", MB_OK);
						return FALSE;
					}

					
					if (var1.Compare(value1) == 0)
					{
						if (value2.IsEmpty())
						{
							if (var2.IsEmpty())
							{
								AfxMessageBox(message, MB_OK);
								return FALSE;
							}
						}				
						else
						{
							if (var2.Compare(value2) != 0)
							{
								AfxMessageBox(message, MB_OK);
								return FALSE;
							}
						}
					}
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
				else if (strcmp(pcmd, "Msg") ==0)
				{
					CString message = replaceVars(parms,NULL);
					int rv = AfxMessageBox(message,MB_OK);
				}
				else if (strcmp(pcmd, "Message") ==0)
				{
					int rv = AfxMessageBox(parms,MB_YESNO);
					if (rv == IDNO)
						return FALSE;
				}
				else if (strcmp(pcmd, "ImportFiles") == 0)
				{
					char *p2 = strchr(parms, ',');

					if (p2)
						*p2++ = '\0';
					else
					{
						AfxMessageBox("You havent entered an extension - all files will be copied",MB_OK);
					}

					CString todir = replaceVars(parms, NULL);
					CString ext = p2;
					CString returnDir = BrowseDir(curWidget);
					CopyDir(returnDir,todir,ext,FALSE);
				}
				else if (strcmp(pcmd, "DisplayImage") == 0)
				{
					// This is to dsiplay an image in a separate dialog
					CImgDlg imgDlg(parms);
					int retVal = imgDlg.DoModal();
				}
				else if (strcmp(pcmd, "OpenURL") == 0)
				{
					// This is to dsiplay an image in a separate dialog
					CString Location = replaceVars(parms,NULL);
					OpenBrowser((CHAR*)(LPCTSTR)Location);
				}
				
				else if (strcmp(pcmd, "OpenViewer") == 0)
				{
					// This is to dsiplay an image in a separate dialog
					CString Location = replaceVars(parms,NULL);
					OpenViewer((CHAR*)(LPCTSTR)Location);
				}
				

				else if (strcmp(pcmd, "ViewFile") == 0)
				{
					// This is to dsiplay an image in a separate dialog
					CString view = replaceVars("%Root%ImageEye.exe ",NULL);
					CString gif_file = replaceVars(parms,NULL);
					view += gif_file;
					ExecuteCommand((char *) (LPCSTR) view, SW_SHOWDEFAULT,INFINITE);
				}
				else if (strcmp(pcmd, "ShowSum") == 0)
				{
					CSumDlg sumdlg;
					int retVal = sumdlg.DoModal();
				}
				else if (strcmp(pcmd, "ShowSummary") == 0)
				{
					interpret("Reload(%Root%Configs\\%CustomizationList%)",curWidget);
					interpret("ShowSum()",curWidget);
					interpret("Reload(%Root%)",curWidget);

				}
#if 0
				else if (strcmp(pcmd, "NewNCIDialog") == 0)
				{
					CString entryName;
					CNewDialog newDlg;
					newDlg.DoModal();
					entryName = newDlg.GetData();
					SetGlobal(parms, entryName);
				}
#endif
				else if (strcmp(pcmd, "inform") == 0)
				{
					char *p2 = strchr(parms, ',');

					if (p2)
						*p2++ = '\0';
					else
						p2 = "Specify Directory here";
						
					CString entryName;
					CWnd myWnd;
					entryName = GetGlobal(parms);
					CString p2path = replaceVars(p2,NULL);
					if (entryName != "") {
						myWnd.MessageBox( entryName + " is saved in " + p2path, "Information", MB_OK);
					}
					// Delete the global var now...
				}	
				else if (strcmp(pcmd, "GenerateFileList") == 0 || 
						 strcmp(pcmd, "GenerateDirList") == 0)
				{
					char *p2 = strchr(parms, ',');

					if (p2)
						*p2++ = '\0';
					CString value = replaceVars(parms, NULL);

					WIDGET *w;
					if (strcmp(parms, "self") == 0)
						w = curWidget;
					else
						w = findWidget(parms);

					if (w)
					{
						CString p2path = replaceVars(p2,NULL);
						GenerateList(pcmd, w, p2path);
					}
				}
				else if (strcmp(pcmd, "SelectItem") ==0)
				{
					WIDGET* tmpWidget = findWidget((char*) (LPCTSTR)curWidget->target);
					if (!tmpWidget)
						return FALSE;
					CString comboValue = replaceVars(parms,NULL);				
					if (!(comboValue.IsEmpty()))
						((CComboBox*)tmpWidget->control)->SelectString(0, comboValue);

				}

				else if (strcmp(pcmd, "BrowseFile") == 0)
				{
					if (curWidget)
						BrowseFile(curWidget);
				}
			
				else if (strcmp(pcmd, "BrowseDir") == 0)
				{
					if (curWidget)
						BrowseDir(curWidget);
				}
#if 0			
				else if (strcmp(pcmd, "NewConfigDialog") == 0)
				{
					if (curWidget)
						NewConfig(curWidget, CString(parms),"");
				}

				else if (strcmp(pcmd, "CopyConfig") == 0)
				{
					if (curWidget)
						NewConfig(curWidget, CString(parms),"Create Copy");
				}
#endif
				else if (strcmp(pcmd, "CopyDir") == 0)
				{
					char *p2 = strchr(parms, ',');
					if (p2)
					{
						*p2++ = '\0';
						CString from = replaceVars(parms, NULL);
						CString to = replaceVars(p2, NULL);
						CopyDir(from, to, NULL, TRUE);
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
						SetGlobal(name, value);
					}
				}
				else if (strcmp(pcmd, "ShowDescription") == 0)
				{
					if (curWidget)
					{
						int i = ((CCheckListBox*)curWidget->control)->GetCurSel();
						WIDGET *t = findWidget((char *)(LPCSTR) curWidget->target);
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
							WIDGET *w = findWidget(parms);
							if (w)
								w->control->EnableWindow(newval);
							parms = p2;
							if (parms)
								p2 = strchr(parms, ',');
						}
					}
				}
				else if (strcmp(pcmd, "toggleEnabled2") == 0)
				{
					// convert first parm into boolean...
                    char *p2 = strchr(parms, ',');
					if (p2)
					{
						*p2++ = '\0';
						CString value = replaceVars(parms, NULL);
						BOOL newval = (!value.IsEmpty());
						char *parms1 = p2;
						p2 = strchr(parms1, ',');
						while (parms1)
						{
							if (p2)
								*p2++ = '\0';
							WIDGET *w = findWidget(parms);
							if (w)
								w->control->EnableWindow(newval);
							parms1 = p2;
							if (parms1)
								p2 = strchr(parms1, ',');
						}
					}
				}
			}
			// This is an extra free...
			//free(pcmd);
		}

	return TRUE;
}


