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
////////////////////////////////////////////////////////////////
// helper.cpp
// helper routines
//
// Revision History
// 01/28/97    xxxxxxxxxxxxx
////////////////////////////////////////////////////////////////

#include <windows.h>
#include <ddeml.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>      

#include <dos.h>
#include <shivaerr.h>      // WIN16 uses Shiva RAS
#if __cplusplus
extern "C"
{
#include <shivaras.h>      // Shiva RAS APIs
}
#endif

#include "helper16.h"

TCHAR g_szShivaInstallPath[255]			= {'\0'},
      g_szNetscapeInstallPath[255]		= {'\0'},
      g_szShivaSRemoteConfigFile[255]	= {'\0'},
      g_szNavProgramFolderName[255]		= {'\0'};

//***************************************************************
//***************************************************************
// Utility Routines..
//***************************************************************
//***************************************************************

//////////////////////////////////////////////////////////////////
// Sleeps for the specified number of seconds
//////////////////////////////////////////////////////////////////
void Sleep(UINT nSec)
{
	if (nSec == 0)
		return;
		
	MSG msg;
	time_t currTime, quitTime;
	time(&currTime);		// get current time
	quitTime = currTime + nSec;

	while (currTime < quitTime)
	{
		if(GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		time(&currTime);		// get current time
	}
}


//***************************************************************
//***************************************************************
// Helper Routines dealing paths..
//***************************************************************
//***************************************************************

//////////////////////////////////////////////////////////////////
// Get Shiva install path.
// path name is copied into csFilePath
// return TRUE if successful
//////////////////////////////////////////////////////////////////
BOOL GetShivaInstallPath(TCHAR* csFilePath)
{
   if (!csFilePath)
   	return FALSE;
   
   if (strlen(g_szShivaInstallPath) > 0)          // Shiva install path saved?
   {
   	strcpy(csFilePath, g_szShivaInstallPath);
   	return TRUE;
   }
   
   TCHAR szPreferredFile[255];
   // Get Shiva preferred file path in win.ini:  use sremote.ini entry as default 
   int nBytesCopied = GetProfileString(INI_SR_CONNWZ_SECTION, INI_SR_PREFFILE_KEY, 
                      INI_SR_DFLT_PREFFILE, szPreferredFile, sizeof(szPreferredFile));

	// Get Shiva install path in win.ini:  use preferred file (default: sremote.ini) entry
   nBytesCopied = GetProfileString(INI_SR_CONNWZ_SECTION, szPreferredFile, 
                  INI_SR_DFLT_PREFFILE, g_szShivaInstallPath, sizeof(g_szShivaInstallPath));
                  
   strcpy(csFilePath, g_szShivaInstallPath);
   
   assert(nBytesCopied > 0);
   return (nBytesCopied > 0);
}

//////////////////////////////////////////////////////////////////
// Get ShivaRemote Configuration file:
// path name is copied into csFilePath 
// return TRUE if successful
//////////////////////////////////////////////////////////////////
BOOL GetShivaSRemoteConfigFile(TCHAR* csFilePath)
{
   if (!csFilePath)
   	return FALSE;
   
   if (strlen(g_szShivaSRemoteConfigFile) > 0)	// Shiva install path saved, just copy
   {
   	strcpy(csFilePath, g_szShivaSRemoteConfigFile);
   }
   else 														// Shiva config file not avail, construct it
   {
	   TCHAR szPreferredFile[64];
   
   	GetShivaInstallPath(g_szShivaSRemoteConfigFile);
   	strcat(g_szShivaSRemoteConfigFile, "\\");
   
	   // Get Shiva preferred file path in win.ini:  use sremote.ini entry as default 
   	GetProfileString(INI_SR_CONNWZ_SECTION, INI_SR_PREFFILE_KEY, 
			INI_SR_DFLT_PREFFILE, szPreferredFile, sizeof(szPreferredFile));
   
	   strcat(g_szShivaSRemoteConfigFile, szPreferredFile);
   	strcpy(csFilePath, g_szShivaSRemoteConfigFile);
	}
	   
   return TRUE;
}

////////////////////////////////////////////////////////////////
// Returns a full path to the install directory for Netscape
////////////////////////////////////////////////////////////////
BOOL GetNetscapeInstallPath(TCHAR* lpBuff)
{
	if (!lpBuff)
		return FALSE;
	if (strlen(g_szNetscapeInstallPath) > 0)          // Shiva install path saved?
	{
		strcpy(lpBuff, g_szNetscapeInstallPath);
	   	return TRUE;
	}
	
	char szCurrentVersion[_MAX_PATH],
		szCurrentVersionIniSection[_MAX_PATH];
	BOOL bResult = FALSE;
	int  nPathSize;
   
	assert(lpBuff);
	if (lpBuff)
	{  
		// get current version, if not found, use default: INI_NS_CURRVER_DEFLT
		nPathSize = ::GetPrivateProfileString(INI_NAVIGATOR_SECTION, INI_CURRENTVERSION_KEY, 
					INI_NS_CURRVER_DEFLT, szCurrentVersion, _MAX_PATH, INI_NETSCAPE_FILE);
		assert(nPathSize);
 		                   
		if (nPathSize > 0)
		{  
			// build section name "Netscape Navigator-xxx" where xxx == current version
			strcpy(szCurrentVersionIniSection, INI_NS_APPNAME_PREFIX);
			strcat(szCurrentVersionIniSection, szCurrentVersion);
	                  
			nPathSize = ::GetPrivateProfileString(szCurrentVersionIniSection, INI_INSTALL_DIR_KEY, 
						INI_NS_INSTALLDIR_DEFLT, lpBuff, _MAX_PATH, INI_NETSCAPE_FILE);
	
			// if failed to get path, then use default path
			if (nPathSize == 0 || strlen(lpBuff) == 0)
				strcpy(lpBuff, INI_NS_INSTALLDIR_DEFLT);
	                        
			strcpy(g_szNetscapeInstallPath, lpBuff);   // save install path
			bResult = TRUE;      
		}
	}
   
	assert(bResult);
	return bResult;
}

////////////////////////////////////////////////////////////////
// Returns a Netscape Communicator program group name
////////////////////////////////////////////////////////////////
BOOL GetNetscapeProgramGroupName(TCHAR* lpBuff)
{
	if (!lpBuff)
		return FALSE;

	if (strlen(g_szNavProgramFolderName) > 0)   // program folder saved?
	{
		strcpy(lpBuff, g_szNavProgramFolderName);
		return TRUE;
	}
	
	char szCurrentVersion[_MAX_PATH],
		szCurrentVersionIniSection[_MAX_PATH];
	BOOL bResult = FALSE;
	int  nPathSize;
   
	assert(lpBuff);
	if (lpBuff)
	{  
		// get current version, if not found, use default: INI_NS_CURRVER_DEFLT
		nPathSize = ::GetPrivateProfileString(INI_NAVIGATOR_SECTION, INI_CURRENTVERSION_KEY, 
					INI_NS_CURRVER_DEFLT, szCurrentVersion, _MAX_PATH, INI_NETSCAPE_FILE);
		assert(nPathSize);
 		                   
		if (nPathSize > 0)
		{  
			// build section name "Netscape Navigator-xxx" where xxx == current version
			strcpy(szCurrentVersionIniSection, INI_NS_APPNAME_PREFIX);
			strcat(szCurrentVersionIniSection, szCurrentVersion);

			// get program group name	                  
			nPathSize = ::GetPrivateProfileString(szCurrentVersionIniSection, INI_NS_PROGGRPNAME_KEY,
						INI_NS_PROGGRPNAME_DEFLT, lpBuff, _MAX_PATH, INI_NETSCAPE_FILE);
	
			// if failed to get path, then use default path
			if (nPathSize == 0 || strlen(lpBuff) == 0)
				strcpy(lpBuff, INI_NS_INSTALLDIR_DEFLT);
	                        
			strcpy(g_szNavProgramFolderName, lpBuff);   // save install path
			bResult = TRUE;      
		}
	}
   
	assert(bResult);
	return bResult;
}

//////////////////////////////////////////////////////////////////
// Get Shiva SR connection file path name base on an connection (account) name.
// path name is copied into csFilePath
// return TRUE if successful
//////////////////////////////////////////////////////////////////
BOOL GetConnectionFilePath(LPCSTR AccountName, TCHAR* csFilePath, BOOL bIncludePath)
{
	char szIniPath[_MAX_PATH],	// path to Shiva's install path with Shiva INI files
		 szIniFName[20],	// Shiva's INI file name
		 szFilePath[_MAX_PATH],	// path to Shiva's INI file
		 szTemplate[_MAX_PATH],	// template for searching sr files
		 szEntryFName[_MAX_PATH];	// entry file name
		 	
	// sample of Shiva's section in win.ini:
	// [ConnectW Config]
	// preferred file=sremote.ini
	// sremote.ini=C:\Netscape\program

   BOOL bResult = GetShivaInstallPath(szIniPath);
	
	strcat(szIniPath, "\\"); 				// (c:\netscape\program\)
	strcpy(szFilePath, szIniPath);			// (c:\netscape\program\)
	strcpy(szTemplate, szFilePath);
	strcat(szTemplate, SR_ALL_CONNFILES);// (c:\netscape\program\*.sr)
	strcat(szIniPath, szIniFName);			// (c:\netscape\program\sremote.ini)

	// find connection file name for the entry
	struct _find_t c_file;
	long hFile;
	BOOL bEntryFound = FALSE;
	if ((hFile = _dos_findfirst(szTemplate, _A_NORMAL, &c_file)) == 0)
	{
		do      	// seek thru connection files to match entry name
		{
			char szCurrEntryName[RAS_MaxEntryName];
		    			 
			strcpy(szEntryFName, szFilePath);	// (c:\netscape\program\)
			strcat(szEntryFName, c_file.name);	// (c:\netscape\program\current.sr)
		    		
			// get Shiva entry description (name)
			int nResult = GetPrivateProfileString("Dial-In Configuration", "Description", "", 
									szCurrEntryName, RAS_MaxEntryName, szEntryFName);
		
			// match?
			if ((!strcmpi(szCurrEntryName, AccountName)) || (strstr(szCurrEntryName, AccountName)))
			{
				bEntryFound = TRUE;
				if (!bIncludePath)
					strcpy(szEntryFName, c_file.name);
			}
		} while (!bEntryFound && (_dos_findnext(&c_file) == 0));
	}
    
	strcpy(csFilePath, szEntryFName);
   
	return bEntryFound;
}
 
 
//***************************************************************
//***************************************************************
// Helper Routines for editing program group items
//***************************************************************
//***************************************************************

////////////////////////////////////////////////////////////////
// DDE call back function
////////////////////////////////////////////////////////////////
HDDEDATA CALLBACK _export
DdeCallback(UINT wType, UINT wFmt, HCONV hConv, HSZ hsz1,HSZ hsz2, 
         HDDEDATA hDDEData, DWORD dwData1, DWORD dwData2)
{
    return NULL;  // nothing needs to be done here...
}

/////////////////////////////////////////////////////////////////
// Sends the given command string to the Program Manager
// return TRUE if successful
/////////////////////////////////////////////////////////////////
BOOL SendShellCommand(DWORD ddeInst, LPCSTR lpszCommand)
{
	HSZ         hszServTop;
	HCONV       hconv;
	int         nLen;
	HDDEDATA    hData;
	DWORD       dwResult;
	BOOL        bResult = FALSE;

	// Create string handle to service/topic
	hszServTop = DdeCreateStringHandle(ddeInst, "PROGMAN", CP_WINANSI);

	// Attempt to start conversation with server app
	if ((hconv = DdeConnect(ddeInst, hszServTop, hszServTop, NULL)) != NULL) 
	{
		// Get length of the command string
		nLen = strlen(lpszCommand);

		// Send command to server app
		hData = DdeClientTransaction((LPBYTE)lpszCommand, // data to pass
                                     nLen + 1,            // length of data
                                     hconv,               // handle of conversation
                                     NULL,                // handle of name-string
                                     CF_TEXT,             // clipboard format
                                     XTYP_EXECUTE,        // transaction type
                                     1000,                // timeout duration
                                     &dwResult);          // points to transaction result
		if (hData)
			bResult = TRUE;

		// End conversation
		DdeDisconnect(hconv);
	}

	// Free service/topic string handle
	DdeFreeStringHandle(ddeInst, hszServTop);

	// Delay a little to give the shell time to acknowledge the termination
//	Sleep(DDE_WAIT_TIMER);

	return bResult;
}

///////////////////////////////////////////////////////////////// 
// Add an item to an program group (caller should select active
// destination program group prior to calling this function)
// return TRUE if successful
/////////////////////////////////////////////////////////////////
BOOL AddProgramItem(DWORD  ddeInst,          // DDE Instance
                    LPCSTR lpszItemPath,     // command line arguement                 
                    LPCSTR lpszItemTitle,    // program item title
                    LPCSTR lpszItemIconPath) // icon path
{
	BOOL bResult;
	static char NEAR szBuf[1024];
   
	if (lpszItemIconPath)
		wsprintf(szBuf, "[AddItem(%s,%s,%s)]", lpszItemPath, lpszItemTitle, lpszItemIconPath);
	else
		wsprintf(szBuf, "[AddItem(%s,%s)]", lpszItemPath, lpszItemTitle);
	bResult = SendShellCommand(ddeInst, szBuf);
	   
 	assert(bResult);   
	return bResult;
}

///////////////////////////////////////////////////////////////// 
// Delete an item to an program group (caller should select active
// destination program group prior to calling this function)
// return TRUE if successful
/////////////////////////////////////////////////////////////////
BOOL DeleteProgramItem(DWORD  ddeInst,          // DDE Instance
                       LPCSTR lpszItemTitle)    // program item title
{
	BOOL bResult = TRUE;
	static char NEAR szBuf[1024];
   
	sprintf(szBuf, "[DeleteItem(%s)]", lpszItemTitle);
	bResult = SendShellCommand(ddeInst, szBuf);

	assert(bResult);   

	return bResult;
}

//////////////////////////////////////////////////////////////////
// make a program group active
//////////////////////////////////////////////////////////////////
BOOL MakeActiveGroup(DWORD ddeInst, const char * lpszFolder)
{
	// Win3.x will not activate a group if the group is already restored.
	// This is a workaround for the problem
	char lpCommand[512];
	sprintf(lpCommand, "[ShowGroup(%s, 2)][ShowGroup(%s, 1)]", lpszFolder, lpszFolder);
	BOOL bResult = SendShellCommand(ddeInst, lpCommand);
	assert(bResult);
    
	return bResult;
}

///////////////////////////////////////////////////////////////////////
// Add a program item to the StartUp program group
///////////////////////////////////////////////////////////////////////
BOOL AddProgramGroupItem(LPCSTR lpszProgramGroup, // program group name
                         LPCSTR lpszItemPath,     // path of item to be added
                         LPCSTR lpszItemTitle,    // program group item title
                         LPCSTR lpszItemIconPath) // program group item icon path
{
	DWORD ddeInst = 0;
	UINT uDDEInit = DdeInitialize(&ddeInst, DdeCallback, APPCMD_CLIENTONLY, 0);
  
	assert(uDDEInit == DMLERR_NO_ERROR);
	BOOL bResult = FALSE;            
	if (uDDEInit == DMLERR_NO_ERROR) 
	{
		// Make Netscape the active group
		if (bResult = MakeActiveGroup(ddeInst, lpszProgramGroup)) 
		{
//			Sleep(DDE_WAIT_TIMER);

			// Create a program item instance for the Internet provider
			bResult = AddProgramItem(ddeInst, lpszItemPath, lpszItemTitle, 
											lpszItemIconPath);

//			Sleep(DDE_WAIT_TIMER);
		}
		
		DdeUninitialize(ddeInst);
	}
    
	assert(bResult);

	return bResult;
}

///////////////////////////////////////////////////////////////// 
// Delete a program item from a program group
// return TRUE if successful
/////////////////////////////////////////////////////////////////
BOOL DeleteProgramGroupItem(LPCSTR lpszProgramGroup,  // program group name
                            LPCSTR lpszItemTitle)     // program group item title
{
	DWORD ddeInst = 0;
	UINT uDDEInit = DdeInitialize(&ddeInst, DdeCallback, APPCMD_CLIENTONLY, 0);
	assert(uDDEInit == DMLERR_NO_ERROR);
                                        
	BOOL bResult;            
	if (uDDEInit) 
	{
		// Make Netscape Personal Edition the active group
		if (bResult = MakeActiveGroup(ddeInst, lpszProgramGroup)) 
		{
//			Sleep(DDE_WAIT_TIMER);

			// Create a program item instance for the Internet provider
			DeleteProgramItem(ddeInst,lpszItemTitle);

//			Sleep(DDE_WAIT_TIMER);
		}

		DdeUninitialize(ddeInst);
	}

	assert(bResult);
	return bResult;
}                           

/////////////////////////////////////////////////////////////////
// construct a valid win16 name (filename or program item name)
/////////////////////////////////////////////////////////////////
void ParseWin16BadChar(char *pName, BOOL bFileName, int nMaxNameSize)
{
	if ((!pName && pName[0] == '\0') || !nMaxNameSize)
		return;

	// skip pathname if parsing filename
	char *pFilename = pName;
	if (bFileName)						
	{
		char *pChar = strrchr(pName, '\\');
		if (pChar)
			pFilename = ++pChar;
	}
	                                                         
	// allocate buffer: filesize is 8.3 format, + null terminator	                                                         
	int nBufSize = bFileName ? (12 + 1) : (nMaxNameSize + 1);		                                        
	char *temp = (char *) malloc(sizeof(char) * nBufSize);
	memset(temp, '\0', nBufSize);
	
	int nIndex = 0;                                          
	int nMax = (bFileName) ? 8 : nBufSize;
	BOOL bDone = FALSE;						// TRUE if all is parsed
	for (int i = 0; i < nMax; i++)
	{
		if (pFilename[nIndex] == '\0')	//	hit the end, done...
		{
			bDone = TRUE;
			break;
		}

		if (bFileName) 						// parsing file name
		{
			if (pFilename[nIndex] == '.')						// hit end of filename
			{
				strncat(temp, &(pFilename[nIndex]), 4);	// concatenate extension
				bDone = TRUE;
				break;
			}
			switch (pFilename[nIndex])
			{
				// skip bad filename chars
				case ' ' :	case '\'':	case ',' :	case '(' :	case ';' :	
				case '\\':	case '+' :	case '=' :  case '|' :  case '[' :
				case ']' :	case '"' :	case ':' :  case '?' :  case '*' :
				case '>' :	case '<' :
				   ++nIndex;	
			   	--i;
				   break;
				default  :
					temp[i] = pFilename[nIndex++];
			} // switch 
		}
		else
		{
			switch (pFilename[nIndex])
			{
				// skip bad chars
				case ',' :	case '(' :	case ';' :	case '\\':	case '+' :
				case '=' :	case '|' :	case '[' :	case ']' :	case '"' :
				case ':' :	case '?' :	case '*' :	case '>' :	case '<' :
				   ++nIndex;	
			   	--i;
				   break;
				default  :
					temp[i] = pFilename[nIndex++];
			} //switch()
		} // else
	} // for()
	
	// for filenames: append extension if avail
	if (!bDone && bFileName)
	{
		char *pTemp = strrchr(pFilename, '.');
		if (pTemp)
			strncat(temp, pTemp, 4);
	}
	
	strncpy(pFilename, temp, nBufSize);
}
