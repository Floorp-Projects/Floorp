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
// Dialer.cpp
// This file contains Modem Dialing Native APIs, for Win32 (NT & 95 uses
// MS RAS APIs) & Win16 (uses Shiva's dialer & modem routines) 
//
// Revision History:
// Date        Author            Reason
// ----------------------------------------------------------------------------
// 02/27/97    xxxxxxxxxxxxxx    Code cleanup
// 01/26/97    xxxxxxxxxxxxxx    Define Native API for win3.x
//             xxxxxxxxxxxxxx    Define Native API for win95 & winNT
///////////////////////////////////////////////////////////////////////////////

#include <npapi.h>
#include "plugin.h"

// resource include
#ifdef WIN32 // **************************** WIN32 *****************************
#include "resource.h"
#else        // **************************** WIN16 *****************************
#include "asw16res.h"
#include "helper16.h"
#endif // !WIN32
 

#if __cplusplus
extern "C"
{
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

#ifdef WIN32
#include <raserror.h>      // WIN32 uses MS RAS
#else
#include <dos.h>
#include <shivaerr.h>      // WIN16 uses Shiva RAS
#endif
}
#endif

#ifdef WIN32
//**************************** WIN32 Includes ****************************
#include <tapi.h>
#include <winbase.h>       // windows include
//**************************** WIN32 Decls ****************************
#else
//**************************** WIN16 Includes ****************************
#include <windows.h>       // windows include
#if __cplusplus
extern "C"
{
#include <shivaras.h>      // Shiva RAS APIs
}
#endif

//**************************** WIN16 Decls ****************************
#define REGGI_SERVER_NAME			 "Registration Server"	// reggie name
#define DEF_AUTODISCONNECT_PERIOD 10							// default autodisconnect idle period

// Shiva constants..
#define SHIVA_INI_DIALER_SECTION	 "ConnectW Config"	// Shiva INI section name
#define SHIVA_INI_FILENAME_KEY	 "preferred file"	// Shiva INI section name
#define SHIVA_CONNFILE_EXT			 ".sr"				// Shiva connection file extension
#define SHIVA_ALL_CONNFILES		 "*.sr"				// all Shiva connection files
#endif  // !WIN32

#include "errmsg.h"

// java include 
#include "netscape_npasw_SetupPlugin.h"
#include "java_lang_String.h"

#ifndef WIN32
extern BOOL g_bExistingPath;	// TRUE if account creating is an existing account
#endif

char *ReggieScript = NULL;

HANDLE hRasMon = NULL;	//process handle to RasMon on WinNT

#ifdef WIN32
extern BOOL SetLocationInfo(ACCOUNTPARAMS account, LOCATIONPARAMS location);
extern BOOL SetLocationInfoNT(ACCOUNTPARAMS account, LOCATIONPARAMS location);
#endif
extern BOOL ConvertPassword(LPCSTR lpszPassword, LPSTR lpBuf);
extern BOOL getMsgString(char *buf, UINT uID);
extern const char *GetStringPlatformChars(JRIEnv *env, struct java_lang_String *string);
extern int DisplayErrMsgWnd(char *text, int style, HWND hwnd);
extern void GetProfileDirectory(char *profilePath);

enum CallState {StateIdle, StateConnecting, StateConnected, StateDisconnecting};
CallState m_callState;

// keeps the current connection info
RASDIALPARAMS dialParams;

// determine the dialer we're configuring/dialing is a registration server dialder
BOOL RegiMode=FALSE;

// keeps the total number of dialing count
int Dial;

// handle to the current ras connection
HRASCONN hRasConn;

// current connection's ras state
RASCONNSTATE RASstate;

// The number of times we try to dial
#define NUM_ATTEMPTS    3
#define IDDISCONNECTED  31


// handle to our connection status window
HWND hwndStatus    = NULL;
BOOL setStatusHwnd = FALSE;
BOOL NotCanceled   = TRUE; // assume connection will be there unless user cancels
BOOL LineDrop      = FALSE;
BOOL deviceErr	   = FALSE;// assume no hardware err
HWND hwndNavigator = NULL;  

// str of sr file's description line
char acctDescription[256];

	 

void SafeEndDialog()
{
	if(hwndStatus) {
		EndDialog(hwndStatus, TRUE);
		hwndStatus = NULL;
	}
}

void SafeSetWindowText(int iField, const char *pText)
{
	if(hwndStatus) {
		HWND hField = GetDlgItem(hwndStatus, iField);
		if(hField)  {
			SetWindowText(hField, pText);
		}
	}
}

BOOL CALLBACK
statusDlgcallback(HWND hWnd,
                  UINT uMsg,
                  WPARAM wParam,
                  LPARAM lParam)
{
	BOOL bRetval = FALSE;

	if (!setStatusHwnd) {
		hwndStatus = hWnd;
		setStatusHwnd = TRUE;
	}

	switch(uMsg)    {

		case WM_COMMAND:    {

			WORD wNotifyCode = HIWORD(wParam);
            WORD wID = LOWORD(wParam);
            HWND hControl = (HWND)lParam;

            switch(wID) {
				case IDDISCONNECTED:
//                if (AfxMessageBox(IDS_LOST_CONNECTION, MB_YESNO) == IDYES)
//                   m_pMainWnd->PostMessage(WM_COMMAND, IDC_DIAL);
					break;

				case IDCANCEL:  {
					// RasHangUp & destroy dialog box
					bRetval = TRUE;
					NotCanceled = FALSE;

#ifdef WIN32
					char strText[255];

					getMsgString(strText, IDS_CANCELDIAL);
		            SafeSetWindowText(IDC_DIAL_STATUS, strText);		

					RasHangUp(hRasConn);
					Sleep(3000);
#else
					assert(g_lpfnRasHangUp);
					(*g_lpfnRasHangUp)(hRasConn);
					Sleep(3);
#endif
					SafeEndDialog();
					break;
				}
			}
		}

	}

	return bRetval;
}

void SetCallState(CallState newState)
{
	m_callState = newState;
    
	switch (m_callState) {
		case StateConnected:
			// destroy our connection status window
			SafeEndDialog();
			break;

		case StateConnecting:
			// creates status dialog box
			HWND hwndParent = GetActiveWindow();
			int nResult;
			
			nResult = DialogBox(DLLinstance, MAKEINTRESOURCE(IDD_STATUS), hwndParent, (DLGPROC)statusDlgcallback);
			assert(nResult != -1);
			break;
	}
}


void DisplayDialErrorMsg(DWORD dwError)
{
    char    szErr[255];
	char    szErrStr[255];

#ifdef WIN32
    RasGetErrorString((UINT)dwError, szErr, sizeof(szErr));
#else
	(*g_lpfnRasGetErrorString)((UINT)dwError, szErr, sizeof(szErr));
#endif
    // Some of the default error strings are pretty lame
    switch (dwError) {
        case ERROR_NO_DIALTONE:
            getMsgString(szErr, IDS_NO_DIALTONE);
            break;

        case ERROR_LINE_BUSY:
            getMsgString(szErr, IDS_LINE_BUSY);
            break;
#ifdef WIN32
		case ERROR_PROTOCOL_NOT_CONFIGURED:
			getMsgString(szErr, IDS_PROTOCOL_NOT_CONFIGURED);
#endif
        default:
            break;
    }

	getMsgString(szErrStr, IDS_CONNECTION_FAILED);
	strcat(szErrStr, szErr);

	HWND hwnd;
	if (hwndStatus)
		hwnd = hwndStatus;
	else 
		hwnd = hwndNavigator;

	DisplayErrMsgWnd(szErrStr,  MB_OK | MB_ICONEXCLAMATION, hwnd);  
}

