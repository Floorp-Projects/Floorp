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
