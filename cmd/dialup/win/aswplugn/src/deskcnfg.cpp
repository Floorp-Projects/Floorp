/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
///////////////////////////////////////////////////////////////////////////////
//
// Deskcnfg.cpp
// Revision History:
// Date        Author            Reason
// ----------------------------------------------------------------------------
// 02/03/97    xxxxxxxxxxxxxx    Port Native API for win3.x
//             xxxxxxxxxxxxxx    Define Native API for win95 & winNT
///////////////////////////////////////////////////////////////////////////////

#include <npapi.h>
#include "plugin.h"

// resource include
#ifdef WIN32 // **************************** WIN32 *****************************
#include "resource.h"
#else        // **************************** WIN16 *****************************
#include "asw16res.h"
#endif // !WIN32

// windows include
#ifdef WIN32
// ********************* Win32 includes **************************
#include <winbase.h>
#include <shlobj.h>
#include <regstr.h>
#include <shlguid.h>
#else  // WIN16
// ********************* Win16 includes & consts **************************
#include <windows.h>
#include <helper16.h>
#include <shivaras.h>              		// Shiva RAS APIs 

#define MAX_PROGRAMITEM_NAME		40		// max length of program item name
#endif //WIN32

#include <string.h>
#include <stdlib.h>
#include "errmsg.h"

// java includes 
#include "netscape_npasw_SetupPlugin.h"
#include "java_lang_String.h"

char IconFile[_MAX_PATH] = { '\0' };
extern BOOL getMsgString(char *buf, UINT uID);
extern const char *GetStringPlatformChars(JRIEnv *env, struct java_lang_String *string);
extern BOOL FileExists (LPCSTR lpszFileName);
extern char acctDescription[256];	 

#ifndef WIN32
BOOL g_bExistingPath = FALSE;			// TRUE if account creating is an existing account
#endif

#ifdef WIN32 // ************************** WIN32 ****************************
//********************************************************************************
// utility function
//
// SameStrings
//
// Checks for string equality between a STRRET and a LPCTSTR
//********************************************************************************
static BOOL
SameStrings(LPITEMIDLIST pidl, STRRET& lpStr1, LPCTSTR lpStr2)
{

	char    buf[MAX_PATH];
	char *mystr;

    switch (lpStr1.uType) {

		case STRRET_WSTR:
			 WideCharToMultiByte(CP_OEMCP, WC_DEFAULTCHAR,
			 lpStr1.pOleStr, -1, buf, sizeof(buf), NULL, NULL);
			 return strcmp(lpStr2, buf) == 0;

		case STRRET_OFFSET:
			 mystr=((char *)pidl) + lpStr1.uOffset;
			 return strcmp(lpStr2, ((char *)pidl) + lpStr1.uOffset) == 0;

		case STRRET_CSTR:
			 mystr=lpStr1.cStr;
			 return strcmp(lpStr2, lpStr1.cStr) == 0;
    }

    return FALSE;
}


//********************************************************************************
// utility function
//
// GetSize
//
//********************************************************************************
static LPITEMIDLIST
Next(LPCITEMIDLIST pidl)
{
	LPSTR lpMem=(LPSTR)pidl;

	lpMem += pidl->mkid.cb;
	return (LPITEMIDLIST)lpMem;
}


//********************************************************************************
// utility function
//
// GetSize
//
//********************************************************************************
static UINT
GetSize(LPCITEMIDLIST pidl)
{
    UINT cbTotal = 0;

    if (pidl) {
		cbTotal += sizeof(pidl->mkid.cb);
		while (pidl->mkid.cb) {
			cbTotal += pidl->mkid.cb;
			pidl = Next(pidl);
		}
    }

    return cbTotal;
}


//********************************************************************************
// utility function
//
// Create
//
//********************************************************************************
static LPITEMIDLIST
Create(UINT cbSize)
{
    IMalloc*     pMalloc;
    LPITEMIDLIST pidl = 0;

	if (FAILED(SHGetMalloc(&pMalloc)))
		return 0;

    pidl = (LPITEMIDLIST)pMalloc->Alloc(cbSize);

    if (pidl)
		memset(pidl, 0, cbSize);
 
	pMalloc->Release();
    
	return pidl;
}