void ConnectErr(DWORD dwError)
{
	
	char strText[255];

	if(hwndStatus) {
        getMsgString((char *)strText, IDS_DIAL_ERR);
        SafeSetWindowText(IDC_DIAL_STATUS, strText);
		Sleep(1000);
		EndDialog(hwndStatus, TRUE);
		hwndStatus = NULL;
	}

	deviceErr = TRUE;		// some sort of device err
	DisplayDialErrorMsg(dwError);

	return;
}


void ProcessRasDialEvent(RASCONNSTATE rasconnstate, DWORD dwError)
{
	char strText[255];

    switch (rasconnstate) {
        case RASCS_OpenPort:
			while (hwndStatus == NULL)		//wait for status dialog shows up first
				Sleep(1000);

            getMsgString(strText, IDS_OPENING_PORT);
            SafeSetWindowText(IDC_DIAL_STATUS, strText);		
			if (dwError)
				ConnectErr(dwError);
			else
				Sleep(1000);
            break;

        case RASCS_PortOpened:

            getMsgString(strText, IDS_INIT_MODEM);
            SafeSetWindowText(IDC_DIAL_STATUS, strText);
			if (dwError)
				ConnectErr(dwError);
			else
				Sleep(1000);

            break;

        case RASCS_ConnectDevice:

            if (Dial == 1)
            {
               getMsgString(strText, IDS_DIALING);
               SafeSetWindowText(IDC_DIAL_STATUS, strText);
            } else {
               char    szBuf[128];

               getMsgString(strText, IDS_DIALING_OF);
                    wsprintf(szBuf, (LPCSTR)strText, Dial, NUM_ATTEMPTS);
               SafeSetWindowText(IDC_DIAL_STATUS, strText);
            }
			if (dwError)
				ConnectErr(dwError);
			else
				Sleep(1000);

            break;


        case RASCS_Authenticate:
            getMsgString(strText, IDS_VERIFYING);
            SafeSetWindowText(IDC_DIAL_STATUS, strText);
			if (dwError)
				ConnectErr(dwError);
			else
				Sleep(1000);

            break;

        case RASCS_Authenticated:
            getMsgString(strText, IDS_LOGGING_ON);
            SafeSetWindowText(IDC_DIAL_STATUS, strText);
			if (dwError)
				ConnectErr(dwError);
			else
				Sleep(1000);

            break;

        case RASCS_Connected:
            getMsgString(strText, IDS_CONNECTED);
            SafeSetWindowText(IDC_DIAL_STATUS, strText);
            SetCallState(StateConnected);
			if (dwError)
				ConnectErr(dwError);
			else
				Sleep(1000);

            break;

        case RASCS_Disconnected:
			// If this is an unexpected disconnect then hangup and take 
			// down the status dialog box
            if (m_callState == StateConnected) {
#ifdef WIN32
				::RasHangUp(hRasConn);
				Sleep(3000);
#else
				assert(g_lpfnRasHangUp);
				(*g_lpfnRasHangUp)(hRasConn);
				Sleep(3);
#endif
               // here we pass redial msg if needed.
               SafeEndDialog();

            } else if (m_callState == StateConnecting) {
#ifdef WIN32
				::RasHangUp(hRasConn);
			   	Sleep(3000);
#else
				assert(g_lpfnRasHangUp);
				(*g_lpfnRasHangUp)(hRasConn);
			   Sleep(3);
#endif
               if (dwError != SUCCESS) {
                  if (hwndStatus)  {
                     getMsgString(strText, IDS_DISCONNECTING);
                     SafeSetWindowText(IDC_DIAL_STATUS, strText);
                  }
                  SafeEndDialog();
                  LineDrop = TRUE;  // remove if we ask users for redial
                  DisplayDialErrorMsg(dwError);
               }
            }

            SetCallState(StateIdle);
            break;

        case RASCS_WaitForModemReset:
            getMsgString(strText, IDS_DISCONNECTING);
            SafeSetWindowText(IDC_DIAL_STATUS, strText);
			if (dwError)
				ConnectErr(dwError);
			else
				Sleep(1000);

            break;

		default:
			if (dwError)
				ConnectErr(dwError);
			break;

    }
}


//********************************************************************************
// RasDialFunc
//
//  call back function for RasDial
//
//********************************************************************************
#ifdef WIN32 //************************ WIN 32 *****************************
void CALLBACK
RasDialFunc(HRASCONN     hRasConn,
            UINT         uMsg,
            RASCONNSTATE rasconnstate,
            DWORD        dwError,
            DWORD        dwExtendedError)
#else //************************ WIN 16 *****************************
void 
RasDialFunc(UINT uMsg,                    // type of dial event
            RASCONNSTATE rasconnstate,    // connection state to be entered
            DWORD dwError)                // error
#endif // !WIN32
{
	if (uMsg == WM_RASDIALEVENT)
	{                                      // ignore all other messages
		RASstate = rasconnstate;
		ProcessRasDialEvent(rasconnstate, dwError);
	}
}


