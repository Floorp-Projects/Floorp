/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
//
#include <windows.h>
#include <windowsx.h>      
#include <string.h>

#ifndef MAPI_OLE	// Because MSFT doesn't do this for us :-(
#include <mapi.h>
#endif

#include "port.h"
#include "resource.h"

//
// Variables...
// 
extern HINSTANCE  hInst;
HINSTANCE         m_hInstMapi;
LHANDLE           mapiSession = 0;

// 
// Forward declarations...
//
void    LoadNSCPVersionFunc(HWND hWnd);
void    DoMAPILogon(HWND hWnd);
void    DoMAPILogoff(HWND hWnd);
void    DoMAPIFreeBuffer(HWND hWnd, LPVOID buf, BOOL alert);
void    DoMAPISendMail(HWND hWnd);
void    DoMAPISendDocuments(HWND hWnd);
void    DoMAPIFindNext(HWND hWnd);
void    DoMAPIReadMail(HWND hWnd);
void    DoMAPIDeleteMail(HWND hWnd);
void    DoMAPIDetails(HWND hWnd);
void    DoMAPIResolveName(HWND hWnd);
void    DoMAPIResolveNameFreeBuffer(HWND hWnd);
void    SetFooter(LPSTR msg);
void    DoMAPI_NSCP_Sync(HWND hWnd);
LPSTR   GetMAPIError(LONG errorCode);
extern void DisplayMAPIReadMail(HWND hWnd, lpMapiMessage msgPtr);
lpMapiMessage   GetMessage(HWND hWnd, LPSTR id);

void
SetFooter(LPSTR msg)
{
  extern HWND   hWnd;

  SetDlgItemText(hWnd, ID_STATIC_RESULT, msg);
}

char FAR *
GetMAPIError(LONG errorCode)
{
  static char FAR msg[128];

  switch (errorCode) {                            
  case MAPI_E_FAILURE:
    lstrcpy(msg, "General MAPI Failure");
    break;

  case MAPI_E_INSUFFICIENT_MEMORY:
    strcpy(msg, "Insufficient Memory");
    break;

  case MAPI_E_LOGIN_FAILURE:
    strcpy(msg, "Login Failure");
    break;

  case MAPI_E_TOO_MANY_SESSIONS:
    strcpy(msg, "Too many MAPI sessions");
    break;

  case MAPI_E_INVALID_SESSION:
    strcpy(msg, "Invalid Session!");
    break;

  case MAPI_E_INVALID_MESSAGE:
    strcpy(msg, "Message identifier was bad!");
    break;

  case MAPI_E_NO_MESSAGES:
    strcpy(msg, "No messages were found!");
    break;

  case MAPI_E_ATTACHMENT_WRITE_FAILURE:
    strcpy(msg, "Attachment write failure!");
    break;

  case MAPI_E_DISK_FULL:
    strcpy(msg, "Attachment write failure! DISK FULL");
    break;

  case MAPI_E_AMBIGUOUS_RECIPIENT:
    strcpy(msg, "Recipient requested is not a unique address list entry.");
    break;

  case MAPI_E_UNKNOWN_RECIPIENT:
    strcpy(msg, "Recipient requested does not exist.");
    break;

  case MAPI_E_NOT_SUPPORTED:
    strcpy(msg, "Not supported by messaging system");
    break;

  case SUCCESS_SUCCESS:
    strcpy(msg, "Success on MAPI operation");
    break;

  case MAPI_E_INVALID_RECIPS:
    strcpy(msg, "Recipient specified in the lpRecip parameter was\nunknown. No dialog box was displayed.");
    break;

  case MAPI_E_ATTACHMENT_OPEN_FAILURE:
    strcpy(msg, "One or more files could not be located. No message was sent.");    
    break;

  case MAPI_E_ATTACHMENT_NOT_FOUND:
    strcpy(msg, "The specified attachment was not found. No message was sent.");
    break;

  case MAPI_E_BAD_RECIPTYPE:
    strcpy(msg, "The type of a recipient was not MAPI_TO, MAPI_CC, or MAPI_BCC. No message was sent.");
    break;

  default:
    strcpy(msg, "Unknown MAPI Return Code");
    break;
  }

  return((LPSTR) &(msg[0]));
}

