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
// mdmwiz.cpp
// Revision History:
// Date        Author            Reason
// ----------------------------------------------------------------------------
// 02/13/97    xxxxxxxxxxxxxx    Port Native API for win3.x
//             xxxxxxxxxxxxxx    Define Native API for win95 & winNT
///////////////////////////////////////////////////////////////////////////////

#include <npapi.h>
#include "plugin.h"


// windows include
#ifdef WIN32 // ********************* Win32 includes **************************
#include "resource.h"		// win32 resource
#include <winbase.h>
#else        // ********************* Win16 includes **************************
#include "asw16res.h"		// win16 resource
#include <stdlib.h>
#include <windows.h>
#include <tchar.h>
#include "shivaras.h"              					// Shiva RAS APIs
#include "helper16.h"

#define RAS16_DLLNAME   "rasapi16.dll"			// Shiva RAS DLL name
#define MDMWZ16_DLLNAME	"modemwiz.dll"			// SHiva RAS Modem Wizard DLL name
#endif  // !WIN32

#include <string.h>
#include "errmsg.h"           // for DisplayErrMsg()

// java include 
#include "netscape_npasw_SetupPlugin.h"
#include "java_lang_String.h"

JRIGlobalRef	g_globalRefModemList = NULL;


HINSTANCE m_hRasInst;			// RAS DLL instance
#ifndef WIN32
HINSTANCE m_hShivaModemWizInst;	// Shiva Modem Wizard DLL instance
#endif

RASSETENTRYPROPERTIES   m_lpfnRasSetEntryProperties;
RASGETCOUNTRYINFO       m_lpfnRasGetCountryInfo;
RASENUMDEVICES          m_lpfnRasEnumDevices;
RASGETENTRYPROPERTIES   m_lpfnRasGetEntryProperties;
RASVALIDATEENTRYNAME    m_lpfnRasValidateEntryName;
RASDELETEENTRY          m_lpfnRasDeleteEntry;

#ifndef WIN32	// ***************** WIN16 ************************
RASDIAL						g_lpfnRasDial;
RASGETERRORSTRING			g_lpfnRasGetErrorString;
RASHANGUP					g_lpfnRasHangUp;
RASENUMCONNECTIONS      g_lpfnRasEnumConnections;
RASSETENTRYDIALPARAMS	g_lpfnRasSetEntryDialParams;

// Shiva RAS extension APIs
SRSETUPMODEMWIZARD		g_lpfnSetupModemWizard; 
SRSETDEVICEINFO			g_lpfnSetDeviceInfo;
SRSETDIALONDEMANDINFO	g_lpfnSetDialOnDemandInfo;
SRSETCOMPRESSIONINFO		g_lpfnSetCompressionInfo;
SRSETDIALSTRINGINFO		g_lpfnSetDialStringInfo;
SRSETCONNECTIONINFO		g_lpfnSetConnectionInfo;
#endif

#ifdef WIN32 
// for NT40
RASSETAUTODIALENABLE    m_lpfnRasSetAutodialEnable;
RASSETAUTODIALADDRESS   m_lpfnRasSetAutodialAddress;
RASSETAUTODIALPARAM     m_lpfnRasSetAutodialParam;
RASSETCREDENTIALS       m_lpfnRasSetCredentials;
#endif

extern const char *GetStringPlatformChars(JRIEnv *env, struct java_lang_String *string);

#ifdef WIN32
char **ModemList = NULL;
int ModemListLen = 0;
#else
char g_szAddedModem[256] = { '\0' };
#endif

#ifdef WIN32 // ***************************** WIN32 ******************************

