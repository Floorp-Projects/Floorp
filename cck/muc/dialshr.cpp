/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
//#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <time.h>   
#include <dos.h>
#include <winbase.h>
#include <raserror.h>
#include <shlobj.h>
#include <regstr.h>
#include <tapi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <winsock.h>

#include "dialshr.h"
#include "resource.h"
#include "xp_mem.h"
#include "prefapi.h"
    
#define trace

#define MAX_ENTRIES     20

// Navigator quit message (see QuitNavigator)
#define ID_APP_SUPER_EXIT 34593

// The number of times we try to dial
#define NUM_ATTEMPTS    3
#define IDDISCONNECTED  31

enum CallState
{
	StateIdle,
	StateConnecting,
	StateConnected,
	StateDisconnecting
};


CallState					gCallState = StateIdle;

UINT						gDialAttempts = 0;		// keeps the total number of dialing count
HRASCONN					gRasConn;				// handle to the current ras connection
RASCONNSTATE				gRASstate;				// current connection's ras state
RASDIALPARAMS				gDialParams;				// keeps the current connection info
HWND						gHwndStatus = NULL;		// handle to our connection status window
BOOL						gCancelled = FALSE;		// assume connection will be there unless user cancels
BOOL						gLineDrop = FALSE;
BOOL						gDeviceErr = FALSE; 		// assume no hardware err
HWND						gHwndNavigator = NULL;
HANDLE						gRasMon = NULL;		// process handle to RasMon on WinNT

HINSTANCE					m_hRasInst = NULL;

// NT/95 entrypoints
RASDIAL						m_lpfnRasDial;
RASHANGUP					m_lpfnRasHangUp;
RASGETERRORSTRING			m_lpfnRasGetErrorString;
RASSETENTRYPROPERTIES		m_lpfnRasSetEntryProperties;
RASSETENTRYDIALPARAMS		m_lpfnRasSetEntryDialParams;
RASGETCOUNTRYINFO			m_lpfnRasGetCountryInfo;
RASENUMCONNECTIONS			m_lpfnRasEnumConnections;
RASENUMENTRIES				m_lpfnRasEnumEntries;
RASENUMDEVICES				m_lpfnRasEnumDevices;
RASGETENTRYPROPERTIES		m_lpfnRasGetEntryProperties;
RASVALIDATEENTRYNAME		m_lpfnRasValidateEntryName;
RASDELETEENTRY				m_lpfnRasDeleteEntry;

// NT entrypoints
RASSETAUTODIALENABLE		m_lpfnRasSetAutodialEnable;
RASSETAUTODIALADDRESS		m_lpfnRasSetAutodialAddress;
RASGETAUTODIALADDRESS		m_lpfnRasGetAutodialAddress;
RASSETAUTODIALPARAM			m_lpfnRasSetAutodialParam;
RASENUMAUTODIALADDRESSES	m_lpfnRasEnumAutodialAddresses;
RASSETCREDENTIALS			m_lpfnRasSetCredentials;

size_t						stRASENTRY;
size_t						stRASCONN;
size_t						stRASCTRYINFO;
size_t						stRASDIALPARAMS;
size_t						stRASDEVINFO;
size_t						stRASENTRYNAME;

//********************************************************************************
// LoadRasFunctions()
//********************************************************************************
BOOL LoadRasFunctions( LPCSTR lpszLibrary )
{
	//    ASSERT(!m_hRasInst);
	m_hRasInst = ::LoadLibrary( lpszLibrary );
	if ( (UINT)m_hRasInst > 32 ) 
	{
		m_lpfnRasSetEntryProperties = (RASSETENTRYPROPERTIES)::GetProcAddress( m_hRasInst, "RasSetEntryProperties" );
		m_lpfnRasGetCountryInfo = (RASGETCOUNTRYINFO)::GetProcAddress( m_hRasInst, "RasGetCountryInfo" );
		m_lpfnRasEnumDevices = (RASENUMDEVICES)::GetProcAddress( m_hRasInst, "RasEnumDevices" );
		m_lpfnRasGetEntryProperties = (RASGETENTRYPROPERTIES)::GetProcAddress( m_hRasInst, "RasGetEntryProperties");
		m_lpfnRasValidateEntryName = (RASVALIDATEENTRYNAME)::GetProcAddress( m_hRasInst,"RasValidateEntryName" );
		m_lpfnRasDeleteEntry = (RASDELETEENTRY)::GetProcAddress( m_hRasInst, "RasDeleteEntry" );
		m_lpfnRasHangUp = (RASHANGUP)::GetProcAddress( m_hRasInst, "RasHangUpA" );
		m_lpfnRasDial = (RASDIAL)::GetProcAddress( m_hRasInst, "RasDialA" );
		m_lpfnRasEnumConnections = (RASENUMCONNECTIONS)::GetProcAddress( m_hRasInst, "RasEnumConnectionsA" );
		m_lpfnRasSetEntryDialParams = (RASSETENTRYDIALPARAMS)::GetProcAddress( m_hRasInst, "RasSetEntryDialParamsA" );
		m_lpfnRasEnumEntries = (RASENUMENTRIES)::GetProcAddress( m_hRasInst, "RasEnumEntriesA" );
		
		SizeofRAS95();		
		return TRUE;
	} 
	else 
	{
		MessageBox( NULL, "Please install Dial-up Networking", "Netscape", MB_ICONSTOP );     
	
		::FreeLibrary( m_hRasInst );
		m_hRasInst = NULL;
		return FALSE; 
	}
}

//********************************************************************************
// LoadRasFunctionsNT()
//********************************************************************************
BOOL LoadRasFunctionsNT( LPCSTR lpszLibrary )
{
	m_hRasInst = ::LoadLibrary( lpszLibrary );
	
    if ( (UINT)m_hRasInst > 32 ) 
	{
		m_lpfnRasSetEntryProperties = (RASSETENTRYPROPERTIES)::GetProcAddress( m_hRasInst, "RasSetEntryPropertiesA" );		
		m_lpfnRasGetCountryInfo = (RASGETCOUNTRYINFO)::GetProcAddress( m_hRasInst, "RasGetCountryInfoA" );
		m_lpfnRasEnumDevices = (RASENUMDEVICES)::GetProcAddress( m_hRasInst, "RasEnumDevicesA" );
		m_lpfnRasGetEntryProperties = (RASGETENTRYPROPERTIES)::GetProcAddress( m_hRasInst, "RasGetEntryPropertiesA" );
		m_lpfnRasValidateEntryName = (RASVALIDATEENTRYNAME)::GetProcAddress( m_hRasInst, "RasValidateEntryNameA" );
		m_lpfnRasDeleteEntry = (RASDELETEENTRY)::GetProcAddress( m_hRasInst, "RasDeleteEntryA" );
		m_lpfnRasEnumEntries = (RASENUMENTRIES)::GetProcAddress( m_hRasInst, "RasEnumEntriesA" );
		m_lpfnRasHangUp = (RASHANGUP)::GetProcAddress( m_hRasInst, "RasHangUpA" );
		m_lpfnRasDial = (RASDIAL)::GetProcAddress( m_hRasInst, "RasDialA" );
		m_lpfnRasEnumConnections = (RASENUMCONNECTIONS)::GetProcAddress( m_hRasInst, "RasEnumConnectionsA" );
		m_lpfnRasSetEntryDialParams = (RASSETENTRYDIALPARAMS)::GetProcAddress(m_hRasInst, "RasSetEntryDialParamsA" );
		
		// AUTODIAL
		m_lpfnRasSetAutodialEnable = (RASSETAUTODIALENABLE)::GetProcAddress( m_hRasInst, "RasSetAutodialEnableA" );
		m_lpfnRasSetAutodialAddress = (RASSETAUTODIALADDRESS)::GetProcAddress( m_hRasInst, "RasSetAutodialAddressA" );
		m_lpfnRasGetAutodialAddress = (RASGETAUTODIALADDRESS)::GetProcAddress( m_hRasInst, "RasGetAutodialAddressA" );
		m_lpfnRasSetAutodialParam = (RASSETAUTODIALPARAM)::GetProcAddress( m_hRasInst, "RasSetAutodialParamA" );
		m_lpfnRasSetCredentials = (RASSETCREDENTIALS)::GetProcAddress( m_hRasInst, "RasSetCredentialsA" );
		m_lpfnRasEnumAutodialAddresses = (RASENUMAUTODIALADDRESSES)::GetProcAddress( m_hRasInst, "RasEnumAutodialAddressesA" ); 
		
		SizeofRASNT40();
		return TRUE;       
	}                
   	else 
	{
		MessageBox( NULL, "Please install Dial-up Networking", "Netscape", MB_ICONSTOP );
		::FreeLibrary( m_hRasInst );
		m_hRasInst = NULL;
	    return FALSE;       
	}
}
    
//********************************************************************************
// FreeRasFunctions()
//********************************************************************************
void FreeRasFunctions()
{
	if ( (UINT)m_hRasInst > 32 )
		FreeLibrary( m_hRasInst );
}



//********************************************************************************
// GetModemList (s/b GetModems)
// Returns list of modems available for use ('installed' by the user). For Win95
// this list come from the OS, and each entry contains 2 strings - the first is
// the Modem Name, and the second is the device type (both are needed to select
// the device to use to make a Dial-up connection).
//********************************************************************************
BOOL GetModemList( char*** resultModemList, int* numDevices )
{
	DWORD           dwBytes = 0, dwDevices;
    LPRASDEVINFO    lpRnaDevInfo;
    
    // First find out how much memory to allocate
    (*m_lpfnRasEnumDevices)( NULL, &dwBytes, &dwDevices );
    lpRnaDevInfo = (LPRASDEVINFO)malloc( dwBytes );
    
	if ( !lpRnaDevInfo )
		return (FALSE);
    
    lpRnaDevInfo->dwSize = stRASDEVINFO;
	(*m_lpfnRasEnumDevices)( lpRnaDevInfo, &dwBytes, &dwDevices );
    
    // copy all entries to the char array
    *resultModemList = new char* [dwDevices+1];
    if ( *resultModemList == NULL )
		return FALSE;
    
    *numDevices = dwDevices;
    for ( unsigned short i=0; i < dwDevices; i++ ) 
    {
		(*resultModemList)[i] = new char[ strlen( lpRnaDevInfo[ i ].szDeviceName) + 1 ];
		if ( ( *resultModemList)[ i ] == NULL )
			return FALSE;
		strcpy( ( *resultModemList)[ i ], lpRnaDevInfo[ i ].szDeviceName );
    }
    return TRUE;
}
    
    
//********************************************************************************
// GetModemType
// Returns the type for the selected modem.
//********************************************************************************
BOOL GetModemType( char *strModemName, char *strModemType)
{
    DWORD           dwBytes = 0, dwDevices;
    LPRASDEVINFO    lpRnaDevInfo;
      
	// First get Modem (okay - Device) info from Win95
	// find out how much memory to allocate
	(*m_lpfnRasEnumDevices)( NULL, &dwBytes, &dwDevices );
	lpRnaDevInfo = (LPRASDEVINFO)malloc( dwBytes );
	
	if ( !lpRnaDevInfo )
		return NULL;
	
	lpRnaDevInfo->dwSize = stRASDEVINFO;
	(*m_lpfnRasEnumDevices)( lpRnaDevInfo, &dwBytes, &dwDevices );
	
	// If match the modem given from JS then return the associated Type
	for ( unsigned short i = 0; i < dwDevices; i++ ) 
	{
		if ( 0 == strcmp(strModemName, lpRnaDevInfo[i].szDeviceName ) ) 
		{
			strModemType = new char[ strlen( lpRnaDevInfo[ i ].szDeviceType ) + 1 ];
			strcpy( strModemType, lpRnaDevInfo[ i ].szDeviceType );
			return TRUE;
		}                
	}
	return FALSE;
}
    
//********************************************************************************
// EnableDialOnDemand  (win95)
// Set the magic keys in the registry to enable dial on demand
//********************************************************************************
void EnableDialOnDemand95( LPSTR lpProfileName, BOOL flag )
{
	HKEY    hKey;
	DWORD   dwDisposition;
	long    result;
	char*	szData;
	
	// We need to tell windows about dialing on demand
	result = RegCreateKeyEx( HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\Winsock\\Autodial",
							NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition );
	
	// err, oops
	if ( result != ERROR_SUCCESS )
		return;
	
	szData = "url.dll";
	result = RegSetValueEx( hKey, "AutodialDllName32", NULL, REG_SZ, (LPBYTE)szData, strlen( szData ) );
	
	szData = "AutodialHookCallback";
	result = RegSetValueEx( hKey, "AutodialFcnName32", NULL, REG_SZ, (LPBYTE)szData, strlen( szData ) );
	
	RegCloseKey( hKey );
	
	// set the autodial flag first
	result = RegCreateKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
							NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition );
	
	// err, oops
	if ( result != ERROR_SUCCESS )
		return;
	
	// set the autodial and idle-time disconnect
	DWORD		dwValue = flag;
	result = RegSetValueEx( hKey, "EnableAutodial", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof( DWORD ) );
	
	// try setting autodisconnect here
	dwValue = 1;
	result = RegSetValueEx( hKey, "EnableAutoDisconnect", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof( DWORD ) );
	
	// default auto-disconnect after 5 minutes
	dwValue = 5;
	result = RegSetValueEx( hKey, "DisconnectIdleTime", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof( DWORD ) );
	
	RegCloseKey( hKey );
	
	// set the autodisconnect flags here too
	result = RegCreateKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\Internet Settings",
						NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition );
	
	// err, oops
	if ( result != ERROR_SUCCESS )
		return;
	
	dwValue = 1;
	result = RegSetValueEx( hKey, "EnableAutoDisconnect", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof( DWORD ) );
	
	// default auto-disconnect after 5 minutes
	dwValue = 5;
	result = RegSetValueEx( hKey, "DisconnectIdleTime", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof( DWORD ) );
	
	RegCloseKey( hKey );
	
	// OK, let's tell it which profile to autodial
	result = RegCreateKeyEx( HKEY_CURRENT_USER, "RemoteAccess", NULL, NULL, NULL,
						KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition );
	
	// err, oops
	if ( result != ERROR_SUCCESS )
		return;
	
	if ( flag )	// enable dod
	{
		result = RegSetValueEx( hKey, "InternetProfile", NULL, REG_SZ, (LPBYTE)lpProfileName, strlen( lpProfileName ) );
		result = RegSetValueEx( hKey, "Default", NULL, REG_SZ, (LPBYTE)lpProfileName, strlen( lpProfileName ) );
	}
	else			// disable dod
	{
		result = RegSetValueEx( hKey, "InternetProfile", NULL, REG_SZ, NULL, strlen( lpProfileName ) );
		result = RegSetValueEx( hKey, "Default", NULL, REG_SZ, NULL, strlen( lpProfileName ) );
	}
	RegCloseKey( hKey );
}

    
//********************************************************************************
//
// lineCallbackFuncNT  (winNT40)
//
// Sets the RAS structure for Dial on Demand, NT40 doesn't use registry like win95
//********************************************************************************
void FAR PASCAL
lineCallbackFuncNT(DWORD /* hDevice */, DWORD /* dwMsg */, DWORD /* dwCallbackInstance */,
	DWORD /* dwParam1 */, DWORD /* dwParam2 */, DWORD /* dwParam3 */)
{
}
    