void
ShowMessage(HWND hWnd, LPSTR msg)
{
  MessageBox(hWnd, msg, "Info Message", MB_ICONINFORMATION);
}

BOOL
OpenMAPI(void)
{
#ifdef WIN16
	m_hInstMapi = LoadLibrary("Y:\\ns\\cmd\\winfe\\mapi\\MAPI.DLL");	
#else
  m_hInstMapi = LoadLibrary(".\\COMPONENTS\\MAPI32.DLL");
#endif

  if (!m_hInstMapi)
  {
    ShowMessage(NULL, "Error Loading the MAPI DLL...Probably not found!");
    return(FALSE);
  }

  return(TRUE);
}

void
CloseMAPI(void)
{
  if(m_hInstMapi)
  {
    FreeLibrary(m_hInstMapi);
  }
}

void 
ProcessCommand(HWND hWnd, int id, HWND hCtl, UINT codeNotify) 
{ 
  switch (id) 
  {
  case ID_BUTTON_SYNC:
    DoMAPI_NSCP_Sync(hWnd);
    break;

  case ID_BUTTON_NSCPVERSION:
    LoadNSCPVersionFunc(hWnd);
    break;

  case ID_BUTTON_LOGON:
    DoMAPILogon(hWnd);
    break;

  case ID_BUTTON_LOGOFF:
    DoMAPILogoff(hWnd);
    break;

  case ID_BUTTON_FINDNEXT:
  case ID_MENU_MAPIFINDNEXT:
    DoMAPIFindNext(hWnd);
    break;

  case ID_BUTTON_READMAIL:
  case ID_MENU_MAPIREADMAIL:
    DoMAPIReadMail(hWnd);
    break;

  case ID_BUTTON_MAIL:
    {
    extern CALLBACK LOADDS 
           MailDlgProc(HWND hWndMain, UINT wMsg, WPARAM wParam, LPARAM lParam);

    DialogBox(hInst, MAKEINTRESOURCE(ID_DIALOG_MAIL), hWnd, 
      (DLGPROC)MailDlgProc);
    }
    break;

  case ID_BUTTON_DELETEMAIL:
  case ID_MENU_MAPIDELETEMAIL:
    DoMAPIDeleteMail(hWnd);
    break;

  case ID_MENU_MYEXIT:
	  DestroyWindow(hWnd);
	  break;

  case ID_BUTTON_CLEAR:
  case ID_MENU_CLEARRESULTS:
    ListBox_ResetContent(GetDlgItem(hWnd, ID_LIST_RESULT));      
	  break;

  case ID_BUTTON_FREEBUFFER:
    DoMAPIResolveNameFreeBuffer(hWnd);
    break;

  case ID_BUTTON_RESOLVENAME:
    DoMAPIResolveName(hWnd);
    break;

  case ID_BUTTON_DETAILS:
    DoMAPIDetails(hWnd);
    break;

  case ID_MENU_MYABOUT:
	  MessageBox(hWnd,	
			  "Netscape MAPI Test Harness\nWritten by: Rich Pizzarro (rhp@netscape.com)",	
			  "About",
			  MB_ICONINFORMATION);
	  break;
  
  default:
    break;
  }
}

void
DoMAPILogon(HWND hWnd)
{
  char  msg[1024];
  char  user[128] = "";
  char  pw[128] = "";

  // Get Address of MAPI function...
  ULONG (FAR PASCAL *lpfnMAPILogon)(ULONG, LPSTR, LPSTR, FLAGS, ULONG, LPLHANDLE);   
  
#ifdef WIN16		
  (FARPROC&) lpfnMAPILogon = GetProcAddress(m_hInstMapi, "MAPILOGON"); 
#else
  (FARPROC&) lpfnMAPILogon = GetProcAddress(m_hInstMapi, "MAPILogon"); 
#endif
  
  if (!lpfnMAPILogon)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }

  GetDlgItemText(hWnd, ID_EDIT_USERNAME, user, sizeof(user));
  GetDlgItemText(hWnd, ID_EDIT_PW, pw, sizeof(pw));

  LONG  rc = (*lpfnMAPILogon)((ULONG) hWnd, user, pw, 
                  MAPI_FORCE_DOWNLOAD | MAPI_NEW_SESSION, 0, &mapiSession);
  if (rc == SUCCESS_SUCCESS)
  {
    wsprintf(msg, "Success with session = %d", mapiSession);
    ShowMessage(hWnd, msg);
    SetFooter("Logon success");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from Logon\nError=[%s]", 
                      rc, GetMAPIError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("Logon failed");
  }
}