//********************************************************************************
//
// LoadRasFunctions()
//
// Used by NPP_Initialize() to Load Proper RAS DLL for Win95 & Win3.x
//
//********************************************************************************
BOOL LoadRasFunctions(LPCSTR lpszLibrary)
{
    m_hRasInst = ::LoadLibrary(lpszLibrary);
    if (m_hRasInst) {
        if ((m_lpfnRasSetEntryProperties = (RASSETENTRYPROPERTIES)::GetProcAddress(m_hRasInst,
                                           "RasSetEntryProperties")) &&
            (m_lpfnRasGetCountryInfo     = (RASGETCOUNTRYINFO)::GetProcAddress(m_hRasInst,
                                           "RasGetCountryInfo")) &&
            (m_lpfnRasEnumDevices        = (RASENUMDEVICES)::GetProcAddress(m_hRasInst, 
            										 "RasEnumDevices")) &&
            (m_lpfnRasGetEntryProperties = (RASGETENTRYPROPERTIES)::GetProcAddress(m_hRasInst,
                									 "RasGetEntryProperties")) &&
            (m_lpfnRasValidateEntryName  = (RASVALIDATEENTRYNAME)::GetProcAddress(m_hRasInst,
                									 "RasValidateEntryName")) &&
            (m_lpfnRasDeleteEntry = (RASDELETEENTRY)::GetProcAddress(m_hRasInst,
                									 "RasDeleteEntry")))
            return TRUE;
        else
            ::FreeLibrary(m_hRasInst);
    }
	
    m_hRasInst = NULL;
    return FALSE;
}

//********************************************************************************
//
// LoadRasFunctionsNT()
//
// Used by NPP_Initialize() to Load Proper RAS DLL for WinNT
//
//********************************************************************************
BOOL LoadRasFunctionsNT(LPCSTR lpszLibrary)
{
    m_hRasInst = ::LoadLibrary(lpszLibrary);
    if (m_hRasInst) {
        m_lpfnRasSetEntryProperties = (RASSETENTRYPROPERTIES)::GetProcAddress(m_hRasInst,
            "RasSetEntryPropertiesA");

        if (m_lpfnRasSetEntryProperties) {

            m_lpfnRasGetCountryInfo = (RASGETCOUNTRYINFO)::GetProcAddress(m_hRasInst,
                "RasGetCountryInfoA");
            m_lpfnRasEnumDevices = (RASENUMDEVICES)::GetProcAddress(m_hRasInst, "RasEnumDevicesA");
            m_lpfnRasGetEntryProperties = (RASGETENTRYPROPERTIES)::GetProcAddress(m_hRasInst,
                "RasGetEntryPropertiesA");
            m_lpfnRasValidateEntryName = (RASVALIDATEENTRYNAME)::GetProcAddress(m_hRasInst,
                "RasValidateEntryNameA");
            m_lpfnRasDeleteEntry = (RASDELETEENTRY)::GetProcAddress(m_hRasInst,
                "RasDeleteEntryA");

         // for NT40
            m_lpfnRasSetAutodialEnable = (RASSETAUTODIALENABLE)::GetProcAddress(m_hRasInst,
                "RasSetAutodialEnableA");
            m_lpfnRasSetAutodialAddress = (RASSETAUTODIALADDRESS)::GetProcAddress(m_hRasInst,
                "RasSetAutodialAddressA");
            m_lpfnRasSetCredentials = (RASSETCREDENTIALS)::GetProcAddress(m_hRasInst,
                "RasSetCredentialsA");
            m_lpfnRasSetAutodialParam = (RASSETAUTODIALPARAM)::GetProcAddress(m_hRasInst,
                "RasSetAutodialParamA");

         return TRUE;
      } else {
            ::FreeLibrary(m_hRasInst);
        }
    }

    m_hRasInst = NULL;
    return FALSE;
}

#else        // ***************************** WIN16 ******************************

