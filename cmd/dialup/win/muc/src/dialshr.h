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
#ifdef WIN32
#include  "ras.h"
#include "shlobj.h"
#include "winerror.h"
#else
#include "ras16.h"
#endif

#ifndef WIN32
#define MAX_PATH	100
#endif

typedef DWORD (WINAPI* RASSETENTRYPROPERTIES)(LPSTR, LPSTR, LPBYTE, DWORD, LPBYTE, DWORD);
typedef DWORD (WINAPI* RASGETCOUNTRYINFO)(LPRASCTRYINFO, LPDWORD);
typedef DWORD (WINAPI* RASENUMDEVICES)(LPRASDEVINFO, LPDWORD, LPDWORD);
typedef DWORD (WINAPI* RASGETENTRYPROPERTIES)(LPSTR, LPSTR, LPRASENTRY, LPDWORD, LPBYTE, LPDWORD);
typedef DWORD (WINAPI* RASVALIDATEENTRYNAME)(LPSTR, LPSTR);
typedef DWORD (WINAPI* RASDELETEENTRY)(LPSTR, LPSTR);
typedef DWORD (WINAPI* RASHANGUP)(HRASCONN);
typedef DWORD (WINAPI* RASDIAL)(LPRASDIALEXTENSIONS,LPTSTR,LPRASDIALPARAMS,DWORD,
    								LPVOID,LPHRASCONN);
typedef DWORD (WINAPI* RASENUMCONNECTIONS)(LPRASCONN,LPDWORD,LPDWORD);
typedef DWORD (WINAPI* RASSETENTRYDIALPARAMS)(LPTSTR,LPRASDIALPARAMS,BOOL);
typedef DWORD (WINAPI* RASENUMENTRIES)(LPTSTR,LPTSTR,LPRASENTRYNAME,LPDWORD,LPDWORD);
    
#ifdef WIN32
// for NT40 only
typedef DWORD (WINAPI* RASSETAUTODIALADDRESS)(LPSTR, DWORD, LPRASAUTODIALENTRYA, DWORD, DWORD);
typedef DWORD (WINAPI* RASGETAUTODIALADDRESS)(LPSTR, DWORD, LPRASAUTODIALENTRYA, LPDWORD, LPDWORD);
typedef DWORD (WINAPI* RASSETAUTODIALENABLE)(DWORD, BOOL);
typedef DWORD (WINAPI* RASSETAUTODIALPARAM)(DWORD, LPVOID, DWORD);
typedef DWORD (WINAPI* RASENUMAUTODIALADDRESSES)(LPTSTR *,LPDWORD,LPDWORD);
typedef DWORD (WINAPI* RASSETCREDENTIALS)(LPTSTR, LPTSTR, LPRASCREDENTIALS, BOOL);
#endif
    
extern HINSTANCE				m_hRasInst;
extern RASSETENTRYPROPERTIES	m_lpfnRasSetEntryProperties;
extern RASGETCOUNTRYINFO		m_lpfnRasGetCountryInfo;
extern RASENUMDEVICES			m_lpfnRasEnumDevices;
extern RASGETENTRYPROPERTIES	m_lpfnRasGetEntryProperties;
extern RASVALIDATEENTRYNAME		m_lpfnRasValidateEntryName;
extern RASDELETEENTRY			m_lpfnRasDeleteEntry;
extern RASHANGUP				m_lpfnRasHangUp;
extern RASDIAL					m_lpfnRasDial;
extern RASENUMCONNECTIONS		m_lpfnRasEnumConnections;
extern RASSETENTRYDIALPARAMS	m_lpfnRasSetEntryDialParams;
extern RASENUMENTRIES			m_lpfnRasEnumEntries;
    
#ifdef WIN32
// for NT40 only
extern RASSETAUTODIALENABLE		m_lpfnRasSetAutodialEnable;
extern RASSETAUTODIALADDRESS	m_lpfnRasSetAutodialAddress;
extern RASSETAUTODIALPARAM		m_lpfnRasSetAutodialParam;
extern RASENUMAUTODIALADDRESSES	m_lpfnRasEnumAutodialAddresses;
extern RASSETCREDENTIALS        m_lpfnRasSetCredentials;