void
DoMAPILogoff(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnMAPILogoff) ( LHANDLE lhSession, ULONG ulUIParam, 
                                FLAGS flFlags, ULONG ulReserved);

#ifdef WIN16		
  (FARPROC&) lpfnMAPILogoff = GetProcAddress(m_hInstMapi, "MAPILOGOFF"); 
#else
  (FARPROC&) lpfnMAPILogoff = GetProcAddress(m_hInstMapi, "MAPILogoff"); 
#endif
  
  if (!lpfnMAPILogoff)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }

  char  msg[1024];
  LONG  rc = (*lpfnMAPILogoff)(mapiSession, (ULONG) hWnd, 0, 0);
  if (rc == SUCCESS_SUCCESS)
  {
    wsprintf(msg, "Successful logoff");
    ShowMessage(hWnd, msg);
    SetFooter(msg);
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from Logoff\nError=[%s]", 
                      rc, GetMAPIError(rc));

    ShowMessage(hWnd, msg);
    SetFooter("Logoff failed");
  }

  mapiSession = 0;
}

void
DoMAPIFreeBuffer(HWND hWnd, LPVOID buf, BOOL alert)
{
  ULONG (FAR PASCAL *lpfnMAPIFreeBuffer) (LPVOID lpBuffer);

#ifdef WIN16		
  (FARPROC&) lpfnMAPIFreeBuffer = GetProcAddress(m_hInstMapi, "MAPIFREEBUFFER"); 
#else
  (FARPROC&) lpfnMAPIFreeBuffer = GetProcAddress(m_hInstMapi, "MAPIFreeBuffer"); 
#endif
  
  if (!lpfnMAPIFreeBuffer)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }

  char  msg[1024];
  LONG  rc = (*lpfnMAPIFreeBuffer)(buf);
#ifdef WIN32
  if (rc == S_OK)
#else
  if (rc == SUCCESS_SUCCESS)
#endif
  {
    wsprintf(msg, "Successful Free Buffer Operation");
    if (alert)
      ShowMessage(hWnd, msg);
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from Logoff", rc);
    ShowMessage(hWnd, msg);
  }
}

void
DoMAPIFindNext(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnMAPIFindNext) (LHANDLE lhSession, ULONG ulUIParam, 
           LPTSTR lpszMessageType, LPTSTR lpszSeedMessageID, FLAGS flFlags, 
           ULONG ulReserved, LPTSTR lpszMessageID);

#ifdef WIN16		
  (FARPROC&) lpfnMAPIFindNext = GetProcAddress(m_hInstMapi, "MAPIFINDNEXT"); 
#else
  (FARPROC&) lpfnMAPIFindNext = GetProcAddress(m_hInstMapi, "MAPIFindNext"); 
#endif

  if (!lpfnMAPIFindNext)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }

  // Clear the list before we start...
  ListBox_ResetContent(GetDlgItem(hWnd, ID_LIST_RESULT));

  char  msg[1024];
  char  messageID[512];
  LONG  rc;                                                               
#ifdef WIN32
  FLAGS	flags = MAPI_GUARANTEE_FIFO | MAPI_LONG_MSGID | MAPI_UNREAD_ONLY;
#else                                                                     	
  FLAGS	flags = MAPI_GUARANTEE_FIFO | MAPI_UNREAD_ONLY;
#endif
  
  while ( (rc = (*lpfnMAPIFindNext) (mapiSession, 
           (ULONG) hWnd, 
           NULL, 
           NULL, 
		   flags,
           0,
           messageID)) == SUCCESS_SUCCESS)
  {
      // 

      lpMapiMessage mapiMsg = GetMessage(hWnd, messageID);
      if (mapiMsg != NULL)
      {
        wsprintf(msg, "%s: \"%s\" Sender: %s", 
                  messageID, 
                  mapiMsg->lpszSubject, 
                  mapiMsg->lpOriginator->lpszName);
        DoMAPIFreeBuffer(hWnd, mapiMsg, FALSE);
      }
      else
      {
        lstrcpy(msg, messageID);
      }

      ListBox_InsertString(GetDlgItem(hWnd, ID_LIST_RESULT), 0, msg);
  }

  wsprintf(msg, "Enumeration ended: Return code %d from MAPIFindNext\nCondition=[%s]", 
                    rc, GetMAPIError(rc));
  ShowMessage(hWnd, msg);
  SetFooter("Enumeration ended");
}