//********************************************************************************
// utility function
//
// ConcatPidls
//
//********************************************************************************
static LPITEMIDLIST
ConcatPidls(LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2)
{
    UINT         cb1 = GetSize(pidl1) - sizeof(pidl1->mkid.cb);
    UINT         cb2 = GetSize(pidl2);
    LPITEMIDLIST pidlNew = Create(cb1 + cb2);

    if (pidlNew) {
		memcpy(pidlNew, pidl1, cb1);
		memcpy(((LPSTR)pidlNew) + cb1, pidl2, cb2);
    }

	UINT cb3 = GetSize(pidlNew);

	return pidlNew;
}


//********************************************************************************
//
// GetMyComputerFolder
//
// This routine returns the ISHellFolder for the virtual My Computer folder,
// and also returns the PIDL.
//********************************************************************************
static HRESULT
GetMyComputerFolder(LPSHELLFOLDER *ppshf, LPITEMIDLIST *ppidl)
{
    IMalloc*        pMalloc;
    HRESULT         hres;

    hres = SHGetMalloc(&pMalloc);
    if (FAILED(hres))
		return hres;

    // Get the PIDL for "My Computer"
    hres = SHGetSpecialFolderLocation(/*pWndOwner->m_hWnd*/NULL, CSIDL_DRIVES, ppidl);
    if (SUCCEEDED(hres)) {
		IShellFolder*   pshf;

		hres = SHGetDesktopFolder(&pshf);
		if (SUCCEEDED(hres)) {
			 // Get the shell folder for "My Computer"
			 hres = pshf->BindToObject(*ppidl, NULL, IID_IShellFolder, (LPVOID *)ppshf);
			 pshf->Release();
		}
    }

    pMalloc->Release();

    return hres;
}


//********************************************************************************
//
// GetDialupNetworkingFolder
//
// This routine returns the ISHellFolder for the virtual Dial-up Networking
// folder, and also returns the PIDL.
//********************************************************************************
static HRESULT
GetDialUpNetworkingFolder(LPSHELLFOLDER *ppshf, LPITEMIDLIST *ppidl)
{
    HRESULT         hres;

    IMalloc*        pMalloc = NULL;
    IShellFolder*   pmcf = NULL;
    LPITEMIDLIST    pidlmc;


    char szDialupName[256];
    HKEY hKey;
    DWORD cbData;

    //
    // Poke around in the registry to find out what the Dial-Up Networking
    //   folder is called on this machine
    //
    szDialupName[0] = '\0';
    if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_CLASSES_ROOT, 
                 "CLSID\\{992CFFA0-F557-101A-88EC-00DD010CCC48}", 
                 NULL, 
                 KEY_QUERY_VALUE, 
                 &hKey)) {

		cbData = sizeof(szDialupName);
		RegQueryValueEx(hKey, "", NULL, NULL, (LPBYTE)szDialupName, &cbData);

    }

    // if we didn't get anything just use the default
    if(szDialupName[0] == '\0') {
		char *strText;
		strText = "Dial-Up Networking";
		strcpy(szDialupName, (LPCSTR)strText);
	}

    RegCloseKey(hKey);


    //
    // OK, now look for that folder
    //

	hres = SHGetMalloc(&pMalloc);
    if (FAILED(hres))
		return hres;

    // Get the virtual folder for My Computer
    hres = GetMyComputerFolder(&pmcf, &pidlmc);
    if (SUCCEEDED(hres)) {
		IEnumIDList*    pEnumIDList;

		// Now we need to find the "Dial-Up Networking" folder
		hres = pmcf->EnumObjects(/*pWndOwner->m_hWnd*/ NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList);
		if (SUCCEEDED(hres)) {
			LPITEMIDLIST    pidl;

			int flag = 1;
			while ((NOERROR == (hres = pEnumIDList->Next(1, &pidl, NULL))) && (flag)) {
				STRRET  name;

				name.uType = STRRET_CSTR;  // preferred choice
				hres = pmcf->GetDisplayNameOf(pidl, SHGDN_INFOLDER, &name);
				if (FAILED(hres)) {
					pMalloc->Free(pidl);
					flag = 0;
				   //break;
				}

				if (SameStrings(pidl, name, szDialupName)) {
					*ppidl = ConcatPidls(pidlmc, pidl);
					hres = pmcf->BindToObject(pidl, NULL, IID_IShellFolder, (LPVOID *)ppshf);
					pMalloc->Free(pidl);
					flag = 0;
					//break;
				}
            
				if (flag)
					pMalloc->Free(pidl);
			}

			pEnumIDList->Release();
		}
	
		pmcf->Release();
		pMalloc->Free(pidlmc);
    }

    pMalloc->Release();


    return hres;
}


