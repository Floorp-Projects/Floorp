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
// confhook.cpp : implementation file
//

#include "stdafx.h"
#include <shellapi.h>

#include "confhook.h"

extern "C" {
#include "xpgetstr.h"
extern int MK_MSG_CANT_FIND_CALLPOINT;
};



//
// NEEDED STRINGS
// ==============
// MK_MSG_CANT_FIND_CONFAPP - Can't find the app to run
// IDS_CONFAPP				- "Conferencing Application" type string, I think?
//

// 
// Other internal defines...
//
#define	CONFAPP_UNAVAILABLE		0
#define	CONFAPP_CONFERENCE		1
#define	CONFAPP_THIRDPARTY		2

//
// Registry stuff:
//
#define	_MAX_ARG_LEN_					128	  

//
// Sections for profile lookups...
//
static TCHAR	lpszWin32ConfAppBranch[] = _T("Software\\Netscape\\ConfApplication");
static TCHAR	lpszWin16ConfAppSection[] = _T("ConfApplication");
static int		confAppAvailibility = CONFAPP_UNAVAILABLE;

BOOL 
FileExists(LPCSTR szFile) 
{
	struct _stat buf;
    int result;

    result = _stat( szFile, &buf );
    return (result == 0);
}

//
// This routine is specific to looking up values in the new ConfApp 
// registry/ini file section.
//
CString 
FEU_GetConfAppProfileString(const CString &queryString) 
{ 
	char	argBuffer[_MAX_PATH + 1];
	CString returnVal(""); 

	returnVal = "";
	strcpy(argBuffer, "");

	//
	// If the defined conference application on the system is
	// still a leftover NSCP Conference, we should still use
	// that setting. In this case, simply return the default
	// value for Netscape Conference.
	//
	if (confAppAvailibility == CONFAPP_CONFERENCE)
	{
		//
		// First, we will assign a value to argBuffer that corresponds
		// to NSCP Conference's value for this particular argument.
		//
		if (queryString == IDS_CONFAPP_FULLPATHNAME)
		{
		CString installDirectory, executable;

	#ifdef WIN32
			installDirectory = FEU_GetInstallationDirectory(szLoadString(IDS_CONFERENCE_REGISTRY), szLoadString(IDS_PATHNAME));
			executable = "\\NSCONF32.EXE";
	#else // XP_WIN16
			installDirectory = FEU_GetInstallationDirectory(szLoadString(IDS_CONFERENCE),szLoadString(IDS_INSTALL_DIRECTORY));
			executable = "\\NSCSTART.EXE";
	#endif

			wsprintf(argBuffer, "%s%s", installDirectory, executable);
		}
		else if (queryString == IDS_CONFAPP_NOBANNER)
		{
			strcpy(argBuffer, "/b");
		}
		else if (queryString == IDS_CONFAPP_MIMEFILE)
		{
			strcpy(argBuffer, "/f");
		}		
		else if (queryString == IDS_CONFAPP_DIRECTIP)
		{
			strcpy(argBuffer, "/d");
		}
		else if (queryString == IDS_CONFAPP_EMAIL)
		{
			strcpy(argBuffer, "/i");
		}
		else if (queryString == IDS_CONFAPP_SERVER)
		{
			strcpy(argBuffer, "/s");
		}
		else if (queryString == IDS_CONFAPP_USERNAME)
		{
			strcpy(argBuffer, "");
		}
	
		returnVal = argBuffer;
		return returnVal;
	}

#ifdef _WIN32 

	HKEY hKey; 
	LONG lResult; 
	char szVal[_MAX_PATH + 1]; 
	
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpszWin32ConfAppBranch, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) 
	{ 	
		unsigned long cbData = sizeof(szVal); 
		lResult = RegQueryValueEx(hKey, queryString, NULL, NULL, (LPBYTE) szVal, &cbData); 
		RegCloseKey(hKey); 

		if (lResult == ERROR_SUCCESS) 
		{
			returnVal = szVal; 	
		}
	} 
