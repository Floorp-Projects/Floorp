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
#include "stdafx.h"
#include <string.h>
#include <stdio.h>
#include <time.h>   
#include "dos.h"

#ifdef WIN32  
#include <winbase.h>
#include <raserror.h>
#include <shlobj.h>
#include <regstr.h>
#include <tapi.h>
#endif
    
#include "dialshr.h"
    
HINSTANCE               m_hRasInst;
     
RASSETENTRYPROPERTIES   m_lpfnRasSetEntryProperties;
RASGETCOUNTRYINFO       m_lpfnRasGetCountryInfo;
RASENUMDEVICES          m_lpfnRasEnumDevices;
RASGETENTRYPROPERTIES   m_lpfnRasGetEntryProperties;
RASVALIDATEENTRYNAME    m_lpfnRasValidateEntryName;
RASDELETEENTRY          m_lpfnRasDeleteEntry;
RASHANGUP                               m_lpfnRasHangUp;
RASDIAL                                 m_lpfnRasDial;
RASENUMCONNECTIONS              m_lpfnRasEnumConnections;
RASSETENTRYDIALPARAMS   m_lpfnRasSetEntryDialParams;
RASENUMENTRIES                  m_lpfnRasEnumEntries;
    
#ifdef WIN32
// for NT40
RASSETAUTODIALENABLE    m_lpfnRasSetAutodialEnable;
RASSETAUTODIALADDRESS   m_lpfnRasSetAutodialAddress;
RASGETAUTODIALADDRESS   m_lpfnRasGetAutodialAddress;
RASSETAUTODIALPARAM             m_lpfnRasSetAutodialParam;
RASSETCREDENTIALS               m_lpfnRasSetCredentials;
RASENUMAUTODIALADDRESSES        m_lpfnRasEnumAutodialAddresses;
#else
#define LocalFree       free    
#define MAX_PATH        100       
#endif
    
#define MAX_ENTRIES     20
void *ReggieScript;     //Ptr to Script data from Reggie
  
//********************************************************************************
//
// LoadRasFunctions()
//
//********************************************************************************
BOOL LoadRasFunctions(LPCSTR lpszLibrary)
{
	//    ASSERT(!m_hRasInst);
    m_hRasInst = ::LoadLibrary(lpszLibrary);
    if ((UINT)m_hRasInst > 32) 
	{
		m_lpfnRasSetEntryProperties = (RASSETENTRYPROPERTIES)::GetProcAddress(m_hRasInst,
	    "RasSetEntryProperties");
    
		m_lpfnRasGetCountryInfo = (RASGETCOUNTRYINFO)::GetProcAddress(m_hRasInst,
		"RasGetCountryInfo");
		m_lpfnRasEnumDevices = (RASENUMDEVICES)::GetProcAddress(m_hRasInst, "RasEnumDevices");
		m_lpfnRasGetEntryProperties = (RASGETENTRYPROPERTIES)::GetProcAddress(m_hRasInst,
		"RasGetEntryProperties");
		m_lpfnRasValidateEntryName = (RASVALIDATEENTRYNAME)::GetProcAddress(m_hRasInst,
		"RasValidateEntryName");
		m_lpfnRasDeleteEntry = (RASDELETEENTRY)::GetProcAddress(m_hRasInst,
		"RasDeleteEntry");

		m_lpfnRasHangUp = (RASHANGUP)::GetProcAddress(m_hRasInst, "RasHangUpA");
		m_lpfnRasDial = (RASDIAL)::GetProcAddress(m_hRasInst, "RasDialA");
		m_lpfnRasEnumConnections = (RASENUMCONNECTIONS)::GetProcAddress(m_hRasInst,
		"RasEnumConnectionsA");
		m_lpfnRasSetEntryDialParams = (RASSETENTRYDIALPARAMS)::GetProcAddress(m_hRasInst,
		"RasSetEntryDialParamsA");
		m_lpfnRasEnumEntries = (RASENUMENTRIES)::GetProcAddress(m_hRasInst,
		"RasEnumEntriesA");

		return TRUE;
    
	} 
	else 
	{
#ifdef WIN32 // win95
		MessageBox(NULL,"Please install Dial-up Networking", "Netscape", MB_ICONSTOP);     
#else		// win16
		MessageBox(NULL,"Please install Shiva Remote Dialer", "Netscape", MB_ICONSTOP);     
#endif
		::FreeLibrary(m_hRasInst);
		m_hRasInst = NULL;
	return FALSE; 
    }
}
    
    
    