//********************************************************************************
//
// GetDialupConnectionPIDL
//
//********************************************************************************
static HRESULT
GetDialUpConnectionPIDL(LPCTSTR lpConnectionName, LPITEMIDLIST *ppidl)
{
    HRESULT         hres;

    IMalloc*        pMalloc = NULL;
    IShellFolder*   pshf = NULL;
    LPITEMIDLIST    pidldun;

	// Initialize out parameter
	*ppidl = NULL;

    hres = SHGetMalloc(&pMalloc);
    if (FAILED(hres))
		return hres;

    // First get the Dial-Up Networking virtual folder
    hres = GetDialUpNetworkingFolder(&pshf, &pidldun);
    if (SUCCEEDED(hres) && (pshf != NULL)) {
		IEnumIDList*    pEnumIDList;

		// Enumerate the files looking for the desired connection
		hres = pshf->EnumObjects(/*pWndOwner->m_hWnd*/NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList);
		if (SUCCEEDED(hres)) {
			LPITEMIDLIST    pidl;

			int flag=1;
			while ((NOERROR == (hres = pEnumIDList->Next(1, &pidl, NULL))) && (flag)) {
				STRRET  name;
				
				name.uType = STRRET_CSTR;  // preferred choice
				hres = pshf->GetDisplayNameOf(pidl, SHGDN_INFOLDER, &name);
				if (FAILED(hres)) {
				   pMalloc->Free(pidl);
				   flag = 0;
				   //break;
				}

				if (SameStrings(pidl, name, lpConnectionName)) {
				   *ppidl = ConcatPidls(pidldun, pidl);
				   pMalloc->Free(pidl);
				   flag = 0;
				   //break;
				}

				if (flag)
				   pMalloc->Free(pidl);
			}

			pEnumIDList->Release();
		}

		pshf->Release();
		pMalloc->Free(pidldun);

    }

    pMalloc->Release();

    return hres;
}


//********************************************************************************
//
// GetNetscapePIDL
//
//********************************************************************************
static void
GetNetscapePidl(LPITEMIDLIST * ppidl)
{


    char            szPath[MAX_PATH], * p;
    OLECHAR         olePath[MAX_PATH];
    IShellFolder  * pshf;

    GetModuleFileName(DLLinstance, szPath, sizeof(szPath));
    //GetModuleFileName(AfxGetInstanceHandle(), szPath, sizeof(szPath));

    //we need to take off \plugins\npasw.dll from the path
	p = strrchr(szPath, '\\');
    if(p) 
		*p = '\0';
	p = strrchr(szPath, '\\');
    if(p) 
		*p = '\0';
    strcat(szPath, "\\netscape.exe");

    HRESULT hres = SHGetDesktopFolder(&pshf);
    if (SUCCEEDED(hres)) {
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR) szPath, -1, (LPWSTR) olePath, sizeof(olePath));

		ULONG lEaten;
		pshf->ParseDisplayName(NULL, NULL, (LPOLESTR) olePath, &lEaten, ppidl, NULL);
	}

	return;
}


//********************************************************************************
//
// CreateLink
//
// Creates a shell shortcut to the PIDL
//********************************************************************************
static HRESULT
CreateLink(LPITEMIDLIST pidl, LPCTSTR lpszPathLink, LPCTSTR lpszDesc)
{
    HRESULT     hres;
    IShellLink* psl = NULL;
	LPCTSTR IconPath = IconFile;

    // Get a pointer to the IShellLink interface.
	//CoInitialize(NULL); // work around for Nav thread lock bug

	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
							IID_IShellLink, (LPVOID FAR*)&psl);
    if (SUCCEEDED(hres)) {
		IPersistFile* ppf;
		// Set the path to the shortcut target, and add the description.
		psl->SetIDList(pidl);
		psl->SetDescription(lpszDesc);
		if(IconPath && IconPath[0])
			psl->SetIconLocation(IconPath, 0);

		// Query IShellLink for the IPersistFile interface for saving the
		// shortcut in persistent storage.
		hres = psl->QueryInterface(IID_IPersistFile, (LPVOID FAR*)&ppf);

		if (SUCCEEDED(hres)) {
			WORD wsz[MAX_PATH];

			// Ensure that the string is ANSI.
			MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH);

			// Save the link by calling IPersistFile::Save.
			hres = ppf->Save((LPCOLESTR)wsz, STGM_READ);
			ppf->Release();
		}

		psl->Release();

	}

	//CoUninitialize();

	return hres;
}