void
DoMAPIReadMail(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnMAPIReadMail) (LHANDLE lhSession, ULONG ulUIParam, 
           LPTSTR lpszMessageID, FLAGS flFlags, ULONG ulReserved, 
           lpMapiMessage FAR * lppMessage);

#ifdef WIN16		
  (FARPROC&) lpfnMAPIReadMail = GetProcAddress(m_hInstMapi, "MAPIREADMAIL"); 
#else
  (FARPROC&) lpfnMAPIReadMail = GetProcAddress(m_hInstMapi, "MAPIReadMail"); 
#endif
  
  if (!lpfnMAPIReadMail)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }

  char            msg[1024];
  char            lpszMessageID[512];
  lpMapiMessage   lpMessage = NULL;
  FLAGS           flFlags = 0;

  DWORD selected = ListBox_GetCurSel(GetDlgItem(hWnd, ID_LIST_RESULT));
  if (selected == LB_ERR)
  {
    ShowMessage(hWnd, "You need to select a valid message. Make sure\nyou have done a MAPIFindNext and selected\none of the resulting messages.");
    return;
  }

  ListBox_GetText(GetDlgItem(hWnd, ID_LIST_RESULT), selected, lpszMessageID);

  // Do the various flags for this call...
  if (BST_CHECKED == Button_GetCheck(GetDlgItem(hWnd, IDC_CHECK_BODYASFILE)))
  {
    flFlags |= MAPI_BODY_AS_FILE;
  }

  if (BST_CHECKED == Button_GetCheck(GetDlgItem(hWnd, IDC_CHECK_ENVELOPEONLY)))
  {
    flFlags |= MAPI_ENVELOPE_ONLY;
  }

  if (BST_CHECKED == Button_GetCheck(GetDlgItem(hWnd, IDC_CHECK_PEEK)))
  {
    flFlags |= MAPI_PEEK;
  }
  
  if (BST_CHECKED == Button_GetCheck(GetDlgItem(hWnd, IDC_CHECK_SUPPRESSATTACH)))
  {
    flFlags |= MAPI_SUPPRESS_ATTACH;
  }

  char *ptr = strchr( (const char *) lpszMessageID, ':');
  if (ptr) *ptr = '\0';

  LONG  rc = (*lpfnMAPIReadMail)
          (mapiSession, 
          (ULONG) hWnd, 
          lpszMessageID,
          flFlags,
          0, 
          &lpMessage);

  // Deal with error up front and return if need be...
  if (rc != SUCCESS_SUCCESS)
  {
    wsprintf(msg, "FAILURE: Return code %d from MAPIReadMail\nError=[%s]", 
                      rc, GetMAPIError(rc));

    ShowMessage(hWnd, msg);
    SetFooter("ReadMail failed");
    return;
  }

  // Now display the message and then return...
  DisplayMAPIReadMail(hWnd, lpMessage);
  DoMAPIFreeBuffer(hWnd, lpMessage, TRUE);
}

void
DoMAPIDeleteMail(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnMAPIDeleteMail) (LHANDLE lhSession, ULONG ulUIParam, 
            LPTSTR lpszMessageID, FLAGS flFlags, ULONG ulReserved);

#ifdef WIN16		
  (FARPROC&) lpfnMAPIDeleteMail = GetProcAddress(m_hInstMapi, "MAPIDELETEMAIL"); 
#else
  (FARPROC&) lpfnMAPIDeleteMail = GetProcAddress(m_hInstMapi, "MAPIDeleteMail"); 