void EnableDialOnDemandNT(LPSTR lpProfileName, BOOL flag)
{
	RASAUTODIALENTRY        rasAutodialEntry;
	DWORD                   dwBytes = 0;
	DWORD                   dwNumDevs;
	HLINEAPP                lineApp;
	DWORD                   dwApiVersion;
	LINEINITIALIZEEXPARAMS  lineInitExParams;
	LINETRANSLATECAPS       lineTranslateCaps;
	int                     rtn;
	
	// Initialize TAPI. We need to do this in order to get the dialable
	// number and to bring up the location dialog
	
	dwApiVersion = 0x00020000;
	lineInitExParams.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;
	lineInitExParams.dwTotalSize = sizeof( LINEINITIALIZEEXPARAMS );
	lineInitExParams.dwNeededSize = sizeof (LINEINITIALIZEEXPARAMS );
	
	rtn = lineInitializeEx( &lineApp, gDLL, lineCallbackFuncNT, 
		     NULL, &dwNumDevs, &dwApiVersion, &lineInitExParams );

	if ( rtn == 0)
	{
		lineTranslateCaps.dwTotalSize = sizeof( LINETRANSLATECAPS );
		lineTranslateCaps.dwNeededSize = sizeof( LINETRANSLATECAPS );
		rtn = lineGetTranslateCaps( lineApp, dwApiVersion, &lineTranslateCaps );
	}               
	
	rasAutodialEntry.dwFlags = 0;
	rasAutodialEntry.dwDialingLocation = lineTranslateCaps.dwCurrentLocationID;
	strcpy( rasAutodialEntry.szEntry, lpProfileName ); 
	rasAutodialEntry.dwSize = sizeof( RASAUTODIALENTRY );
	
	// set auto dial params
	int     val = flag;
	rtn = ( *m_lpfnRasSetAutodialParam )( RASADP_DisableConnectionQuery, &val, sizeof( int ) );

	if ( rtn == ERROR_INVALID_PARAMETER )
	{
		trace( "dialer.cpp : EnableDialOnDemandNT - Invalid Parameter. Can't set Autodial Parameters. (r)" );
		return;
	}	 
	else if ( rtn == ERROR_INVALID_SIZE )
	{
		trace( "dialer.cpp : EnableDialOnDemandNT - Invalid size. Can't set Autodial Parameters. (r)" );
		return;
	}	 
	else if ( rtn )
	{
		trace( "dialer.cpp : EnableDialOnDemandNT - Can't set Autodial Parameters. Error %d. (r)", rtn );
		return;
	}	 

	
	if ( flag )        // set dod entry if the flag is enabled
	{
		rtn = ( *m_lpfnRasSetAutodialAddress )( "www.netscape.com", 0, &rasAutodialEntry, sizeof(RASAUTODIALENTRY), 1);

		if ( rtn == ERROR_INVALID_PARAMETER )
		{
			trace( "dialer.cpp : EnableDialOnDemandNT - Invalid Parameter. Can't set Autodial Address. (r)" );
			return;
		}
		else if ( rtn == ERROR_INVALID_SIZE )
		{
			trace( "dialer.cpp : EnableDialOnDemandNT - Invalid size. Can't set Autodial Address. (r)" );
			return;
		}
		else if ( rtn )
		{
			trace( "dialer.cpp : EnableDialOnDemandNT - Can't set Autodial Address. Error %d. (r)", rtn );
			return;
		}
	}	 

	rtn = ( *m_lpfnRasSetAutodialEnable )( rasAutodialEntry.dwDialingLocation, flag );

	if ( rtn )
	{
		trace( "dialer.cpp : EnableDialOnDemandNT - Can't set Autodial Enable. Error %d. (r)", rtn );
		return;
	}	 

	return;
}


//********************************************************************************
// utility function
// sameStrings
// checks for string equality between a STRRET and a LPCTSTR
//********************************************************************************
static BOOL sameStrings( LPITEMIDLIST pidl, STRRET& lpStr1, LPCTSTR lpStr2 )
{
	char    buf[ MAX_PATH ];
	char*	mystr;
	int		cch; 
    
	switch ( lpStr1.uType )
	{
		case STRRET_WSTR:
			cch = WideCharToMultiByte( CP_ACP, WC_SEPCHARS | WC_COMPOSITECHECK,
					lpStr1.pOleStr, -1, buf, MAX_PATH, NULL, NULL );
    
			cch = GetLastError();
			return strcmp( lpStr2, buf ) == 0;
    
		case STRRET_OFFSET:
			mystr = ((char*)pidl) + lpStr1.uOffset;
			return strcmp( lpStr2, ((char*)pidl) + lpStr1.uOffset ) == 0;
    
		case STRRET_CSTR:
			mystr = lpStr1.cStr;
			return strcmp( lpStr2, lpStr1.cStr ) == 0;
	}
    
	return FALSE;
}
    
//********************************************************************************
// utility function
// procStrings
//********************************************************************************
static BOOL procStrings( LPITEMIDLIST pidl, STRRET& lpStr1, char* lpStr2 )
{
	char*		mystr;
	int			cch; 
    
	switch ( lpStr1.uType )
	{
		case STRRET_WSTR:
			cch = WideCharToMultiByte( CP_ACP, WC_SEPCHARS | WC_COMPOSITECHECK,
					lpStr1.pOleStr, -1, lpStr2, MAX_PATH, NULL, NULL );
			return TRUE;
						
		case STRRET_OFFSET:
			mystr = ((char*)pidl) + lpStr1.uOffset;
			strcpy( lpStr2, ((char*)pidl) + lpStr1.uOffset );
			return TRUE;
    
		case STRRET_CSTR:
			mystr = lpStr1.cStr;
			strcpy( lpStr2, lpStr1.cStr );
			return TRUE;
	}
    
	return FALSE;
}
    
    
//********************************************************************************
// utility function
// next
//********************************************************************************
static LPITEMIDLIST next( LPCITEMIDLIST pidl )
{
	LPSTR lpMem=(LPSTR)pidl;
	
	lpMem += pidl->mkid.cb;
	return (LPITEMIDLIST)lpMem;
}
    
    
//********************************************************************************
// utility function
// getSize
//********************************************************************************
static UINT getSize(LPCITEMIDLIST pidl)
{
	UINT cbTotal = 0;
    
	if ( pidl )
	{
		cbTotal += sizeof( pidl->mkid.cb );
		while ( pidl->mkid.cb )
		{
			cbTotal += pidl->mkid.cb;
			pidl = next( pidl );
		}
	}
    
	return cbTotal;
}
    
    
//********************************************************************************
// utility function
// create
//********************************************************************************
static LPITEMIDLIST create( UINT cbSize )
{
	IMalloc*     pMalloc;
	LPITEMIDLIST pidl = 0;
    
	if ( FAILED( SHGetMalloc( &pMalloc ) ) )
		return 0;
    
	pidl = (LPITEMIDLIST)pMalloc->Alloc( cbSize );
    
	if ( pidl )
		memset( pidl, 0, cbSize );
     
	pMalloc->Release();
	
	return pidl;
}
    
    
    
//********************************************************************************
// utility function
// ConcatPidls
//********************************************************************************
static LPITEMIDLIST concatPidls( LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2 )
{
	UINT         cb1 = getSize( pidl1 ) - sizeof( pidl1->mkid.cb );
	UINT         cb2 = getSize( pidl2 );
	LPITEMIDLIST pidlNew = create( cb1 + cb2 );
    
	if ( pidlNew )
	{
		memcpy( pidlNew, pidl1, cb1 );
		memcpy( ((LPSTR)pidlNew ) + cb1, pidl2, cb2 );
	}
    
	return pidlNew;
}
    
    
//********************************************************************************
// GetMyComputerFolder
// This routine returns the ISHellFolder for the virtual My Computer folder,
// and also returns the PIDL.
//********************************************************************************
HRESULT GetMyComputerFolder( LPSHELLFOLDER* ppshf, LPITEMIDLIST* ppidl )
{
	IMalloc*        pMalloc;
	HRESULT         hres;
    
	hres = SHGetMalloc( &pMalloc );
	if ( FAILED( hres ) )
		return hres;
    
	// Get the PIDL for "My Computer"
	hres = SHGetSpecialFolderLocation( /*pWndOwner->m_hWnd*/NULL, CSIDL_DRIVES, ppidl );
	if ( SUCCEEDED( hres ) )
	{
		IShellFolder*   pshf;
    
		hres = SHGetDesktopFolder( &pshf );
		if ( SUCCEEDED( hres ) )
		{
			// Get the shell folder for "My Computer"
			hres = pshf->BindToObject( *ppidl, NULL, IID_IShellFolder, (LPVOID *)ppshf );
			pshf->Release();
		}
	}
    
	pMalloc->Release();
    
	return hres;
}
    
    
//********************************************************************************
// GetDialupNetworkingFolder
// This routine returns the ISHellFolder for the virtual Dial-up Networking
// folder, and also returns the PIDL.
//********************************************************************************
static HRESULT getDialUpNetworkingFolder(LPSHELLFOLDER *ppshf, LPITEMIDLIST *ppidl)
{
	HRESULT         hres;
    
	IMalloc*        pMalloc = NULL;
	IShellFolder*   pmcf = NULL;
	LPITEMIDLIST    pidlmc;
    
    
	char szDialupName[ 256 ];
	HKEY hKey;
	DWORD cbData;
    
	// Poke around in the registry to find out what the Dial-Up Networking
	//   folder is called on this machine

	szDialupName[ 0 ] = '\0';
	if ( ERROR_SUCCESS == RegOpenKeyEx(HKEY_CLASSES_ROOT, 
				     "CLSID\\{992CFFA0-F557-101A-88EC-00DD010CCC48}", 
				     NULL, 
				     KEY_QUERY_VALUE, 
				     &hKey ) )
	{
    	cbData = sizeof( szDialupName );
		RegQueryValueEx( hKey, "", NULL, NULL, (LPBYTE)szDialupName, &cbData );
	}
    
	// if we didn't get anything just use the default
	if( szDialupName[ 0 ] == '\0' )
	{
		char*	strText;
		strText = "Dial-Up Networking";
		//CString strText;
		//strText.LoadString(IDS_DIAL_UP_NW);
		strcpy( szDialupName, (LPCSTR)strText );
	}
    
	RegCloseKey( hKey );
    
	// OK, now look for that folder
    
	hres = SHGetMalloc( &pMalloc );
	if ( FAILED( hres ) )
		return hres;
    
	// Get the virtual folder for My Computer
	hres = GetMyComputerFolder( &pmcf, &pidlmc );
	if ( SUCCEEDED( hres ) )
	{
		IEnumIDList*    pEnumIDList;
    
		// Now we need to find the "Dial-Up Networking" folder
		hres = pmcf->EnumObjects( NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList );
		if ( SUCCEEDED( hres ) )
		{
			LPITEMIDLIST    pidl;
    
			int flag = 1;
			STRRET  name;
			while ( ( NOERROR == ( hres = pEnumIDList->Next( 1, &pidl, NULL ) ) ) && ( flag ) ) 
			{
			    memset( &name, 0, sizeof( STRRET ) );
    
				name.uType = STRRET_CSTR;  // preferred choice
				hres = pmcf->GetDisplayNameOf( pidl, SHGDN_INFOLDER, &name );
				if ( FAILED( hres ) )
				{
					pMalloc->Free( pidl );
					flag = 0;
					//break;
				}
    
				if ( sameStrings( pidl, name, szDialupName ) )
				{
					*ppidl = concatPidls(pidlmc, pidl);
					hres = pmcf->BindToObject( pidl, NULL, IID_IShellFolder, (LPVOID*)ppshf );
					int rtn = GetLastError();
					pMalloc->Free( pidl );
					flag = 0;
					//break;
				}
				
				if ( flag )
					pMalloc->Free( pidl );
			}
    
			pEnumIDList->Release();
		}
    
		pmcf->Release();
		pMalloc->Free( pidlmc );
	}
    
	pMalloc->Release();
	return hres;
}

//********************************************************************************
// GetDialUpConnection
//********************************************************************************
BOOL GetDialUpConnection95( CONNECTIONPARAMS** connectionNames, int* numNames )
{
	HRESULT			hres;
    
	IMalloc*		pMalloc = NULL;
	IShellFolder*	pshf = NULL;
	LPITEMIDLIST	pidldun;
	LPITEMIDLIST	pidl;
	STRRET			name;
	char			temp[ MAX_PATH ];
	int				flag = 1;
	int				i =0;
    
    
	// Initialize out parameter
	hres = SHGetMalloc( &pMalloc );
	if ( FAILED( hres ) )
		return FALSE;
    
	// First get the Dial-Up Networking virtual folder
	hres = getDialUpNetworkingFolder( &pshf, &pidldun );
	if ( SUCCEEDED( hres ) && ( pshf != NULL ) ) 
	{
		IEnumIDList*    pEnumIDList;
    
		// Enumerate the files looking for the desired connection
		hres = pshf->EnumObjects( NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList );
    
		if ( SUCCEEDED( hres ) ) 
		{
			*numNames = 0;
			while ( NOERROR == ( hres = pEnumIDList->Next( 1, &pidl, NULL ) ) )
				(*numNames)++;
    
			pEnumIDList->Reset();
    
			*connectionNames = new CONNECTIONPARAMS[ *numNames ];
			if( *connectionNames == NULL )
				return FALSE;
    
			while ( ( NOERROR == ( hres = pEnumIDList->Next( 1, &pidl, NULL ) ) ) && ( flag ) ) 
			{
				name.uType = STRRET_CSTR;  // preferred choice
				hres = pshf->GetDisplayNameOf( pidl, SHGDN_INFOLDER, &name );
	
				if ( FAILED( hres ) ) 
				{
					pMalloc->Free( pidl );
					flag = 0;
					//break;
				}
    
				procStrings( pidl, name, temp );
				if( strcmp( temp, "Make New Connection" ) !=0 )
				{
					strcpy( (*connectionNames)[ i ].szEntryName, temp );
					(*connectionNames)[ i ].pidl = concatPidls( pidldun, pidl );
					i++;
				}

				if ( flag )
					pMalloc->Free( pidl );
			}
    
			pEnumIDList->Release();
		}
    
		pshf->Release();
		pMalloc->Free( pidldun );
	}
    
	pMalloc->Release();
	*numNames = i;
    
	return TRUE;
}

//********************************************************************************
// GetDialUpConnection
//********************************************************************************
BOOL GetDialUpConnectionNT( CONNECTIONPARAMS** connectionNames, int* numNames )
{
	RASENTRYNAME*	rasEntryName;
	DWORD           cb;
	DWORD           cEntries;
	int             i;
	char*			szPhoneBook;
	
	szPhoneBook = NULL;
	rasEntryName = new RASENTRYNAME[ MAX_ENTRIES ]; 
	
	if( rasEntryName == NULL )
		return FALSE;
	
	memset( rasEntryName, 0, MAX_ENTRIES );
	rasEntryName[ 0 ].dwSize = sizeof( RASENTRYNAME );
	cb = sizeof( RASENTRYNAME ) * MAX_ENTRIES;
	
	int rtn = (*m_lpfnRasEnumEntries)( NULL, szPhoneBook, rasEntryName, &cb, &cEntries );
	if ( rtn !=0 )
		return FALSE;
	
	*numNames = cEntries;
	
	*connectionNames = new CONNECTIONPARAMS[ *numNames + 1 ]; 
	
	for( i = 0; i < *numNames; i++ )
		strcpy ( (*connectionNames)[ i ].szEntryName, rasEntryName[ i ].szEntryName );
	
	delete []rasEntryName;
	return TRUE;
}

//********************************************************************************
// getDialupConnectionPIDL
//********************************************************************************
static HRESULT getDialUpConnectionPIDL( LPCTSTR lpConnectionName, LPITEMIDLIST* ppidl )
{
	HRESULT			hres;
	
	IMalloc*		pMalloc = NULL;
	IShellFolder*	pshf = NULL;
	LPITEMIDLIST	pidldun;
	
	// Initialize out parameter
	*ppidl = NULL;
	
	hres = SHGetMalloc( &pMalloc );
	if ( FAILED( hres ) )
		return hres;
	
	// First get the Dial-Up Networking virtual folder
	hres = getDialUpNetworkingFolder( &pshf, &pidldun );
	if ( SUCCEEDED( hres ) && ( pshf != NULL ) )
	{
		IEnumIDList*    pEnumIDList;
	
		// Enumerate the files looking for the desired connection
		hres = pshf->EnumObjects( /*pWndOwner->m_hWnd*/NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList );
		if ( SUCCEEDED( hres ) )
		{
			LPITEMIDLIST    pidl;
	
			int flag = 1;
			while ( ( NOERROR == ( hres = pEnumIDList->Next( 1, &pidl, NULL ) ) ) && flag )
			{
				STRRET  name;
	
				name.uType = STRRET_CSTR;  // preferred choice
				hres = pshf->GetDisplayNameOf( pidl, SHGDN_INFOLDER, &name );
				if ( FAILED( hres ) )
				{
					pMalloc->Free( pidl );
					flag = 0;
					//break;
				}
	
				if ( sameStrings( pidl, name, lpConnectionName ) )
				{
					*ppidl = concatPidls( pidldun, pidl );
					pMalloc->Free( pidl );
					flag = 0;
					//break;
				}
	
				if ( flag )
					pMalloc->Free(pidl);
			}
	
			pEnumIDList->Release();
		}
	
		pshf->Release();
		pMalloc->Free( pidldun );
	}
	
	pMalloc->Release();
	return hres;
}