//********************************************************************************
// IsDialerConnected
//
// checks if the dialer is still connected
//********************************************************************************
static BOOL IsDialerConnected()
{

   BOOL bConnected = FALSE;
#if 0
   if ((RASstate == RASCS_Connected) && (connected == TRUE))
      return TRUE;

   return FALSE;
#endif

	RASCONN *pInfo = NULL, *lpTemp = NULL;
	DWORD code, count = 0;
	DWORD dSize = stRASCONN;
	char szMessage[256]="";

#ifdef WIN32
	HLOCAL hBuffer = NULL;

	// try to get a buffer to receive the connection data
	hBuffer = LocalAlloc(LPTR, (UINT) dSize);
	if (!hBuffer)           // Err: trouble allocating buffer
	{
		return FALSE;
	}
	pInfo = (RASCONN*) hBuffer;  

	// see if there are any open connections
	assert(pInfo);
	pInfo->dwSize = (DWORD) stRASCONN;
#else                          
	RASCONN connInfo;
	connInfo.dwSize = stRASCONN;
#endif

	// ------------------- Enumerate connections --------------------------		
#ifdef WIN32
	code = RasEnumConnections(pInfo, &dSize, &count);
#else
	assert(g_lpfnRasEnumConnections);
	if (g_lpfnRasEnumConnections)
		code = (*g_lpfnRasEnumConnections)(&connInfo, &dSize, &count);

	// ------- re-enumerate connection with larger buffer ---------
#endif

	if (ERROR_BUFFER_TOO_SMALL == code) { // buffer too small...
		// free the old buffer & re-allocate bigger buffer
#ifdef WIN32
		LocalFree(hBuffer);
		hBuffer = LocalAlloc(LPTR, (UINT) dSize);
		if (!hBuffer)
			return FALSE;
		pInfo = (RASCONN*) hBuffer;  // Err: trouble allocating buffer
#else
		pInfo = (RASCONN*) malloc((size_t)dSize);
		if (!pInfo)
			return FALSE;
#endif // !WIN32

		// try to enumerate connections again
		pInfo->dwSize = dSize;
		
#ifdef WIN32
		if (0 != RasEnumConnections(pInfo, &dSize, &count)) {
#else
		if (0 != (*g_lpfnRasEnumConnections)(pInfo, &dSize, &count)) {
#endif
			// can't enumerate connections, assume none is active
			count = 0;
		}
	}

#ifdef WIN32
	LocalFree(hBuffer);

	// removes regi icon
	if ((RegiMode) && (count == 0))  {
		char regiRAS[50];
		getMsgString((char *)regiRAS, IDS_REGGIE_PROGITEM_NAME);
		DWORD ret = (*m_lpfnRasDeleteEntry) (NULL, (LPSTR) (const char *) regiRAS);

		// delete NT4.0 RasMon process
		if ((platformOS == VER_PLATFORM_WIN32_NT) && (hRasMon)) {
			CloseHandle(hRasMon);
			hRasMon = NULL;
		}

	}

#else
	free(pInfo);
#endif

	return (count > 0);
}



//********************************************************************************
// native method:
//
// DialerConnect
//
// initiates the dialer to connect (used if Dial on Demand is disabled)
// assume RASDAILPARAM is already configured
//********************************************************************************
extern JRI_PUBLIC_API(jbool)
native_netscape_npasw_SetupPlugin_SECURE_0005fDialerConnect(JRIEnv* env,
											 struct netscape_npasw_SetupPlugin* ThisPlugin)
{
	// return if dialer already connected
#ifndef WIN32
	if (IsDialerConnected())
		return TRUE;
#endif
 
	DWORD dwError;
	BOOL connectSucceed=TRUE;
	hwndNavigator = GetActiveWindow();

	// Let RNA do the dial
	Dial = 1;
	hRasConn = NULL; 		// init global connection handle
	NotCanceled = TRUE; // assume connection is not canceled by the user, unless otherwise

#ifdef WIN32 // ************************** WIN 32 *************************
	// Win95 starts RASDIAL
	if (platformOS == VER_PLATFORM_WIN32_WINDOWS) {
		// do the dialing here
		dwError = RasDial(NULL, NULL, &dialParams, 1, RasDialFunc /*NULL*/, &hRasConn);
    } 
    // WinNT40 find system phone book first then start RASDIAL
    else if (platformOS == VER_PLATFORM_WIN32_NT) {
      char *sysDir;
      char *pbpath;
      sysDir = (char *)malloc(sizeof(char) * MAX_PATH);
      if (sysDir)  {
         GetSystemDirectory(sysDir, MAX_PATH);
         pbpath = (char *)malloc(sizeof(char) * strlen(sysDir) + 30);
         if (pbpath) {
            strcpy(pbpath, sysDir);
            strcat(pbpath, "\\ras\\rasphone.pbk");
            strcat(pbpath, "\0");

				// starts up RASMON process
				PROCESS_INFORMATION pi;
				BOOL                fRet;
				STARTUPINFO         sti;
				UINT                err = ERROR_SUCCESS;
				char RASMONpath[40];

				strcpy(RASMONpath, sysDir);
				strcat(RASMONpath, "\\rasmon.exe");
				strcat(RASMONpath, "\0");

				memset(&sti,0,sizeof(sti));
				sti.cb = sizeof(STARTUPINFO);

				// Run the RASMON app
				fRet = CreateProcess(RASMONpath, NULL,
					NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi);

				hRasMon = pi.hProcess;

				Sleep(3000);

				//do the dialing here
				dwError = RasDial(NULL, pbpath, &dialParams, 1, RasDialFunc /*NULL*/, &hRasConn);

            free(sysDir);
            free(pbpath);


         } else {
            free(sysDir);
            // Err: not enough memory for pbpath!
            return (FALSE);
         }
      } else {
         // Err: not enough memory for sysDir;
         return (FALSE);
      }
   }
   
#else  // ****************************** WIN16 *****************************

	HWND hNavWnd = GetActiveWindow(); 		// save Navigator's window to set active later..
	
	// WIN16 uses ShivaRemote RAS APIs
//	dwError = (*g_lpfnRasDial)(NULL, NULL, &dialParams, 0, (void *) RasDialFunc, &hRasConn);   // async call
	dwError = (*g_lpfnRasDial)(NULL, NULL, &dialParams, 0, NULL, &hRasConn); // sync call

#endif // !WIN32

#ifdef WIN32
	if (dwError == SUCCESS) {  // Dialing succeeded
#else
	if (dwError == 0) {
#endif
		// display connections status dialog & dispatch window msgs...
#ifdef WIN32
		SetCallState(StateConnecting);
		MSG msg;
		while (((RASstate != RASCS_Connected) && (RASstate != RASCS_Disconnected)) &&
				(NotCanceled) &&
				(!LineDrop) &&
				(!deviceErr))   
		{

			if(::GetMessage(&msg, NULL, 0, 0))  {
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
			else  {
				// WM_QUIT!!!
				break;
			}
		}

		SafeEndDialog();

		// sets flag back so we can get a new handle next time
		setStatusHwnd = FALSE;
		if ((RASstate != RASCS_Connected) || (!NotCanceled))
			connectSucceed = FALSE;
#endif

	} else {  // dialing failed!!!, display err msg
		connectSucceed = FALSE;
		DisplayDialErrorMsg(dwError);
	}

	if (!connectSucceed) {

		// hangup connection
		if (hRasConn) {
#ifdef WIN32 // ***************************** WIN32 ***********************************
			RasHangUp(hRasConn);
			SafeEndDialog();

			// give RasHangUp some time till complete hangup
			Sleep(3000);
#else        // ***************************** WIN16 ***********************************
			assert(g_lpfnRasHangUp);
			(*g_lpfnRasHangUp)(hRasConn);
			Sleep(3);
#endif       // !WIN32
		}

		assert(m_lpfnRasDeleteEntry);
#ifdef WIN32
		// remove the RegiServer RAS, for any reason if we fail to connect
		char regiRAS[50];
		getMsgString((char *)regiRAS, IDS_REGGIE_PROGITEM_NAME);
		DWORD ret = (*m_lpfnRasDeleteEntry) (NULL, (LPSTR) (const char *) regiRAS);
#else
		DWORD ret = (*m_lpfnRasDeleteEntry) (NULL, REGGI_SERVER_NAME);
#endif        
	}

#ifndef WIN32
	SetActiveWindow(hNavWnd);
#endif
                                             
	return (connectSucceed);
}



//********************************************************************************
//
// DialerHangup
//
//********************************************************************************
void DialerHangup() 
{
	RASCONN *Info = NULL, *lpTemp = NULL;
	DWORD code, count = 0;
	char szMessage[256] = { '\0' };
	DWORD dSize = stRASCONN;
    
#ifdef WIN32
	HLOCAL hBuffer = NULL;
	// try to get a buffer to receive the connection data
	hBuffer = LocalAlloc(LPTR, (UINT) dSize);
	Info = (RASCONN*) hBuffer;
#else
	Info = (RASCONN*) malloc(size_t(dSize));
#endif

	if (!Info)
	{
		return;
	}
	
   // set RAS struct size
 	Info->dwSize = dSize;

	// enumerate open connections
#ifdef WIN32
	code = RasEnumConnections (Info, &dSize, &count);
#else
	assert(g_lpfnRasEnumConnections);
	code = (*g_lpfnRasEnumConnections)(Info, &dSize, &count);
#endif
	if (ERROR_BUFFER_TOO_SMALL == code) {
	
		// free the old buffer & allocate a new bigger one
#ifdef WIN32
		LocalFree(hBuffer);
		hBuffer = LocalAlloc(LPTR, (UINT) dSize);
		Info = (RASCONN *) hBuffer;
#else 
		free(Info);
		Info = (RASCONN*) malloc(size_t(dSize));
#endif
		if(!Info)
		{
			return;
		}

		// try to enumerate again
		Info->dwSize = dSize;
#ifdef WIN32
		if (RasEnumConnections(Info, &dSize, &count) != 0) {
			LocalFree(hBuffer);
#else 
		if ((*g_lpfnRasEnumConnections)(Info, &dSize, &count) != 0) {
			free(Info);
#endif
			return;
		}
	}

	// check for no connections
	if (0 == count) {
#ifdef WIN32
		LocalFree(hBuffer);
#else
		free(Info);
#endif
		return;
	}

#if 0
	// ask user if they want to hang up.
	// we check for IDNO & leave hangup loop outside because 
	// hangup will never get called if we don't display
	// messagebox.
	if(IDNO == MessageBox(NULL, "There are open modem connections.  Would you like to close them?", 
                          "Dial-Up Networking", MB_YESNO)) {
#ifdef WIN32
		LocalFree(hBuffer);
#else
		free(Info);
#endif
		return;
	}
#endif

	// just hang up everything
	for (int i = 0; i < (int) count; i++) {
#ifdef WIN32
		RasHangUp(Info[i].hrasconn);
		Sleep(3000);
#else
		assert(g_lpfnRasHangUp);
		(*g_lpfnRasHangUp)(Info[i].hrasconn);
		Sleep(3);
#endif
	}

#ifdef WIN32
	LocalFree(hBuffer);
#else
	free(Info);
#endif

	// removes regi icon
	if (RegiMode)  {
#ifdef WIN32
		char regiRAS[50];
		getMsgString((char *)regiRAS, IDS_REGGIE_PROGITEM_NAME);
		DWORD ret = (*m_lpfnRasDeleteEntry) (NULL, (LPSTR) (const char *) regiRAS);

		// delete NT4.0 RasMon process
		if ((platformOS == VER_PLATFORM_WIN32_NT) && (hRasMon)) {
			CloseHandle(hRasMon);
			hRasMon = NULL;
		}

#else
		DWORD ret = (*m_lpfnRasDeleteEntry) (NULL, REGGI_SERVER_NAME);
#endif
	}


}




//********************************************************************************
// native method:
//
// DialerHangup
//
// hangs up the dialer when:
// 1. after regi communication is complete
// 2. user tries to close the ASW while connected to regi
//********************************************************************************
extern JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fDialerHangup(JRIEnv* env,
											struct netscape_npasw_SetupPlugin* ThisPlugin)
{
	DialerHangup();
}


//********************************************************************************
//
// findDialerData
//
// search the javascript array for specific string value
//********************************************************************************
char *findDialerData(JRIEnv* env,
                jstringArray dialerData,
                char *name)
{
   long   arraylen;
   void   *jri_str;
   const  char *arrayline;
   char   *lineptr;
   char   *key;
   char   *value = NULL;

   arraylen = JRI_GetObjectArrayLength(env, dialerData);
   for (short i=0; i<arraylen; i++) {
      /* get a string from the Javascript array */
      jri_str = JRI_GetObjectArrayElement(env, dialerData, i);
      arrayline = GetStringPlatformChars(env, (java_lang_String *)jri_str);
      lineptr = (char *)arrayline;

      /* parse the string into key and value */
      key = strtok(lineptr, "=");
      if (strcmp(key, name) == 0) {
         // found the keyname we're looking for, get it's value
         value = strtok(NULL, "");    // now should just be the result
         break;
      }
   }

   return (char *)value;
}



//********************************************************************************
//
// fillAccountParameters
//
// fill in account information, given from JS array
//********************************************************************************
void fillAccountParameters(JRIEnv *env,
                           jstringArray dialerData,
                           ACCOUNTPARAMS *account,
                           BOOL RegiMode)
{
	char *value;

	// isp name
	if (RegiMode)  {
#ifdef WIN32
		char accountName[50];
		getMsgString((char *)&accountName, IDS_REGGIE_PROGITEM_NAME);
		strcpy(account->ISPName, accountName);   //default for regi server
#else
		strcpy(account->ISPName, REGGI_SERVER_NAME);
#endif
	} else {
		value = findDialerData(env, dialerData, "AccountName");
		strcpy(account->ISPName, value ? value : "My Account"); 
		strcpy(acctDescription, account->ISPName);
	}

	// file name
	value = findDialerData(env, dialerData, "FileName");
	strcpy(account->FileName, value ? value : "My Account");

	// DNS
	value = findDialerData(env, dialerData, "DNSAddress");
	strcpy(account->DNS, value ? value : "0.0.0.0");

	// DNS2
	value = findDialerData(env, dialerData, "DNSAddress2");
	strcpy(account->DNS2, value ? value : "0.0.0.0");

	// domain name
	value = findDialerData(env, dialerData, "DomainName");
	strcpy(account->DomainName, value ? value : "");

	// login name
	value = findDialerData(env, dialerData, "LoginName");
	strcpy(account->LoginName, value ? value : "");

	// password
	value = findDialerData(env, dialerData, "Password");
	strcpy(account->Password, value ? value : "");

	// script file name
	value = findDialerData(env, dialerData, "ScriptFileName");
	strcpy(account->ScriptFileName, value ? value : "");

   // script enabled?
   value = findDialerData(env, dialerData, "ScriptEnabled");
   if (value)
   {
		account->ScriptEnabled = (strcmp(value, "TRUE") == 0);
		
		// get script content
		value = findDialerData(env, dialerData, "Script");
		if (value)
		{
			ReggieScript = (char*) malloc(strlen(value) + 1);
			strcpy(ReggieScript, value);
		}		   	
	}
   else
      account->ScriptEnabled = 0;

   // need TTY window?
   value = findDialerData(env, dialerData, "NeedsTTYWindow");
   if (value)
		account->NeedsTTYWindow = (value && (strcmp(value, "TRUE") == 0));
   else
      account->NeedsTTYWindow = 0;

   // isp phone number
   value = findDialerData(env, dialerData, "ISPPhoneNum");
   if (value)
      strcpy(account->ISPPhoneNum, value);
   else
      strcpy(account->ISPPhoneNum, "");

   // ISDN phone number
   value = findDialerData(env, dialerData, "ISDNPhoneNum");
   if (value)
      strcpy(account->ISDNPhoneNum, value);
   else
      strcpy(account->ISDNPhoneNum, "");

   // VJ compression enabled?
   value = findDialerData(env, dialerData, "VJCompresssionEnabled");
   if (value)
      account->VJCompressionEnabled = (value && (strcmp(value, "TRUE") == 0));
   else
      account->VJCompressionEnabled = 0;
      
   // International mode?
   value = findDialerData(env, dialerData, "IntlMode");
   if (value)
		account->IntlMode = (strcmp(value, "TRUE") == 0);
   else
      account->IntlMode = 0;

   // dial on demand?
   value = findDialerData(env, dialerData, "DialOnDemand");
   if (value)
   		account->DialOnDemand = (strcmp(value, "TRUE") == 0);
   else
      account->DialOnDemand = 1;
}




//********************************************************************************
//
// fillLocationParameters
//
// fill in location information, given from JS array
//********************************************************************************
void fillLocationParameters(JRIEnv* env,
                            jstringArray dialerData,
                            LOCATIONPARAMS *location,
                            BOOL RegiMode)
{
   char *value;

   // modem name
   value = findDialerData(env, dialerData, "ModemName");
   strcpy(location->ModemName, value ? value : "");

   // modem type
   value = findDialerData(env, dialerData, "ModemType");
   strcpy(location->ModemType, value ? value : "");

   // dial type
   value = findDialerData(env, dialerData, "DialType");
   if (value)
   	location->DialType = (strcmp(value, "TONE") == 0);
   else
      location->DialType = 1;

   // outside line access
   value = findDialerData(env, dialerData, "OutsideLineAccess");
   strcpy(location->OutsideLineAccess, value ? value : "");

   // disable call waiting?
   value = findDialerData(env, dialerData, "DisableCallWaiting");
   if (value)
      location->DisableCallWaiting = (strcmp(value, "TRUE") == 0);
   else
      location->DisableCallWaiting = 0;
   
   // disable call waiting code
   value = findDialerData(env, dialerData, "DisableCallWaitingCode");
   strcpy(location->DisableCallWaitingCode, value ? value : "");

   // user area code
   value = findDialerData(env, dialerData, "UserAreaCode");
   strcpy(location->UserAreaCode, value ? value : "");

   // user country code
   value = findDialerData(env, dialerData, "CountryCode");
   if (value) {
      char *stopstr = "\0";
      location->UserCountryCode = (short)strtol(value, &stopstr, 10);
   } else
      location->UserCountryCode = 1;   // default to US

   // dial as long distance?
   value = findDialerData(env, dialerData, "DialAsLongDistance");
   if (value)
   	location->DialAsLongDistance = (strcmp(value, "TRUE") == 0);
   else
      location->DialAsLongDistance = 0;

   // long distance access
   value = findDialerData(env, dialerData, "LongDistanceAccess");
   strcpy(location->LongDistanceAccess, value ? value : "");

   // dial area code?
   value = findDialerData(env, dialerData, "DialAreaCode");
   if (value)
      location->DialAreaCode = (strcmp(value, "TRUE") == 0);
   else
      location->DialAreaCode = 0;

   // dial prefix code
   value = findDialerData(env, dialerData, "DialPrefix");
   strcpy(location->DialPrefix, value ? value : "");

   // dial suffix code
   value = findDialerData(env, dialerData, "DialSuffix");
   strcpy(location->DialSuffix, value ? value : "");

   // use both ISDN lines?
   value = findDialerData(env, dialerData, "UseBothISDNLines");
   if (value)
      location->UseBothISDNLines = (strcmp(value, "TRUE") == 0);
   else
      location->UseBothISDNLines = 0;

   // 56k ISDN?
   value = findDialerData(env, dialerData, "56kISDN");
   if (value)
     location->b56kISDN = (strcmp(value, "TRUE") == 0);
   else
      location->b56kISDN = 0;

   // disconnect time
   value = findDialerData(env, dialerData, "DisconnectTime");

   if (value) {
	   location->DisconnectTime = atoi(value);
   } else {
	   location->DisconnectTime = 5;
   }


   
}



//********************************************************************************
//
// ParseNumber
//
// Parses a canonical TAPI phone number into country code, area code, and
// local subscriber number
//********************************************************************************
static void
ParseNumber(LPCSTR lpszCanonical, LPDWORD lpdwCountryCode, LPSTR lpszAreaCode, LPSTR lpszLocal)
{
	//*** sscanf dependency removed for win16 compatibility

	char temp[256];
	int p1, p2;

	// Initialize our return values
	*lpdwCountryCode = 1;  // North America Calling Plan
	*lpszAreaCode = '\0';
	*lpszLocal = '\0';

	if (!lpszCanonical || !*lpszCanonical)
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
	while (lpszCanonical[p1] == ' ') p1++;

	// Handle the country code if '+' prefix seen
	if (lpszCanonical[p1] == '+')
	{
		p1++;
		if (!isdigit(lpszCanonical[p1])) return;

		p2 = p1;
		while (isdigit(lpszCanonical[p1])) p1++;
		strncpy(temp, &lpszCanonical[p2], p1-p2);
		*lpdwCountryCode = atoi(temp);
	}

	// Allow spaces
	while (lpszCanonical[p1] == ' ') p1++;

	// Handle the area code if '(' prefix seen
	if (lpszCanonical[p1] == '(')
	{
		p1++;
		if (!isdigit(lpszCanonical[p1])) return;

		p2 = p1;
		while (isdigit(lpszCanonical[p1])) p1++;
		strncpy(lpszAreaCode, &lpszCanonical[p2], p1-p2);

		p1++;      // Skip over the trailing ')'
	}

	// Allow spaces
	while (lpszCanonical[p1] == ' ') p1++;

	// Whatever's left is the subscriber number (possibly including the whole string)
	strcpy(lpszLocal, &lpszCanonical[p1]);
}

//********************************************************************************
//
// OnlyOneSet
//
// Just an XOR of DialAsLongDistance & DialAreaCode - if only one of them is
// set then we can't use MS Locations (if neither are set then we can use 
// locations, but disable use of both - they just don't allow disable of each
// individually)
//********************************************************************************
BOOL OnlyOneSet(const LOCATIONPARAMS& location)
{
	if (location.DialAsLongDistance && !location.DialAreaCode)
		return TRUE;
	else if (!location.DialAsLongDistance && location.DialAreaCode)
		return TRUE;
	else
		return FALSE;
}

//********************************************************************************
// PrefixAvail() returns TRUE if there are prefixes that makes location unusable
//********************************************************************************
BOOL PrefixAvail(const LOCATIONPARAMS& Location)
{
	return (Location.DisableCallWaiting && Location.DisableCallWaitingCode[0] != 0) ||
			 (Location.OutsideLineAccess && Location.OutsideLineAccess[0] != 0);
}

//********************************************************************************
//
// ComposeNumber
//
// Create a phone number encompassing all of the location information to hack
// around dialup networking ignoring the location information if you turn off
// the "dial area and country code" flag
//********************************************************************************
static void 
ComposeNumber(ACCOUNTPARAMS &account, const LOCATIONPARAMS &Location, char csNumber[])
{
	// if they need to dial something to get an outside line next
	if (Location.OutsideLineAccess[0] != 0) {
        strcat(csNumber, Location.OutsideLineAccess);
        strcat(csNumber, " ");
	}

	// add disable call waiting if it exists
	if (Location.DisableCallWaiting && Location.DisableCallWaitingCode[0] != 0) {
		strcat(csNumber, Location.DisableCallWaitingCode);
		strcat(csNumber, " ");
	}
	
	if (account.IntlMode) { 
	
	  // In international mode we don't fill out the area code or
	  //   anything, just the exchange part
	  strcat(csNumber, account.ISPPhoneNum);
	}
	else {

	  // lets parse the number into pieces so we can get the area code & country code
	  DWORD nCntry;
	  char szAreaCode[32];
	  char szPhoneNumber[32];
	  ParseNumber(account.ISPPhoneNum, &nCntry, szAreaCode, szPhoneNumber);

	  // dial the 1 (country code) first if they want it
	  if (Location.DialAsLongDistance) {

		  char Cntry[10];
#ifdef WIN32
		  ultoa(nCntry, Cntry, 10);
#else
          itoa((int) nCntry, Cntry, 10);
#endif

		  if (strcmp(Location.LongDistanceAccess, "") == 0)
			  strcat(csNumber, Cntry);
		  else 
			  strcat(csNumber, Location.LongDistanceAccess);
	      strcat(csNumber, " ");
	  }
	   
	  // dial the area code next if requested
	  if (Location.DialAreaCode) {
	      strcat(csNumber, szAreaCode);
	      strcat(csNumber, " ");
	  }
	
	  // dial the local part of the number
	  strcat(csNumber, szPhoneNumber);
	}
}


//********************************************************************************
//
// GetCountryID
//
//********************************************************************************
static BOOL
GetCountryID(DWORD dwCountryCode, DWORD &dwCountryID)
{
	assert(m_lpfnRasGetCountryInfo);
	if (NULL == m_lpfnRasGetCountryInfo)
		return FALSE;
		
	RASCTRYINFO *pCI = NULL;
	BOOL bRetval = FALSE;

	DWORD dwSize = stRASCTRYINFO + 256;
	pCI = (RASCTRYINFO *)malloc((UINT) dwSize);
	if(pCI)  {
		pCI->dwSize = stRASCTRYINFO;
		pCI->dwCountryID = 1;
      
		while ((m_lpfnRasGetCountryInfo)(pCI, &dwSize) == 0) {
			if (pCI->dwCountryCode == dwCountryCode) {
				dwCountryID = pCI->dwCountryID;
				bRetval = TRUE;
				break;
			}
			pCI->dwCountryID = pCI->dwNextCountryID;
		}

		free(pCI);
		pCI = NULL;
	}

	return(bRetval);
}

//********************************************************************************
//
// ProcessScriptLogin
//
// Generate a script file and return the name of the file.  The
//   caller is responsible for freeing the script file name
//********************************************************************************
BOOL ProcessScriptedLogin(LPSTR lpszBuf, const char *lpszScriptFile)
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
//
// FileExists
//
//********************************************************************************
BOOL FileExists (LPCSTR lpszFileName)
{
	BOOL bResult = FALSE;

#ifdef WIN32 // ********************* Win32 **************************
	HANDLE hFile=NULL;

	// opens the file for READ
	hFile = CreateFile(lpszFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile != INVALID_HANDLE_VALUE) {  // openned file is valid
		bResult = TRUE;
		CloseHandle(hFile);
	}
#else        // ********************* Win16 **************************
	OFSTRUCT    of;
	HFILE hFile = OpenFile(lpszFileName, &of, OF_EXIST);
	bResult = hFile != HFILE_ERROR;
	if (bResult)
	_lclose(hFile);
#endif

	return bResult;
}


#ifdef WIN32
//********************************************************************************
//
// EnableDialOnDemand  (win95)
//
// Set the magic keys in the registry to enable dial on demand
//********************************************************************************
static void
EnableDialOnDemand(LPSTR lpProfileName)
{
    HKEY    hKey;
    DWORD   dwDisposition;
    long    result;
    char    *szData;

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
    DWORD dwValue = TRUE;
    result = RegSetValueEx(hKey, "EnableAutodial", NULL, REG_BINARY, (LPBYTE)&dwValue, sizeof(DWORD));


    //
    // set the autodial flag here too
    //
    result = RegCreateKeyEx(HKEY_USERS, ".Default\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
                            NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);

    // err, oops
    if (result != ERROR_SUCCESS)
       return;

    // set the autodial and idle-time disconnect
    dwValue = TRUE;
    result = RegSetValueEx(hKey, "EnableAutodial", NULL, REG_BINARY, (LPBYTE)&dwValue, sizeof(DWORD));


    //
    // OK, let's tell it which profile to autodial
    //
    result = RegCreateKeyEx(HKEY_CURRENT_USER, "RemoteAccess", NULL, NULL, NULL,
                            KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);

    // err, oops
    if (result != ERROR_SUCCESS)
       return;

    result = RegSetValueEx(hKey, "InternetProfile", NULL, REG_SZ, (LPBYTE)lpProfileName, strlen(lpProfileName));
    result = RegSetValueEx(hKey, "Default", NULL, REG_SZ, (LPBYTE)lpProfileName, strlen(lpProfileName));

    RegCloseKey(hKey);
}

//********************************************************************************
//
// EnableDialOnDemand  (winNT40)
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


//********************************************************************************
// EnableDialOnDemand  (win16)
//********************************************************************************
void EnableDialOnDemandNT(LPSTR lpProfileName)
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
	int     val = 1;
	rtn = (*m_lpfnRasSetAutodialParam)(RASADP_DisableConnectionQuery, &val, sizeof(int));
	 
	rtn = (*m_lpfnRasSetAutodialAddress)("www.netscape.com", 0, &rasAutodialEntry, 
	          sizeof(RASAUTODIALENTRY), 1);                          
	rtn = (*m_lpfnRasSetAutodialEnable)(rasAutodialEntry.dwDialingLocation, TRUE);
}

#else  // ************************************* WIN16 *************************************

//////////////////////////////////////////////////////////////////////////////////
// Enable Shiva DOD
//////////////////////////////////////////////////////////////////////////////////
BOOL EnableDialOnDemand16(LPCSTR szName, BOOL bSet)
{ 
	char szIniFName[255],	// ShivaRemote Config (ini) file name
		szConnFilePath[100];		// path to Shiva's INI file
		 	
	// get Shiva INI filename (sremote.ini)
	BOOL bResult = GetShivaSRemoteConfigFile(szIniFName);
	if (bResult)
	{
		// Get ShivaRemote connection file for the specified connection
		bResult = GetConnectionFilePath(szName, szConnFilePath, TRUE);
		     
      if (g_lpfnSetDialOnDemandInfo && bResult)
      	bResult = (*g_lpfnSetDialOnDemandInfo)(bSet, szConnFilePath) == 0;
	}
		
	return bResult;
}          

#endif // !WIN32


//********************************************************************************
//
// ToNumericAddress
//
// Converts from dotted address to numeric internet address
//********************************************************************************
BOOL ToNumericAddress(LPCSTR lpszAddress, DWORD& dwAddress)
{
	//*** sscanf dependency removed for win16 compatibility

	char temp[256];
	int a, b, c, d;
	int p1, p2;

	strcpy(temp, lpszAddress);

	p2 = p1 = 0;
	while (temp[p1] != '.') p1++;
	temp[p1] = '\0';
	a = atoi(&temp[p2]);
	
	p2 = ++p1;
	while (temp[p1] != '.') p1++;
	temp[p1] = '\0';
	b = atoi(&temp[p2]);

	p2 = ++p1;
	while (temp[p1] != '.') p1++;
	temp[p1] = '\0';
	c = atoi(&temp[p2]);

	p2 = ++p1;
	d = atoi(&temp[p2]);

        // Must be in network order (different than Intel byte ordering)
        LPBYTE  lpByte = (LPBYTE)&dwAddress;

        *lpByte++ = BYTE(a);
        *lpByte++ = BYTE(b);
        *lpByte++ = BYTE(c);
        *lpByte = BYTE(d);

	return TRUE;
}


#ifdef WIN32
//********************************************************************************
//
// SetAutoDisconnect
//
// Sets the autodisconnect time if idle
//
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
	if (platformOS == VER_PLATFORM_WIN32_WINDOWS) {

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
#endif

//********************************************************************************
//
// CreateRNAEntry
//
// Create a dial-up networking profile
//********************************************************************************
static BOOL
CreateRNAEntry(ACCOUNTPARAMS account, const LOCATIONPARAMS& location)
{
	DWORD    dwRet;
	BOOL     ret = 0;
	RASENTRY rasEntry;

	// abort if RAS API ptrs are invalid & mem alloc fails
	assert(m_lpfnRasSetEntryProperties);
#ifdef WIN32
	if (!m_lpfnRasSetEntryProperties)
#else
	assert(g_lpfnRasSetEntryDialParams && g_lpfnSetDialStringInfo);
	if (!m_lpfnRasSetEntryProperties || 
		 !g_lpfnRasSetEntryDialParams || 
		 !g_lpfnSetDialStringInfo)
#endif // !WIN32
		return FALSE;

	// Initialize the RNA struct
	memset(&rasEntry, 0, stRASENTRY);
	rasEntry.dwSize = stRASENTRY;

	rasEntry.dwfOptions = RASEO_ModemLights | RASEO_RemoteDefaultGateway;

	// Only allow compression if reg server says its OK
	if (account.VJCompressionEnabled)
		 rasEntry.dwfOptions |= RASEO_IpHeaderCompression | RASEO_SwCompression;

	if (account.NeedsTTYWindow)
#ifdef WIN32	
		if (platformOS == VER_PLATFORM_WIN32_WINDOWS)
			rasEntry.dwfOptions |= RASEO_TerminalBeforeDial;  //win95 bug! RASEO_TerminalBeforeDial means terminal after dial
		else
#endif
			rasEntry.dwfOptions |= RASEO_TerminalAfterDial;

	// If Intl Number (not NorthAmerica), or Area Code w/o LDAccess (1) or
	// visa-versa, then abandon using Location - NOTE: for Intl Number we
	// should be able to use location, check it out!
#ifdef WIN32		
	if (account.IntlMode || OnlyOneSet(location)) {
#else
	if (account.IntlMode || OnlyOneSet(location) || PrefixAvail(location)) {
#endif
	   char szNumber[RAS_MaxPhoneNumber + 1];
	   szNumber[0] = '\0';

	   ComposeNumber(account, location, szNumber);
	   strcpy(rasEntry.szLocalPhoneNumber, szNumber);
	   
#ifdef WIN32
	   strcpy(rasEntry.szAreaCode, "415");  // hack around MS bug--ignored
	   rasEntry.dwCountryCode = 1; // hack around MS bug -- ignored
#endif
	} else {
		// Let Win95 decide to dial the area code or not
		rasEntry.dwfOptions |= RASEO_UseCountryAndAreaCodes;

		// Configure the phone number
		ParseNumber(account.ISPPhoneNum, &rasEntry.dwCountryCode, 
						rasEntry.szAreaCode,	rasEntry.szLocalPhoneNumber);
		  
		if (!account.IntlMode) {
			// if not internationalize version, check the area code and make
			// sure we got a valid area code, if not throw up a err msg
			if (rasEntry.szAreaCode[0] == '\0') {
				// Err: The service provider's phone number is missing its area code
				//    (or is not in TAPI cannonical form in the configuration file).
				//    Account creation will fail until this is fixed.
				char *buf = (char *)malloc(sizeof(char) * 255);
				if (buf) {
					if (getMsgString(buf, IDS_MISSING_AREA_CODE))
						DisplayErrMsgWnd(buf, MB_OK | MB_ICONEXCLAMATION, hwndNavigator);
					free(buf);
				}
				return (FALSE);
			}
		}
	}

	// Now that we have the country code, we need to find the associated
	// country ID
	GetCountryID(rasEntry.dwCountryCode, rasEntry.dwCountryID);

	// Configure the IP data
	rasEntry.dwfOptions |= RASEO_SpecificNameServers;
	if (account.DNS[0])
	  ToNumericAddress(account.DNS, *(LPDWORD)&rasEntry.ipaddrDns);

	if (account.DNS2[0])
	  ToNumericAddress(account.DNS2, *(LPDWORD)&rasEntry.ipaddrDnsAlt);
                                                               
	// Configure the protocol and device settings here:

	// Negotiate TCP/IP
	rasEntry.dwfNetProtocols = RASNP_Ip;

	// Point-to-Point protocal (PPP)
	rasEntry.dwFramingProtocol = RASFP_Ppp;

	// modem's information
	strcpy(rasEntry.szDeviceName, location.ModemName);
	strcpy(rasEntry.szDeviceType, location.ModemType);

	// If we have a script, then store it too
	if (account.ScriptEnabled) 
	{
		BOOL rtnval = TRUE;
		
		// if there is script content, 'Translate' and store in file 
	   if (ReggieScript)
	   { 
	   	// construct script filename if it does not exists
	   	if (strlen(account.ScriptFileName) == 0)
	   	{
	   		GetProfileDirectory(account.ScriptFileName);
	  			int nIndex = strlen(account.ScriptFileName);
	   		strncat(account.ScriptFileName, account.ISPName, 8);
	   		strcat(account.ScriptFileName, ".scp");
#ifndef WIN32
				ParseWin16BadChar(account.ScriptFileName);
#endif
	   	} 
		   rtnval = ProcessScriptedLogin((LPSTR)ReggieScript, account.ScriptFileName);
		   free(ReggieScript);
      }

		/* if there really is a script file (from ISP or Reggie) then use it */
		if (rtnval && FileExists(account.ScriptFileName))
		{
			strcpy(rasEntry.szScript, account.ScriptFileName);

			// convert forward slash to backward slash
			int nLen = strlen(rasEntry.szScript);
			for (int i=0; i < nLen; i++)
				if (rasEntry.szScript[i] == '/')
					rasEntry.szScript[i] = '\\';
		}
	}

	// dialing on demand is cool.  let's do that on win95 now
#ifdef WIN32  // ************************ Win32 *************************
    if ((account.DialOnDemand) && (platformOS == VER_PLATFORM_WIN32_WINDOWS) && (!RegiMode))
		EnableDialOnDemand((LPSTR)(LPCSTR)account.ISPName);
#endif       //WIN32

	dwRet = (*m_lpfnRasSetEntryProperties)(NULL, (LPSTR)(LPCSTR)account.ISPName,
										  (LPBYTE)&rasEntry, stRASENTRY, NULL, 0);
	assert(dwRet == 0);
	if (dwRet)
	{
		return -1;
	}
	
	// We need to set the login name and password with a separate call
	// why doesn't this work for winNT40??
	memset(&dialParams, 0, sizeof(dialParams));
	dialParams.dwSize = stRASDIALPARAMS;
	strcpy(dialParams.szEntryName, account.ISPName);
	strcpy(dialParams.szUserName, account.LoginName);
	strcpy(dialParams.szPassword, account.Password);

   // Creating connection entry!
#ifdef WIN32 // *************** Win32 *****************

	// if win95, go ahead change connection info and return
	if (platformOS == VER_PLATFORM_WIN32_WINDOWS) {

	  ret = (RasSetEntryDialParams((LPSTR)(LPCSTR)account.ISPName,
												&dialParams, FALSE)==0); //Returns 0 for okay

	} else if (platformOS == VER_PLATFORM_WIN32_NT) {
	  // if winNT40, creates a connection info in phonebook and then enable
	  // Dial on Demand afterwords.

	  // here we need to find system phonebook first!
	  // something like ... "c:\\winnt40\\system32\\ras\\rasphone.pbk"

	  char *sysDir;
	  char *pbPath;
	  RASCREDENTIALS credentials;
	  sysDir = (char *)malloc(sizeof(char) * MAX_PATH);
	  if (sysDir)  {
		 GetSystemDirectory(sysDir, MAX_PATH);
		 pbPath = (char *)malloc(sizeof(char) * strlen(sysDir) + 30);
		 if (pbPath) {
			strcpy(pbPath, sysDir);
			strcat(pbPath, "\\ras\\rasphone.pbk");
			strcat(pbPath, "\0");

			ret = (RasSetEntryDialParams(pbPath, &dialParams, FALSE) == 0);
        
			// sets up user login info for new phonebook entry
			memset(&credentials, 0, sizeof(RASCREDENTIALS));
			credentials.dwSize = sizeof(RASCREDENTIALS);
			credentials.dwMask = RASCM_UserName | RASCM_Password;
			strcpy(credentials.szUserName, account.LoginName);
			strcpy(credentials.szPassword, account.Password);
			strcpy(credentials.szDomain, account.DomainName);

			ret = (m_lpfnRasSetCredentials(pbPath, (LPSTR)(LPCSTR)account.ISPName,
									&credentials, FALSE) == 0);

			free(sysDir);
			free(pbPath);
     
			// enable dial on demand for NT4, don't do it if it's regi
            if ((ret == TRUE) && (account.DialOnDemand) && (!RegiMode))
			   EnableDialOnDemandNT((LPSTR)(LPCSTR)account.ISPName);
		 } else {
			free(sysDir);
			// Err: not enough memory for pbPath!
			return -2;                                                   
		 }
	  } else {
		 // Err: not enough memory for sysDir;
		 return -3;
	  } // if (sysDir)
	} // else if (platformOS == VER_PLATFORM_WIN32_NT

	SetAutoDisconnect(location.DisconnectTime);

#else         // *************** Win16 *****************
	strcpy(dialParams.szDomain, account.DomainName);

	ret = (*g_lpfnRasSetEntryDialParams)(NULL, &dialParams, FALSE) == 0;

	if (ret)		// RasSetEntryDialParams() succeeds
   {
   	// set dial on demand for non-reggie connections
		if ((account.DialOnDemand) && (!RegiMode))
			EnableDialOnDemand16(account.ISPName, TRUE);
	
		// set tone or pulse dialing
		if (g_lpfnSetDialStringInfo)			
			(*g_lpfnSetDialStringInfo)(location.DialType, MODEMVOLUME_Low);

		// for Reggie connection: disable PPP compression to avoid connection drop w/ PortMasters for RPI modems
		if (g_lpfnSetCompressionInfo)
			(*g_lpfnSetCompressionInfo)(!RegiMode);
			
		// set connection info: autoreconnect, autodisconnect & idle period
		// for Reggie: disable reconnect dialog to avoid 2 places for reconnect 
		if (g_lpfnSetConnectionInfo)
			(*g_lpfnSetConnectionInfo)(account.ISPName, !RegiMode, TRUE, location.DisconnectTime);
	}
	
#endif

	return ret;
}

//********************************************************************************
// native method:
//
// IsDialerConnected
//
// checks if the dialer is still connected
//********************************************************************************
extern JRI_PUBLIC_API(jbool)
native_netscape_npasw_SetupPlugin_SECURE_0005fIsDialerConnected(JRIEnv* env,
												 struct netscape_npasw_SetupPlugin* ThisPlugin)
{
	jbool bResult = IsDialerConnected();
	return bResult;
}

//********************************************************************************
// native method:
//
// DialerConfig
//
// setup and configures the dialer and networking stuff
// used in 3 conditions:
// 1. when calling regi for a new account
// 2. to configure new account from regi on users system
// 3. when optionally register Navigator in existing account path
//********************************************************************************
extern JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fDialerConfig(JRIEnv* env,
											struct netscape_npasw_SetupPlugin* ThisPlugin,
											jstringArray JSdialerDataArray,
											jbool JSregiMode)
{
   hwndNavigator = GetActiveWindow();

   ACCOUNTPARAMS account;
   LOCATIONPARAMS location;

   RegiMode = JSregiMode; 

   if (!JSdialerDataArray) {
      // Err: no dialer data array passed, can't configure dialer
      return;
   }

   // now we try to get values from the JS array and put them into corresponding 
   // account and location parameters
   fillAccountParameters(env, JSdialerDataArray, &account, RegiMode);
   fillLocationParameters(env, JSdialerDataArray, &location, RegiMode);

    // if Reggie call then decrypt the 'shuffled' password
    if (RegiMode) {

        char Password[64];

        if (!ConvertPassword(account.Password, Password)) {

            // Err: Invalid pasword in Netscape registration file (regserv.ias).
            char *buf = (char *)malloc(sizeof(char) * 255);
            if (buf) {
               if (getMsgString(buf, IDS_BAD_PASSWORD))
                   DisplayErrMsgWnd(buf, MB_OK | MB_ICONEXCLAMATION, hwndNavigator);
              free(buf);
            }

            return;
        }
        strcpy(account.Password, Password);
    }

   // configure & creating Dial-Up Networking profile here for Win95 & WinNT40
   // win16 use Shiva's RAS 
   if (!(CreateRNAEntry(account, location)))  {

      // Err: Unable to crate RNA phone book entry!
      char *buf = (char *)malloc(sizeof(char) * 255);
      if (buf) {
         if (getMsgString(buf, IDS_NO_RNA_REGSERVER))
            DisplayErrMsgWnd(buf, MB_OK | MB_ICONEXCLAMATION, hwndNavigator);
         free(buf);
      }
   }

#ifdef WIN32
   int ret;
   if (platformOS == VER_PLATFORM_WIN32_WINDOWS) {
	   // sets the location stuff
	   ret = SetLocationInfo(account, location);
	   //ret = DisplayDialableNumber();
   } else {
	   ret = SetLocationInfoNT(account, location);
   }
#else
	// check if Account Path is existing!!!
	char *value = findDialerData(env, JSdialerDataArray, "Path");
   if (value)
		g_bExistingPath = (strcmp(value, "Existing") == 0);

#endif
}
