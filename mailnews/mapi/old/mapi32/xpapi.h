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
//  Various routines for MAPI functions.
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//
#ifndef __XPAPI_H
#define __XPAPI_H

#ifdef WIN16

#include <string.h>
#include <direct.h>
#include <shellapi.h>
#include <stdlib.h>
#else
#include <winreg.h>
#endif 
 
#ifdef WIN16
extern "C" {
#ifndef MAPI_OLE	// Because MSFT doesn't do this for us :-(
#include <mapi.h>
#endif 
}           
#else
//#include <mapi.h>
#endif


#ifdef WIN32
#define MAPI_IMPLEMENT(param) param PASCAL 
#define LOAD_DS
#else        
#define LOAD_DS 	__loadds
#define MAPI_IMPLEMENT(param) extern "C" param FAR PASCAL
 
#endif   

#ifdef WIN16
#define _MAX_PATH 				260  /* max. length of full pathname*/
#define MAPI_E_LOGON_FAILURE  	3 
#define MAPI_E_ACCESS_DENIED	6  
#define INVALID_HANDLE_VALUE 	(HANDLE)-1  
#define KEY_QUERY_VALUE 		0x0001  
#define HKEY_LOCAL_MACHINE 		((HKEY)0x80000002)
#define HKEY_ROOT				HKEY_CLASSES_ROOT
#else 
#define HKEY_ROOT				((HKEY)0x80000002)
#endif

//
// registry keys
//
#ifdef WIN32
static char szNavigatorSection[] = "Software\\Netscape\\Netscape Navigator";
static char szNavigatorCurVersionSection[] = "Software\\Netscape\\Netscape Navigator\\%s\\Main";
static char szCurrentVersionKey[] = "CurrentVersion";
static char szInstallDirKey[] = "Install Directory";
static char szMapiSection[] = "Software\\Netscape\\Netscape Navigator\\MAPI";
static char szTempFiles[] = "TempFiles";  
static char szMapiLog[] = "NSMAPI32.LOG";
#else 
//32 bit key strings for trying to read the 32bit registry
static char szNavigatorSection32[] = "Software\\Netscape\\Netscape Navigator";
static char szNavigatorCurVersionSection32[] = "Software\\Netscape\\Netscape Navigator\\%s\\Main";
static char szMapiSection32[] = "Software\\Netscape\\Netscape Navigator\\MAPI";

// ini section and key strings 
static char szNetscapeINI[] = "nscp.ini";
static char szNavigatorSection[] = "Netscape Navigator";
static char szNavigatorCurVersionSection[] = "Netscape Navigator-%s";
static char szCurrentVersionKey[] = "CurrentVersion";
static char szInstallDirKey[] = "Install Directory";
static char szMapiSection[] = "MAPI";
static char szTempFiles[] = "TempFiles";
static char szExeName[] = "NAVSTART.EXE";
static char szMapiLog[] = "NSMAPI16.LOG";
#endif                                   

//Since REGSAM is just an ACCESS_MASK which is just a DWORD and it's not 
//declared in win16 we'll make one hear for the purpose of keeping parameters
//the same even though the access rights don't get used for win16.

typedef DWORD REGSAM;


// XP declarations

int 	LOAD_DS Is_16_OR_32_BIT_CommunitorRunning();
WORD 	LOAD_DS XP_CallProcess(LPCSTR pPath, LPCSTR pCmdLine);
HKEY LOAD_DS RegOpenParent(LPCSTR pSection, HKEY hRootKey, REGSAM access);
HKEY LOAD_DS RegCreateParent(LPCSTR pSection, HKEY hMasterKey);
BOOL LOAD_DS GetConfigInfoStr(LPCSTR pSection, LPCSTR pKey, LPSTR pBuf, int lenBuf, HKEY hMasterKey);
BOOL LOAD_DS GetConfigInfoNum(LPCSTR pSection, LPCSTR pKey, DWORD* pVal, HKEY hMasterKey);
BOOL LOAD_DS SetConfigInfoStr(LPCSTR pSection, LPCSTR pKey, LPSTR pStr, HKEY hMasterKey); 

BOOL  LOAD_DS XP_GetInstallDirectory(LPCSTR pcurVersionSection, LPCSTR pInstallDirKey, LPSTR path, UINT nSize, HKEY hKey); 
BOOL  LOAD_DS XP_GetVersionInfoString(LPCSTR pNavigatorSection, LPCSTR pCurrentVersionKey, LPSTR pcurVersionStr, UINT nSize, HKEY hKey);
DWORD LOAD_DS XP_GetInstallLocation(LPSTR pPath, UINT nSize);
BOOL LOAD_DS  XP_CopyFile(LPCSTR lpExistingFile, LPCSTR lpNewFile, BOOL bFailifExist);

#endif    // __XPAPI_H