#ifdef WIN32
//********************************************************************************
//
// LoadRasFunctionsNT()
//
//********************************************************************************
BOOL LoadRasFunctionsNT(LPCSTR lpszLibrary)
{
	m_hRasInst = ::LoadLibrary(lpszLibrary);
	
    if ((UINT)m_hRasInst > 32) 
	{
	m_lpfnRasSetEntryProperties = (RASSETENTRYPROPERTIES)::GetProcAddress(m_hRasInst,
	   "RasSetEntryPropertiesA");

		m_lpfnRasGetCountryInfo = (RASGETCOUNTRYINFO)::GetProcAddress(m_hRasInst,
		    "RasGetCountryInfoA");
		m_lpfnRasEnumDevices = (RASENUMDEVICES)::GetProcAddress(m_hRasInst, "RasEnumDevicesA");
		m_lpfnRasGetEntryProperties = (RASGETENTRYPROPERTIES)::GetProcAddress(m_hRasInst,
		"RasGetEntryPropertiesA");
		m_lpfnRasValidateEntryName = (RASVALIDATEENTRYNAME)::GetProcAddress(m_hRasInst,
		"RasValidateEntryNameA");
		m_lpfnRasDeleteEntry = (RASDELETEENTRY)::GetProcAddress(m_hRasInst,
		"RasDeleteEntryA");
		m_lpfnRasEnumEntries = (RASENUMENTRIES)::GetProcAddress(m_hRasInst,
		    "RasEnumEntriesA");
		m_lpfnRasHangUp = (RASHANGUP)::GetProcAddress(m_hRasInst, "RasHangUpA");
		m_lpfnRasDial = (RASDIAL)::GetProcAddress(m_hRasInst, "RasDialA");
		m_lpfnRasEnumConnections = (RASENUMCONNECTIONS)::GetProcAddress(m_hRasInst,
		    "RasEnumConnectionsA");
		m_lpfnRasSetEntryDialParams = (RASSETENTRYDIALPARAMS)::GetProcAddress(m_hRasInst,
			 "RasSetEntryDialParamsA");
    
		// AUTODIAL
		m_lpfnRasSetAutodialEnable = (RASSETAUTODIALENABLE)::GetProcAddress(m_hRasInst,
		    "RasSetAutodialEnableA");
		m_lpfnRasSetAutodialAddress = (RASSETAUTODIALADDRESS)::GetProcAddress(m_hRasInst,
		    "RasSetAutodialAddressA");
		m_lpfnRasGetAutodialAddress = (RASGETAUTODIALADDRESS)::GetProcAddress(m_hRasInst,
		    "RasGetAutodialAddressA");
		m_lpfnRasSetAutodialParam = (RASSETAUTODIALPARAM)::GetProcAddress(m_hRasInst,
		    "RasSetAutodialParamA");
		m_lpfnRasSetCredentials = (RASSETCREDENTIALS)::GetProcAddress(m_hRasInst,
		    "RasSetCredentialsA");
		m_lpfnRasEnumAutodialAddresses = (RASENUMAUTODIALADDRESSES)::GetProcAddress(m_hRasInst,
		    "RasEnumAutodialAddressesA"); 

	}                
    
	else 
	{
		MessageBox(NULL, "Please install Dial-up Networking", "Netscape", MB_ICONSTOP);
		::FreeLibrary(m_hRasInst);
	m_hRasInst = NULL;
	    return FALSE;       
	}
	return TRUE;       
}
    