//********************************************************************************
// getNetscapePIDL
//********************************************************************************
static void getNetscapePidl( LPITEMIDLIST* ppidl )
{
	char				szPath[ MAX_PATH ], *p;
	OLECHAR				olePath[ MAX_PATH ];
	IShellFolder*		pshf;
	
	GetModuleFileName( gDLL, szPath, sizeof( szPath ) );
	//GetModuleFileName(AfxGetInstanceHandle(), szPath, sizeof(szPath));
	
	//we need to take off \plugins\npasw.dll from the path
	p = strrchr( szPath, '\\' );
	if ( p ) 
		*p = '\0';
	p = strrchr( szPath, '\\' );
	if ( p ) 
		*p = '\0';
	strcat( szPath, "\\netscape.exe" );
	
	HRESULT hres = SHGetDesktopFolder( &pshf );
	if ( SUCCEEDED( hres ) )
	{
		MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, (LPCSTR)szPath, -1, (LPWSTR)olePath, sizeof( olePath ) );
	
		ULONG lEaten;
		pshf->ParseDisplayName( NULL, NULL, (LPOLESTR)olePath, &lEaten, ppidl, NULL );
	}
	
	return;
}


//********************************************************************************
// getMsgString
// loads a Message String from the string table
//********************************************************************************
static BOOL getMsgString( char* buf, UINT uID )
{
	if ( !buf )
		return FALSE;

	int ret = LoadString( gDLL, uID, buf, 255 );

	return ret;
}

//********************************************************************************
// getSystemDirectory_1()
// intended to be a more fail-safe version of GetSystemDirectory
// returns:
//	NULL if the path cannot be obtained for any reason
//	otherwise, the "GetSystemDirectory" path
//********************************************************************************
static char* getSystemDirectory_1()
{
	UINT			startLength = MAX_PATH;
	UINT			sysPathLength = MAX_PATH;
	char*			sysPath;
	
start:
	sysPath = (char*)malloc( sizeof(char) * startLength );
	if ( !sysPath ) 
		return NULL;
	sysPathLength = ::GetSystemDirectory( sysPath, startLength );
	if ( sysPathLength == 0 )
		return NULL;
	if ( sysPathLength > startLength )
	{
		free( sysPath );
		startLength = sysPathLength;
		goto start;
	}
	return sysPath;
}

//********************************************************************************
// getPhoneBookNT()
// returns the path to the NT Phone Book file (rasphone.pbk)
// returns:
//	NULL if the path cannot be obtained for any reason
//	otherwise, the phone book file path
//********************************************************************************
static char* getPhoneBookNT()
{
	char*			sysPath;
	char*			pbPath;
	
	sysPath = getSystemDirectory_1();
	if ( !sysPath )
		return FALSE;
		
	pbPath = (char*)malloc( sizeof(char) * strlen( sysPath ) + 30 );
	if ( !pbPath )
	{
		free( sysPath );
		return FALSE;
	}
	
	strcpy( pbPath, sysPath );
	strcat( pbPath, "\\ras\\rasphone.pbk" );
	strcat( pbPath, "\0" );
	free( sysPath );
	return pbPath;	
}

//********************************************************************************
// startRasMonNT()
// starts the "rasmon" process (rasmon.exe)
// returns:
//	FALSE if the process cannot be started for any reason
//	TRUE otherwise
//********************************************************************************
static BOOL startRasMonNT()
{
	// starts up RASMON process
	PROCESS_INFORMATION pi;
	BOOL                ret;
	STARTUPINFO         sti;
	UINT                err = ERROR_SUCCESS;
	char*				sysPath;
	char*				rasMonPath;
	
	sysPath = getSystemDirectory_1();
	if ( !sysPath )
		return FALSE;

	rasMonPath = (char*)malloc( sizeof(char) * strlen( sysPath ) + 30 );
	if ( !rasMonPath )
	{
		free( sysPath );
		return FALSE;
	}
	
	strcpy( rasMonPath, sysPath );
	strcat( rasMonPath, "\\rasmon.exe" );
	strcat( rasMonPath, "\0" );
	free( sysPath );
	
	memset( &sti, 0, sizeof( sti ) );
	sti.cb = sizeof( STARTUPINFO );
	
	// Run the RASMON app
	ret = ::CreateProcess( rasMonPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi );
	if ( ret == TRUE )
	{
		gRasMon = pi.hProcess;
		Sleep( 3000 );
	}
	free( rasMonPath );
	return ret;
}

static void setStatusDialogText( int iField, const char* pText )
{
	if ( gHwndStatus )
	{
		HWND hField = GetDlgItem( gHwndStatus, iField );
		if ( hField )
			SetWindowText( hField, pText );
	}
}

static void removeStatusDialog()
{
	if ( gHwndStatus )
	{
		EndDialog( gHwndStatus, TRUE );
		gHwndStatus = NULL;
	}
}

static int displayErrMsgWnd( char* text, int style, HWND hwnd )
{
	char		title[ 50 ];
	getMsgString( (char*)&title, IDS_APP_NAME );
	
	if ( hwnd == NULL )
		hwnd = GetActiveWindow();
		
	int ret = MessageBox( hwnd, text, title, style );
	return ret;
}

static void displayDialErrorMsg( DWORD dwError )
{
	char    szErr[ 256 ];
	char    szErrStr[ 256 ];
	HWND	wind;
	
	ASSERT( m_lpfnRasGetErrorString );
	(*m_lpfnRasGetErrorString)( (UINT)dwError, szErr, sizeof(szErr) );
	
	// some of the default error strings are pretty lame
	switch ( dwError )
	{
		case ERROR_NO_DIALTONE:
			getMsgString( szErr, IDS_NO_DIALTONE );
		break;
	
		case ERROR_LINE_BUSY:
			getMsgString( szErr, IDS_LINE_BUSY );
		break;

		case ERROR_PROTOCOL_NOT_CONFIGURED:
			getMsgString( szErr, IDS_PROTOCOL_NOT_CONFIGURED );

		default:
		break;
	}
	
	getMsgString( szErrStr, IDS_CONNECTION_FAILED );
	strcat( szErrStr, szErr );
	
	wind = gHwndStatus;
	if ( !wind )
		wind = gHwndNavigator;
	
	displayErrMsgWnd( szErrStr, MB_OK | MB_ICONEXCLAMATION, wind );  
}

static void connectErr( DWORD dwError )
{
	
	char strText[ 255 ];

	if ( gHwndStatus )
	{
        getMsgString( (char*)strText, IDS_DIAL_ERR );
        setStatusDialogText( IDC_DIAL_STATUS, strText );
		Sleep( 1000 );
		removeStatusDialog();
	}

	gDeviceErr = TRUE;		// some sort of device err
	displayDialErrorMsg( dwError );
}

BOOL CALLBACK statusDialogCallback( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	BOOL		bRetval = FALSE;
	DWORD		dwRet;

	if ( !gHwndStatus )
		gHwndStatus = hWnd;

	switch ( uMsg )
	{
		case WM_INITDIALOG:
			return TRUE;
		break;
		
		case WM_COMMAND:
		{
			WORD wNotifyCode = HIWORD( wParam );
            WORD wID = LOWORD( wParam );
            HWND hControl = (HWND)lParam;

            switch ( wID )
            {
				case IDDISCONNECTED:
//                if (AfxMessageBox(IDS_LOST_CONNECTION, MB_YESNO) == IDYES)
//                   m_pMainWnd->PostMessage(WM_COMMAND, IDC_DIAL);
					break;

				case IDCANCEL:
				{
					// RasHangUp & destroy dialog box
					bRetval = TRUE;
					gCancelled = TRUE;

					ASSERT( m_lpfnRasHangUp );

					char strText[ 255 ];

					getMsgString( strText, IDS_CANCELDIAL );
		            setStatusDialogText( IDC_DIAL_STATUS, strText );		
	
					dwRet = ( *m_lpfnRasHangUp )( gRasConn );

					if ( dwRet == ERROR_INVALID_HANDLE)
						trace("dialer.cpp : statusDialogCallback - Can't hangup. Invalid Connection Handle. (r)");
					else if ( dwRet && dwRet != ERROR_INVALID_HANDLE )
						trace("dialer.cpp : statusDialogCallback - Can't hangup. Error %d. (r)", dwRet);

					Sleep( 3000 );
					removeStatusDialog();
					break;
				}
			}
		}

	}
	return bRetval;
}

static void setCallState( CallState newState )
{
	gCallState = newState;
    
	switch ( gCallState )
	{
		case StateConnected:
			// destroy our connection status window
			removeStatusDialog();
		break;

		case StateConnecting:
			// creates status dialog box
			HWND	hwndParent = GetActiveWindow();
			int		nResult;
			
			nResult = DialogBox( gDLL, MAKEINTRESOURCE( IDD_STATUS ), hwndParent, (DLGPROC)statusDialogCallback );
			ASSERT( nResult != -1 );
		break;
	}
}

//********************************************************************************
// RasDialFunc
//  call back function for RasDial
//********************************************************************************
void CALLBACK
RasDialFunc( HRASCONN hRasConn, UINT uMsg,
	RASCONNSTATE rasConnState, DWORD dwError, DWORD dwExtendedError )
{
	if ( uMsg != WM_RASDIALEVENT )
		return;
	                                     // ignore all other messages
	gRASstate = rasConnState;

	char		strText[ 255 ];
	DWORD		dwRet;

	switch ( rasConnState )
	{
		case RASCS_OpenPort:
			// wait for status dialog to show up first
			while ( gHwndStatus == NULL )
				Sleep( 1000 );
		
			getMsgString( strText, IDS_OPENING_PORT );
			setStatusDialogText( IDC_DIAL_STATUS, strText );		
			if ( dwError )
				connectErr( dwError );
			else
				Sleep( 1000 );
		break;
		
		case RASCS_PortOpened:
			getMsgString( strText, IDS_INIT_MODEM );
			setStatusDialogText( IDC_DIAL_STATUS, strText );
			if ( dwError )
				connectErr( dwError );
			else
				Sleep( 1000 );
		
		break;
		
		case RASCS_ConnectDevice:
			if ( gDialAttempts == 1 )
			{
				getMsgString( strText, IDS_DIALING );
				setStatusDialogText( IDC_DIAL_STATUS, strText );
			}
			else
			{
				char    szBuf[ 128 ];
		
				getMsgString(strText, IDS_DIALING_OF);
				wsprintf( szBuf, (LPCSTR)strText, gDialAttempts, NUM_ATTEMPTS );
				setStatusDialogText( IDC_DIAL_STATUS, strText );
			}
			if ( dwError )
				connectErr( dwError );
			else
				Sleep( 1000 );
		break;
		
		case RASCS_Authenticate:
			getMsgString( strText, IDS_VERIFYING );
			setStatusDialogText( IDC_DIAL_STATUS, strText );
			if ( dwError )
				connectErr( dwError );
			else
				Sleep( 1000 );
		break;
		
		case RASCS_Authenticated:
			getMsgString( strText, IDS_LOGGING_ON );
			setStatusDialogText( IDC_DIAL_STATUS, strText );
			if ( dwError )
				connectErr( dwError );
			else
				Sleep( 1000 );
		break;
		
		case RASCS_Connected:
			getMsgString( strText, IDS_CONNECTED );
			setStatusDialogText( IDC_DIAL_STATUS, strText );
			setCallState( StateConnected );
			if ( dwError )
				connectErr( dwError );
			else
				Sleep( 1000 );
		break;
		
		case RASCS_Disconnected:
			// If this is an unexpected disconnect then hangup and take 
			// down the status dialog box
			if ( gCallState == StateConnected || gCallState == StateConnecting )
			{
				ASSERT( m_lpfnRasHangUp );
				dwRet = ( *m_lpfnRasHangUp )( gRasConn );
		
				if ( dwRet == ERROR_INVALID_HANDLE )
					trace("dialer.cpp : ProcessRasDialEvent (stateConnected) - Can't hangup. Invalid Connection Handle.");
				else if ( dwRet && dwRet != ERROR_INVALID_HANDLE )
					trace("dialer.cpp : ProcessRasDialEvent (stateConnected) - Can't hangup. Error %d", dwRet);

				Sleep( 3000 );
				if ( gCallState == StateConnecting )
				{
					if ( dwError != SUCCESS )
					{
						if ( gHwndStatus )
						{
					 		getMsgString( strText, IDS_DISCONNECTING );
					 		setStatusDialogText( IDC_DIAL_STATUS, strText );
					 	}
					}
				}

				// here we pass redial msg if needed.
				removeStatusDialog();
				if ( gCallState == StateConnecting )
				{
					gLineDrop = TRUE;  // remove if we ask users for redial
					displayDialErrorMsg( dwError );
				}
			}
		
			setCallState( StateIdle );
		break;
		
		case RASCS_WaitForModemReset:
			getMsgString( strText, IDS_DISCONNECTING );
			setStatusDialogText( IDC_DIAL_STATUS, strText );
			if ( dwError )
			{
				trace("dialer.cpp : ProcessRasDialEvent (WaitForModemReset) - Connection Error %d", dwError);
				connectErr( dwError );
			}
			else
				Sleep( 1000 );
		
		break;
		
		default:
			if ( dwError )
			{
				trace( "dialer.cpp : ProcessRasDialEvent (default case) - Connection Error %d", dwError );
				connectErr( dwError );
			}
		break;
	}
}




//********************************************************************************
// DialerConnect
// initiates a dialer connection (used if Dial on Demand is disabled)
// assumes the dialer has already been configured
// returns:
//	FALSE if the dialer cannot be connected
//	TRUE if the dialer is already connected or if it was able to connect
//		successfully
//********************************************************************************
BOOL DialerConnect()
{
	DWORD			dwError;
	DWORD			dwRet;
	BOOL			connectSucceed = TRUE;

	gHwndNavigator = GetActiveWindow();

	// return if dialer already connected
	if ( IsDialerConnected() )
		return TRUE;
 
	gDialAttempts = 1;
	gRasConn = NULL; 		// init global connection handle
	gCancelled = FALSE;		// assume connection is not canceled by the user, unless otherwise


	char*			pbPath = NULL;
	LPVOID			notifierProc = NULL;
	DWORD			notifierType = 0;
	
	notifierType = 1;
	notifierProc = (LPVOID)RasDialFunc;
	
	// WinNT40 find system phone book first then start RASDIAL
	if ( gPlatformOS == VER_PLATFORM_WIN32_NT )
	{
		startRasMonNT();	// should check the error here, but I don't know
							// what to do if we fail (report that error?)

		pbPath = getPhoneBookNT();
	}

	ASSERT( m_lpfnRasDial );
	// do the dialing here
	dwError = ( *m_lpfnRasDial )( NULL, pbPath, &gDialParams, notifierType, notifierProc, &gRasConn );

	if ( dwError == ERROR_NOT_ENOUGH_MEMORY )
		trace( "dialer.cpp : [native] DialerConnect - Not enough memory for dialing activity. Dialing failed." );

	else if ( dwError && dwError != ERROR_NOT_ENOUGH_MEMORY )
		trace( "dialer.cpp : [native] DialerConnect - Dialing failed. Error code = %d", dwError );

	   
	if ( dwError == SUCCESS )
	{
		// Dialing succeeded
		// display connections status dialog & dispatch window msgs...

		MSG			msg;
		setCallState( StateConnecting );
		while ( (	( gRASstate != RASCS_Connected ) &&
					( gRASstate != RASCS_Disconnected ) ) &&
				( !gCancelled ) &&
				( !gLineDrop ) &&
				( !gDeviceErr) )
		{
			if ( ::GetMessage( &msg, NULL, 0, 0 ) )
			{
				::TranslateMessage( &msg );
				::DispatchMessage( &msg );
			}
			else
			{
				// WM_QUIT!!!
				break;
			}
		}
		
		removeStatusDialog();
		if ( ( gRASstate != RASCS_Connected ) || gCancelled )
			connectSucceed = FALSE;

	}
	else
	{
		// dialing failed!!!, display err msg
		connectSucceed = FALSE;
		displayDialErrorMsg( dwError );
	}

	if ( !connectSucceed )
	{
		// hangup connection
		if ( gRasConn )
		{
			ASSERT( m_lpfnRasHangUp );
			dwRet = ( *m_lpfnRasHangUp )( gRasConn );
			
			if ( dwRet == ERROR_INVALID_HANDLE )
				trace("dialer.cpp : [native] DialerConnect - Can't hangup. Invalid Connection Handle.");
			else if ( dwRet && dwRet != ERROR_INVALID_HANDLE )
				trace("dialer.cpp : [native] DialerConnect - Can't hangup. Error %d", dwRet);

			// give RasHangUp some time till complete hangup
			removeStatusDialog();
			// wait three seconds for tear-down of notification proc.
			// should figure out how to do this better (maybe set something
			// in RasDialFunc which ensures us not getting called again?)
			Sleep( 3000 );
		}
	}

	SetActiveWindow( gHwndNavigator );
                                             
	return connectSucceed;
}