#endif
  
  if (!lpfnMAPIDeleteMail)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }
 
  char            msg[1024];
  char            lpszMessageID[512];
  lpMapiMessage   lpMessage = NULL;

  DWORD selected = ListBox_GetCurSel(GetDlgItem(hWnd, ID_LIST_RESULT));
  if (selected == LB_ERR)
  {
    ShowMessage(hWnd, "You need to select a valid message. Make sure\nyou have done a MAPIFindNext and selected\none of the resulting messages.");
    return;
  }

  ListBox_GetText(GetDlgItem(hWnd, ID_LIST_RESULT), selected, lpszMessageID);

  char *ptr = strchr( (const char *) lpszMessageID, ':');
  if (ptr) *ptr = '\0';

  LONG  rc = (*lpfnMAPIDeleteMail)
          (mapiSession, 
          (ULONG) hWnd,
          lpszMessageID, 
          0,
          0);
  
  // Deal with the return code...
  if (rc == SUCCESS_SUCCESS)
  {
    wsprintf(msg, "Successful deletion");
    ShowMessage(hWnd, msg);
    SetFooter(msg);  

    // If it worked, refresh the list...
    ShowMessage(hWnd, "The message list will now be refreshed\nsince one message was deleted.");
    DoMAPIFindNext(hWnd);
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from MAPIDeleteMail\nError=[%s]", 
                      rc, GetMAPIError(rc));

    ShowMessage(hWnd, msg);
    SetFooter("Logoff failed");
  }
}

// This is for the name lookup stuff...
lpMapiRecipDesc       lpRecip = NULL;

void
DoMAPIResolveName(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnMAPIResolveName) (LHANDLE lhSession, ULONG ulUIParam, 
            LPTSTR lpszName, FLAGS flFlags, ULONG ulReserved, 
            lpMapiRecipDesc FAR * lppRecip); 

#ifdef WIN16		
  (FARPROC&) lpfnMAPIResolveName = GetProcAddress(m_hInstMapi, "MAPIRESOLVENAME"); 
#else
  (FARPROC&) lpfnMAPIResolveName = GetProcAddress(m_hInstMapi, "MAPIResolveName"); 
#endif
  
  if (!lpfnMAPIResolveName)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }

  if (lpRecip != NULL)
  {
    ShowMessage(hWnd, "We need to free memory from a previous call...");
    DoMAPIFreeBuffer(hWnd, lpRecip, TRUE);
    lpRecip = NULL;
  }

  char        userName[512];
  char        msg[1024];
  FLAGS       flFlags = 0;    // We support none...

  GetDlgItemText(hWnd, IDC_EDIT_RESOLVENAME, userName, sizeof(userName));
  LONG  rc = (*lpfnMAPIResolveName)
                      (mapiSession, 
                       (ULONG) hWnd, 
                       userName,
                       flFlags, 
                       0, 
                       &lpRecip);

  // Deal with error up front and return if need be...
  if (rc != SUCCESS_SUCCESS)
  {
    wsprintf(msg, "FAILURE: Return code %d from DoMAPIResolveName\nError=[%s]", 
                      rc, GetMAPIError(rc));

    ShowMessage(hWnd, msg);
    SetFooter("DoMAPIResolveName failed");
    return;
  }

  // If we get here, we should probably show the information that we
  // got back
  wsprintf(msg, "Received information for %s\nName=[%s]\nAddress=[%s]\nID=[%s]",
    userName, lpRecip->lpszName, lpRecip->lpszAddress, (LPSTR) lpRecip->lpEntryID);
  ShowMessage(hWnd, msg);
}

void    
DoMAPIResolveNameFreeBuffer(HWND hWnd)
{
  if (lpRecip == NULL)
  {
    ShowMessage(hWnd, "There is no memory allocated from MAPIResolveName()\nto be freed. Request ignored.");
  }
  else
  {
    DoMAPIFreeBuffer(hWnd, lpRecip, TRUE);
    lpRecip = NULL;
  }
}

void
DoMAPIDetails(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnMAPIDetails) (LHANDLE lhSession, ULONG ulUIParam, 
            lpMapiRecipDesc lpRecip, FLAGS flFlags, ULONG ulReserved);

#ifdef WIN16		
  (FARPROC&) lpfnMAPIDetails = GetProcAddress(m_hInstMapi, "MAPIDetails"); 
#else
  (FARPROC&) lpfnMAPIDetails = GetProcAddress(m_hInstMapi, "MAPIDetails"); 
