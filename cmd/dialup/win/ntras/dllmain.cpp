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
#include <windows.h>
#include "resource.h"

BOOL IsDUNSetup(void);
BOOL IsAdmin(void);
HINSTANCE hDLLinstance=NULL;

//************************************************************************
// IsNetworkSetupNT()
//
//	return 1, if we're Win95
//  return 2, if NT4.0 and DUN is setup properly
//	return 3, if NT4.0 w/o DUN, or NT3.51
//
//************************************************************************
__declspec(dllexport)
int WINAPI
IsNetworkSetupNT(HWND hInstallerWnd)
{
	int platformOS;		// platform OS  (95 or NT40)
	int rtnVal;

	OSVERSIONINFO *lpOsVersionInfo = (OSVERSIONINFO *)malloc(sizeof(OSVERSIONINFO));
	lpOsVersionInfo->dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (GetVersionEx(lpOsVersionInfo)) {
		platformOS = (int)lpOsVersionInfo->dwPlatformId;
	} else {
		free(lpOsVersionInfo);
		return -1;
	}

	switch (platformOS)  {

		case VER_PLATFORM_WIN32_WINDOWS:
				rtnVal = 1;
				break;

		case VER_PLATFORM_WIN32_NT:

				// if we are running in WinNT, make sure we are in NT4.0 not in NT3.x
				
				if (lpOsVersionInfo->dwMajorVersion < 4) {

					// running in NT3.x !!!  need to warn users.
					// "Accout Setup does not work in NT3.x"
					char buf[255];
					int ret = LoadString(hDLLinstance, OS_NT351, buf, 255);
					MessageBox(hInstallerWnd, (char *)&buf, NULL, MB_OK | MB_ICONEXCLAMATION);

				} else {

					// we're in NT4.x or later version
					// check if Network is setup already
					BOOL setup = IsDUNSetup();
					if (!setup) {

						BOOL isAdmin = IsAdmin();

						if (isAdmin) {

							char buf[255];
							int ret = LoadString(hDLLinstance, NO_DUN, buf, 255);
							MessageBox(hInstallerWnd, (char *)&buf, NULL, MB_OK | MB_ICONEXCLAMATION);

						} else {

							char buf[255];
							int ret = LoadString(hDLLinstance, NO_ADMIN, buf, 255);
							MessageBox(hInstallerWnd, (char *)&buf, NULL, MB_OK | MB_ICONEXCLAMATION);

						}
					}
				}

				rtnVal = 2;
				break;
	}
	
	free(lpOsVersionInfo);

	return rtnVal;

}



//********************************************************************************
//
// Check if user has proper Network setup for PE
//
//********************************************************************************

/*
__declspec(dllexport)
BOOL WINAPI
IsNetworkSetupNT()
{

	BOOL rtn;
	BOOL isAdmin;
	BOOL setup;

	// check if Network is setup already
	rtn = IsDUNSetup();

	// check for user's logon premission, if Network
	// is not setup properly.
	if (rtn == FALSE) {
		isAdmin = IsAdmin();

		if (isAdmin)
			setup = rtn;
		else
			setup = -999;
	
	} else {
		
		setup = rtn;
	}
	
	return setup;
}
*/


//********************************************************************************
//
// check if a user is an Administrator on NT40
//
//********************************************************************************
BOOL IsAdmin(void)
{
	HANDLE hAccessToken;
    UCHAR InfoBuffer[1024];
    PTOKEN_GROUPS ptgGroups = (PTOKEN_GROUPS)InfoBuffer;
    DWORD dwInfoBufferSize;
    PSID psidAdministrators;
    SID_IDENTIFIER_AUTHORITY siaNtAuthority = SECURITY_NT_AUTHORITY;
    UINT x;
    BOOL bSuccess;

	if(!OpenProcessToken(GetCurrentProcess(),TOKEN_READ,&hAccessToken))
		return(FALSE);
 
	bSuccess = GetTokenInformation(hAccessToken,TokenGroups,InfoBuffer,
									1024, &dwInfoBufferSize);

	CloseHandle(hAccessToken);

	if( !bSuccess )
		return FALSE;
 
	if(!AllocateAndInitializeSid(&siaNtAuthority, 2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&psidAdministrators))
		return FALSE;
 
   // assume that we don't find the admin SID.
	bSuccess = FALSE;
 
	for(x=0;x<ptgGroups->GroupCount;x++) {
		if( EqualSid(psidAdministrators, ptgGroups->Groups[x].Sid) ) {
			bSuccess = TRUE;
			break;
         }
	}

	FreeSid(&psidAdministrators);
	return bSuccess;
}


//********************************************************************************
//
// check if Dial-Up Networking is installed
//
//********************************************************************************
BOOL IsDUNSetup(void) {

    BOOL bInstall = FALSE;
	BOOL bAdmin = FALSE;
    HKEY hKey;
    LONG rtn;
    char szBuf[MAX_PATH];
	char *buf = NULL;


    // Let's see if its installed
	// "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\RemoteAccess"
    if (rtn = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\RemoteAccess", 0, KEY_QUERY_VALUE, &hKey))
		if (rtn != ERROR_SUCCESS)
			return (FALSE);

    // the key is there, was it actually installed though...
    // look for some RAS keys

	szBuf[0] = '\0';
	if(!bInstall) {
        DWORD cbData = sizeof(szBuf);
        rtn = RegQueryValueEx(hKey, "DisplayName", NULL, NULL, (LPBYTE)szBuf, &cbData);
        if(strlen(szBuf) == 0)
	        return (FALSE);

		RegCloseKey(hKey);

		// how about autodial manager....
		// "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\RasAuto"
		if(ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\RasAuto", 0, KEY_QUERY_VALUE, &hKey))
	        return (FALSE);

	    RegCloseKey(hKey);

    }

	return (TRUE);
}


//************************************************************************
// DllEntryPoint
//************************************************************************
HINSTANCE g_hDllInstance = NULL;

BOOL WINAPI 
DllMain( HINSTANCE  hinstDLL,           // handle of DLL module 
                DWORD  fdwReason,               // reason for calling function 
                LPVOID  lpvReserved)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_PROCESS_DETACH:
		{			
			hDLLinstance = hinstDLL;       // keeps DLL instance
			break;
		}
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
}