//********************************************************************************
// LoadRas16Functions()
//
// Used by NPP_Initialize() to Load Proper Shiva RAS APIs Win3.x
//********************************************************************************
BOOL LoadRas16Functions()
{
	TCHAR szInstallPath[255], szLibPath[255];
	BOOL bGetInstallPath = GetShivaInstallPath(szInstallPath);
	assert(bGetInstallPath);
	
	if (!bGetInstallPath)  // abort if failed to find install path
		return FALSE;

	// construct path for Shiva CommModule RAS libraries to be loaded	
	strcpy(szLibPath, szInstallPath);	
	strcat(szLibPath, "\\");
	int nEndPath = strlen(szLibPath);
	
	strcat(szLibPath, RAS16_DLLNAME);
	m_hRasInst = ::LoadLibrary(szLibPath);

	strcpy(szLibPath, szInstallPath);	
	strcat(szLibPath, "\\");
	strcat(szLibPath, MDMWZ16_DLLNAME);
	m_hShivaModemWizInst = ::LoadLibrary(szLibPath);
   
	BOOL bRasLibLoaded = ((UINT)m_hRasInst >= 32),
	bModemWizLoaded = ((UINT)m_hShivaModemWizInst >= 32);
   	  
	if (bRasLibLoaded && bModemWizLoaded)
	{
		BOOL bGotProc;				// TRUE if all procedures are succesfully loaded
		bGotProc =	(m_lpfnRasSetEntryProperties	= (RASSETENTRYPROPERTIES)::GetProcAddress(m_hRasInst, "RasSetEntryProperties")) &&
						(m_lpfnRasEnumDevices			= (RASENUMDEVICES)::GetProcAddress(m_hRasInst, "RasEnumDevices")) &&
						(m_lpfnRasGetEntryProperties	= (RASGETENTRYPROPERTIES)::GetProcAddress(m_hRasInst, "RasEnumEntries")) &&
						(m_lpfnRasGetCountryInfo		= (RASGETCOUNTRYINFO)::GetProcAddress(m_hRasInst, "RasGetCountryInfo")) &&
						(m_lpfnRasValidateEntryName	= (RASVALIDATEENTRYNAME)::GetProcAddress(m_hRasInst, "RasValidateEntryNameA")) &&
						(m_lpfnRasDeleteEntry 			= (RASDELETEENTRY)::GetProcAddress(m_hRasInst, "RasDeleteEntry"))&&
						(g_lpfnRasDial						= (RASDIAL)::GetProcAddress(m_hRasInst, "RasDial")) &&
						(g_lpfnRasSetEntryDialParams	= (RASSETENTRYDIALPARAMS)::GetProcAddress(m_hRasInst, "RasSetEntryDialParams")) &&
						(g_lpfnRasGetErrorString		= (RASGETERRORSTRING)::GetProcAddress(m_hRasInst, "RasGetErrorString")) &&
						(g_lpfnRasHangUp					= (RASHANGUP)::GetProcAddress(m_hRasInst, "RasHangUp")) &&
						(g_lpfnRasEnumConnections		= (RASENUMCONNECTIONS)::GetProcAddress(m_hRasInst, "RasEnumConnections")) && 
						
						// Shiva RAS extension
						(g_lpfnSetupModemWizard 		= (SRSETUPMODEMWIZARD)::GetProcAddress(m_hShivaModemWizInst, "SetupModemWizard")) &&
						(g_lpfnSetDeviceInfo		 		= (SRSETDEVICEINFO)::GetProcAddress(m_hRasInst, "SetDeviceInfo")) &&
						(g_lpfnSetDialOnDemandInfo		= (SRSETDIALONDEMANDINFO)::GetProcAddress(m_hRasInst, "SetDialOnDemandInfo")) &&
						(g_lpfnSetCompressionInfo		= (SRSETCOMPRESSIONINFO)::GetProcAddress(m_hRasInst, "SetCompressionInfo")) &&
						(g_lpfnSetDialStringInfo		= (SRSETDIALSTRINGINFO)::GetProcAddress(m_hRasInst, "SetDialStringInfo")) &&
						(g_lpfnSetConnectionInfo		= (SRSETCONNECTIONINFO)::GetProcAddress(m_hRasInst, "SetConnectionInfo"));
 						
		if (bGotProc)
			return TRUE; 
	}
   
	// free loaded libs
	if (bRasLibLoaded)
		::FreeLibrary(m_hRasInst);
	if (bModemWizLoaded)
		::FreeLibrary(m_hShivaModemWizInst);

   // failed to load ShivaRemote DLLs
	return FALSE;
}
#endif // !WIN32