#endif    
//********************************************************************************
//
// FreeRasFunctions()
//
//********************************************************************************
void FreeRasFunctions()
{
	if((UINT)m_hRasInst > 32)
		FreeLibrary(m_hRasInst);
}
//********************************************************************************
//
// GetModemList (s/b GetModems)
//
// Returns list of modems available for use ('installed' by the user). For Win95
// this list come from the OS, and each entry contains 2 strings - the first is
// the Modem Name, and the second is the device type (both are needed to select
// the device to use to make a Dial-up connection).
//********************************************************************************
BOOL GetModemList(char ***resultModemList, int *numDevices)
{
	DWORD           dwBytes = 0, dwDevices;
    LPRASDEVINFO    lpRnaDevInfo;
    
    // First find out how much memory to allocate
    (*m_lpfnRasEnumDevices)(NULL, &dwBytes, &dwDevices);
    lpRnaDevInfo = (LPRASDEVINFO)malloc(dwBytes);
    
	if(!lpRnaDevInfo)
		return (FALSE);
    
    lpRnaDevInfo->dwSize = stRASDEVINFO;
	(*m_lpfnRasEnumDevices)(lpRnaDevInfo, &dwBytes, &dwDevices);
    
    // copy all entries to the char array
    *resultModemList = new char* [dwDevices+1];
    if(*resultModemList == NULL)
	return FALSE;
    
    *numDevices = dwDevices;
    for (unsigned short i=0; i<dwDevices; i++) 
    {
	(*resultModemList)[i] = new char[strlen(lpRnaDevInfo[i].szDeviceName)+1];
	if((*resultModemList)[i] == NULL)
		return FALSE;
	strcpy((*resultModemList)[i], lpRnaDevInfo[i].szDeviceName);
    }
    return TRUE;
}
    
    
//********************************************************************************
//
// GetModemType
//
// Returns the type for the selected modem.
//********************************************************************************
BOOL GetModemType( char *strModemName, char *strModemType)
{
    DWORD           dwBytes = 0, dwDevices;
    LPRASDEVINFO    lpRnaDevInfo;
      
    // First get Modem (okay - Device) info from Win95
    // find out how much memory to allocate
	(*m_lpfnRasEnumDevices)(NULL, &dwBytes, &dwDevices);
    lpRnaDevInfo = (LPRASDEVINFO)malloc(dwBytes);
    
    if(!lpRnaDevInfo)
		return (NULL);
    
    lpRnaDevInfo->dwSize = stRASDEVINFO;
    (*m_lpfnRasEnumDevices)(lpRnaDevInfo, &dwBytes, &dwDevices);
    
    // If match the modem given from JS then return the associated Type
    for (unsigned short i=0; i<dwDevices; i++) 
    {
	if (0 == strcmp(strModemName, lpRnaDevInfo[i].szDeviceName)) 
	{
		strModemType = new char[strlen(lpRnaDevInfo[i].szDeviceType) + 1];
	    strcpy(strModemType,lpRnaDevInfo[i].szDeviceType);
		return TRUE;
	}                
    }
    return FALSE;
}
    
    
    
