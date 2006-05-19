/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

/******************************************************
 *
 *  proto-ntutil.h - Prototypes for utility functions used
 *  throughout slapd on NT.
 *
 ******************************************************/
#if defined( _WINDOWS )

#ifndef _PROTO_NTUTIL
#define _PROTO_NTUTIL

/* 
 *
 * ntreg.c  
 *
 */
extern int SlapdGetRegSZ( LPTSTR lpszRegKey, LPSTR lpszValueName, LPTSTR lpszValue );

/* 
 *
 * getopt.c  
 *
 */
extern int getopt (int argc, char *const *argv, const char *optstring);

/* 
 *
 * ntevent.c  
 *
 */
extern BOOL MultipleInstances();
extern BOOL SlapdIsAService();
extern void InitializeSlapdLogging( LPTSTR lpszRegLocation, LPTSTR lpszEventLogName, LPTSTR lpszMessageFile );
extern void ReportSlapdEvent(WORD wEventType, DWORD dwIdEvent, WORD wNumInsertStrings, 
						char *pszStrings);
extern BOOL ReportSlapdStatusToSCMgr(
					SERVICE_STATUS *serviceStatus,
					SERVICE_STATUS_HANDLE serviceStatusHandle,
					HANDLE Event,
					DWORD dwCurrentState,
                    DWORD dwWin32ExitCode,
                    DWORD dwCheckPoint,
                    DWORD dwWaitHint);
extern void WINAPI SlapdServiceCtrlHandler(DWORD dwOpcode);
extern BOOL SlapdGetServerNameFromCmdline(char *szServerName, char *szCmdLine);

/* 
 *
 * ntgetpassword.c  
 *
 */
#ifdef NET_SSL
extern char *Slapd_GetPassword();
extern void CenterDialog(HWND hwndParent, HWND hwndDialog);
#endif /* NET_SSL */

#endif /* _PROTO_NTUTIL */

#endif /* _WINDOWS */