static const char c_szModemCPL[] = "rundll32.exe Shell32.dll,Control_RunDLL modem.cpl,,add";

//********************************************************************************
// native method:
//
// OpenModemWizard
//
// Runs the Modem Wizard (in this case the Win95 MdmWiz, and waits for conclusion)
//********************************************************************************

extern JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fOpenModemWizard(JRIEnv* env,
											   struct netscape_npasw_SetupPlugin* ThisPlugin)
{
#ifdef WIN32 //****************************** WIN32 ***************************
	PROCESS_INFORMATION pi;
	BOOL                fRet;
	STARTUPINFO         sti;
	UINT                err = ERROR_SUCCESS;

	memset(&sti,0,sizeof(sti));
	sti.cb = sizeof(STARTUPINFO);

	// BUGBUG-- need to check to make sure modem CPL is not already running,
	// bag out if that's the case

	// Run the modem wizard
	fRet = CreateProcess(NULL, (LPSTR)c_szModemCPL,
				NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi);

	if (!fRet)
		err = GetLastError();
	else {
		CloseHandle(pi.hThread);

		// Wait for the modem wizard process to complete
		WaitForSingleObject(pi.hProcess,INFINITE);
		CloseHandle(pi.hProcess);
    }
#else  //*********************************** WIN16 *******************************
	assert(g_lpfnSetupModemWizard);
	if (!g_lpfnSetupModemWizard)
		return;
		
   DWORD dwResult;
   
   CSETUPMODEMPARAMS setup_modem_param;
   memset( &setup_modem_param, 0, sizeof( CSETUPMODEMPARAMS));

	// get & set new modem via Modem Wizard...   
   BOOL bResult;
   if (bResult = (*g_lpfnSetupModemWizard)(GetActiveWindow(), &setup_modem_param))
   {
		dwResult = (*g_lpfnSetDeviceInfo)(&setup_modem_param);
		if (dwResult == 0)
			strcpy(g_szAddedModem, setup_modem_param.szModemType);
		else
			g_szAddedModem[0] = '0';
   }

#endif // WIN32

#if 0
	//
	// Update Win9x's internal structures so everyone knows about the modem
	//
	FinishModemConfiguration();
#endif
}


//********************************************************************************
// native method:
//
// IsModemWizardOpen
//
// Returns FALSE (modem wiz is always finished by return of StartModemWizard())
//********************************************************************************
extern JRI_PUBLIC_API(jbool)
native_netscape_npasw_SetupPlugin_SECURE_0005fIsModemWizardOpen(JRIEnv* env,
												 struct netscape_npasw_SetupPlugin* ThisPlugin)
{
	return(FALSE);
}


//********************************************************************************
// native method:
//
// CloseModemWizard
//
// Don't do anything? (may be able to force close of ModemWiz - need to check)
//********************************************************************************
extern JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fCloseModemWizard(JRIEnv* env,
												struct netscape_npasw_SetupPlugin* ThisPlugin)
{
	return;
}


//********************************************************************************
// native method:
//
// GetModemList (s/b GetModems)
//
// Returns list of modems available for use ('installed' by the user). For Win95
// this list come from the OS, and each entry contains 2 strings - the first is
// the Modem Name, and the second is the device type (both are needed to select
// the device to use to make a Dial-up connection).
//********************************************************************************
extern JRI_PUBLIC_API(jref)
native_netscape_npasw_SetupPlugin_SECURE_0005fGetModemList(JRIEnv* env,
											struct netscape_npasw_SetupPlugin* ThisPlugin)
{
	if (!m_lpfnRasEnumDevices)
		return NULL;
		
	DWORD           dwBytes = 0, dwDevices = 0;
	LPRASDEVINFO    lpRnaDevInfo;
	struct java_lang_String *value = NULL;

