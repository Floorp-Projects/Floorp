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
// Ckdnsdun.cpp
// 
// Routines for checking Dial-Up Networking & DNS
//
// Revision History:
// Date        Author            Reason
// ----------------------------------------------------------------------------
//             xxxxxxxxxxxxxx    Define PIs
///////////////////////////////////////////////////////////////////////////////

// resource include
#ifdef WIN32 // **************************** WIN32 *****************************
#include "resource.h"
#else       // **************************** WIN16 *****************************
#include "asw16res.h"
#endif // !WIN32

#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "errmsg.h"

extern BOOL getMsgString(char *buf, UINT uID);

//********************************************************************************
// 
// CheckDNS
//
// for Win95
// If user has DNS enabled, when setting up an account, we need to warn them
// that there might be problems.
//********************************************************************************
void CheckDNS()
{

    char buf[256];
    HKEY hKey;
    DWORD cbData;
    LONG res;

	// open the key if registry
    if(ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                     "System\\CurrentControlSet\\Services\\VxD\\MSTCP", 
                                     NULL, 
                                     KEY_ALL_ACCESS   , 
                                     &hKey))
        return;

    cbData = sizeof(buf);
    res = RegQueryValueEx(hKey, "EnableDNS", NULL, NULL, (LPBYTE)buf, &cbData);

    RegCloseKey(hKey);

    // if DNS is enabled we need to warn the user
    if(buf[0] == '1') {

		BOOL correctWinsockVersion = FALSE;

		// check for user's winsock version first, see if it's winsock2
		WORD wVersionRequested;  
		WSADATA wsaData; 
		int err; 
		wVersionRequested = MAKEWORD(2, 2); 
 
		err = WSAStartup(wVersionRequested, &wsaData); 
 
		if (err != 0) {
			
			// user doesn't have winsock2, so check their winsock's date
			char winDir[MAX_PATH];
			UINT uSize = MAX_PATH;
			char winsockFile[MAX_PATH];

			winDir[0]='\0';
			winsockFile[0]='\0';
			GetWindowsDirectory((char *)&winDir, uSize);
			strcpy(winsockFile, winDir);
			strcat(winsockFile, "\\winsock.dll");

			HANDLE hfile = CreateFile((char *)&winsockFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

			if (hfile != INVALID_HANDLE_VALUE) {  // openned file is valid
				
				FILETIME lastWriteTime;
				BOOL rtnval = GetFileTime(hfile, NULL, NULL, &lastWriteTime);

				SYSTEMTIME systemTime;
				rtnval = FileTimeToSystemTime(&lastWriteTime, &systemTime);

				if ((systemTime.wYear >= 1996) && (systemTime.wMonth >= 8) && (systemTime.wDay >= 24))
					correctWinsockVersion = TRUE;

				CloseHandle(hfile);
			}
		} else {
			correctWinsockVersion = TRUE;
		}


		if (!correctWinsockVersion) {
			// Err: Your system is configured for another Domain Name System (DNS) server.
			//    You might need to edit your DNS configuration.  Check the User's Guide
			//    for more information.
			char buf[255];
			if (getMsgString(buf, IDS_DNS_ALREADY)) 
				DisplayErrMsg(buf, MB_OK | MB_ICONASTERISK);
		}
    }

	return;
}


static const char c_szRNA[] = "rundll.exe setupx.dll,InstallHinfSection RNA 0 rna.inf";