//********************************************************************************
// DialerHangup
// terminates a dialer connection
//	if there is an active connection, it is terminated
// returns:
//	nothing
//********************************************************************************
void DialerHangup() 
{
	RASCONN*		info = NULL;
	RASCONN*		lpTemp = NULL;
	DWORD			code;
	DWORD			count = 0;
	DWORD			dSize = stRASCONN;
	DWORD			dwRet;
    BOOL			triedOnce = FALSE;
    
tryAgain:
	// try to get a buffer to receive the connection data
	info = (RASCONN*)LocalAlloc( LPTR, (UINT)dSize );

	if ( !info )
		return;
	
	// set RAS struct size
 	info->dwSize = dSize;

	ASSERT( m_lpfnRasEnumConnections);
	// enumerate open connections
	code = ( *m_lpfnRasEnumConnections )( info, &dSize, &count );

	if ( code != 0 && triedOnce == TRUE )
	{
		LocalFree( info );
		return;
	}
	
	if ( code == ERROR_BUFFER_TOO_SMALL )
	{
		triedOnce = TRUE;
		
		// free the old buffer & allocate a new bigger one
		LocalFree( info );
		goto tryAgain;
	}
	
	// check for no connections
	if ( count == 0 )
	{
		LocalFree( info );
		return;
	}

	ASSERT( m_lpfnRasHangUp );
	// just hang up everything
	for ( int i = 0; i < (int) count; i++ )
	{
		dwRet = ( *m_lpfnRasHangUp )( info[ i ].hrasconn );

		if ( dwRet == ERROR_INVALID_HANDLE )
			trace( "dialer.cpp : DialerHangup - Can't hangup. Invalid Connection Handle." );
		else if ( dwRet && dwRet != ERROR_INVALID_HANDLE )
			trace("dialer.cpp : DialerHangup - Can't hangup. Error %d.", dwRet);

		Sleep( 3000 );
	}

	LocalFree( info );
}

//********************************************************************************
// findDialerData
// search the javascript array for specific string value
//********************************************************************************
char* findDialerData( char** dialerData, char* name )
{
	char**	entry = dialerData;
	char*	value;
	
	while ( *entry )
	{
		if ( strncmp( *entry, name, strlen( name ) ) == 0 )
		{
			value = strchr( *entry, '=' );
			if ( value )
				value++;
			break;
		}
		entry++;
	}
	
	return (char*)value;
}


//********************************************************************************
// fillAccountParameters
// fill in account information, given from JS array
//********************************************************************************
static void fillAccountParameters( char** dialerData, ACCOUNTPARAMS* account )
{
	char*		value;
	
	value = findDialerData( dialerData, "AccountName" );
	strcpy( account->ISPName, value ? value : "My Account" ); 
	
	// file name
	value = findDialerData( dialerData, "FileName" );
	strcpy( account->FileName, value ? value : "My Account" );
	// DNS
	value = findDialerData( dialerData, "DNSAddress" );
	strcpy( account->DNS, value ? value : "0.0.0.0" );
	// DNS2
	value = findDialerData( dialerData, "DNSAddress2" );
	strcpy( account->DNS2, value ? value : "0.0.0.0" );
	// domain name
	value = findDialerData( dialerData, "DomainName" );
	strcpy( account->DomainName, value ? value : "" );
	// login name
	value = findDialerData( dialerData, "LoginName" );
	strcpy(account->LoginName, value ? value : "");
	// password
	value = findDialerData( dialerData, "Password" );
	strcpy( account->Password, value ? value : "" );
	// script file name
	value = findDialerData( dialerData, "ScriptFileName" );
	strcpy( account->ScriptFileName, value ? value : "" );
	// script enabled?
	value = findDialerData( dialerData, "ScriptEnabled" );
	if ( value )
	{
		account->ScriptEnabled = ( strcmp( value, "TRUE" ) == 0 );
		// get script content
		value = findDialerData( dialerData, "Script" );
		if ( value )
		{
		//	ReggieScript = (char*) malloc(strlen(value) + 1);
		//	strcpy(ReggieScript, value);
		}		   	
	}
	else
		account->ScriptEnabled = 0;
	// need TTY window?
	value = findDialerData( dialerData, "NeedsTTYWindow" );
	account->NeedsTTYWindow = ( value && ( strcmp( value, "TRUE" ) == 0 ) );
	// VJ compression enabled?
	value = findDialerData( dialerData, "VJCompresssionEnabled" );
	account->VJCompressionEnabled = ( value && ( strcmp( value, "TRUE" ) == 0 ) );
	// International mode?
	value = findDialerData( dialerData, "IntlMode" );
	account->IntlMode = ( value && ( strcmp( value, "TRUE" ) == 0 ) );
	// dial on demand?
	value = findDialerData( dialerData, "DialOnDemand" );
	account->DialOnDemand = ( value && ( strcmp( value, "TRUE" ) == 0 ) );
	// isp phone number
	value = findDialerData( dialerData, "ISPPhoneNum" );
	if ( value )
		strcpy( account->ISPPhoneNum, value );
	else
		strcpy( account->ISPPhoneNum, "" );
	// ISDN phone number
	value = findDialerData( dialerData, "ISDNPhoneNum" );
	if ( value )
		strcpy( account->ISDNPhoneNum, value );
	else
		strcpy( account->ISDNPhoneNum, "" );
	
}


//********************************************************************************
// fillLocationParameters
// fill in location information, given from JS array
//********************************************************************************
static void fillLocationParameters( char** dialerData, LOCATIONPARAMS* location )
{
	char*		value;
	
	// modem name
	value = findDialerData( dialerData, "ModemName" );
	strcpy( location->ModemName, value ? value : "" );
	// modem type
	value = findDialerData( dialerData, "ModemType" );
	strcpy( location->ModemType, value ? value : "" );
	// dial type
	value = findDialerData( dialerData, "DialType" );
	location->DialType = !( value && ( strcmp( value, "TONE" ) == 0 ) );
	// outside line access
	value = findDialerData( dialerData, "OutsideLineAccess" );
	strcpy( location->OutsideLineAccess, value ? value : "" );
	// disable call waiting?
	value = findDialerData( dialerData, "DisableCallWaiting");
	location->DisableCallWaiting = ( value && ( strcmp(value, "TRUE") == 0 ) );
	// disable call waiting code
	value = findDialerData( dialerData, "DisableCallWaitingCode" );
	strcpy( location->DisableCallWaitingCode, value ? value : "" );
	// user area code
	value = findDialerData( dialerData, "UserAreaCode" );
	strcpy( location->UserAreaCode, value ? value : "" );
	// user country code
	value = findDialerData( dialerData, "CountryCode" );
	if ( value )
	{
		char*	stopstr = "\0";
		location->UserCountryCode = (short)strtol( value, &stopstr, 10 );
	}
	else
		location->UserCountryCode = 1;   // default to US
	// dial as long distance?
	value = findDialerData( dialerData, "DialAsLongDistance");
	location->DialAsLongDistance = ( value && ( strcmp( value, "TRUE" ) == 0 ) );	
	// long distance access
	value = findDialerData( dialerData, "LongDistanceAccess" );
	strcpy( location->LongDistanceAccess, value ? value : "" );
	// dial area code?
	value = findDialerData( dialerData, "DialAreaCode" );
	location->DialAreaCode = ( value && ( strcmp( value, "TRUE" ) == 0 ) );	
	// dial prefix code
	value = findDialerData( dialerData, "DialPrefix" );
	strcpy( location->DialPrefix, value ? value : "" );
	// dial suffix code
	value = findDialerData( dialerData, "DialSuffix" );
	strcpy( location->DialSuffix, value ? value : "" );
	// use both ISDN lines?
	value = findDialerData( dialerData, "UseBothISDNLines" );
	location->UseBothISDNLines = ( value && ( strcmp( value, "TRUE" ) == 0 ) );	
	// 56k ISDN?
	value = findDialerData( dialerData, "56kISDN" );
	location->b56kISDN = ( value && ( strcmp( value, "TRUE" ) == 0 ) );
	// disconnect time
	value = findDialerData( dialerData, "DisconnectTime" );
	if ( value )
		location->DisconnectTime = atoi( value );
	else
		location->DisconnectTime = 5;
}

//********************************************************************************
// onlyOneSet
// Just an XOR of DialAsLongDistance & DialAreaCode - if only one of them is
// set then we can't use MS Locations (if neither are set then we can use 
// locations, but disable use of both - they just don't allow disabling of each
// individually)
//********************************************************************************
static BOOL onlyOneSet( const LOCATIONPARAMS& location )
{
	return ( location.DialAsLongDistance ?
		( location.DialAreaCode ? FALSE : TRUE ) :
		( location.DialAreaCode ? TRUE : FALSE ) );
}

//********************************************************************************
// prefixAvail
// returns:
//	TRUE if there are prefixes that make location unusable
//********************************************************************************
static BOOL prefixAvail( const LOCATIONPARAMS& location )
{
	return ( location.DisableCallWaiting && location.DisableCallWaitingCode[ 0 ] != 0 ) ||
		( location.OutsideLineAccess && location.OutsideLineAccess[ 0 ] != 0 );
}

//********************************************************************************
// parseNumber
//
// Parses a canonical TAPI phone number into country code, area code, and
// local subscriber number
//********************************************************************************
static void parseNumber( LPCSTR lpszCanonical, LPDWORD lpdwCountryCode,
	LPSTR lpszAreaCode, LPSTR lpszLocal)
{
	//*** sscanf dependency removed for win16 compatibility
	char		temp[ 256 ];
	int			p1, p2;

	// Initialize our return values
	*lpdwCountryCode = 1;  // North America Calling Plan
	*lpszAreaCode = '\0';
	*lpszLocal = '\0';

	if ( !lpszCanonical || !*lpszCanonical )
		return;

	// We allow three variations (two too many):
	//  -: +1 (415) 428-3838    (TAPI canonical number)
	//  -: (415) 428-3838       (TAPI canonical number minus country code)
	//  -: 428-3838             (subscriber number only)
	//
	// NOTE: this approach only works if there is a city/area code. The TAPI
	// spec says the city/area code is optional for countries that have a flat
	// phone numbering system

	// Take my advice, always start at the beginning.
	p1 = 0;

	// Allow spaces
	while ( lpszCanonical[p1] == ' ' )
		p1++;

	// Handle the country code if '+' prefix seen
	if ( lpszCanonical[p1] == '+' )
	{
		p1++;
		if ( !isdigit( lpszCanonical[ p1 ] ) )
			return;

		p2 = p1;
		while ( isdigit( lpszCanonical[ p1 ] ) )
			p1++;
		strncpy( temp, &lpszCanonical[ p2 ], p1 - p2 );
		*lpdwCountryCode = atoi( temp );
	}

	// Allow spaces
	while ( lpszCanonical[p1] == ' ' )
		p1++;

	// Handle the area code if '(' prefix seen
	if ( lpszCanonical[ p1 ] == '(' )
	{
		p1++;
		if ( !isdigit( lpszCanonical[ p1 ] ) )
			return;

		p2 = p1;
		while ( isdigit( lpszCanonical[ p1 ] ) )
			p1++;
		strncpy( lpszAreaCode, &lpszCanonical[ p2 ], p1 - p2 );

		p1++;      // Skip over the trailing ')'
	}

	// Allow spaces
	while ( lpszCanonical[p1] == ' ' )
		p1++;

	// Whatever's left is the subscriber number (possibly including the whole string)
	strcpy( lpszLocal, &lpszCanonical[ p1 ] );
}

//********************************************************************************
// composeNumber
// create a phone number encompassing all of the location information to hack
// around dialup networking ignoring the location information if you turn off
// the "dial area and country code" flag
//********************************************************************************
static void composeNumber( ACCOUNTPARAMS& account, const LOCATIONPARAMS& location, char csNumber[] )
{
	// if they need to dial something to get an outside line next
	if ( location.OutsideLineAccess[ 0 ] != 0 )
	{
        strcat( csNumber, location.OutsideLineAccess );
        strcat( csNumber, " " );
	}

	// add disable call waiting if it exists
	if ( location.DisableCallWaiting && location.DisableCallWaitingCode[ 0 ] != 0 )
	{
		strcat( csNumber, location.DisableCallWaitingCode );
		strcat( csNumber, " ");
	}
	
	if ( account.IntlMode )
	{ 
	
		// In international mode we don't fill out the area code or
		//   anything, just the exchange part
		strcat( csNumber, account.ISPPhoneNum );
	}
	else
	{

		// lets parse the number into pieces so we can get the area code & country code
		DWORD		nCntry;
		char		szAreaCode[ 32 ];
		char		szPhoneNumber[ 32 ];
		parseNumber( account.ISPPhoneNum, &nCntry, szAreaCode, szPhoneNumber );
		
		// dial the 1 (country code) first if they want it
		if ( location.DialAsLongDistance )
		{
			char	cntry[ 10 ];
			ultoa( nCntry, cntry, 10 );
		
			if ( strcmp( location.LongDistanceAccess, "" ) == 0 )
				strcat( csNumber, cntry );
			else 
				strcat( csNumber, location.LongDistanceAccess );
		
			strcat( csNumber, " " );
		}
		
		// dial the area code next if requested
		if ( location.DialAreaCode )
		{
			strcat( csNumber, szAreaCode );
			strcat( csNumber, " " );
		}
	
		// dial the local part of the number
		strcat( csNumber, szPhoneNumber );
	}
}

//********************************************************************************
// toNumericAddress
// converts from dotted address to numeric internet address
//********************************************************************************
static BOOL toNumericAddress( LPCSTR lpszAddress, DWORD& dwAddress )
{
	//*** sscanf dependency removed for win16 compatibility

	char		temp[ 256 ];
	int			a, b, c, d;
	int			p1, p2;

	strcpy( temp, lpszAddress );

	p2 = p1 = 0;
	while ( temp[ p1 ] != '.' )
		p1++;
	temp[ p1 ] = '\0';
	a = atoi( &temp[ p2 ] );
	
	p2 = ++p1;
	while ( temp[ p1 ] != '.' )
		p1++;
	temp[ p1 ] = '\0';
	b = atoi( &temp[ p2 ] );

	p2 = ++p1;
	while ( temp[ p1 ] != '.' )
		p1++;
	temp[ p1 ] = '\0';
	c = atoi( &temp[ p2 ] );

	p2 = ++p1;
	d = atoi( &temp[ p2 ] );

    // Must be in network order (different than Intel byte ordering)
    LPBYTE  lpByte = (LPBYTE)&dwAddress;

    *lpByte++ = BYTE( a );
    *lpByte++ = BYTE( b );
    *lpByte++ = BYTE( c );
    *lpByte = BYTE( d );

	return TRUE;
}