	// First find out how much memory to allocate
	DWORD dwResult = (*m_lpfnRasEnumDevices)(NULL, &dwBytes, &dwDevices);
	
	// no devices configured, return
	if (dwBytes == 0)
		return NULL;

	lpRnaDevInfo = (LPRASDEVINFO)malloc((size_t) dwBytes);

	if (!lpRnaDevInfo)
	{
		return NULL;
	}

	lpRnaDevInfo->dwSize = stRASDEVINFO;
   
   // now get the # devices..
	dwResult = (*m_lpfnRasEnumDevices)(lpRnaDevInfo, &dwBytes, &dwDevices);
	assert(dwResult == 0);

	// Allocate JavaScript array to store modem list 
	void *resultModemArray = JRI_NewObjectArray(env, dwDevices, class_java_lang_String(env), NULL);
	if (NULL == resultModemArray)
		return NULL;

	// lock the JRI array reference, dispose old reference if necessary
	if (g_globalRefModemList)
		JRI_DisposeGlobalRef(env, g_globalRefModemList); 
	g_globalRefModemList=JRI_NewGlobalRef(env, resultModemArray); 

#ifdef WIN32

	if (!ModemList) {
		
		// allocate array to store modem lists

		ModemList = (char **)malloc(sizeof(char *) * dwDevices);	
		ModemListLen = dwDevices;

		if (!ModemList)
			return NULL;

	} else {

		//free the old ModemList, and allocate new one

		for (int i=0; i<ModemListLen; i++)
			free(ModemList[i]);
		ModemList = NULL;

		ModemList = (char **)malloc(sizeof(char *) * dwDevices);	
		ModemListLen = dwDevices;

		if (!ModemList)
			return NULL;
	}

#endif

	for (unsigned short i=0; i<dwDevices; i++) {
		value = JRI_NewStringPlatform(env, lpRnaDevInfo[i].szDeviceName,
									strlen(lpRnaDevInfo[i].szDeviceName), NULL, 0);
 		JRI_SetObjectArrayElement(env, resultModemArray, i, value);

#ifdef WIN32
			ModemList[i] = strdup(lpRnaDevInfo[i].szDeviceName);
#endif
	}

	free(lpRnaDevInfo);                

	return (jref)resultModemArray;
}


//********************************************************************************
// native method:
//
// GetModemType
//
// Returns the type for the selected modem.
//********************************************************************************
extern JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_npasw_SetupPlugin_SECURE_0005fGetModemType(JRIEnv* env,
											struct netscape_npasw_SetupPlugin* ThisPlugin,
											java_lang_String *JSModemName)
{
	if (!m_lpfnRasEnumDevices)
		return NULL;

	DWORD							dwBytes = 0, dwDevices;
	LPRASDEVINFO				lpRnaDevInfo;
	struct java_lang_String *value = NULL;
	const char					*modem_name = NULL;

    // First get Modem (okay - Device) info from Win95
	// find out how much memory to allocate
	(*m_lpfnRasEnumDevices)(NULL, &dwBytes, &dwDevices);
	
	// no devices configured, abort..
	if (dwBytes ==0)
		return NULL;
	
	lpRnaDevInfo = (LPRASDEVINFO)malloc((size_t)dwBytes);

	if (!lpRnaDevInfo)
		return (NULL);

	lpRnaDevInfo->dwSize = stRASDEVINFO;
	
	(*m_lpfnRasEnumDevices)(lpRnaDevInfo, &dwBytes, &dwDevices);

	// Convert modem type to 'C' string
	if (JSModemName != NULL)
		modem_name = GetStringPlatformChars(env, JSModemName);

	// If match the modem given from JS then return the associated Type
	for (unsigned short i=0; i<dwDevices; i++)
	{
		if (!strcmp(modem_name, lpRnaDevInfo[i].szDeviceName)) {
			value = JRI_NewStringPlatform(env, lpRnaDevInfo[i].szDeviceType,
									strlen(lpRnaDevInfo[i].szDeviceType), NULL, 0);
			free(lpRnaDevInfo);
			return(value);
		}
	}

	free(lpRnaDevInfo);

	return(NULL);
}