//********************************************************************************
//
// CreateDialerShortcut
//
// Creates a shell shortcut to the PIDL
//********************************************************************************
short CreateDialerShortcut(char* szDesktop,     // Desktop path
						   LPCSTR AccountName,  // connectoid/phonebook entry name
						   IMalloc* pMalloc,
						   char *szPath,     // path to PE folder
						   char *strDesc)      // shortcut description
{
	HRESULT      hres;
	LPITEMIDLIST pidl;

	char    Desktop[MAX_PATH];
	szDesktop[0] = '\0';
	DWORD   cbData;
	HKEY    hKey;
	long res;

	// gets Desktop folder path from registry for both win95 & winNT40
	// "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"
    if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_CURRENT_USER, REGSTR_PATH_SPECIAL_FOLDERS, NULL, KEY_QUERY_VALUE, &hKey))
		return (-3);

    cbData = MAX_PATH;
    res = RegQueryValueEx(hKey, "Desktop", NULL, NULL, (LPBYTE)Desktop, &cbData);

	RegCloseKey(hKey);

	strcpy(szDesktop, Desktop);

	// win95 only
	if (platformOS == VER_PLATFORM_WIN32_WINDOWS) {
		
		// Get a PIDL that points to the dial-up connection
		hres = GetDialUpConnectionPIDL(AccountName, &pidl);

		if (FAILED(hres)) {
			
			// Err: Unable to create shortcut to RNA phone book entry
			char *buf = (char *)malloc(sizeof(char) * 255);
			if (!buf)
				// Err: Out of Memory
				return (-6);

			if (getMsgString(buf, IDS_NO_RNA)) {
				if (IDOK != DisplayErrMsg(buf, MB_OK | MB_ICONEXCLAMATION)) {
					free(buf);
					return (FALSE);
				}
			}

			free(buf);     
			return(-1);
		}

		// If the dial-up networking folder is open when we create the RNA
		// entry, then we will have failed to get a PIDL above. The dial-up
		// networking folder itself won't display the icon until it is closed
		// and re-opened. There's nothing we can do but return
		if (!pidl) {
			pMalloc->Release();
			return(-2);
		}

		// Create a shortcut on the desktop
		char strPath[MAX_PATH];
		strcpy(strPath, szDesktop);
		strcat(strPath, "\\");
		strcat(strPath, strDesc);
		strcat(strPath, ".Lnk");
		CreateLink(pidl, strPath, strDesc);
		
		// And one in our PE folder
		strcpy(strPath, szPath);
		strcat(strPath, "\\");
		strcat(strPath, strDesc);
		strcat(strPath, ".Lnk");
		CreateLink(pidl, strPath, strDesc);
		
	} else if (platformOS == VER_PLATFORM_WIN32_NT) {  // WinNT40 here
		
		// make sure the phonebook entry we created still exists

		char *sysDir = (char *)malloc(sizeof(char) * _MAX_PATH);
		if (!sysDir)
			return (-5);
		
		char pbPath[MAX_PATH];
		GetSystemDirectory(sysDir, MAX_PATH);
		strcpy(pbPath, sysDir);
		strcat(pbPath, "\\ras\\rasphone.pbk");
		strcat(pbPath, "\0");
		free(sysDir);
		
		RASENTRYNAME rasEntryName[MAX_PATH];
		if (!rasEntryName)
			return (-7);

		rasEntryName[0].dwSize = stRASENTRYNAME;
		DWORD size = stRASENTRYNAME * MAX_PATH;
		DWORD entries;
		
		if (0 != RasEnumEntries(NULL, pbPath, rasEntryName, &size, &entries))
			return (-4);

		BOOL exists = FALSE;
		DWORD i=0;
		
		while ((i < entries) && (!exists)) {

			if (strcmp(rasEntryName[i].szEntryName, AccountName) == 0)
				exists = TRUE;

			i++;
		}

		// create a shortcut file on desktop
		if (exists) {
			HANDLE hfile = NULL;
			
			// create phonebook entry shortcut file on desktop, overwrites if exists
			SECURITY_ATTRIBUTES secAttrib;
			memset(&secAttrib, 0, sizeof(SECURITY_ATTRIBUTES));
			secAttrib.nLength = sizeof(SECURITY_ATTRIBUTES);
			secAttrib.lpSecurityDescriptor = NULL;
			secAttrib.bInheritHandle = FALSE;

			// construct phonebook entry shortcut file name
			char file[MAX_PATH];
			strcpy(file, szDesktop);
			strcat(file, "\\");
			strcat(file, AccountName);
			strcat(file, ".rnk");
			
			hfile = CreateFile(file, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
				               &secAttrib, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			if (hfile == INVALID_HANDLE_VALUE)
				return (-9); 

			CloseHandle(hfile);
			hfile = NULL;

			// writes shortcut file data in the following format:
			//    [Dial-Up Shortcut]
			//    Entry=stuff
			//    Phonebook=C:\WINNT40\System32\RAS\rasphone.pbk (default system phonebook)

			WritePrivateProfileString("Dial-Up Shortcut", "Entry", AccountName, file);
			WritePrivateProfileString("Dial-Up Shortcut", "Phonebook", pbPath, file);

			// create the same shortcut file in our PE folder
			strcpy(file, szPath);
			strcat(file, "\\");
			strcat(file, AccountName);
			strcat(file, ".rnk");

			hfile = CreateFile(file, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
				               &secAttrib, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			if (hfile == INVALID_HANDLE_VALUE) 
				return (-10);

			CloseHandle(hfile);
			WritePrivateProfileString("Dial-Up Shortcut", "Entry", AccountName, file);
			WritePrivateProfileString("Dial-Up Shortcut", "Phonebook", pbPath, file);

		} else {
			
			return (-8);
		}
	}

	return (0);
}

//********************************************************************************
//
// CreateProgramItems
//
// adds 2 icons:
// Dialer - to Dial-Up Networking folder, Desktop & our PE folder
// Navigator - to our PE folder
//********************************************************************************
static short
CreateProgramItems(LPCSTR AccountName,
				   LPCSTR CustomIniPath)

{
	char         szPath[MAX_PATH];
	LPITEMIDLIST pidl;

	char         szBuf[MAX_PATH];

	IMalloc*     pMalloc;
	SHGetMalloc(&pMalloc);

	// gets the path to "Programs" folder
	if (platformOS == VER_PLATFORM_WIN32_WINDOWS) {

		SHGetSpecialFolderLocation(NULL, CSIDL_PROGRAMS, &pidl);
		SHGetPathFromIDList(pidl, szBuf);
		pMalloc->Free(pidl);

	} else if (platformOS == VER_PLATFORM_WIN32_NT) {

		// NT4.0: get the "Programs" folder for "All Users"

		HKEY hKey;
		DWORD bufsize = sizeof(szBuf);

		if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
										 "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
										 NULL, 
										 KEY_QUERY_VALUE,  
										 &hKey)) {
			RegQueryValueEx(hKey, "PathName", NULL, NULL, (LPBYTE)szBuf, &bufsize);
			strcat(szBuf, "\\Profiles\\All Users\\Start Menu\\Programs");

		} else {

			return (-1);
		}
		
	}

	//
	// gets Netscape PE folder here
	//
	
	char buf[256];
	char *csFolderName;
	csFolderName =(char *)malloc(sizeof(char) * 256);

	if (!csFolderName)
		return (-4);

	strcpy(csFolderName, "Netscape Personal Edition");
	
	// check for custom folder name
	if(::GetPrivateProfileString("General", "InstallFolder", (const char *)csFolderName, buf, sizeof(buf), CustomIniPath))
		strcpy(csFolderName,buf);

	strcpy(szPath, szBuf);
	strcat(szPath, "\\");
	strcat(szPath, csFolderName);

	free(csFolderName);

	//
	//  First do Dialer Icon
	//

	// Create a dialer icon shortcut description
	char strDesc[MAX_PATH];

