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
//
//  Various routines for Address Book functions.
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
#define _MAX_PATH 				260  /* max. length of full pathname*/
#define INVALID_HANDLE_VALUE 	(HANDLE)-1  
#define KEY_QUERY_VALUE 		0x0001  
#define HKEY_LOCAL_MACHINE 		((HKEY)0x80000002)
#define HKEY_ROOT				HKEY_CLASSES_ROOT
#else 
#define HKEY_ROOT				((HKEY)0x80000002)
#endif

#ifdef WIN32
#define NAB_IMPLEMENT(param) param PASCAL 
#define LOAD_DS
#else        
#define LOAD_DS 	__loadds
#define NAB_IMPLEMENT(param) extern "C" param FAR PASCAL
#endif  

//
// registry keys
//
#ifdef WIN32
static char szNavigatorSection[] = "Software\\Netscape\\Netscape Navigator";
static char szNavigatorCurVersionSection[] = "Software\\Netscape\\Netscape Navigator\\%s\\Main";
static char szCurrentVersionKey[] = "CurrentVersion";
static char szInstallDirKey[] = "Install Directory";
static char szTempFiles[] = "TempFiles";  
static char szNABLog[] = "NAB32.LOG";
#else 
//32 bit key strings for trying to read the 32bit registry
static char szNavigatorSection32[] = "Software\\Netscape\\Netscape Navigator";
static char szNavigatorCurVersionSection32[] = "Software\\Netscape\\Netscape Navigator\\%s\\Main";

// ini section and key strings 
static char szNetscapeINI[] = "nscp.ini";
static char szNavigatorSection[] = "Netscape Navigator";
static char szNavigatorCurVersionSection[] = "Netscape Navigator-%s";
static char szCurrentVersionKey[] = "CurrentVersion";
static char szInstallDirKey[] = "Install Directory";
static char szTempFiles[] = "TempFiles";
static char szExeName[] = "NAVSTART.EXE";
static char szNABLog[] = "NAB16.LOG";
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