//********************************************************************************
// getCountryID
//********************************************************************************
static BOOL getCountryID( DWORD dwCountryCode, DWORD& dwCountryID )
{
	ASSERT( m_lpfnRasGetCountryInfo );
	if ( !m_lpfnRasGetCountryInfo )
	{
		trace( "dialer.cpp : GetCountryID - RasGetCountryinfo func not availble. (r)" );
		return FALSE;
	}
		
	RASCTRYINFO*		pCI = NULL;
	BOOL				bRetval = FALSE;

	DWORD				dwSize = stRASCTRYINFO + 256;
	pCI = (RASCTRYINFO*)malloc( (UINT) dwSize );
	
	if ( pCI )
	{
		pCI->dwSize = stRASCTRYINFO;
		pCI->dwCountryID = 1;
      
		while ( ( *m_lpfnRasGetCountryInfo )( pCI, &dwSize ) == 0 )
		{
			if ( pCI->dwCountryCode == dwCountryCode )
			{
				dwCountryID = pCI->dwCountryID;
				bRetval = TRUE;
				break;
			}
			pCI->dwCountryID = pCI->dwNextCountryID;
		}

		free( pCI );
		pCI = NULL;
	}

	return bRetval;
}


//********************************************************************************
// createLink
// creates a shell shortcut to the PIDL
//********************************************************************************
static HRESULT createLink( LPITEMIDLIST pidl, LPCTSTR lpszPathLink, LPCTSTR lpszDesc,
	char* iconPath )
{
    HRESULT			hres;
    IShellLink*		psl = NULL;

    // Get a pointer to the IShellLink interface.
	//CoInitialize(NULL); // work around for Nav thread lock bug

	hres = CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
							IID_IShellLink, (LPVOID FAR*)&psl );
    if ( SUCCEEDED( hres ) )
    {
		IPersistFile* ppf;
		// Set the path to the shortcut target, and add the description.
		psl->SetIDList( pidl );
		psl->SetDescription( lpszDesc );
		if( iconPath && iconPath[ 0 ] )
			psl->SetIconLocation( iconPath, 0 );

		// Query IShellLink for the IPersistFile interface for saving the
		// shortcut in persistent storage.
		hres = psl->QueryInterface( IID_IPersistFile, (LPVOID FAR*)&ppf );

		if ( SUCCEEDED( hres ) )
		{
			WORD		wsz[ MAX_PATH ];

			// Ensure that the string is ANSI.
			MultiByteToWideChar( CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH );

			// Save the link by calling IPersistFile::Save.
			hres = ppf->Save( (LPCOLESTR)wsz, STGM_READ );
			ppf->Release();
		}

		psl->Release();
	}

	//CoUninitialize();

	return hres;
}

//********************************************************************************
// fileExists
//********************************************************************************
static BOOL fileExists( LPCSTR lpszFileName )
{
	BOOL bResult = FALSE;

	HANDLE hFile=NULL;

	// opens the file for READ
	hFile = CreateFile(lpszFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile != INVALID_HANDLE_VALUE) {  // openned file is valid
		bResult = TRUE;
		CloseHandle(hFile);
	}
	return bResult;
}


//********************************************************************************
// processScriptLogin
// generate a script file and return the name of the file.  The
//   caller is responsible for freeing the script file name
//********************************************************************************
static BOOL processScriptedLogin( LPSTR lpszBuf, const char* lpszScriptFile )
{
    // validate our args just for fun
    if (!lpszBuf || (lpszBuf[0] == '\0') || !lpszScriptFile)
        return(FALSE);

    // open the actual script file  
    FILE * fp = fopen(lpszScriptFile, "w");
    if (!fp)
        return(FALSE);   

    // generate a prolog
    char timebuf[24];
    char datebuf[24];
    _strtime(timebuf);
    _strdate(datebuf);
    fprintf(fp, "; %s\n; Created: %s at %s\n;\n;\nproc main\n", lpszScriptFile, datebuf, timebuf);

    // Send a return to poke the server.  Is this needed?
    fprintf(fp, "transmit \"^M\"\n"); 

    for (int i = 0; lpszBuf; i++) {
       LPSTR   lpszDelim;

       // Each event consists of two parts:
       //   1. string to wait for
       //   2. string to reply with
       //
       // The string to reply with is optional. A '|' separates the two strings
       // and another '|' separates each event
       lpszDelim = strchr(lpszBuf, '|');  
       if(lpszDelim)
          *lpszDelim = '\0';
   
       // we are in the "wait for event"
       fprintf(fp, "waitfor \"%s\",matchcase until 30\n", lpszBuf); 

       // skip to the next bit
       lpszBuf = lpszDelim ? lpszDelim + 1 : NULL;

       if (lpszBuf) {
          // now look for the reply event
          lpszDelim = strchr(lpszBuf, '|');  
          if(lpszDelim)
          *lpszDelim = '\0';
       
          // we are in the "reply with" event
          // NOTE: we will want to get the ^M value from someone else
          //   since different ISPs will probably want different ones
          if (!stricmp(lpszBuf, "%name"))
             fprintf(fp, "transmit $USERID\n"); 
          else if(!stricmp(lpszBuf, "%password"))
             fprintf(fp, "transmit $PASSWORD\n"); 
          else if(lpszBuf[0])
             fprintf(fp, "transmit \"%s\"\n", lpszBuf); 

          fprintf(fp, "transmit \"^M\"\n"); 
       }

      lpszBuf = lpszDelim ? lpszDelim + 1 : NULL;
    }                                     

   // writeout the ending bits and cleanup
    fprintf(fp, "endproc\n");
    fclose(fp);

    return(TRUE);
}


//********************************************************************************
// createPhoneBookEntry
// Create a dial-up networking profile
//********************************************************************************
static BOOL createPhoneBookEntry( ACCOUNTPARAMS account, const LOCATIONPARAMS& location )
{
	DWORD		dwRet;
	BOOL		ret = FALSE;
	RASENTRY	rasEntry;
	
	// abort if RAS API ptrs are invalid & mem alloc fails
	ASSERT( m_lpfnRasSetEntryProperties );
	ASSERT( m_lpfnRasSetEntryDialParams );
		
	if ( !m_lpfnRasSetEntryProperties || 
		 !m_lpfnRasSetEntryDialParams )
		return FALSE;

	// Initialize the RNA struct
	memset( &rasEntry, 0, stRASENTRY );
	rasEntry.dwSize = stRASENTRY;
	rasEntry.dwfOptions = RASEO_ModemLights | RASEO_RemoteDefaultGateway;

	// Only allow compression if reg server says its OK
	if ( account.VJCompressionEnabled )
		 rasEntry.dwfOptions |= RASEO_IpHeaderCompression | RASEO_SwCompression;

	if ( account.NeedsTTYWindow )
		if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
			rasEntry.dwfOptions |= RASEO_TerminalBeforeDial;  //win95 bug! RASEO_TerminalBeforeDial means terminal after dial
		else
			rasEntry.dwfOptions |= RASEO_TerminalAfterDial;

	// If Intl Number (not NorthAmerica), or Area Code w/o LDAccess (1) or
	// visa-versa, then abandon using Location - NOTE: for Intl Number we
	// should be able to use location, check it out!
	if ( account.IntlMode || onlyOneSet( location ) )
	{
		char szNumber[ RAS_MaxPhoneNumber + 1 ];
		szNumber[ 0 ] = '\0';

		composeNumber( account, location, szNumber );
		strcpy( rasEntry.szLocalPhoneNumber, szNumber );
	   
		strcpy( rasEntry.szAreaCode, "415" );	// hack around MS bug -- ignored
		rasEntry.dwCountryCode = 1;				// hack around MS bug -- ignored
	}
	else
	{
		// Let Win95 decide to dial the area code or not
		rasEntry.dwfOptions |= RASEO_UseCountryAndAreaCodes;

		trace( "dialer.cpp : Use country and area codes = %d", rasEntry.dwfOptions );

		// Configure the phone number
		parseNumber( account.ISPPhoneNum, &rasEntry.dwCountryCode, 
						rasEntry.szAreaCode, rasEntry.szLocalPhoneNumber );
		  
		if ( !account.IntlMode )
		{
			// if not internationalize version, check the area code and make
			// sure we got a valid area code, if not throw up a err msg
			if ( rasEntry.szAreaCode[ 0 ] == '\0' )
			{
				// Err: The service provider's phone number is missing its area code
				//    (or is not in TAPI cannonical form in the configuration file).
				//    Account creation will fail until this is fixed.
				char*	buf = (char*)malloc( sizeof(char) * 255 );
				if ( buf )
				{
					if ( getMsgString( buf, IDS_MISSING_AREA_CODE ) )
						displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, gHwndNavigator );
					free( buf );
				}
				return FALSE;
			}
		}
	}

	// Now that we have the country code, we need to find the associated
	// country ID
	getCountryID( rasEntry.dwCountryCode, rasEntry.dwCountryID );

	// Configure the IP data
	rasEntry.dwfOptions |= RASEO_SpecificNameServers;
	if ( account.DNS[ 0 ] )
		toNumericAddress( account.DNS, *(LPDWORD)&rasEntry.ipaddrDns );

	if ( account.DNS2[ 0 ] )
		toNumericAddress( account.DNS2, *(LPDWORD)&rasEntry.ipaddrDnsAlt );

	// Configure the protocol and device settings here:

	// Negotiate TCP/IP
	rasEntry.dwfNetProtocols = RASNP_Ip;

	// Point-to-Point protocol (PPP)
	rasEntry.dwFramingProtocol = RASFP_Ppp;

	// modem's information
	strcpy( rasEntry.szDeviceName, location.ModemName );
	strcpy( rasEntry.szDeviceType, location.ModemType );

	// If we have a script, then store it too
	if ( account.ScriptEnabled ) 
	{
		BOOL rtnval = TRUE;
		
/*		// if there is script content, 'Translate' and store in file 
		if ( ReggieScript )
		{ 
		   	// construct script filename if it does not exists
		 	if ( strlen( account.ScriptFileName ) == 0 )
		   	{
				GetProfileDirectory( account.ScriptFileName );
				int nIndex = strlen( account.ScriptFileName );
				strncat( account.ScriptFileName, account.ISPName, 8 );
				strcat( account.ScriptFileName, ".scp" );
		   	} 
			rtnval = ProcessScriptedLogin( (LPSTR)ReggieScript, account.ScriptFileName );
			free( ReggieScript );
		}


		// if there really is a script file (from ISP or Reggie) then use it
		if ( rtnval && FileExists( account.ScriptFileName ) )
		{
			strcpy( rasEntry.szScript, account.ScriptFileName );

			// convert forward slash to backward slash
			int nLen = strlen( rasEntry.szScript );
			for ( int i = 0; i < nLen; i++ )
				if ( rasEntry.szScript[ i ] == '/' )
					rasEntry.szScript[ i ] = '\\';
		}
*/
	}

	dwRet = ( *m_lpfnRasValidateEntryName )( NULL, (LPSTR)account.ISPName );
	
	if ( dwRet == ERROR_ALREADY_EXISTS )
		dwRet = 0;
		
	ASSERT( dwRet == 0 );
	if ( dwRet == ERROR_INVALID_NAME )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasValidateEntryName) - Invalid Name. Can't set RasEntry properties. (r)");
		return FALSE;
	}
	else if ( dwRet == ERROR_ALREADY_EXISTS )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasValidateEntryName) - This name already exists. (r)");
		return FALSE;
	}
	else if ( dwRet )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasValidateEntryName) - Can't Validate account name. Error %d. (r)", dwRet);
		return FALSE;
	}
	
	dwRet = ( *m_lpfnRasSetEntryProperties )( NULL, (LPSTR)(LPCSTR)account.ISPName,
										  (LPBYTE)&rasEntry, stRASENTRY, NULL, 0 );
	ASSERT( dwRet == 0 );
	//if ( dwRet )
		//return -1;		// ??? this is going to return TRUE
	if ( dwRet == ERROR_BUFFER_INVALID )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryProperties) - Invalid Buffer. Can't set RasEntry properties. (r)");
		return FALSE;
	}
	else if ( dwRet == ERROR_CANNOT_OPEN_PHONEBOOK )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryProperties) - Can't open phonebook. Corrupted phonebook or missing components. Can't set RasEntry properties. (r)");
		return FALSE;
	}
	else if ( dwRet )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryProperties) - Can't set RasEntry properties. Error %d. (r)", dwRet);
		return FALSE;
	}

	// We need to set the login name and password with a separate call
	// why doesn't this work for winNT40??
	memset( &gDialParams, 0, sizeof( gDialParams ) );
	gDialParams.dwSize = stRASDIALPARAMS;
	strcpy( gDialParams.szEntryName, account.ISPName );
	strcpy( gDialParams.szUserName, account.LoginName );
	strcpy( gDialParams.szPassword, account.Password );

	// Creating connection entry!
   	char*		pbPath = NULL;

	if ( gPlatformOS = VER_PLATFORM_WIN32_NT )
		pbPath = getPhoneBookNT();
	else
		pbPath = (char*)account.ISPName;

	dwRet = ( *m_lpfnRasSetEntryDialParams )( pbPath, &gDialParams, FALSE );

	if ( dwRet == ERROR_BUFFER_INVALID )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryDialParams) - Invalid Buffer. Can't set RasEntry Dial properties. (r)");
		return FALSE;
	}
	else if ( dwRet == ERROR_CANNOT_OPEN_PHONEBOOK )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryDialParams) - Can't open phonebook. Corrupted phonebook or missing components. Can't set RasEntry Dial properties. (r)");
		return FALSE;
	}
	else if ( dwRet == ERROR_CANNOT_FIND_PHONEBOOK_ENTRY )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryDialParams) - Phonebook entry does not exist. Can't set RasEntry Dial properties. (r)");
		return FALSE;
	}
	else if ( dwRet )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryDialParams) - Can't set RasEntry Dial properties. Error %d. (r)", dwRet);
		return FALSE;
	}

	ret = ( dwRet == 0 );

	if ( gPlatformOS = VER_PLATFORM_WIN32_NT )
	{
		RASCREDENTIALS credentials;

		// sets up user login info for new phonebook entry
		memset( &credentials, 0, sizeof( RASCREDENTIALS ) );
		credentials.dwSize = sizeof( RASCREDENTIALS );
		credentials.dwMask = RASCM_UserName | RASCM_Password;
		strcpy( credentials.szUserName, account.LoginName );
		strcpy( credentials.szPassword, account.Password );
		strcpy( credentials.szDomain, account.DomainName );

		dwRet = ( *m_lpfnRasSetCredentials )( pbPath, (LPSTR)account.ISPName, &credentials, FALSE );

		if ( dwRet == ERROR_INVALID_PARAMETER )
		{
			trace( "dialer.cpp : CreateRNAEntry (RasSetCredentials) - Invalid Parameter. Can't set user credentials. (r)" );
			return FALSE;
		}
		else if ( dwRet == ERROR_CANNOT_OPEN_PHONEBOOK )
		{
			trace( "dialer.cpp : CreateRNAEntry (RasSetCredentials) - Can't open phonebook. Corrupted phonebook or missing components. Can't set user credentials. (r)" );
			return FALSE;
		}
		else if ( dwRet == ERROR_CANNOT_FIND_PHONEBOOK_ENTRY )
		{
			trace( "dialer.cpp : CreateRNAEntry (RasSetCredentials) - Phonebook entry does not exist. Can't set user credentials. (r)" );
			return FALSE;
		}
		else if ( dwRet == ERROR_INVALID_SIZE )
		{
			trace( "dialer.cpp : CreateRNAEntry (RasSetCredentials) - Invalid size value. Can't set user credentials. (r)" );
			return FALSE;
		}
		else if ( dwRet )
		{
			trace( "dialer.cpp : CreateRNAEntry (RasSetCredentials) - Can't set user credentials. Error %d. (r)", dwRet );
			return FALSE;
		}
		ret = ( dwRet == 0 );

		if ( pbPath )
			free( pbPath );
	}	

	// dialing on demand is cool.  let's do that on win95 now
	if ( ret == TRUE && account.DialOnDemand )
		if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
			EnableDialOnDemand95( (LPSTR)account.ISPName, TRUE );
		else if ( gPlatformOS == VER_PLATFORM_WIN32_NT )
			EnableDialOnDemandNT( (LPSTR)account.ISPName, TRUE );
	
	//SetAutoDisconnect( location.DisconnectTime );

	return ret;
}