#ifdef UPGRADE
	if (???entryInfo.bUpgrading) {
		char *csTmp = "Dialer";
		strcpy(strDesc, AccountName);
		strcat(strDesc, " ");
		strcat(strDesc, csTmp);
	} else {
		strcpy(strDesc, "Dial");
		strcat(strDesc, " ");
		strcat(strDesc, AccountName);
	}
#else
	strcpy(strDesc,"Dial");
//	strcat(strDesc, " ");
	strDesc[strlen(strDesc)+1] = '\0';
	strDesc[strlen(strDesc)] = (char)32;
	strcat(strDesc, AccountName);
#endif

	char szDesktop[512];

	//
	// create dialer shortcut icon on desktop and in PE folder
	//

	int rtn = CreateDialerShortcut(szDesktop, AccountName, pMalloc, szPath, strDesc);
	
	if (rtn != 0)
		return rtn;


#ifdef FOLDER_IN_START_MENU
	// Cleanup
	pMalloc->Free(pidl);
    pMalloc->Release();
#endif

    ::Sleep(250);

    return(0);
}

#else // ****************************** WIN16 ************************

//********************************************************************************
//
// CreateProgramItems
//
// Dialer - to Dial-Up Networking folder, Desktop & our PE folder
// Navigator - to our PE folder
//********************************************************************************
BOOL CreateProgramItems16(LPCSTR AccountName, LPCSTR IconFileName, LPCSTR CustomIniPath)
{
	// get Netscape window
 	HWND hNavWnd = GetActiveWindow();

	// obtain Program Group Name
	char csProgramGroup[256],
			csProgramGroupDeflt[256],				// default program group name
			csItemPath[256],
			csItemName[MAX_PROGRAMITEM_NAME + 1];
//			csItemIconPath[256];
	int nBytesCopied;
	BOOL bResult; 
	
	bResult = GetNetscapeProgramGroupName(csProgramGroupDeflt);
   
	// get custom folder name
	nBytesCopied = GetPrivateProfileString("General", "InstallFolder", csProgramGroupDeflt, 
														csProgramGroup, 256, CustomIniPath);
	// create item name (truncate if item name too long
	strcpy(csItemName, "Dial ");
	strncat(csItemName, AccountName, MAX_PROGRAMITEM_NAME - strlen(csItemName));
	                    
	// parse out invalid char in program item name
	ParseWin16BadChar(csItemName, FALSE, MAX_PROGRAMITEM_NAME);

/*
	// Check if ACCTSET.INI override for Icon name
	nBytesCopied = GetPrivateProfileString("General", "NavIconName", "", csItemIconPath, 
														sizeof(csItemIconPath), CustomIniPath))
*/

	// get path to connection Shiva SR file
	bResult = GetConnectionFilePath(acctDescription, csItemPath);

	if (bResult)
	{
		bResult = AddProgramGroupItem(csProgramGroup, csItemPath, csItemName, IconFileName); 
	}
   
   // set Netscape window active to prevent program mgr on top of netscape
   // no need for existing account because, AS terminates after program item is created.
	if (!g_bExistingPath)
		SetActiveWindow(hNavWnd);

	return bResult;
}
#endif  // WIN16