#ifdef WIN32
//********************************************************************************
//
// EnableDialOnDemand  (win95)
//
// Set the magic keys in the registry to enable dial on demand
//********************************************************************************
void EnableDialOnDemand95(LPSTR lpProfileName, BOOL flag)
{
    
	HKEY    hKey;
	DWORD   dwDisposition;
	long    result;
	char  * szData;
    
	//
	// We need to tell windows about dialing on demand
	//
	result = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\Winsock\\Autodial",
			NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
    
	// err, oops
	if (result != ERROR_SUCCESS)
		return;
    
	szData = "url.dll";
	result = RegSetValueEx(hKey, "AutodialDllName32", NULL, REG_SZ, (LPBYTE)szData, strlen(szData));
    
	szData = "AutodialHookCallback";
	result = RegSetValueEx(hKey, "AutodialFcnName32", NULL, REG_SZ, (LPBYTE)szData, strlen(szData));
    
	RegCloseKey(hKey);
	
	//
	// set the autodial flag first
	//
	result = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
				NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
    
	// err, oops
	if (result != ERROR_SUCCESS)
	return;
    
	// set the autodial and idle-time disconnect
	DWORD dwValue = flag;
	result = RegSetValueEx(hKey, "EnableAutodial", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
    
	// try setting autodisconnect here
	dwValue = 1;
	result = RegSetValueEx(hKey, "EnableAutoDisconnect", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
    
	// default auto-disconnect after 5 minutes
	dwValue = 5;
	result = RegSetValueEx(hKey, "DisconnectIdleTime", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
    
	RegCloseKey(hKey);
    
	//
	// set the autodisconnect flags here too
	//
	result = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\Internet Settings",
				NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
    
	// err, oops
	if (result != ERROR_SUCCESS)
	return;
    
	dwValue = 1;
	result = RegSetValueEx(hKey, "EnableAutoDisconnect", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
    
	// default auto-disconnect after 5 minutes
	dwValue = 5;
	result = RegSetValueEx(hKey, "DisconnectIdleTime", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
    
	RegCloseKey(hKey);
    
	//
	// OK, let's tell it which profile to autodial
	//
	result = RegCreateKeyEx(HKEY_CURRENT_USER, "RemoteAccess", NULL, NULL, NULL,
				KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
    
	// err, oops
	if (result != ERROR_SUCCESS)
	return;
    
	if(flag)        // enable dod
	{
		result = RegSetValueEx(hKey, "InternetProfile", NULL, REG_SZ, (LPBYTE)lpProfileName, strlen(lpProfileName));
		result = RegSetValueEx(hKey, "Default", NULL, REG_SZ, (LPBYTE)lpProfileName, strlen(lpProfileName));
	}
	else            // disable dod
	{
		result = RegSetValueEx(hKey, "InternetProfile", NULL, REG_SZ, NULL, strlen(lpProfileName));
		result = RegSetValueEx(hKey, "Default", NULL, REG_SZ, NULL, strlen(lpProfileName));
	}
	RegCloseKey(hKey);
}
    
    
    
//********************************************************************************
//
// lineCallbackFuncNT  (winNT40)
//
// Sets the RAS structure for Dial on Demand, NT40 doesn't use registry like win95
//********************************************************************************
    
void FAR PASCAL
lineCallbackFuncNT(DWORD /* hDevice */,
		     DWORD /* dwMsg */,
		     DWORD /* dwCallbackInstance */,
		     DWORD /* dwParam1 */,
		     DWORD /* dwParam2 */,
		     DWORD /* dwParam3 */)
{
}
    
    
void EnableDialOnDemandNT(LPSTR lpProfileName, BOOL flag)
{
	RASAUTODIALENTRY        rasAutodialEntry;
	DWORD                   dwBytes = 0;
	DWORD                   dwNumDevs;
	HLINEAPP                m_LineApp;
	DWORD                   dwApiVersion;
	LINEINITIALIZEEXPARAMS  m_LineInitExParams;
	LINETRANSLATECAPS       m_LineTranslateCaps;
	int                     rtn;
    
	// Initialize TAPI. We need to do this in order to get the dialable
	// number and to bring up the location dialog
    
	dwApiVersion = 0x00020000;
	m_LineInitExParams.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;
	m_LineInitExParams.dwTotalSize = sizeof(LINEINITIALIZEEXPARAMS);
	m_LineInitExParams.dwNeededSize = sizeof(LINEINITIALIZEEXPARAMS);
    
	rtn = lineInitializeEx(&m_LineApp, DLLinstance, lineCallbackFuncNT, 
				     NULL, &dwNumDevs, &dwApiVersion, &m_LineInitExParams);
	if ( rtn == 0)
	{
		m_LineTranslateCaps.dwTotalSize = sizeof(LINETRANSLATECAPS);
	    m_LineTranslateCaps.dwNeededSize = sizeof(LINETRANSLATECAPS);
	    rtn = lineGetTranslateCaps(m_LineApp, dwApiVersion, &m_LineTranslateCaps);
	}               
    
	rasAutodialEntry.dwFlags = 0;
	rasAutodialEntry.dwDialingLocation = m_LineTranslateCaps.dwCurrentLocationID;
	strcpy(rasAutodialEntry.szEntry, lpProfileName);                //entry 
	rasAutodialEntry.dwSize = sizeof(RASAUTODIALENTRY);
    
  
	// set auto dial params
	int     val = flag;
	rtn = (*m_lpfnRasSetAutodialParam)( RASADP_DisableConnectionQuery, &val, sizeof(int));
     
	if(flag)        // set dod entry if the flag is enabled
		rtn = (*m_lpfnRasSetAutodialAddress)("www.netscape.com", 0, &rasAutodialEntry, 
			     sizeof(RASAUTODIALENTRY), 1);                          
	rtn = (*m_lpfnRasSetAutodialEnable)(rasAutodialEntry.dwDialingLocation, flag);
	return;
}
    
    
//********************************************************************************
// utility function
//
// SameStrings
//
// Checks for string equality between a STRRET and a LPCTSTR
//********************************************************************************
BOOL SameStrings(LPITEMIDLIST pidl, STRRET& lpStr1, LPCTSTR lpStr2)
{
    
	char    buf[MAX_PATH];
	char    *mystr;
	int             cch; 
    
	switch (lpStr1.uType) {
		case STRRET_WSTR:
			cch = WideCharToMultiByte(CP_ACP, WC_SEPCHARS | WC_COMPOSITECHECK,
					lpStr1.pOleStr, -1, buf, MAX_PATH, NULL, NULL);
    
			cch = GetLastError();
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
// SameStrings
//
// Checks for string equality between a STRRET and a LPCTSTR
//********************************************************************************
BOOL ProcStrings(LPITEMIDLIST pidl, STRRET& lpStr1, char* lpStr2)
{
	char    *mystr;
	int             cch; 
    
	switch (lpStr1.uType) {
		case STRRET_WSTR:
			cch = WideCharToMultiByte(CP_ACP, WC_SEPCHARS | WC_COMPOSITECHECK,
					lpStr1.pOleStr, -1, lpStr2, MAX_PATH, NULL, NULL);
			return TRUE;
						
		case STRRET_OFFSET:
			mystr=((char *)pidl) + lpStr1.uOffset;
			strcpy(lpStr2, ((char *)pidl) + lpStr1.uOffset);
			return TRUE;
    
		case STRRET_CSTR:
			mystr=lpStr1.cStr;
			strcpy(lpStr2, lpStr1.cStr);
			return TRUE;
	}
    
	return FALSE;
}
    
    
//********************************************************************************
// utility function
//
// GetSize
//
//********************************************************************************
LPITEMIDLIST Next(LPCITEMIDLIST pidl)
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
UINT GetSize(LPCITEMIDLIST pidl)
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
LPITEMIDLIST Create(UINT cbSize)
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
LPITEMIDLIST ConcatPidls(LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2)
{
	UINT         cb1 = GetSize(pidl1) - sizeof(pidl1->mkid.cb);
	UINT         cb2 = GetSize(pidl2);
	LPITEMIDLIST pidlNew = Create(cb1 + cb2);
    
	if (pidlNew) {
		memcpy(pidlNew, pidl1, cb1);
		memcpy(((LPSTR)pidlNew) + cb1, pidl2, cb2);
	}
    
	return pidlNew;
}
    
    
//********************************************************************************
//
// GetMyComputerFolder
//
// This routine returns the ISHellFolder for the virtual My Computer folder,
// and also returns the PIDL.
//********************************************************************************
HRESULT GetMyComputerFolder(LPSHELLFOLDER *ppshf, LPITEMIDLIST *ppidl)
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
HRESULT
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
		//CString strText;
		//strText.LoadString(IDS_DIAL_UP_NW);
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
		hres = pmcf->EnumObjects( NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList);
		if (SUCCEEDED(hres)) {
			LPITEMIDLIST    pidl;
    
			int flag = 1;
			STRRET  name;
			while ((NOERROR == (hres = pEnumIDList->Next(1, &pidl, NULL))) && (flag)) 
			{
			    memset(&name, 0, sizeof(STRRET));
    
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
					int rtn = GetLastError();
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
// GetDialUpConnection
//
//********************************************************************************
BOOL GetDialUpConnection95(CONNECTIONPARAMS **connectionNames, int *numNames)
{
	HRESULT         hres;
    
	IMalloc*        pMalloc = NULL;
	IShellFolder*   pshf = NULL;
	LPITEMIDLIST    pidldun;
	LPITEMIDLIST    pidl;
	STRRET                  name;
	char                    temp[MAX_PATH];
	int                             flag=1;
	int                             i =0;
    
    
	// Initialize out parameter
	hres = SHGetMalloc(&pMalloc);
	if (FAILED(hres))
		return FALSE;
    
	// First get the Dial-Up Networking virtual folder
	hres = GetDialUpNetworkingFolder(&pshf, &pidldun);
	if (SUCCEEDED(hres) && (pshf != NULL)) 
	{
		IEnumIDList*    pEnumIDList;
    
		// Enumerate the files looking for the desired connection
		hres = pshf->EnumObjects(NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList);
    
		if (SUCCEEDED(hres)) 
		{
			*numNames = 0;
			while (NOERROR == (hres = pEnumIDList->Next(1, &pidl, NULL)))
				(*numNames) ++;
    
			pEnumIDList->Reset();
    
			*connectionNames = new CONNECTIONPARAMS [*numNames];
			if(*connectionNames == NULL)
				return FALSE;
    
			while ((NOERROR == (hres = pEnumIDList->Next(1, &pidl, NULL))) && (flag)) 
			{
				name.uType = STRRET_CSTR;  // preferred choice
				hres = pshf->GetDisplayNameOf(pidl, SHGDN_INFOLDER, &name);
	
				if (FAILED(hres)) 
				{
					pMalloc->Free(pidl);
					flag = 0;
					//break;
				}
    
				ProcStrings(pidl, name, temp);
				if(strcmp(temp, "Make New Connection") !=0)
				{
					strcpy((*connectionNames)[i].szEntryName, temp);
					(*connectionNames)[i].pidl = ConcatPidls(pidldun, pidl);
					i++;
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
	*numNames = i;
    
	return TRUE;
}
//********************************************************************************
//
// GetDialUpConnection
//
//********************************************************************************
BOOL GetDialUpConnectionNT(CONNECTIONPARAMS **connectionNames, int *numNames)
{
	RASENTRYNAME    *rasEntryName;
	DWORD           cb;
	DWORD           cEntries;
	int             i;
	char			*szPhoneBook;

	szPhoneBook = NULL;
	rasEntryName = new RASENTRYNAME[MAX_ENTRIES]; 
	
	if(rasEntryName == NULL)
		return FALSE;
		
    memset(rasEntryName, 0, MAX_ENTRIES);
    rasEntryName[0].dwSize = sizeof(RASENTRYNAME);
    cb = sizeof(RASENTRYNAME) * MAX_ENTRIES;
     
    int rtn = (*m_lpfnRasEnumEntries)(NULL, szPhoneBook, rasEntryName, &cb, &cEntries);
	if( rtn !=0 )
		return FALSE;
    
	*numNames = cEntries;
    
	*connectionNames = new CONNECTIONPARAMS [*numNames + 1]; 
  
	for(i=0; i<*numNames; i++)
    {
		strcpy((*connectionNames)[i].szEntryName, rasEntryName[i].szEntryName);
    }   
    
    delete []rasEntryName;
    return TRUE;
}

#else
//********************************************************************************
//
// EnableDialOnDemand16
//
//********************************************************************************
#define SHIVA_CONNFILE_EXT			".sr"				// Shiva connection file extension
#define SHIVA_ALL_CONNFILES			"*.sr"				// all Shiva connection files
#define SHIVA_INI_DIALER_SECTION	"ConnectW Config"	// Shiva INI section name
#define SHIVA_INI_FILENAME_KEY		"preferred file"	// Shiva INI section name

void EnableDialOnDemand16(char *AccountName, BOOL flag)
{   
	char szIniPath[MAX_PATH],	// path to Shiva's install path with Shiva INI files
		 szIniFName[20],		// Shiva's INI file name
		 szFilePath[MAX_PATH],	// path to Shiva's INI file
		 szTemplate[MAX_PATH],	// template for searching sr files
		 szEntryFName[MAX_PATH];	// entry file name
	
	GetProfileString(SHIVA_INI_DIALER_SECTION, SHIVA_INI_FILENAME_KEY, "", szIniFName, sizeof(szIniFName)); 
	GetProfileString(SHIVA_INI_DIALER_SECTION, szIniFName, "", szIniPath, sizeof(szIniPath)); 
  
	// construct sremote.ini path and isp.sr path names
	strcat(szIniPath, "\\"); 				// (c:\netscape\program\)
	strcpy(szFilePath, szIniPath);			// (c:\netscape\program\)
	strcpy(szTemplate, szFilePath);
	strcat(szTemplate, SHIVA_ALL_CONNFILES);// (c:\netscape\program\*.sr)
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

 			// match? shiva 4.0 or above checks the description in .sr file
 			if ((!strcmpi(szCurrEntryName, AccountName)) || (strstr(szCurrEntryName, AccountName)))
 				bEntryFound = TRUE;
			// shiva 3.0 or below checks the .sr file name
			else if(strstr(c_file.name, AccountName))
				bEntryFound = TRUE;
 		} while (!bEntryFound && (_dos_findnext(&c_file) == 0));
    }
    
	if(flag)
	{
		WritePrivateProfileString("DialOnDemand", "Enable", "Yes", szIniPath);   
		WritePrivateProfileString("DialOnDemand", "DialOnDemand", szEntryFName, szIniPath); 
	}
	else
	{
		WritePrivateProfileString("DialOnDemand", "Enable", "No", szIniPath);   
		WritePrivateProfileString("DialOnDemand", "DialOnDemand", "", szIniPath); 
	}       
 }        
 
//********************************************************************************
//
// GetDialUpConnection16
//
//********************************************************************************
BOOL GetDialUpConnection16(CONNECTIONPARAMS **connectionNames, int *numNames)
{
	char szIniPath[MAX_PATH],	// path to Shiva's install path with Shiva INI files
		 szIniFName[20],		// Shiva's INI file name
		 szFilePath[MAX_PATH],	// path to Shiva's INI file
		 szTemplate[MAX_PATH],	// template for searching sr files
		 szEntryFName[MAX_PATH];	// entry file name
	
	GetProfileString(SHIVA_INI_DIALER_SECTION, SHIVA_INI_FILENAME_KEY, "", szIniFName, sizeof(szIniFName)); 
	GetProfileString(SHIVA_INI_DIALER_SECTION, szIniFName, "", szIniPath, sizeof(szIniPath)); 
  
	// construct sremote.ini path and isp.sr path names
	strcat(szIniPath, "\\"); 				// (c:\netscape\program\)
	strcpy(szFilePath, szIniPath);			// (c:\netscape\program\)
	strcpy(szTemplate, szFilePath);
	strcat(szTemplate, SHIVA_ALL_CONNFILES);// (c:\netscape\program\*.sr)
	strcat(szIniPath, szIniFName);			// (c:\netscape\program\sremote.ini)

	// find connection file name for the entry
    struct _find_t c_file;
    long hFile;
	int nResult, i;

	// count number of .sr files
	i = 0;
    if ((hFile = _dos_findfirst(szTemplate, _A_NORMAL, &c_file)) == 0)
    {
    	do      	// seek thru connection files to match entry name
     	{
    		i ++;	
 		} while (_dos_findnext(&c_file) == 0);
    }
	else
		return FALSE;
    
    *numNames = i;
	*connectionNames = new CONNECTIONPARAMS [*numNames + 1]; 
	char szCurrEntryName[RAS_MaxEntryName];

	// get the file name list  
	int 	j;
	char	temp_fname[MAX_PATH];

	i = 0;          
	if ((hFile = _dos_findfirst(szTemplate, _A_NORMAL, &c_file)) == 0)
    {
    	do      	// seek thru connection files to match entry name
     	{
    		strcpy(szEntryFName, szFilePath);	// (c:\netscape\program\)
    		strcat(szEntryFName, c_file.name);	// (c:\netscape\program\current.sr)
    		
    		// get Shiva entry description (name)
    		nResult = GetPrivateProfileString("Dial-In Configuration", "Description", "", 
    								szCurrEntryName, RAS_MaxEntryName, szEntryFName);
			if(nResult != 0)
				strcpy((*connectionNames)[i].szEntryName, szCurrEntryName);
			else                     
			{
				strcpy(temp_fname, c_file.name);
				for(j=strlen(temp_fname); temp_fname[j]!='.' && j>0; j--) ;
				temp_fname[j] = '\0';
				strcpy((*connectionNames)[i].szEntryName, temp_fname);
			}
			i++;

		} while (_dos_findnext(&c_file) == 0);
    }

    return TRUE;
}
 
#endif
    
//********************************************************************************
//
// IsDialerConnected
//
// checks if the dialer is still connected
//********************************************************************************
BOOL IsDialerConnected()
{
	RASCONN *Info = NULL, *lpTemp = NULL;
	DWORD code, count = 0;
    DWORD dSize = stRASCONN;
    char szMessage[256]="";
    
    // try to get a buffer to receive the connection data
    if(!(Info = (RASCONN *)LocalAlloc(LPTR, dSize)))
		// Err: trouble allocating buffer
	return (FALSE);
   
    // see if there are any open connections
    Info->dwSize = stRASCONN;
    code = (*m_lpfnRasEnumConnections) (Info, &dSize, &count);
  
    if (/*ERROR_BUFFER_TOO_SMALL*/ 0 != code) 
	{
		// free the old buffer
	LocalFree(Info);
    
	// allocate a new bigger one
	Info = (RASCONN *)LocalAlloc(LPTR, dSize);
	if(!Info)
	// Err: trouble allocating buffer
	   return (FALSE);
    
	// try to enumerate again
	Info->dwSize = dSize;
	if((*m_lpfnRasEnumConnections)(Info, &dSize, &count) != 0) {
	    LocalFree(Info);
			// can't enumerate connections, assume none is active
	    return (FALSE);
	}
    }
    
    // check for no connections
    if (0 == count) {
	LocalFree(Info);
	return (FALSE);
    }
    
    LocalFree(Info);
    
    return (TRUE);
    
}
