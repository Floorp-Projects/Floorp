/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
//
//  XPAPI.CPP
//  API implementation file for mapi16.dll and mapi32.dll
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "xpapi.h"
#include "mapiutl.h"

WORD LOAD_DS XP_CallProcess(LPCSTR pPath, LPCSTR pCmdLine)
{            
	WORD wReturn = 0;
#ifndef WIN16
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;

	memset(&startupInfo, 0, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);
	if (wReturn = CreateProcess(pPath, (LPTSTR)pCmdLine, NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &startupInfo, &processInfo))
	{
		WaitForInputIdle(processInfo.hProcess, 120000);
	}
#else       
	// char szMsg[80];	
	char szExecute[512];
	lstrcpy(szExecute, pPath);
	lstrcat(szExecute, " ");
	lstrcat(szExecute, pCmdLine);
	wReturn = WinExec(szExecute,SW_SHOW);
	
	if (wReturn < 32) 
	{
	    wReturn = 0;
	}
#endif
	return wReturn;
}

HKEY LOAD_DS RegOpenParent(LPCSTR pSection, HKEY hRootKey, REGSAM access)
{
	HKEY hKey;
#ifndef WIN16
	if (RegOpenKeyEx(hRootKey, pSection, 0, access, &hKey) != ERROR_SUCCESS)
	{
		return(NULL);
	}        
#else
    if (RegOpenKey(hRootKey, pSection, &hKey) != ERROR_SUCCESS)
    {
    	return(NULL);
    }
#endif
	return(hKey);
}

HKEY LOAD_DS RegCreateParent(LPCSTR pSection, HKEY hMasterKey)
{
	HKEY hKey;
	if (RegCreateKey(hMasterKey, pSection, &hKey) != ERROR_SUCCESS)
	{
		return(NULL);
	}        
	return(hKey);
}

BOOL LOAD_DS GetConfigInfoStr(LPCSTR pSection, LPCSTR pKey, LPSTR pBuf, int lenBuf, HKEY hMasterKey)
{
	HKEY hKey;
	hKey = RegOpenParent(pSection, hMasterKey, KEY_QUERY_VALUE);
	if (!hKey)
	{
		return(FALSE);
	}

	DWORD len = (DWORD)lenBuf;
#ifndef WIN16
	BOOL retVal = (RegQueryValueEx(hKey, pKey, NULL, NULL, (LPBYTE)pBuf, &len) == ERROR_SUCCESS);
#else
	BOOL retVal = (RegQueryValue(hKey, pKey, pBuf, (long far*)&len) == ERROR_SUCCESS);
#endif
	RegCloseKey(hKey);
	return(retVal);
}

BOOL LOAD_DS GetConfigInfoNum(LPCSTR pSection, LPCSTR pKey, DWORD* pVal, HKEY hMasterKey)
{
	HKEY hKey;
	hKey = RegOpenParent(pSection, hMasterKey, KEY_QUERY_VALUE);
	if (!hKey)
	{
		return(FALSE);
	}

	DWORD len = sizeof(DWORD);
#ifndef WIN16
	BOOL retVal = (RegQueryValueEx(hKey, pKey, NULL, NULL, (LPBYTE)pVal, &len) == ERROR_SUCCESS);
#else
	BOOL retVal = (RegQueryValue(hKey, pKey, (char far*)pVal, (long far*)&len) == ERROR_SUCCESS);
#endif
	RegCloseKey(hKey);
	return(retVal);
}

BOOL LOAD_DS SetConfigInfoStr(LPCSTR pSection, LPCSTR pKey, LPSTR pStr, HKEY hMasterKey)
{
	HKEY hKey;
	hKey = RegCreateParent(pSection, hMasterKey);
	if (!hKey)
	{
		return(FALSE);
	}
#ifndef WIN16
	BOOL retVal = (RegSetValueEx(hKey, pKey, 0, REG_SZ, (LPBYTE)pStr, lstrlen(pStr) + 1) == ERROR_SUCCESS);
#else
	BOOL retVal = (RegSetValue(hKey, pKey, REG_SZ, pStr, lstrlen(pStr) + 1) == ERROR_SUCCESS);
#endif
	RegCloseKey(hKey);
	return(retVal);
}


BOOL LOAD_DS XP_GetInstallDirectory(LPCSTR pcurVersionSection, LPCSTR pInstallDirKey, LPSTR path, UINT nSize, HKEY hKey)
{   
#ifdef WIN32   
	if (!GetConfigInfoStr(pcurVersionSection, pInstallDirKey, path, nSize, hKey))
	{  
		return FALSE;
	}
	else
	{
		return TRUE;
	}       
#else
	if ( 0 < GetPrivateProfileString(pcurVersionSection, pInstallDirKey,"ERROR", path, nSize, szNetscapeINI))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
#endif
}


BOOL LOAD_DS XP_GetVersionInfoString(LPCSTR pNavigatorSection, LPCSTR pCurrentVersionKey, LPSTR pcurVersionStr, UINT nSize, HKEY hKey)
{ 
#ifdef WIN32
	if (!GetConfigInfoStr(pNavigatorSection, pCurrentVersionKey, pcurVersionStr, nSize, HKEY_LOCAL_MACHINE))
	{
		return FALSE;
	}               
	else
	{
		return TRUE;
	}
#else 
	if ( 0 < GetPrivateProfileString(pNavigatorSection, pCurrentVersionKey,"ERROR", pcurVersionStr, nSize, szNetscapeINI))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
#endif

}   