#else // XP_WIN16 

	char szPath[_MAX_PATH + 1]; 
	const char *pOldProfile = theApp.m_pszProfileName; 
	
	UINT nSize = GetWindowsDirectory(szPath, _MAX_PATH + 1); 
	XP_STRCPY(szPath + nSize, szLoadString(IDS_NSCPINI)); 
	theApp.m_pszProfileName = szPath; 
	
	returnVal = theApp.GetProfileString(lpszWin16ConfAppSection, queryString, NULL); 

	theApp.m_pszProfileName = pOldProfile; 	
#endif

    return returnVal; 
} 

//
// This function is needed to see if a conferencing endpoint
// is defined on the machine. Return true if there is or false
// if it doesn't exist.
//
BOOL
FEU_IsConfAppAvailable(void)
{
	//
	// Check to see if a conf. app is defined on the local
	// machine.
	//
	CString fileName = FEU_GetConfAppProfileString(IDS_CONFAPP_FULLPATHNAME); 
	if (fileName.IsEmpty())
	{
		if (FEU_IsConferenceAvailable())
		{
			confAppAvailibility = CONFAPP_CONFERENCE;
			return TRUE;
		}
		else
		{
			confAppAvailibility = CONFAPP_UNAVAILABLE;
			return FALSE;
		}
	}

	// 
	// We should really make sure there is something at the location 
	// defined before we go further.
	//
	if (FileExists(fileName)) 
	{
		confAppAvailibility = CONFAPP_THIRDPARTY;
		return TRUE;
	}
	else
	{
		confAppAvailibility = CONFAPP_UNAVAILABLE;
		return FALSE;
	}
}

//
// This will be the central function where we will do the launching
// of Conference on this endpoint. 
// 
// Args: makeCall - true if this is going to make a call or false
// if we are just launching the appliction.
//
void
LaunchConfEndpoint(char *commandLine, HWND parent)
{
	CString executable;
	char	*cline = commandLine;
	char	cbuf[_MAX_ARG_LEN_];
	
	executable = FEU_GetConfAppProfileString(IDS_CONFAPP_FULLPATHNAME); 

	//
	// Check to see if the executable is defined on the machine. If it is not, then
	// show a message and return.
	// 
	if (executable.IsEmpty())
	{
		CString s;
		if (s.LoadString( IDS_CONFERENCE ))
		{
			::MessageBox(parent, XP_GetString(MK_MSG_CANT_FIND_CALLPOINT), s, MB_OK | MB_APPLMODAL);
		}
		return;
	}

	//
	// Check if the commandline is null...if so, we need to try to find the banner
	// suppression flag and pass it to the executable.
	//
	if (!commandLine)
	{
		CString nobannerFlag = FEU_GetConfAppProfileString(IDS_CONFAPP_NOBANNER);
		strcpy(cbuf, nobannerFlag);
		cline = &(cbuf[0]);
	}

	int uSpawn =  (int) ShellExecute(NULL, "open", executable, cline, ".", SW_SHOW);
	if(uSpawn <= 32)	
	{
		char szMsg[80];
        switch(uSpawn) 
		{
		case 0:
		case 8:
			sprintf(szMsg, szLoadString(IDS_WINEXEC_0_8));
			break;
		case 2:                                      
		case 3:
			sprintf(szMsg, szLoadString(IDS_WINEXEC_2_3));
			break;
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			sprintf(szMsg, szLoadString(IDS_WINEXEC_10_THRU_15));
			break;
		case 16:
			sprintf(szMsg, szLoadString(IDS_WINEXEC_16));
			break;
		case 21:
			sprintf(szMsg, szLoadString(IDS_WINEXEC_21));
			break;
		default:
			sprintf(szMsg, szLoadString(IDS_WINEXEC_XX), uSpawn);
			break;
		}        
		
		CString s;
		if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
		{
			::MessageBox(parent, szMsg, s, MB_OK | MB_APPLMODAL);
		}
	}
}

