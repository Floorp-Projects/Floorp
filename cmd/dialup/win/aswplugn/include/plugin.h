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
// plugin.h
// this file is used for both win32 & win16 ported from plugin32.h
//
// Revision history:
// Date     Author         Reason
// -------------------------------------------------------------
// 01/26/97 xxxxxxxxxxxxxx Routine definition                         
//          xxxxxxxxxxxxxx Routine definition (plugin32.h)
//////////////////////////////////////////////////////////////// 

#ifndef _INC_PLUGIN_H_
#define _INC_PLUGIN_H_

#ifdef WINVER
#undef WINVER
#endif
#define WINVER 0x0401 // Win32 ras.h needs this

#ifdef WIN32
#include <ras.h>        // WIN32 uses MS RAS
#else
#include <shivaras.h> 	// Shiva RAS APIs
#endif


// default name of Regi program item
#define REGGIE_PROGITEM_NAME "Registration Server"

// Navigator quit message
#define ID_APP_SUPER_EXIT 34593


// handles to the plugin instance
typedef struct _PluginInstance
{
   NPWindow*      fWindow;
   uint16         fMode;

   HWND           fhWnd;
   WNDPROC        fDefaultWindowProc;
} PluginInstance;

// account parameters
typedef struct ACCOUNTPARAMS {
	char ISPName[60];		//we use user's accountname + ISP name on win32
	char FileName[_MAX_PATH];
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
typedef struct LOCATIONPARAMS {
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
	DWORD DisconnectTime;
} LOCATIONPARAMS;

extern HINSTANCE DLLinstance; // dll instance


extern void* RegDataBuf;   // regi data buffer
extern void* RegDataArray; // regi data array
extern long RegDataLength;
extern BOOL RegiMode;
extern char IconFile[_MAX_PATH];
extern BOOL RegExtendedDataFlag;
extern JRIGlobalRef  globalRef;	// reference to JRI object arrray

extern char **ModemList;
extern int ModemListLen;


// RAS struct sizes
extern size_t stRASENTRY;
extern size_t stRASCONN;
extern size_t stRASCTRYINFO;
extern size_t stRASDIALPARAMS;
extern size_t stRASDEVINFO;
extern size_t stRASCREDENTIALS;
extern size_t stRASENTRYNAME;



typedef DWORD (WINAPI* RASSETENTRYPROPERTIES)(LPSTR, LPSTR, LPBYTE, DWORD, LPBYTE, DWORD);
typedef DWORD (WINAPI* RASGETCOUNTRYINFO)(LPRASCTRYINFO, LPDWORD);
typedef DWORD (WINAPI* RASENUMDEVICES)(LPRASDEVINFO, LPDWORD, LPDWORD);
typedef DWORD (WINAPI* RASGETENTRYPROPERTIES)(LPSTR, LPSTR, LPBYTE, LPDWORD, LPBYTE, LPDWORD);
typedef DWORD (WINAPI* RASVALIDATEENTRYNAME)(LPSTR, LPSTR);
typedef DWORD (WINAPI* RASDELETEENTRY)(LPSTR, LPSTR);

// for win16 only
#ifndef WIN32
typedef DWORD (WINAPI* RASDIAL)(LPRASDIALEXTENSIONS, LPSTR, LPRASDIALPARAMS, DWORD, LPVOID, LPHRASCONN);
typedef DWORD (WINAPI* RASGETERRORSTRING)(UINT, LPSTR, DWORD);
typedef DWORD (WINAPI* RASHANGUP)(HRASCONN);
typedef DWORD (WINAPI* RASENUMCONNECTIONS)(LPRASCONN, LPDWORD, LPDWORD);
typedef DWORD (WINAPI* RASSETENTRYDIALPARAMS)(LPSTR, LPRASDIALPARAMS, BOOL);

// Shiva RAS extension APIs
typedef BOOL  (WINAPI* SRSETUPMODEMWIZARD)(HWND, LPCSETUPMODEMPARAMS);
typedef DWORD (WINAPI* SRSETDEVICEINFO)(LPCSETUPMODEMPARAMS); 
typedef DWORD (WINAPI* SRSETCOMPRESSIONINFO)(BOOL);
typedef DWORD (WINAPI* SRSETDIALONDEMANDINFO)(BOOL, LPSTR);
typedef DWORD (WINAPI* SRSETDIALSTRINGINFO)(BOOL, MODEMVOLUME);
typedef DWORD (WINAPI* SRSETCONNECTIONINFO)(LPSTR, BOOL, BOOL, DWORD);
#endif

#ifdef WIN32 // **************************** WIN32 ******************************
// for NT40 only
typedef DWORD (WINAPI* RASSETAUTODIALADDRESS)(LPSTR, DWORD, LPRASAUTODIALENTRYA, DWORD, DWORD);
typedef DWORD (WINAPI* RASSETAUTODIALENABLE)(DWORD, BOOL);
typedef DWORD (WINAPI* RASSETAUTODIALPARAM) (DWORD, LPVOID, DWORD);
typedef DWORD (WINAPI* RASSETCREDENTIALS)(LPTSTR, LPTSTR, LPRASCREDENTIALS, BOOL);

extern RASSETAUTODIALENABLE    m_lpfnRasSetAutodialEnable;
extern RASSETAUTODIALADDRESS   m_lpfnRasSetAutodialAddress;
extern RASSETAUTODIALPARAM	    m_lpfnRasSetAutodialParam;
extern RASSETCREDENTIALS       m_lpfnRasSetCredentials;

#else      // **************************** WIN16 ******************************

extern RASDIAL						g_lpfnRasDial;
extern RASGETERRORSTRING		g_lpfnRasGetErrorString;
extern RASHANGUP					g_lpfnRasHangUp;
extern RASENUMCONNECTIONS		g_lpfnRasEnumConnections;
extern RASSETENTRYDIALPARAMS	g_lpfnRasSetEntryDialParams;

// Shiva RAS extension APIs
extern SRSETUPMODEMWIZARD		g_lpfnSetupModemWizard; 
extern SRSETDEVICEINFO			g_lpfnSetDeviceInfo;
extern SRSETDIALONDEMANDINFO	g_lpfnSetDialOnDemandInfo;
extern SRSETCOMPRESSIONINFO	g_lpfnSetCompressionInfo; 
extern SRSETDIALSTRINGINFO		g_lpfnSetDialStringInfo;
extern SRSETCONNECTIONINFO		g_lpfnSetConnectionInfo;

#endif // !WIN32
      
// library (DLL) instancd handles..      
extern HINSTANCE					m_hRasInst;
#ifndef WIN32
extern HINSTANCE					m_hShivaModemWizInst;
#endif

extern RASSETENTRYPROPERTIES   m_lpfnRasSetEntryProperties;
extern RASGETCOUNTRYINFO       m_lpfnRasGetCountryInfo;
extern RASENUMDEVICES          m_lpfnRasEnumDevices;
extern RASGETENTRYPROPERTIES   m_lpfnRasGetEntryProperties;
extern RASVALIDATEENTRYNAME    m_lpfnRasValidateEntryName;
extern RASDELETEENTRY          m_lpfnRasDeleteEntry;

#ifdef WIN32 // **************************** WIN32 ******************************
extern int platformOS;     // platform OS  (95 or NT40)
void SizeofRASNT40();      // ini WinNT RAS sizes
#endif // WIN32

void SizeofRAS();          // init Win95 & Win3.1 RAS sizes
void trace( const char* traceStatement );

#endif // _INC_PLUGIN_H_