//********************************************************************************
// native method:
//
// DesktopConfig
//
// Sets up user's desktop (creates icons and short cuts)
//********************************************************************************
extern JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fDesktopConfig(JRIEnv* env,
											 struct netscape_npasw_SetupPlugin* ThisPlugin,
											 struct java_lang_String *JSAccountName,
											 struct java_lang_String *JSIconFile,
											 struct java_lang_String *JSAcctsetIniPath)
{
	const char *AccountName=NULL, *CustomIniPath=NULL;

	if (JSAccountName != NULL)
		AccountName = GetStringPlatformChars(env, JSAccountName);

	if (JSIconFile != NULL) {
		const char* IconFileName;
		IconFileName = GetStringPlatformChars(env, JSIconFile);
	    //JS may pass us different file for icon file
		if ((IconFileName) && (strcmp(IconFileName, IconFile) != 0)) {
			if (strcmp(IconFile, "") != 0) {
				if(FileExists(IconFile)) 	// a temp icon file may already existed
					_unlink(IconFile);
			}
			
			// check if icon file exists
			if (FileExists(IconFileName))
				strcpy(IconFile, IconFileName);
			else
				IconFile[0] = '\0';
		}
	}
	
	if (JSAcctsetIniPath != NULL)
		CustomIniPath = GetStringPlatformChars(env, JSAcctsetIniPath);
	

#ifdef WIN32 // **************** WIN32 ****************

	// remove the RegiServer RAS
	char regiRAS[50];
	getMsgString((char *)regiRAS, IDS_REGGIE_PROGITEM_NAME);
	(*m_lpfnRasDeleteEntry) (NULL, (LPSTR) (const char *) regiRAS);

	// creates prgram icons in folders and desktop
	int ret = CreateProgramItems(AccountName, CustomIniPath);

#else // **************** WIN16 ****************

	// remove reggie
	BOOL bResult = DeleteProgramGroupItem("StartUp", REGGIE_PROGITEM_NAME);
	bResult = CreateProgramItems16(AccountName, 
											 strlen(IconFile) > 0 ? IconFile : NULL, 
											 CustomIniPath);
#endif // !WIN32
}

