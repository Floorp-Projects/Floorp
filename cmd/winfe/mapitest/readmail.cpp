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
//
#include <windows.h>
#include <windowsx.h>

#ifndef MAPI_OLE	// Because MSFT doesn't do this for us :-(
#include <mapi.h>
#endif

#include "port.h"
#include "resource.h"

//
// Variables...
// 
extern HINSTANCE   hInst;
extern HINSTANCE   m_hInstMapi;
extern LHANDLE     mapiSession;

// 
// Forward declarations...
//
extern void   ShowMessage(HWND hWnd, LPSTR msg);
extern void   SetFooter(LPSTR msg);
extern LPSTR  GetMAPIError(LONG errorCode);
lpMapiMessage mailPtr = NULL;

void 
ProcessReadMailCommand(HWND hWnd, int id, HWND hCtl, UINT codeNotify) 
{ 
  switch (id) 
  {
  case ID_OK:
  case IDCANCEL:
    EndDialog(hWnd, 0);
    break;

  default:
    break;
  }
}

BOOL CALLBACK LOADDS 
ReadMailDlgProc(HWND hWndMail, UINT wMsg, WPARAM wParam, LPARAM lParam) 
{
  switch (wMsg) 
  {
  case WM_INITDIALOG: 
    {
      DWORD i;
      // Do everything we need to display the message pointed to by
      // mailPtr
      if (!mailPtr)
        break;

      // Start with the basics...
      SetDlgItemText(hWndMail, IDC_EDIT_SUBJECT, mailPtr->lpszSubject);
      SetDlgItemText(hWndMail, IDC_EDIT_DATETIME, mailPtr->lpszDateReceived);
      SetDlgItemText(hWndMail, IDC_EDIT_THREAD, mailPtr->lpszConversationID);
      SetDlgItemText(hWndMail, IDC_EDIT_BODYTEXT, mailPtr->lpszNoteText);

      char    buf[1024];
      wsprintf(buf, "%s (%s)", mailPtr->lpOriginator->lpszName, 
                              mailPtr->lpOriginator->lpszAddress);
      SetDlgItemText(hWndMail, IDC_EDIT_FROM, buf);

      for (i=0; i<mailPtr->nRecipCount; i++)
      {
          wsprintf(buf, "%s (%s)", mailPtr->lpRecips[i].lpszName, 
                                   mailPtr->lpRecips[i].lpszAddress);
          ListBox_InsertString(GetDlgItem(hWndMail, IDC_LIST_RECIPIENTS), 
                  ListBox_GetCount(GetDlgItem(hWndMail, IDC_LIST_RECIPIENTS)), 
                  buf);
      }

      for (i=0; i<mailPtr->nFileCount; i++)
      {
          ListBox_InsertString(GetDlgItem(hWndMail, IDC_LIST_ATTACHMENTS), 
                ListBox_GetCount(GetDlgItem(hWndMail, IDC_LIST_ATTACHMENTS)), 
                mailPtr->lpFiles[i].lpszPathName);
      }
    }
    break;

  case WM_COMMAND:
    HANDLE_WM_COMMAND(hWndMail, wParam, lParam, ProcessReadMailCommand);
    break;

  default:
    return FALSE;
  }
  
  return TRUE;
}

void
DisplayMAPIReadMail(HWND hWnd, lpMapiMessage msgPtr)
{
  mailPtr = msgPtr;
  DialogBox(hInst, MAKEINTRESOURCE(ID_DIALOG_READMAIL), hWnd, 
                (DLGPROC)ReadMailDlgProc);
}