//********************************************************************************
// DialerConfig
//
// setup and configures the dialer and networking stuff
// used in 3 conditions:
// 1. when calling regi for a new account
// 2. to configure new account from regi on users system
// 3. when optionally register Navigator in existing account path
//********************************************************************************
int DialerConfig( char** dialerDataArray )
{
	gHwndNavigator = GetActiveWindow();
	
	ACCOUNTPARAMS		account;
	LOCATIONPARAMS		location;
	
	if ( !dialerDataArray )
		return -1;
	
	// now we try to get values from the JS array and put them into corresponding 
	// account and location parameters
	fillAccountParameters( dialerDataArray, &account );
	fillLocationParameters( dialerDataArray, &location );
	
	// configure & creating Dial-Up Networking profile here for Win95 & WinNT40
	// win16 use Shiva's RAS 
	if ( !( createPhoneBookEntry( account, location ) ) )
	{
	
		// Err: Unable to crate RNA phone book entry!
		char*	buf = (char*)malloc( sizeof(char) * 255 );
		if ( buf )
		{
			if ( getMsgString( buf, IDS_NO_RNA_REGSERVER ) )
			displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, gHwndNavigator );
			free( buf );
		}
	}
	
//	int	ret;
//	if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
//	{
//		// sets the location stuff
//		ret = SetLocationInfo( account, location );
//		//ret = DisplayDialableNumber();
//	}
//	else
//	{
//		ret = SetLocationInfoNT( account, location );
//	}

	return 0;
}


//********************************************************************************
// IsDialerConnected
// checks if the dialer is still connected
//
// returns:
//	TRUE if the dialer is connected
//	FALSE if the dialer is not connected or there is an error trying to
//		determine if the dialer is connected
//********************************************************************************
BOOL IsDialerConnected()
{
	RASCONN*		info = NULL;
	RASCONN*		lpTemp = NULL;
	DWORD			code = 0;
	DWORD			count = 0;
	DWORD			dSize = stRASCONN;
	
	// try to get a buffer to receive the connection data
	if ( ! ( info = (RASCONN*)LocalAlloc( LPTR, dSize ) ) )
		return FALSE;
	
	// see if there are any open connections
	info->dwSize = stRASCONN;
	code = (*m_lpfnRasEnumConnections)( info, &dSize, &count );
	
	if ( 0 != code ) // ERROR_BUFFER_TOO_SMALL ?
	{
		// free the old buffer
		LocalFree( info );
	
		// allocate a new bigger one
		info = (RASCONN*)LocalAlloc( LPTR, dSize );
		if ( !info )
			return FALSE;
	
		// try to enumerate again
		info->dwSize = dSize;
		if ( (*m_lpfnRasEnumConnections) (info, &dSize, &count ) != 0 )
		{
			LocalFree( info );
			// can't enumerate connections, assume none is active
			return FALSE;
		}
	}
	
	// check for no connections
	if ( 0 == count )
	{
		LocalFree( info );
		return FALSE;
	}
	
	LocalFree( info );
	
	return TRUE;	
}


//********************************************************************************
// SetAutoDisconnect
// Sets the autodisconnect time if idle
// the parameter "disconnectTime" is specified as MINUTES, convert it to SECONDS
// as necessary
//********************************************************************************
void SetAutoDisconnect(DWORD disconnectTime)
{
	HKEY    hKey;
	DWORD   dwDisposition;
	long    result;
	DWORD	dwValue;

	// if it's win95
	if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
	{

		//
		// set the autodial flag first
		//
		result = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
								NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);

		// err, oops
		if (result != ERROR_SUCCESS)
		   return;

		// try setting autodisconnect here
		dwValue = 1;
		result = RegSetValueEx(hKey, "EnableAutoDisconnect", NULL, REG_BINARY, (LPBYTE)&dwValue, sizeof(DWORD));

		// default auto-disconnect after 5 minutes or as specified (with a minimal of 3 minutes)
		if (disconnectTime < 3)
			dwValue = 3;
		else
			dwValue = disconnectTime;
		result = RegSetValueEx(hKey, "DisconnectIdleTime", NULL, REG_BINARY, (LPBYTE)&dwValue, sizeof(DWORD));

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
		result = RegSetValueEx(hKey, "EnableAutoDisconnect", NULL, REG_BINARY, (LPBYTE)&dwValue, sizeof(DWORD));

		// default auto-disconnect after 5 minutes
		if (disconnectTime < 3)
			dwValue = 3;
		else
			dwValue = disconnectTime;
		result = RegSetValueEx(hKey, "DisconnectIdleTime", NULL, REG_BINARY, (LPBYTE)&dwValue, sizeof(DWORD));

		RegCloseKey(hKey);


		//
		// also set the autodial flag here
		//
		result = RegCreateKeyEx(HKEY_USERS, ".Default\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
								NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);

		// err, oops
		if (result != ERROR_SUCCESS)
		   return;

		// try setting autodisconnect here
		dwValue = 1;
		result = RegSetValueEx(hKey, "EnableAutoDisconnect", NULL, REG_BINARY, (LPBYTE)&dwValue, sizeof(DWORD));

		// default auto-disconnect after 5 minutes or as specified (with a minimal of 3 minutes)
		if (disconnectTime < 3)
			dwValue = 3;
		else
			dwValue = disconnectTime;
		result = RegSetValueEx(hKey, "DisconnectIdleTime", NULL, REG_BINARY, (LPBYTE)&dwValue, sizeof(DWORD));

		RegCloseKey(hKey);

		//
		// set the autodisconnect flags here too
		//
		result = RegCreateKeyEx(HKEY_USERS, ".Default\\Software\\Microsoft\\Windows\\Internet Settings",
								NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);

		// err, oops
		if (result != ERROR_SUCCESS)
		   return;

		dwValue = 1;
		result = RegSetValueEx(hKey, "EnableAutoDisconnect", NULL, REG_BINARY, (LPBYTE)&dwValue, sizeof(DWORD));

		// default auto-disconnect after 5 minutes
		if (disconnectTime < 3)
			dwValue = 3;
		else
			dwValue = disconnectTime;
		result = RegSetValueEx(hKey, "DisconnectIdleTime", NULL, REG_BINARY, (LPBYTE)&dwValue, sizeof(DWORD));

		RegCloseKey(hKey);

	} else { // NT40

		// we need to convert disconnectTime to # of seconds for NT40
		dwValue = (disconnectTime * 60);

		result = RegOpenKeyEx(HKEY_USERS, ".DEFAULT\\Software\\Microsoft\\RAS Phonebook", NULL, KEY_ALL_ACCESS, &hKey);

		if (result != ERROR_SUCCESS)
			return;

		// now set the auto disconnect seconds
		result = RegSetValueEx(hKey, "IdleHangupSeconds", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));

		RegCloseKey(hKey);


	} 

	return;
}


//********************************************************************************
// CheckDNS
// for Win95
// If user has DNS enabled, when setting up an account, we need to warn them
// that there might be problems.
//********************************************************************************
void CheckDNS()
{
    char		buf[ 256 ];
    HKEY		hKey;
    DWORD		cbData;
    LONG		res;

	// open the key if registry
	if( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
	             "System\\CurrentControlSet\\Services\\VxD\\MSTCP", 
	             NULL, 
	             KEY_ALL_ACCESS   , 
	             &hKey ) )
		return;
	
	cbData = sizeof( buf );
	res = RegQueryValueEx( hKey, "EnableDNS", NULL, NULL, (LPBYTE)buf, &cbData );
	
	RegCloseKey( hKey );
	
	// if DNS is enabled we need to warn the user
	if( buf[ 0 ] == '1' )
	{
		BOOL		correctWinsockVersion = FALSE;
	
		// check for user's winsock version first, see if it's winsock2
		WORD		wVersionRequested;  
		WSADATA		wsaData; 
		int			err; 
	
		wVersionRequested = MAKEWORD( 2, 2 ); 
	
		err = WSAStartup( wVersionRequested, &wsaData ); 
	
		if ( err != 0 )
		{
			// user doesn't have winsock2, so check their winsock's date
			char		winDir[ MAX_PATH ];
			UINT		uSize = MAX_PATH;
			char		winsockFile[ MAX_PATH ];
		
			winDir[ 0 ] = '\0';
			winsockFile[ 0 ] = '\0';
			GetWindowsDirectory( (char*)&winDir, uSize );
			strcpy( winsockFile, winDir );
			strcat( winsockFile, "\\winsock.dll" );
		
			HANDLE hfile = CreateFile( (char*)&winsockFile, GENERIC_READ, 0, NULL,
								OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		
			if ( hfile != INVALID_HANDLE_VALUE )
			{
				// openned file is valid
		
				FILETIME		lastWriteTime;
				BOOL rtnval = GetFileTime( hfile, NULL, NULL, &lastWriteTime );
		
				SYSTEMTIME		systemTime;
				rtnval = FileTimeToSystemTime( &lastWriteTime, &systemTime );
	
				// perfect example of why Windows software is so awful	
				if ( ( systemTime.wYear >= 1996 ) && ( systemTime.wMonth >= 8 ) &&
						( systemTime.wDay >= 24 ) )
					correctWinsockVersion = TRUE;
		
				CloseHandle( hfile );
			}
		}
	
		else
			correctWinsockVersion = TRUE;
	
	
		if ( !correctWinsockVersion )
		{
			// Err: Your system is configured for another Domain Name System (DNS) server.
			//    You might need to edit your DNS configuration.  Check the User's Guide
			//    for more information.
			char	buf[ 255 ];
			if ( getMsgString( buf, IDS_DNS_ALREADY ) ) 
				displayErrMsgWnd( buf, MB_OK | MB_ICONASTERISK, NULL );
		}
	}
	
	return;
}



//********************************************************************************
// CheckDUN
// for Win95
// If user doesn't have Dial-Up Networking installed, they will have problem
// setting up an account.
//********************************************************************************
BOOL CheckDUN()
{
	static const char c_szRNA[] = "rundll.exe setupx.dll,InstallHinfSection RNA 0 rna.inf";
	
	BOOL		bInstall = FALSE;
	HKEY		hKey;
	LONG		res;
	char		szBuf[ MAX_PATH ];
	
	// Let's see if its installed
	if( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
	                 "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\OptionalComponents\\RNA", 
	                 NULL, 
	                 KEY_QUERY_VALUE, 
	                 &hKey ) )
		bInstall = TRUE;
	
	// the key is there, was it actually installed though...
	if( !bInstall )
	{
		DWORD cbData = sizeof( szBuf );
		res = RegQueryValueEx( hKey, "Installed", NULL, NULL, (LPBYTE)szBuf, &cbData );
		if ( szBuf[ 0 ] != '1' && szBuf[ 0 ] != 'Y' && szBuf[ 0 ] != 'y' )
			bInstall = TRUE;
	}
	
	// make sure a random file from the installation exists so that we
	//   know the user actually installed instead of just skipping over
	//   the install step
	if( !bInstall )
	{
		if( GetSystemDirectory( szBuf, sizeof( szBuf ) ) )
		{
			// create a name of one of the files
			strcat( szBuf, "\\pppmac.vxd" );
	
			// let's see if that file exists
			struct _stat stat_struct;
			if( _stat( szBuf, &stat_struct ) != 0 )
			bInstall = TRUE;
	
		}
	}
	
	// if no Dial-Up Networking installed install it now
	if ( bInstall )
	{
		// let the user not install it or exit
		//
		// Err: Dial-Up Networking has not been installed on this machine;
		//    this product will not work until Dial-Up Networking is installed.  
		//    Would you like to install Dial-Up Networking now?
	
		char*	buf = (char*)malloc(sizeof( char ) * 255 );
		if ( !buf )
			// Err: Out of Memory
			return FALSE;
	
		if ( getMsgString( buf, IDS_NO_DUN ) )
		{
			if ( IDOK != displayErrMsgWnd( buf, MB_OKCANCEL | MB_ICONASTERISK, NULL ) )
			{
				free( buf );
				return FALSE;
			}
		}
		free( buf );     
	
	
		// install Dial-Up Networking
		PROCESS_INFORMATION pi;
		STARTUPINFO         sti;
		UINT                err = ERROR_SUCCESS;
	
		memset( &sti, 0, sizeof( sti ) );
		sti.cb = sizeof( STARTUPINFO );
	
		// Run the setup application
		if ( CreateProcess( NULL, (LPSTR)c_szRNA, 
							NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi ) )
		{
			CloseHandle( pi.hThread );
	
			// Wait for the modem wizard process to complete
			WaitForSingleObject( pi.hProcess, INFINITE );
			CloseHandle( pi.hProcess );
		}
	
	}
	
	RegCloseKey( hKey );
	return TRUE;
}


//********************************************************************************
// check if a user is an Administrator on NT40
//********************************************************************************
static BOOL isWinNTAdmin()
{
	HANDLE			hAccessToken;
    UCHAR			infoBuffer[ 1024 ];
    PTOKEN_GROUPS	ptgGroups = (PTOKEN_GROUPS)infoBuffer;
    DWORD			dwInfoBufferSize;
    PSID			psidAdministrators;
    SID_IDENTIFIER_AUTHORITY siaNtAuthority = SECURITY_NT_AUTHORITY;
    UINT			x;
    BOOL			bSuccess;

	if ( !OpenProcessToken( GetCurrentProcess(), TOKEN_READ, &hAccessToken ) )
		return FALSE;
 
	bSuccess = GetTokenInformation( hAccessToken, TokenGroups, infoBuffer,
									1024, &dwInfoBufferSize );

	CloseHandle( hAccessToken );

	if( !bSuccess )
		return FALSE;
 
	if( !AllocateAndInitializeSid( &siaNtAuthority, 2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&psidAdministrators ) )
		return FALSE;
 
   // assume that we don't find the admin SID.
	bSuccess = FALSE;
 
	for ( x = 0; x < ptgGroups->GroupCount; x++ )
	{
		if ( EqualSid( psidAdministrators, ptgGroups->Groups[ x ].Sid ) )
		{
			bSuccess = TRUE;
			break;
         }
	}

	FreeSid( &psidAdministrators );
	return bSuccess;
}