//********************************************************************************
//
// CheckDUN
//
// for Win95
// If user doesn't have Dial-Up Networking installed, they will have problem
// setting up an account.
//********************************************************************************
BOOL CheckDUN()
{
    BOOL bInstall = FALSE;
    HKEY hKey;
    LONG res;
    char szBuf[MAX_PATH];

    // Let's see if its installed
    if(ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                     "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\OptionalComponents\\RNA", 
                                     NULL, 
                                     KEY_QUERY_VALUE, 
                                     &hKey))
        bInstall = TRUE;

    // the key is there, was it actually installed though...
    if(!bInstall) {
        DWORD cbData = sizeof(szBuf);
        res = RegQueryValueEx(hKey, "Installed", NULL, NULL, (LPBYTE)szBuf, &cbData);
        if(szBuf[0] != '1' && szBuf[0] != 'Y' && szBuf[0] != 'y')
            bInstall = TRUE;
    }

    // make sure a random file from the installation exists so that we
    //   know the user actually installed instead of just skipping over
    //   the install step
    if(!bInstall) {
        if(GetSystemDirectory(szBuf, sizeof(szBuf))) {

            // create a name of one of the files
            strcat(szBuf, "\\pppmac.vxd");

            // let's see if that file exists
            struct _stat stat_struct;
            if(_stat(szBuf, &stat_struct) != 0)
                bInstall = TRUE;

        }

    }

    // if no Dial-Up Networking installed install it now
    if(bInstall) {

      // let the user not install it or exit
      //
      // Err: Dial-Up Networking has not been installed on this machine;
      //    this product will not work until Dial-Up Networking is installed.  
      //    Would you like to install Dial-Up Networking now?

      char *buf = (char *)malloc(sizeof(char) * 255);
      if (!buf)
         // Err: Out of Memory
         return (FALSE);

      if (getMsgString(buf, IDS_NO_DUN)) {
         if (IDOK != DisplayErrMsg(buf, MB_OKCANCEL | MB_ICONASTERISK)) {
            free(buf);
            return (FALSE);
         }
      }
      free(buf);     


      // install Dial-Up Networking
        PROCESS_INFORMATION pi;
        STARTUPINFO         sti;
        UINT                err = ERROR_SUCCESS;

        memset(&sti,0,sizeof(sti));
        sti.cb = sizeof(STARTUPINFO);

        // Run the setup application
        if(CreateProcess(NULL, (LPSTR)c_szRNA, 
                NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi)) {

            CloseHandle(pi.hThread);

            // Wait for the modem wizard process to complete
            WaitForSingleObject(pi.hProcess,INFINITE);
            CloseHandle(pi.hProcess);
        }

    }

    RegCloseKey(hKey);
    return(TRUE);

}


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
// CheckDUN_NT
//
// for WinNT40
// If user doesn't have Dial-Up Networking installed, they will have problem
// setting up an account.
//********************************************************************************
BOOL CheckDUN_NT()
{
    BOOL bInstall = FALSE;
	BOOL bAdmin = FALSE;
    HKEY hKey;
    LONG res;
    char szBuf[MAX_PATH];
	char *buf = NULL;


    // Let's see if its installed
	// "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\RemoteAccess"
    if(ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\RemoteAccess", NULL,  KEY_QUERY_VALUE, &hKey))
        bInstall = TRUE;

    // the key is there, was it actually installed though...
    // look for some RAS keys

	szBuf[0] = '\0';
	if(!bInstall) {
        DWORD cbData = sizeof(szBuf);
        res = RegQueryValueEx(hKey, "DisplayName", NULL, NULL, (LPBYTE)szBuf, &cbData);
        if(strlen(szBuf) == 0)
            bInstall = TRUE;

		RegCloseKey(hKey);

		// how about autodial manager....
		// "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\RasAuto"
		if(ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\RasAuto", NULL, KEY_QUERY_VALUE, &hKey))
			bInstall = TRUE;

	    RegCloseKey(hKey);

    }


    // if no Dial-Up Networking installed, warn the users depending on
	// their premissions and return FALSE
    if(bInstall) {

		bAdmin = IsAdmin();

		if (bAdmin) {
			//
			// Err: Dial-Up Networking has not been installed on this machine;
			//    this product will not work until Dial-Up Networking is installed.  
			//    Pleas install Dial-Up Networking before running Accout Setup.

			char buf[255];

			if (getMsgString(buf, IDS_NO_DUN_NT)) {
				DisplayErrMsg(buf, MB_OK | MB_ICONASTERISK);
			}

#if 0
			// install Dial-Up Networking
			PROCESS_INFORMATION pi;
			STARTUPINFO         sti;
			UINT                err = ERROR_SUCCESS;
			char RASphone[MAX_PATH];

			GetSystemDirectory(RASphone, MAX_PATH);
			strcat(RASphone, "\\rasphone.exe");
						

			memset(&sti,0,sizeof(sti));
			sti.cb = sizeof(STARTUPINFO);

			// Run the setup application
			if(CreateProcess((LPCTSTR)&RASphone, NULL, 
					NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi)) {

				CloseHandle(pi.hThread);

				// Wait for the Dial-Up Networking install process to complete
				WaitForSingleObject(pi.hProcess,INFINITE);
				CloseHandle(pi.hProcess);
			}
#endif
			
		} else {

			// user need to have administrator premission to install, and ASW won't
			// work if DUN is not installed
			//
			// Err: You do not have Administrator premission on this machine to intall
			//		Dial-Up Networking. Please make sure you have Administrator premission
			//		in order to install Dial-Up Networking first before running Account Setup.

			char buf[255];

			if (getMsgString(buf, IDS_NO_ADMIN_PREMISSION))
				DisplayErrMsg(buf, MB_OK | MB_ICONASTERISK);
		}

		return(FALSE);

	}

    return(TRUE);

}