//********************************************************************************
//
// GetNewModemList
//
//********************************************************************************
char **GetNewModemList(int *NewModemListLen)
{
	char **NewModemList=NULL;

	assert(m_lpfnRasEnumDevices);
	if (!m_lpfnRasEnumDevices)
		return NULL;
		
	DWORD           dwBytes = 0, dwDevices = 0;
	LPRASDEVINFO    lpRnaDevInfo;

	// First find out how much memory to allocate
	DWORD dwResult = (*m_lpfnRasEnumDevices)(NULL, &dwBytes, &dwDevices);
	
	// no devices configured, return
	if (dwBytes == 0)
		return NULL;

	lpRnaDevInfo = (LPRASDEVINFO)malloc((size_t) dwBytes);

	if (!lpRnaDevInfo)
	{
		return NULL;
	}

	lpRnaDevInfo->dwSize = stRASDEVINFO;
   
   // now get the # devices..
	dwResult = (*m_lpfnRasEnumDevices)(lpRnaDevInfo, &dwBytes, &dwDevices);
	assert(dwResult == 0);

	if (dwDevices > 0)  {

		if (!NewModemList) {
			NewModemList = (char **)malloc((size_t)(sizeof(char *) * dwDevices));	
			if (!NewModemList)
				return NULL;
		}

		*NewModemListLen = (int) dwDevices;	

		for (unsigned short i=0; i<dwDevices; i++) {
			NewModemList[i] = strdup(lpRnaDevInfo[i].szDeviceName);
		}
	}
	
	free(lpRnaDevInfo);                

	return (NewModemList);
}


//********************************************************************************
// native method:
//
// GetCurrentModemName
//
// Returns the newly installed modem.
//
// note: JavaScript will set this newly installed modem as a defult selected modem
//		in Accout Setup modem page.
//
// global variables ModemList and NewModemList is created in function GetModemList()
// we compare the two lists in this function to see if a new modem is added to
// the NewModemList.  If there is a new mdoem, return the device name of the new
// modem, otherwise, we return NULL.
//********************************************************************************
extern JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentModemName(JRIEnv* env,
												   struct netscape_npasw_SetupPlugin* self)
{

#ifdef WIN32
	char **NewModemList=NULL;
	int NewModemListLen=0;
	int i=0, j=0;
	char NewModem[100];
	struct java_lang_String *modem = NULL;

	// get new modem list
	NewModemList = GetNewModemList(&NewModemListLen);

	NewModem[0] = '\0';
	if ((ModemList) && (NewModemList)) {

		for (i=0; i<NewModemListLen; i++) 
		
			if (NewModem[0] == '\0') 
			
				for (j=0; j<ModemListLen; j++) 
				
					if ((strcmp(NewModemList[i], ModemList[j]) != 0) && (j == ModemListLen-1)) {
						strcpy(NewModem, NewModemList[i]);
						break;
					} else if (strcmp(NewModemList[i], ModemList[j]) == 0) {
						break;
					}
	}

	if (NewModem[0] != '\0') {

		// if a new modem is found, create a java string
		modem = JRI_NewStringPlatform(env, NewModem, strlen(NewModem), NULL, 0);

		// add the new modem to the Modem List
		ModemList = (char **)realloc(ModemList, sizeof(char *) * (NewModemListLen));
		ModemList[ModemListLen] = strdup(NewModem);
		ModemListLen++;

	}

	// free NewModemList here!
	if (NewModemList) {
	
		for (i=0; i<NewModemListLen; i++)
			free(NewModemList[i]);

		free(NewModemList);
		NewModemList = NULL;
	}

	return (modem);

#else
   
   if (strlen(g_szAddedModem) > 0)
   {
		struct java_lang_String *modem = JRI_NewStringPlatform(env, g_szAddedModem, strlen(g_szAddedModem), NULL, 0);
   	return modem;
   }
	return (NULL);
#endif
}

