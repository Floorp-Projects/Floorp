/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

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
#ifdef FORTEZZA
extern char *Slapd_GetFortezzaPIN();
#endif
extern void CenterDialog(HWND hwndParent, HWND hwndDialog);
#endif /* NET_SSL */

#endif /* _PROTO_NTUTIL */

#endif /* _WINDOWS */