//********************************************************************************
// CheckDUN_NT
// for WinNT40
// If user doesn't have Dial-Up Networking installed, they will have problem
// setting up an account.
//********************************************************************************
BOOL CheckDUN_NT()
{
	BOOL		bInstall = FALSE;
	BOOL		bAdmin = FALSE;
	HKEY		hKey;
	LONG		res;
	char		szBuf[ MAX_PATH ];
	char*		buf = NULL;
	
	
	// Let's see if its installed
	// "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\RemoteAccess"
	if ( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE,
						"SYSTEM\\CurrentControlSet\\Services\\RemoteAccess",
						NULL, 
						KEY_QUERY_VALUE,
						&hKey ) )
		bInstall = TRUE;
	
	// the key is there, was it actually installed though...
	// look for some RAS keys
	szBuf[ 0 ] = '\0';
	if ( !bInstall )
	{
		DWORD cbData = sizeof( szBuf );
		res = RegQueryValueEx( hKey, "DisplayName", NULL, NULL, (LPBYTE)szBuf, &cbData );
		if( strlen( szBuf ) == 0 )
			bInstall = TRUE;
	
		RegCloseKey( hKey );
	
	// how about autodial manager....
	// "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\RasAuto"
		if ( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE,
						"SYSTEM\\CurrentControlSet\\Services\\RasAuto",
						NULL,
						KEY_QUERY_VALUE,
						&hKey ) )
			bInstall = TRUE;
	
		RegCloseKey( hKey );
	}
	
	
	// if no Dial-Up Networking installed, warn the users depending on
	// their premissions and return FALSE
	if ( bInstall )
	{
		bAdmin = isWinNTAdmin();
	
		if ( bAdmin )
		{
			// Err: Dial-Up Networking has not been installed on this machine;
			//    this product will not work until Dial-Up Networking is installed.  
			//    Pleas install Dial-Up Networking before running Accout Setup.
			char buf[ 255 ];
	
			if ( getMsgString( buf, IDS_NO_DUN_NT ) )
				displayErrMsgWnd( buf, MB_OK | MB_ICONASTERISK, NULL );
	
#if 0
			// install Dial-Up Networking
			PROCESS_INFORMATION pi;
			STARTUPINFO         sti;
			UINT                err = ERROR_SUCCESS;
			char				RASphone[ MAX_PATH ];
			
			GetSystemDirectory( RASphone, MAX_PATH );
			strcat( RASphone, "\\rasphone.exe" );

			memset( &sti, 0, sizeof( sti ) );
			sti.cb = sizeof( STARTUPINFO );
			
			// Run the setup application
			if ( CreateProcess( (LPCTSTR)&RASphone, NULL, 
				NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi ) )
			{
				CloseHandle( pi.hThread );

				// Wait for the Dial-Up Networking install process to complete
				WaitForSingleObject( pi.hProcess, INFINITE );
				CloseHandle( pi.hProcess );
			}
#endif
	
		}
		else
		{
			// user need to have administrator premission to install, and ASW won't
			// work if DUN is not installed
			// Err: You do not have Administrator premission on this machine to intall
			//		Dial-Up Networking. Please make sure you have Administrator premission
			//		in order to install Dial-Up Networking first before running Account Setup.
	
			char		buf[ 255 ];
	
			if ( getMsgString( buf, IDS_NO_ADMIN_PREMISSION ) )
				displayErrMsgWnd( buf, MB_OK | MB_ICONASTERISK, NULL );
		}
		return FALSE;
	}
	return TRUE;
}

//********************************************************************************
// CreateDialerShortcut
// Creates a shell shortcut to the PIDL
//********************************************************************************
static short createDialerShortcut( char* szDesktop,     // Desktop path
	LPCSTR 			accountName,  // connectoid/phonebook entry name
	IMalloc*		pMalloc,
	char*			szPath,     // path to PE folder
	char*			strDesc,
	char*			iconPath )      // shortcut description
{
	HRESULT				hres;
	LPITEMIDLIST		pidl;

	char				desktop[ MAX_PATH ];
	DWORD				cbData;
	HKEY				hKey;
	long				res;

	szDesktop[ 0 ] = '\0';

	// gets Desktop folder path from registry for both win95 & winNT40
	// "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"
    if ( ERROR_SUCCESS != RegOpenKeyEx( HKEY_CURRENT_USER,
    		REGSTR_PATH_SPECIAL_FOLDERS,
    		NULL,
    		KEY_QUERY_VALUE,
    		&hKey ) )
		return -3;		// huh?

    cbData = MAX_PATH;
    res = RegQueryValueEx( hKey, "Desktop", NULL, NULL, (LPBYTE)desktop, &cbData );

	RegCloseKey( hKey );

	strcpy( szDesktop, desktop );

	// win95 only
	if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
	{
		// Get a PIDL that points to the dial-up connection
		hres = getDialUpConnectionPIDL( accountName, &pidl );

		if ( FAILED( hres ) )
		{
			// Err: Unable to create shortcut to RNA phone book entry
			char*	buf = (char*)malloc(sizeof( char ) * 255 );
			if ( !buf )
				// Err: Out of Memory
				return -6;		// huh?

			if ( getMsgString( buf, IDS_NO_RNA ) )
			{
				if ( IDOK != displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, NULL ) )
				{
					free( buf );
					return FALSE;
				}
			}

			free( buf );     
			return -1;
		}

		// If the dial-up networking folder is open when we create the RNA
		// entry, then we will have failed to get a PIDL above. The dial-up
		// networking folder itself won't display the icon until it is closed
		// and re-opened. There's nothing we can do but return
		if ( !pidl )
		{
			pMalloc->Release();
			return -2;		// huh?
		}

		// Create a shortcut on the desktop
		char strPath[ MAX_PATH ];
		strcpy( strPath, szDesktop );
		strcat( strPath, "\\" );
		strcat( strPath, strDesc );
		strcat( strPath, ".Lnk" );
		createLink( pidl, strPath, strDesc, iconPath );
		
		// And one in our PE folder
		strcpy( strPath, szPath );
		strcat( strPath, "\\" );
		strcat( strPath, strDesc );
		strcat( strPath, ".Lnk" );
		createLink( pidl, strPath, strDesc, iconPath );		
	}
	else if ( gPlatformOS == VER_PLATFORM_WIN32_NT )
	{ 
		// WinNT40 here
		// make sure the phonebook entry we created still exists

		char* sysDir = (char*)malloc(sizeof( char ) * MAX_PATH );
		if ( !sysDir )
			return -5;		// huh?
		
		char pbPath[ MAX_PATH ];
		GetSystemDirectory( sysDir, MAX_PATH );
		strcpy( pbPath, sysDir );
		strcat( pbPath, "\\ras\\rasphone.pbk" );
		strcat( pbPath, "\0" );
		free( sysDir );
		
		RASENTRYNAME rasEntryName[ MAX_PATH ];
		if ( !rasEntryName )
			return -7;		// huh?

		rasEntryName[ 0 ].dwSize = stRASENTRYNAME;
		DWORD size = stRASENTRYNAME * MAX_PATH;
		DWORD entries;
		
		if ( 0 != RasEnumEntries( NULL, pbPath, rasEntryName, &size, &entries ) )
			return -4;

		BOOL		exists = FALSE;
		DWORD		i = 0;
		
		while ( ( i < entries) && ( !exists ) )
		{
			if ( strcmp( rasEntryName[ i ].szEntryName, accountName ) == 0 )
				exists = TRUE;
			i++;
		}

		// create a shortcut file on desktop
		if ( exists )
		{
			HANDLE hfile = NULL;
			
			// create phonebook entry shortcut file on desktop, overwrites if exists
			SECURITY_ATTRIBUTES		secAttrib;
			memset( &secAttrib, 0, sizeof( SECURITY_ATTRIBUTES ) );
			secAttrib.nLength = sizeof( SECURITY_ATTRIBUTES );
			secAttrib.lpSecurityDescriptor = NULL;
			secAttrib.bInheritHandle = FALSE;

			// construct phonebook entry shortcut file name
			char file[ MAX_PATH ];
			strcpy( file, szDesktop );
			strcat( file, "\\" );
			strcat( file, accountName );
			strcat( file, ".rnk" );
			
			hfile = CreateFile( file, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
				               &secAttrib, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

			if ( hfile == INVALID_HANDLE_VALUE )
				return -9;	// huh? 

			CloseHandle( hfile );
			hfile = NULL;

			// writes shortcut file data in the following format:
			//    [Dial-Up Shortcut]
			//    Entry=stuff
			//    Phonebook=C:\WINNT40\System32\RAS\rasphone.pbk (default system phonebook)

			WritePrivateProfileString( "Dial-Up Shortcut", "Entry", accountName, file );
			WritePrivateProfileString( "Dial-Up Shortcut", "Phonebook", pbPath, file );

			// create the same shortcut file in our PE folder
			strcpy( file, szPath );
			strcat( file, "\\" );
			strcat( file, accountName );
			strcat( file, ".rnk" );

			hfile = CreateFile( file, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
				               &secAttrib, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

			if ( hfile == INVALID_HANDLE_VALUE ) 
				return -10;

			CloseHandle( hfile );
			WritePrivateProfileString( "Dial-Up Shortcut", "Entry", accountName, file );
			WritePrivateProfileString( "Dial-Up Shortcut", "Phonebook", pbPath, file );

		}
		else
			return -8;	// huh?
	}

	return 0;
}


//********************************************************************************
// createProgramItems
// adds 2 icons:
// Dialer - to Dial-Up Networking folder, Desktop & our PE folder
// Navigator - to our PE folder
//********************************************************************************
static short createProgramItems( LPCSTR accountName, LPCSTR iniPath, char* iconFile )
{
	char         szPath[ MAX_PATH ];
	LPITEMIDLIST pidl;
	char         szBuf[ MAX_PATH ];
	
	IMalloc*     pMalloc;

	SHGetMalloc( &pMalloc );
	
	// gets the path to "Programs" folder
	if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
	{
	
		SHGetSpecialFolderLocation( NULL, CSIDL_PROGRAMS, &pidl );
		SHGetPathFromIDList( pidl, szBuf );
		pMalloc->Free( pidl );
	}
	else if ( gPlatformOS == VER_PLATFORM_WIN32_NT )
	{
		// NT4.0: get the "Programs" folder for "All Users"
		HKEY	hKey;
		DWORD	bufsize = sizeof( szBuf );
	
		if( ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
						 "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
						 NULL, 
						 KEY_QUERY_VALUE,  
						 &hKey ) )
		{
			RegQueryValueEx( hKey, "PathName", NULL, NULL, (LPBYTE)szBuf, &bufsize );
			strcat( szBuf, "\\Profiles\\All Users\\Start Menu\\Programs" );
		}
		else
			return -1;
	}
	
	// gets Netscape PE folder here
	char		buf[ 256 ];
	char*		csFolderName;
	csFolderName = (char*)malloc(sizeof( char ) * 256 );
	
	if ( !csFolderName )
		return -4;		// huh?
	
	strcpy( csFolderName, "Netscape Personal Edition" );
	
	// check for custom folder name
	if( ::GetPrivateProfileString(
				"General",
				"InstallFolder",
				(const char*)csFolderName,
				buf, sizeof( buf ),
				iniPath ) )
	strcpy( csFolderName, buf );	
	strcpy( szPath, szBuf );
	strcat( szPath, "\\" );
	strcat( szPath, csFolderName );
	
	free( csFolderName );
	
	// First do Dialer Icon
	// Create a dialer icon shortcut description
	char strDesc[ MAX_PATH ];
	
#ifdef UPGRADE
	if ( ???entryInfo.bUpgrading )
	{
		char*	csTmp = "Dialer";
		strcpy( strDesc, accountName );
		strcat( strDesc, " " );
		strcat( strDesc, csTmp );
	}
	else
	{
	strcpy( strDesc, "Dial" );
	strcat( strDesc, " " );
	strcat( strDesc, accountName );
	}
#else
	strcpy( strDesc, "Dial" );
	strDesc[ strlen( strDesc ) + 1 ] = '\0';
	strDesc[ strlen( strDesc ) ] = (char)32;
	strcat( strDesc, accountName );
#endif
	
	char		szDesktop[ 512 ];
	
	// create dialer shortcut icon on desktop and in PE folder
	int rtn = createDialerShortcut( szDesktop, accountName, pMalloc, szPath, strDesc, iconFile );
	
	if ( rtn != 0 )
		return rtn;
	
	
#ifdef FOLDER_IN_START_MENU
	// Cleanup
	pMalloc->Free( pidl );
	pMalloc->Release();
#endif
	
	::Sleep( 250 );
	
	return 0;
}


//********************************************************************************
// DesktopConfig
// Sets up user's desktop (creates icons and short cuts)
//********************************************************************************
void DesktopConfig( char* accountName, char* iconFileName,
	char* acctsetIniPath)
{
	static char lastIconFile[ _MAX_PATH ] = { '\0' };

	if ( iconFileName != NULL )
	{
		// JS may pass us different file for icon file
		if ( ( iconFileName ) && ( strcmp( iconFileName, lastIconFile ) != 0 ) )
		{
			if ( strcmp( lastIconFile, "" ) != 0 )
			{
				if ( fileExists( lastIconFile ) ) 	// a temp icon file may already existed
					_unlink( lastIconFile );
			}
			
			// check if icon file exists
			if ( fileExists( iconFileName ) )
				strcpy( lastIconFile, iconFileName );
			else
				lastIconFile[0] = '\0';
		}
	}
	
	// remove the RegiServer RAS
	char	regiRAS[ 50 ];
	getMsgString( (char*)regiRAS, IDS_REGGIE_PROGITEM_NAME );
	(*m_lpfnRasDeleteEntry)( NULL, (LPSTR)regiRAS );

	// creates prgram icons in folders and desktop
	int ret = createProgramItems( accountName, acctsetIniPath, lastIconFile );
}


//********************************************************************************
// QuitNavigator
// quits the navigator
//********************************************************************************
void QuitNavigator()
{
	// Bug#112622 - don't broadcast this, java catches it and dies.
	// Instead, find the hidden window and tell _it_ what to do.

	// The strings in this call are hard-coded because the constants needed
	// do not exist and multiple places are doing similar things.  The 
	// values themselves come from winfe's Netscape.cpp.
	HWND hWnd = FindWindowEx( NULL, NULL, "aHiddenFrameClass", "Netscape's Hidden Frame" );
	PostMessage(/*HWND_BROADCAST*/ hWnd, WM_COMMAND, ID_APP_SUPER_EXIT, 0L );
}

//********************************************************************************
// getCurrentProfileDirectory
// gets the current Navigator user profile directory 
//********************************************************************************
extern char* GetCurrentProfileDirectory()
{
	char*		profilePath = NULL;
	int			bufSize = 256;
	char		buf[ 256 ];
	buf[ 0 ] = '\0';

	if ( PREF_OK == PREF_GetCharPref( "profile.directory", buf, &bufSize ) )
	{
		// make sure we append the last '\' in the profile dir path
		strcat( buf, "\\" );	
		trace( "profile.cpp : GetCurrentProfileDirectory : Got the Current User profile = %s", buf );
	}
	else
		trace( "profile.cpp : GetCurrentProfileDirectory : Error in obtaining Current User profile" );

	profilePath = strdup( buf );

	return profilePath;
}
	

//********************************************************************************
// GetCurrentProfileName
// gets the current Navigator user profile name 
//********************************************************************************
extern char* GetCurrentProfileName()
{
	char*			profileName = NULL;
	int				bufSize = 256;
	char			buf[ 256 ];
	buf[ 0 ] = '\0';

	if ( PREF_OK == PREF_GetCharPref( "profile.name", buf, &bufSize ) )
		trace( "profile.cpp : GetCurrentProfileDirectory : Got the Current profile name = %s", buf );
	else
		trace( "profile.cpp : GetCurrentProfileName : Error in obtaining Current User profile" );

	profileName = strdup( buf );

	return profileName;
}


//********************************************************************************
// SetCurrentProfileName
// changes the current Navigator user profile name to a different name
//********************************************************************************
void SetCurrentProfileName( char* profileName )
{
	assert( profileName );

	if ( NULL == profileName )	// abort if string allocation fails
		return;
	
	if ( PREF_ERROR == PREF_SetDefaultCharPref( "profile.name", profileName ) )
		trace( "profile.cpp : SetCurrentProfileName : Error in setting Current User profile name." );
	else
	{
		trace( "profile.cpp : SetCurrentProfileName : Current User profile is set to = %s", profileName );

		if ( PREF_ERROR == PREF_SetDefaultBoolPref( "profile.temporary", FALSE ) ) 
			trace( "profile.cpp : SetCurrentProfileName : Error in setting Temporary flag to false." );
		else
			trace( "profile.cpp : SetCurrentProfileName : Made the profile to be permanent." );
	}

}

// The format of the entry is
//  LocationX=Y,"name","outside-line-local","outside-line-long-D","area-code",1,0,0,1,"",tone=0,"call-waiting-string"
typedef struct tapiLineStruct
{
	int		nIndex;
	char		csLocationName[ 60 ];
	char		csOutsideLocal[ 20 ];
	char		csOutsideLongDistance[ 20 ];
	char		csAreaCode[ 20 ];
	int		nCountryCode;
	int		nCreditCard;
	int		nDunnoB;
	int		nDunnoC;
	char		csDialAsLongDistance[ 10 ];
	int		nPulseDialing;
	char		csCallWaiting[ 20 ];
} TAPILINE;