#endif
  
  if (!lpfnMAPIDetails)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }

  char        msg[1024];
  FLAGS       flFlags = 0;    // We really don't support these...

  LONG  rc = (*lpfnMAPIDetails)
              (mapiSession, 
              (ULONG) hWnd,
               lpRecip,
               flFlags,
               0);

  if (rc == SUCCESS_SUCCESS)
  {

    wsprintf(msg, "MAPIDetails call succeeded");
    ShowMessage(hWnd, msg);
    SetFooter(msg);
  }
  else
  {

    wsprintf(msg, "FAILURE: Return code %d from MAPIDetails\nError=[%s]", 
                      rc, GetMAPIError(rc));

    if (lpRecip == NULL)
    {
      lstrcat(msg, "\nNOTE: There is no valid pointer from a MAPIResolveName()\ncall to show details about.");
    }

    ShowMessage(hWnd, msg);
    SetFooter("MAPIDetails failed");
  }
}

lpMapiMessage 
GetMessage(HWND hWnd, LPSTR id)
{
  ULONG (FAR PASCAL *lpfnMAPIReadMail) (LHANDLE lhSession, ULONG ulUIParam, 
           LPTSTR lpszMessageID, FLAGS flFlags, ULONG ulReserved, 
           lpMapiMessage FAR * lppMessage);

#ifdef WIN16		
  (FARPROC&) lpfnMAPIReadMail = GetProcAddress(m_hInstMapi, "MAPIREADMAIL"); 
#else
  (FARPROC&) lpfnMAPIReadMail = GetProcAddress(m_hInstMapi, "MAPIReadMail"); 
#endif
  
  if (!lpfnMAPIReadMail)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return NULL;
  }

  char            msg[1024];
  lpMapiMessage   lpMessage = NULL;
  FLAGS           flFlags = 0;

  flFlags |= MAPI_ENVELOPE_ONLY;
  LONG  rc = (*lpfnMAPIReadMail)
          (mapiSession, 
          (ULONG) hWnd, 
          id,
          flFlags,
          0, 
          &lpMessage);

  // Deal with error up front and return if need be...
  if (rc != SUCCESS_SUCCESS)
  {
    wsprintf(msg, "FAILURE: Return code %d from MAPIReadMail\nError=[%s]", 
                      rc, GetMAPIError(rc));

    ShowMessage(hWnd, msg);
    SetFooter("ReadMail failed");
    return NULL;
  }

  return(lpMessage);
}

void    
LoadNSCPVersionFunc(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnLoadNSCPVersion) ( void );

#ifdef WIN16		
  (FARPROC&) lpfnLoadNSCPVersion = GetProcAddress(m_hInstMapi, "MAPIGETNERSCAPEVERSION"); 
#else
  (FARPROC&) lpfnLoadNSCPVersion = GetProcAddress(m_hInstMapi, "MAPIGetNetscapeVersion"); 
#endif
  
  if (!lpfnLoadNSCPVersion)
  {
    ShowMessage(hWnd, "Unable to locate MAPIGetNetscapeVersion() function.");
  }
  else
  {
    ShowMessage(hWnd, "MAPIGetNetscapeVersion() function was FOUND!");
  }
  return;
}

void    
DoMAPI_NSCP_Sync(HWND hWnd)
{
ULONG (FAR PASCAL *lpfnNSCPSync) ( LHANDLE lhSession,
                                          ULONG ulReserved );
#ifdef WIN16		
  (FARPROC&) lpfnNSCPSync = GetProcAddress(m_hInstMapi, "MAPI_NSCP_SYNCHRONIZECLIENT"); 
#else
  (FARPROC&) lpfnNSCPSync = GetProcAddress(m_hInstMapi, "MAPI_NSCP_SynchronizeClient"); 
#endif

  if (!lpfnNSCPSync)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }

  LONG  rc = (*lpfnNSCPSync) (mapiSession, 0);

  char    msg[256];

  // Deal with error up front and return if need be...
  if (rc != SUCCESS_SUCCESS)
  {
    wsprintf(msg, "FAILURE: Return code %d from MAPI_NSCP_SynchronizeClient\nError=[%s]", 
                      rc, GetMAPIError(rc));

    ShowMessage(hWnd, msg);
    SetFooter("MAPI_NSCP_SynchronizeClient failed");
    return;
  }
  else
  {
    wsprintf(msg, "Success for MAPI_NSCP_SynchronizeClient");    
    ShowMessage(hWnd, msg);
    SetFooter("MAPI_NSCP_SynchronizeClient success");
  }
}