extern int platformOS;          // platform OS  (95 or NT40)
#endif       

extern HINSTANCE DLLinstance;   // dll instance    
    
// account parameters
typedef struct ACCOUNTPARAMS 
{
  	char ISPName[32];
   	char FileName[16];
   	char DNS[16];
   	char DNS2[16];
   	char DomainName[255];
   	char LoginName[64];
   	char Password[64];
   	char ScriptFileName[255];
   	BOOL ScriptEnabled;
   	BOOL NeedsTTYWindow;
   	char ISPPhoneNum[64];
   	char ISDNPhoneNum[64];
   	BOOL VJCompressionEnabled;
   	BOOL IntlMode;
   	BOOL DialOnDemand;
} ACCOUNTPARAMS;
    
    
// location parameters
typedef struct LOCATIONPARAMS 
{
  	char ModemName[255];
   	char ModemType[80];
   	BOOL DialType;
   	char OutsideLineAccess[6];
   	BOOL DisableCallWaiting;
   	char DisableCallWaitingCode[6];
   	char UserAreaCode[6];
   	short UserCountryCode;
   	BOOL DialAsLongDistance;
   	char LongDistanceAccess[6];
   	BOOL DialAreaCode;
   	char DialPrefix[32];
   	char DialSuffix[32];
   	BOOL UseBothISDNLines;
   	BOOL b56kISDN;
} LOCATIONPARAMS;
    
// connection name parameters
typedef struct CONNECTIONPARAMS
{
	char			szEntryName[MAX_PATH];          
#ifdef WIN32
   	LPITEMIDLIST	pidl;
#endif
} CONNECTIONPARAMS;
    
    
extern size_t stRASENTRY;
extern size_t stRASCONN;
extern size_t stRASCTRYINFO;
extern size_t stRASDIALPARAMS;
extern size_t stRASDEVINFO;
extern size_t stRASCREDENTIALS;
  
#ifdef WIN32
void SizeofRAS95();
void SizeofRASNT40();
#else
void SizeofRAS16();
#endif
   
BOOL LoadRasFunctions(LPCSTR lpszLibrary);
void FreeRasFunctions();
BOOL GetModemList(char ***, int*);
BOOL GetModemType( char *strModemName, char *strModemType);
BOOL GetDialUpConnection16(CONNECTIONPARAMS **connectionNames, int *numNames);
void EnableDialOnDemand16(LPSTR lpProfileName, BOOL flag);
BOOL IsDialerConnected();
 
#ifdef WIN32
BOOL LoadRasFunctionsNT(LPCSTR lpszLibrary);
BOOL GetDialUpConnection95(CONNECTIONPARAMS **connectionNames, int *numNames);
BOOL GetDialUpConnectionNT(CONNECTIONPARAMS **connectionNames, int *numNames);
BOOL SameStrings(LPITEMIDLIST pidl, STRRET& lpStr1, LPCTSTR lpStr2);
    
LPITEMIDLIST Next(LPCITEMIDLIST pidl);
UINT GetSize(LPCITEMIDLIST pidl);
LPITEMIDLIST Create(UINT cbSize);
LPITEMIDLIST ConcatPidls(LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2);
    
HRESULT GetMyComputerFolder(LPSHELLFOLDER *ppshf, LPITEMIDLIST *ppidl);
    
HRESULT GetDialUpNetworkingFolder(LPSHELLFOLDER *ppshf, LPITEMIDLIST *ppidl);
    
void FAR PASCAL lineCallbackFuncNT(DWORD /* hDevice */,
                     DWORD /* dwMsg */,
                     DWORD /* dwCallbackInstance */,
                     DWORD /* dwParam1 */,
                     DWORD /* dwParam2 */,
                     DWORD /* dwParam3 */);
    
void EnableDialOnDemandNT(LPSTR lpProfileName, BOOL flag);
void EnableDialOnDemand95(LPSTR lpProfileName, BOOL flag);
    
#endif