//********************************************************************************
// readNextInt
// Pull an int off the front of the string and return the new string ptr
//********************************************************************************
char* readNextInt( char* pInString, int* pInt )
{
	char*		pStr;
	char		buf[ 32 ];
	
	if ( !pInString )
	{
		*pInt = 0;
		return NULL;
	}
	
	// copy the string over.  This strchr should be smarter    
	pStr = strchr( pInString, ',' );
	if ( !pStr )
	{
		*pInt = 0;
		return NULL;
	}
	
	// convert the string            
	strncpy( buf, pInString, pStr - pInString );
	buf[ pStr - pInString ] = '\0';
	*pInt = atoi( buf );
	
	// return the part after the int
	return ( pStr + 1 );	
}

//********************************************************************************
// readNextString
// Pull a string from the front of the incoming string and return
//   the first character after the string
//********************************************************************************
char* readNextString( char* pInString, char* csString )
{
	csString[ 0 ] = '\0';
	int		i = 0, x = 0;
	BOOL	copy = FALSE;
	char	newpInString[ MAX_PATH ];
	
	if ( !pInString )
	{
		csString = "";
		return NULL;
	}
	
	// skip over first quote
	if ( pInString[ i ] == '\"' )
		i++;
	
	// copy over stuff by hand line a moron
	while ( pInString[ i ] != '\"' )
	{
		//strcat(csString, (char *)pInString[i]);
		csString[ x ] = pInString[ i ];
		i++;
		x++;
		copy = TRUE;
	}
	
	if ( copy )
		csString[ x ] = '\0';
	
	
	// skip over the closing quote
	if( pInString[ i ] == '\"' )
		i++;
	
	// skip over the comma at the end
	if( pInString[ i ] == ',' )
		i++;
	
	newpInString[ 0 ]='\0';
	x = 0;

	for ( unsigned short j = i; j < strlen( pInString ); j++ )
	{
		//strcat(newpInString, (char *)pInString[j]);
		newpInString[ x ] = pInString[ j ];
		x++;
	}
	
	newpInString[ x ]='\0';
	strcpy( pInString, newpInString );
	
	return pInString;
}

//********************************************************************************
// readTapiLine
// Read a line out of the telephon.ini file and fill the stuff in
//********************************************************************************
void readTapiLine( char* lpszFile, int nLineNumber, TAPILINE* line ) 
{
	char		buf[ 256 ];
	char		pLocation[ 128 ];
	sprintf( pLocation, "Location%d", nLineNumber );
	::GetPrivateProfileString( "Locations", pLocation, "", buf, sizeof( buf ), lpszFile );
	
	char*		pString = buf;
	pString = readNextInt( pString, &line->nIndex );
	pString = readNextString( pString, (char *)&line->csLocationName );
	pString = readNextString( pString, (char *)&line->csOutsideLocal );
	pString = readNextString( pString, (char *)&line->csOutsideLongDistance );
	pString = readNextString( pString, (char *)&line->csAreaCode );
	pString = readNextInt( pString, &line->nCountryCode );
	pString = readNextInt( pString, &line->nCreditCard );
	pString = readNextInt( pString, &line->nDunnoB );
	pString = readNextInt( pString, &line->nDunnoC );
	pString = readNextString( pString, (char *)&line->csDialAsLongDistance );
	pString = readNextInt( pString, &line->nPulseDialing );
	pString = readNextString( pString, (char *)&line->csCallWaiting );	
}



//********************************************************************************
// writeTapiLine
// Given a tapiLine structure write it out to telephon.ini
//********************************************************************************
void writeTapiLine( char* lpszFile, int nLineNumber, TAPILINE* line ) 
{
    char		buffer[ 256 ];
    sprintf( buffer, "%d,\"%s\",\"%s\",\"%s\",\"%s\",%d,%d,%d,%d,\"%s\",%d,\"%s\"",
        line->nIndex,
        (const char*) line->csLocationName,
        (const char*) line->csOutsideLocal,
        (const char*) line->csOutsideLongDistance,
        (const char*) line->csAreaCode,
        line->nCountryCode,
        line->nCreditCard,
        line->nDunnoB,
        line->nDunnoC,
        (const char*) line->csDialAsLongDistance,
        line->nPulseDialing,
        (const char*) line->csCallWaiting );

    char			pLocation[ 32 ];
    sprintf( pLocation, "Location%d", nLineNumber );
    ::WritePrivateProfileString( "Locations", pLocation, buffer, lpszFile );
}

//********************************************************************************
// SetLocationInfo
// sets the location info for win95 dialers
// The format of the entry is
//  LocationX=Y,"name","outside-line-local","outside-line-long-D","area-code",1,0,0,1,"",tone=0,"call-waiting-string"
//********************************************************************************
BOOL SetLocationInfo( ACCOUNTPARAMS account, LOCATIONPARAMS location )
{
	// first read information from telephon.ini
	char		buf[ 256 ];
	
	// get windows directory
	char		lpszDir[ MAX_PATH ];
	if ( GetWindowsDirectory( lpszDir, MAX_PATH ) == 0 )
		return FALSE; // huh?
	
	strcat( lpszDir, "\\telephon.ini" );
	
	// now we build new line information based on the old one and some info
	// see if there were any locations to begin with
	::GetPrivateProfileString( "Locations", "CurrentLocation", "", buf, sizeof( buf ), lpszDir );
	if( buf[ 0 ] == '\0' )
	{
		// build the string
		TAPILINE		line;
		line.nIndex = 0;
		strcpy( line.csLocationName, "Default Location" );
		strcpy( line.csOutsideLocal, location.OutsideLineAccess );
		strcpy( line.csOutsideLongDistance, location.OutsideLineAccess );
		strcpy( line.csAreaCode, location.UserAreaCode );
		line.nCountryCode = location.UserCountryCode;
		line.nCreditCard = 0;
		line.nDunnoB = 0;
		line.nDunnoC = 1;
	
		if ( location.DialAsLongDistance == TRUE )
			strcpy( line.csDialAsLongDistance, "528" );
		else
			strcpy( line.csDialAsLongDistance, "" );

		line.nPulseDialing = ( location.DialType == 0 ? 1 : 0 );
		if( location.DisableCallWaiting )
			strcpy( line.csCallWaiting, location.DisableCallWaitingCode );
		else
			strcpy( line.csCallWaiting, "" );
	
		writeTapiLine( lpszDir, 0, &line );
	
		// need to create a whole location section
		::WritePrivateProfileString( "Locations", "CurrentLocation", "0,0", lpszDir );
		::WritePrivateProfileString( "Locations", "Locations", "1,1", lpszDir );
		::WritePrivateProfileString( "Locations", "Inited", "1", lpszDir );
	}
	else
	{
	
		int nId, nCount;
		sscanf( buf, "%d,%d", &nId, &nCount );
	
		// read the line
		TAPILINE		line;                                
		readTapiLine( lpszDir, nId, &line );
	
		strcpy( line.csOutsideLocal, location.OutsideLineAccess );
		strcpy( line.csOutsideLongDistance, location.OutsideLineAccess );
		if ( location.DisableCallWaiting )
			strcpy( line.csCallWaiting, location.DisableCallWaitingCode );
		else
			strcpy( line.csCallWaiting, "" );
	
		line.nPulseDialing = ( location.DialType == 0 ? 1 : 0 );
	
		if ( strcmp( location.UserAreaCode, "" ) != 0 )
			strcpy( line.csAreaCode, location.UserAreaCode );
	
		if ( location.DialAsLongDistance == TRUE )
			strcpy( line.csDialAsLongDistance, "528" );
		else
			strcpy( line.csDialAsLongDistance, "" );
	
		// write the line back out
		writeTapiLine( lpszDir, nId, &line );
	}
	
	return TRUE;
}
	

//********************************************************************************
// SetLocationInfoNT
// sets the location info for winNT dialers
//********************************************************************************
BOOL SetLocationInfoNT( ACCOUNTPARAMS account, LOCATIONPARAMS location )
{
	LINEINITIALIZEEXPARAMS  m_LineInitExParams;
	HLINEAPP                m_LineApp;
	DWORD                   dwNumDevs;
	LINETRANSLATECAPS       m_LineTranslateCaps;
	
	DWORD					dwApiVersion = 0x00020000;
	
	
	// Initialize TAPI. in order to get current location ID
	m_LineInitExParams.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;
	m_LineInitExParams.dwTotalSize = sizeof( LINEINITIALIZEEXPARAMS );
	m_LineInitExParams.dwNeededSize = sizeof( LINEINITIALIZEEXPARAMS );
	
	
	if ( lineInitialize( &m_LineApp, gDLL, lineCallbackFuncNT, NULL, &dwNumDevs) != 0 )
	{
		char		buf[ 255 ];
		if ( getMsgString( buf, IDS_NO_TAPI ) )
			displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, NULL );
		return FALSE;
	}
	
	m_LineTranslateCaps.dwTotalSize = sizeof( LINETRANSLATECAPS );
	m_LineTranslateCaps.dwNeededSize = sizeof( LINETRANSLATECAPS );
	if ( lineGetTranslateCaps( m_LineApp, dwApiVersion, &m_LineTranslateCaps ) != 0 )
		return FALSE;
	
	
	//m_LineTranslateCaps.dwCurrentLocationID
	
	// gets the location info from registry
	HKEY					hKey;
	char*	keyPath = (char*)malloc( sizeof( char ) * 512 );
	
	assert( keyPath );
	if ( !keyPath )
		return FALSE;
	strcpy( keyPath, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\locations" );
	
	// finds the user profile location in registry
	if( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, keyPath, NULL, KEY_ALL_ACCESS, &hKey ) )
		return FALSE;
	
	DWORD					subKeys;
	DWORD					maxSubKeyLen;
	DWORD					maxClassLen;
	DWORD					values;
	DWORD					maxValueNameLen;
	DWORD					maxValueLen;
	DWORD					securityDescriptor;
	FILETIME				lastWriteTime;
	
	// get some information about this reg key 
	if ( ERROR_SUCCESS != RegQueryInfoKey( hKey, NULL, NULL, NULL, &subKeys, &maxSubKeyLen, &maxClassLen, &values, &maxValueNameLen, &maxValueLen, &securityDescriptor, &lastWriteTime ) )
		return FALSE;
	
	
	// now loop through the location keys to find the one that matches current location ID
	if ( subKeys > 0 )
	{
	
		DWORD		subkeyNameSize = maxSubKeyLen + 1;
		char		subkeyName[ 20 ];
	
		for ( DWORD index = 0; index < subKeys; index++ )
		{
			// gets a location key name
			if ( ERROR_SUCCESS != RegEnumKey( hKey, index, subkeyName, subkeyNameSize ) ) 
				return FALSE;
	
			// try open location key
			char		newSubkeyPath[ 260 ];
			HKEY		hkeyNewSubkey;
	
			strcpy( (char*)newSubkeyPath, keyPath );
			strcat( (char*)newSubkeyPath, "\\" ); 
			strcat( (char*)newSubkeyPath, subkeyName );
	
			if ( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, (char*)newSubkeyPath, NULL, KEY_ALL_ACCESS, &hkeyNewSubkey ) )
				return FALSE;
	
			DWORD		valbuf;
			DWORD		type = REG_SZ;
			DWORD		bufsize = 20;
	
			// get location key's ID value
			if ( ERROR_SUCCESS != RegQueryValueEx( hkeyNewSubkey, "ID", NULL, &type, (LPBYTE)&valbuf, &bufsize ) )
				return FALSE;
	
			// if it matches the default location ID
			if ( valbuf == m_LineTranslateCaps.dwCurrentLocationID )
			{
				// we got the location we want, now change the pulse/tone flag
				DWORD 	flagsVal;
				if ( location.DialType == 0 )
					if ( location.DisableCallWaiting )
						flagsVal = 4;
					else
						flagsVal = 0;
				else if ( location.DisableCallWaiting )
					flagsVal = 5;
				else
					flagsVal = 1;
	
				if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "Flags", NULL, type, (LPBYTE)&flagsVal, bufsize ) )
					return FALSE;
	
				// sets the OutsideAccess
				if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "OutsideAccess", NULL, REG_SZ, (LPBYTE)&location.OutsideLineAccess, strlen(location.OutsideLineAccess ) + 1 ) )
					return FALSE;
	
				if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "LongDistanceAccess", NULL, REG_SZ, (LPBYTE)&location.OutsideLineAccess, strlen(location.OutsideLineAccess ) + 1 ) )
					return FALSE;
	
				// sets call waiting
				char*	callwaiting;
	
				if( location.DisableCallWaiting )
					callwaiting = location.DisableCallWaitingCode;
				else
					callwaiting ="";
	
				if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "DisableCallWaiting", NULL, REG_SZ, (LPBYTE)callwaiting, strlen( callwaiting ) + 1 ) )
					return FALSE;
	
	
				// sets user's area code
				if( strcmp( location.UserAreaCode, "" ) != 0 )
				{
					if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "AreaCode", NULL, REG_SZ, (LPBYTE)location.UserAreaCode, strlen( location.UserAreaCode ) + 1 ) )
						return FALSE;
				}
				else
				{
					// check if we're international, and force a default area code, so that we don't get an error creating a dialer
					if ( account.IntlMode )
					{
						char*	code = "415";
						if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "AreaCode", NULL, REG_SZ, (LPBYTE)code, strlen( code ) + 1 ) )
							return FALSE;
					}
				}
	
			RegCloseKey( hkeyNewSubkey );
			break;
		}
	
		RegCloseKey( hkeyNewSubkey );
		}
	}
	
	RegCloseKey( hKey );
	
	return TRUE;
}

//********************************************************************************
// native method:
// CheckEnvironment
// checks for DUN, RAS function loading correctly
//********************************************************************************
BOOL CheckEnvironment()
{
	// try loading RAS routines in RAS dlls
	// if fails return FALSE
	switch ( gPlatformOS )
	{
		case VER_PLATFORM_WIN32_NT:   // NT
			//check if it's WinNT40 first
			if ( !LoadRasFunctionsNT( "RASAPI32.DLL" ) )
			{
		
				// Err: Unable to dynamically load extended RAS functions!                                
				char*	buf = (char*)malloc( sizeof( char ) * 255 );
				if ( buf )
				{
					if ( getMsgString( buf, IDS_NO_RAS_FUNCTIONS ) )
						displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, NULL );
					free( buf );
				}
		
				return FALSE;
			}
			break;
	
		case VER_PLATFORM_WIN32_WINDOWS:    // defaults to win95	
			if ( !LoadRasFunctions( "RASAPI32.DLL" ) && !LoadRasFunctions( "RNAPH.DLL" ) )
			{
				// Err: Unable to dynamically load extended RAS functions!
				char* buf = (char*)malloc( sizeof( char ) * 255 );
				if ( buf )
				{
					if ( getMsgString( buf, IDS_NO_RAS_FUNCTIONS ) )
						displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, NULL);
					free( buf );
				}
	
				return FALSE;
			}
			break;
	}
	
	// Check to make sure Dial-Up Networking is installed.
	//   It may be uninstalled by user.
	// return FALSE if Dialup Networking is not installed
	switch ( gPlatformOS )
	{
		case VER_PLATFORM_WIN32_NT:
			if ( FALSE == CheckDUN_NT() )
			{
				char buf[ 255 ];
				if ( getMsgString( (char*)buf, IDS_NO_DUN_INSTALLED ) )
					displayErrMsgWnd( (char*)buf, MB_OK | MB_ICONEXCLAMATION, NULL );
				return FALSE;
			}
			break;
		default:
			if ( FALSE == CheckDUN() )
			{
				char buf[ 255 ];
				if ( getMsgString( (char*)buf, IDS_NO_DUN_INSTALLED ) )
					displayErrMsgWnd( (char*)buf, MB_OK | MB_ICONEXCLAMATION, NULL );
				return FALSE;
			}
			break;
	}
	
	// for win95 only:
	// Check to see if DNS is already configured for a LAN connection.
	// If so warn the user that this may cause conflicts, and continue.
	if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
		CheckDNS();
	
	return TRUE;
}