DWORD LOAD_DS XP_GetInstallLocation(LPSTR pPath, UINT nSize)
{           
char curVersionStr[256]; 
char curVersionSection[256];

  if (!pPath) 
    return  MAPI_E_LOGON_FAILURE;
  
#ifdef WIN32
  if (!XP_GetVersionInfoString(szNavigatorSection, szCurrentVersionKey, curVersionStr, 
    sizeof(curVersionStr), HKEY_LOCAL_MACHINE))
  {
    return (MAPI_E_LOGON_FAILURE);
  }
  
  wsprintf(curVersionSection, szNavigatorCurVersionSection, curVersionStr);
		
  if (!XP_GetInstallDirectory(curVersionSection, szInstallDirKey, pPath, 
    nSize, HKEY_LOCAL_MACHINE))
  {
    return (MAPI_E_ACCESS_DENIED);
    
  }
  
  lstrcat(pPath, "\\");
  lstrcat(pPath, "Program\\netscape.exe");
  
  return SUCCESS_SUCCESS;
#else
  if (32 == Is_16_OR_32_BIT_CommunitorRunning())
  {   
    if (!GetConfigInfoStr("snews\\shell\\open", "command", curVersionStr, sizeof(curVersionStr), HKEY_CLASSES_ROOT))
    {
      return (MAPI_E_ACCESS_DENIED);
    }
    else
    {	
      char *pFind = strstr(curVersionStr,"-h");
      
      if (pFind)
      {
        *pFind=0;
        lstrcpy(pPath,curVersionStr);
      }
      else
      {
        return (MAPI_E_ACCESS_DENIED);
      }
    }
    
    return SUCCESS_SUCCESS;
  }
  else //setup up to start navstart since we are a sixteen bit DLL.
  {
    if (!XP_GetVersionInfoString(szNavigatorSection, szCurrentVersionKey, curVersionStr, sizeof(curVersionStr), HKEY_LOCAL_MACHINE))
    {
      return (MAPI_E_LOGON_FAILURE);
    }
    
    wsprintf(curVersionSection, szNavigatorCurVersionSection, curVersionStr);
    
    if (!XP_GetInstallDirectory(curVersionSection, szInstallDirKey, pPath,nSize, HKEY_LOCAL_MACHINE))
    {
      return (MAPI_E_ACCESS_DENIED);		
    }
    
    lstrcat(pPath, "\\");
    lstrcat(pPath, "NAVSTART.EXE");
    return SUCCESS_SUCCESS;	
  }
#endif
}

int LOAD_DS Is_16_OR_32_BIT_CommunitorRunning()
{
  if (FindWindow("AfxFrameOrView", NULL) && !FindWindow("aHiddenFrameClass", NULL))
		return(16);
	else if (FindWindow("aHiddenFrameClass", NULL))
		return(32);
	else 
		return 0;
}

// size of buffer to use for copying files.
#define COPYBUFSIZE 1024

#ifdef WIN16
     
BOOL Win16CopyFile(LPCSTR a_Src, LPCSTR a_Dest, BOOL a_bOverwrite)
{
	OFSTRUCT ofSrc, ofDest;
	HFILE hSrc, hDest;
	BOOL bResult;
	
	ofDest.cBytes = ofSrc.cBytes = sizeof(OFSTRUCT);
	hDest = OpenFile(a_Dest, &ofDest, OF_EXIST);
	if (hDest != HFILE_ERROR && !a_bOverwrite)
		bResult = FALSE;	// file exists but caller doesn't want file overwritten
	else { // file either doesn't exist, or caller wants it overwritten.
		hSrc = OpenFile(a_Src, &ofSrc, OF_READ);
		hDest = OpenFile(a_Dest, &ofDest, OF_WRITE | OF_CREATE);
		if (hSrc != HFILE_ERROR && hDest != HFILE_ERROR) {
			unsigned char buf[COPYBUFSIZE];
			UINT bufsize =  COPYBUFSIZE;
			UINT bytesread;
			
			bResult = TRUE;			
			while (0 != (bytesread = _lread(hSrc, (LPVOID)buf, bufsize))) {
				if ((bytesread == HFILE_ERROR) || 	// check for read error...
											// and write error
					(bytesread != _lwrite(hDest, (LPVOID)buf, bytesread))) {
					bResult = FALSE; // could be out of diskspace
					break;
				}
			}
		}
		else
			bResult = FALSE;

		if (hSrc != HFILE_ERROR)
			_lclose(hSrc);
		if (hDest != HFILE_ERROR)
			_lclose(hDest);
	}
			
	return bResult;
}

#endif  // WIN16

BOOL LOAD_DS 
XP_CopyFile(LPCSTR lpExistingFile, LPCSTR lpNewFile, BOOL bFailifExist)
{                                                  
#ifdef WIN32
  return	CopyFile(lpExistingFile, lpNewFile, bFailifExist);
#else                                                             
  return    Win16CopyFile(lpExistingFile, lpNewFile, TRUE);
#endif
}    
