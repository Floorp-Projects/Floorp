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
#include <mapi.h>         
#include <stdlib.h>

#include "port.h"
#include "resource.h"

//
// Variables...
// 
extern HINSTANCE   m_hInstMapi;
extern LHANDLE     mapiSession;

// 
// Forward declarations...
//
void          DoMAPIFreeBuffer(HWND hWnd, LPVOID buf, BOOL alert);
extern void   ShowMessage(HWND hWnd, LPSTR msg);
void          DoMAPISendMail(HWND hWnd);
void          DoMAPISendDocuments(HWND hWnd);
void          DoMAPISaveMail(HWND hWnd);
void          DoMAPIAddress(HWND hWnd);
extern void   SetFooter(LPSTR msg);
extern LPSTR  GetMAPIError(LONG errorCode);

void 
ProcessMailCommand(HWND hWnd, int id, HWND hCtl, UINT codeNotify) 
{ 
  switch (id) 
  {
  case IDCANCEL:
    EndDialog(hWnd, 0);
    break;

  case ID_BUTTON_MAPISENDMAIL:
    DoMAPISendMail(hWnd);
    break;

  case ID_BUTTON_MAPISENDDOCUMENTS:
    DoMAPISendDocuments(hWnd);
    break;

  case ID_BUTTON_MAPISAVEMAIL:
    DoMAPISaveMail(hWnd);
    break;

  case ID_BUTTON_MAPIADDRESS:
    DoMAPIAddress(hWnd);
    break;

  default:
    break;
  }
}

BOOL CALLBACK LOADDS 
MailDlgProc(HWND hWndMain, UINT wMsg, WPARAM wParam, LPARAM lParam) 
{
  switch (wMsg) 
  {
  case WM_INITDIALOG: 
    break;

  case WM_COMMAND:
    HANDLE_WM_COMMAND(hWndMain, wParam, lParam, ProcessMailCommand);
    break;

  default:
    return FALSE;
  }
  
  return TRUE;
}

static LPSTR lpszDelimChar = ";";

void
TackItOn(LPSTR fileBuf, LPSTR nameBuf, LPSTR addOn)
{
  if (addOn[0] != '\0')
  {
    lstrcat(fileBuf, addOn);
    lstrcat(fileBuf, lpszDelimChar);

    lstrcat(nameBuf, "NAMEOF.FILE");
    lstrcat(nameBuf, lpszDelimChar);
  }
}

void
DoMAPISendDocuments(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnMAPISendDocuments) (ULONG ulUIParam, 
          LPTSTR lpszDelimChar, LPTSTR lpszFullPaths, 
          LPTSTR lpszFileNames, ULONG ulReserved);

#ifdef WIN16		
  (FARPROC&) lpfnMAPISendDocuments = GetProcAddress(m_hInstMapi, "MAPISENDDOCUMENTS"); 
#else
  (FARPROC&) lpfnMAPISendDocuments = GetProcAddress(m_hInstMapi, "MAPISendDocuments"); 
#endif
  
  if (!lpfnMAPISendDocuments)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }

  char  msg[1024];
  char  tempFileName[_MAX_PATH] = "";
  char  lpszFullPaths[(_MAX_PATH + 1) * 4] = "";
  char  lpszFileNames[(_MAX_PATH + 1) * 4] = "";

  // Now get the names of the files to attach...
  GetDlgItemText(hWnd, ID_EDIT_ATTACH1, tempFileName, sizeof(tempFileName));
  TackItOn(lpszFullPaths, lpszFileNames, tempFileName); 

  GetDlgItemText(hWnd, ID_EDIT_ATTACH2, tempFileName, sizeof(tempFileName));
  TackItOn(lpszFullPaths, lpszFileNames, tempFileName); 

  GetDlgItemText(hWnd, ID_EDIT_ATTACH3, tempFileName, sizeof(tempFileName));
  TackItOn(lpszFullPaths, lpszFileNames, tempFileName); 

  GetDlgItemText(hWnd, ID_EDIT_ATTACH4, tempFileName, sizeof(tempFileName));
  TackItOn(lpszFullPaths, lpszFileNames, tempFileName); 

  LONG  rc = (*lpfnMAPISendDocuments)
        ( (ULONG) hWnd,        
          lpszDelimChar, 
          lpszFullPaths, 
          lpszFileNames, 
          0);

  if (rc == SUCCESS_SUCCESS)
  {
    ShowMessage(hWnd, "Success with MAPISendDocuments");
    SetFooter("MAPISendDocuments success");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from MAPISendDocuments\nError=[%s]", 
                      rc, GetMAPIError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("MAPISendDocuments failed");
  }
}

void
FreeMAPIFile(lpMapiFileDesc pv)
{
  if (!pv)
    return;

  if (pv->lpszPathName != NULL)   
    free(pv->lpszPathName);

  if (pv->lpszFileName != NULL)   
    free(pv->lpszFileName);
}

void
FreeMAPIRecipient(lpMapiRecipDesc pv)
{
  if (!pv)
    return;

  if (pv->lpszName != NULL)   
    free(pv->lpszName);

  if (pv->lpszAddress != NULL)
    free(pv->lpszAddress);

  if (pv->lpEntryID != NULL)
    free(pv->lpEntryID);  
}

void
FreeMAPIMessage(lpMapiMessage pv)
{
  ULONG i;

  if (!pv)
    return;

  if (pv->lpszSubject != NULL)
    free(pv->lpszSubject);

  if (pv->lpszNoteText)
      free(pv->lpszNoteText);
  
  if (pv->lpszMessageType)
    free(pv->lpszMessageType);
  
  if (pv->lpszDateReceived)
    free(pv->lpszDateReceived);
  
  if (pv->lpszConversationID)
    free(pv->lpszConversationID);
  
  if (pv->lpOriginator)
    FreeMAPIRecipient(pv->lpOriginator);
  
  for (i=0; i<pv->nRecipCount; i++)
  {
    if (&(pv->lpRecips[i]) != NULL)
    {
      FreeMAPIRecipient(&(pv->lpRecips[i]));
    }
  }

  if (pv->lpRecips != NULL)
  {
    free(pv->lpRecips);
  }

  for (i=0; i<pv->nFileCount; i++)
  {
    if (&(pv->lpFiles[i]) != NULL)
    {
      FreeMAPIFile(&(pv->lpFiles[i]));
    }
  }

  if (pv->lpFiles != NULL)
  {
    free(pv->lpFiles);
  }
  
  free(pv);
  pv = NULL;
}

void
DoMAPISendMail(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnMAPISendMail) (LHANDLE lhSession, ULONG ulUIParam, 
                  lpMapiMessage lpMessage, FLAGS flFlags, ULONG ulReserved);

#ifdef WIN16		
  (FARPROC&) lpfnMAPISendMail = GetProcAddress(m_hInstMapi, "MAPISENDMAIL"); 
#else
  (FARPROC&) lpfnMAPISendMail = GetProcAddress(m_hInstMapi, "MAPISendMail"); 
#endif
  
  if (!lpfnMAPISendMail)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }          

  FLAGS   flFlags = 0;
  char    msg[512];
  char    file1[_MAX_PATH] = "";
  char    file2[_MAX_PATH] = "";
  char    file3[_MAX_PATH] = "";
  char    file4[_MAX_PATH] = "";
  char    toAddr[128];
  char    ccAddr[128];
  char    bccAddr[128];
  char    subject[128];
  char    noteText[4096];
  char    dateReceived[128] = "N/A";
  char    threadID[128] = "N/A";;
  char    origName[128] = "N/A";;
  char    origAddress[128] = "N/A";;

  GetDlgItemText(hWnd, ID_EDIT_TOADDRESS, toAddr, sizeof(toAddr));
  GetDlgItemText(hWnd, ID_EDIT_CCADDRESS, ccAddr, sizeof(ccAddr));
  GetDlgItemText(hWnd, ID_EDIT_BCCADDRESS, bccAddr, sizeof(bccAddr));
  GetDlgItemText(hWnd, ID_EDIT_SUBJECT, subject, sizeof(subject));
  GetDlgItemText(hWnd, ID_EDIT_NOTETEXT, noteText, sizeof(noteText));

  // Do the one flag we support for this call...
  if (BST_CHECKED == Button_GetCheck(GetDlgItem(hWnd, ID_CHECK_SHOWDIALOG)))
  {
    flFlags |= MAPI_DIALOG;
  }

  // Build the message to send off...
  lpMapiMessage  msgPtr = (MapiMessage *)malloc(sizeof(MapiMessage));
  if (msgPtr == NULL)
  {
    return;
  }
  
  memset(msgPtr, 0, sizeof(MapiMessage)); 

  //
  // At this point, we need to populate the structure of information
  // we are passing in via the *lppMessage
  //

  // Set all of the general information first!
  msgPtr->lpszSubject = strdup(subject);
  msgPtr->lpszNoteText = strdup(noteText);
  msgPtr->lpszDateReceived = strdup(dateReceived);
  msgPtr->lpszConversationID = strdup(threadID);
  msgPtr->flFlags = flFlags;

  // Now deal with the recipients of this message
  DWORD       realRecips = 0;
  if (toAddr[0] != '\0')    ++realRecips;
  if (ccAddr[0] != '\0')    ++realRecips;
  if (bccAddr[0] != '\0')   ++realRecips;

  msgPtr->lpRecips = (lpMapiRecipDesc) malloc((size_t) (sizeof(MapiRecipDesc) * realRecips));
  if (!msgPtr->lpRecips)
  {
    FreeMAPIMessage(msgPtr);
    return;
  }

  msgPtr->nRecipCount = realRecips;
  memset(msgPtr->lpRecips, 0, (size_t) (sizeof(MapiRecipDesc) * msgPtr->nRecipCount));

  DWORD       rCount = 0;
  if (toAddr[0] != '\0')
  {
    msgPtr->lpRecips[rCount].lpszName = strdup(toAddr);
    msgPtr->lpRecips[rCount].lpszAddress = strdup(toAddr);
    msgPtr->lpRecips[rCount].ulRecipClass = MAPI_TO;
    rCount++;
  }

  if (ccAddr[0] != '\0')
  {
    msgPtr->lpRecips[rCount].lpszName = strdup(ccAddr);
    msgPtr->lpRecips[rCount].lpszAddress = strdup(ccAddr);
    msgPtr->lpRecips[rCount].ulRecipClass = MAPI_CC;    
    rCount++;
  }

  if (bccAddr[0] != '\0')
  {
    msgPtr->lpRecips[rCount].lpszName = strdup(bccAddr);
    msgPtr->lpRecips[rCount].lpszAddress = strdup(bccAddr);
    msgPtr->lpRecips[rCount].ulRecipClass = MAPI_BCC;
    rCount++;
  }

  // Now get the names of the files to attach...
  GetDlgItemText(hWnd, ID_EDIT_ATTACH1, file1, sizeof(file1));
  GetDlgItemText(hWnd, ID_EDIT_ATTACH2, file2, sizeof(file2));
  GetDlgItemText(hWnd, ID_EDIT_ATTACH3, file3, sizeof(file3));
  GetDlgItemText(hWnd, ID_EDIT_ATTACH4, file4, sizeof(file4));

  DWORD       realFiles = 0;
  if (file1[0] != '\0')    ++realFiles;
  if (file2[0] != '\0')    ++realFiles;
  if (file3[0] != '\0')    ++realFiles;
  if (file4[0] != '\0')    ++realFiles;

  // Now deal with the list of attachments! Since the nFileCount should be set
  // correctly, this loop will automagically be correct
  //
  msgPtr->nFileCount = realFiles;
  if (realFiles > 0)
  {
    msgPtr->lpFiles = (lpMapiFileDesc) malloc((size_t) (sizeof(MapiFileDesc) * realFiles));
    if (!msgPtr->lpFiles)
    {
      FreeMAPIMessage(msgPtr);
      return;
    }

    memset(msgPtr->lpFiles, 0, (size_t) (sizeof(MapiFileDesc) * msgPtr->nFileCount));
  }

  rCount = 0;
  if (file1[0] != '\0')
  {
    msgPtr->lpFiles[rCount].lpszPathName = strdup((LPSTR)file1);
    msgPtr->lpFiles[rCount].lpszFileName = strdup((LPSTR)file1);
    ++rCount;
  }

  if (file2[0] != '\0')
  {
    msgPtr->lpFiles[rCount].lpszPathName = strdup((LPSTR)file2);
    msgPtr->lpFiles[rCount].lpszFileName = strdup((LPSTR)file2);
    ++rCount;
  }

  if (file3[0] != '\0')
  {
    msgPtr->lpFiles[rCount].lpszPathName = strdup((LPSTR)file3);
    msgPtr->lpFiles[rCount].lpszFileName = strdup((LPSTR)file3);
    ++rCount;
  }

  if (file4[0] != '\0')
  {
    msgPtr->lpFiles[rCount].lpszPathName = strdup((LPSTR)file4);
    msgPtr->lpFiles[rCount].lpszFileName = strdup((LPSTR)file4);
    ++rCount;
  }

  // Finally, make the call...
  LONG  rc = (*lpfnMAPISendMail)
           (mapiSession, 
           (ULONG) hWnd, 
           msgPtr, 
           flFlags, 
           0);

  if (rc == SUCCESS_SUCCESS)
  {
    ShowMessage(hWnd, "Success with MAPISendMail");
    SetFooter("MAPISendMail success");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from MAPISendMail\nError=[%s]", 
                      rc, GetMAPIError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("MAPISendMail failed");
  }

  // Now cleanup and move on...
  FreeMAPIMessage(msgPtr);
}

void
DoMAPISaveMail(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnMAPISaveMail) (LHANDLE lhSession, ULONG ulUIParam, 
                  lpMapiMessage lpMessage, FLAGS flFlags, ULONG ulReserved,
                  LPTSTR lpszMessageID);

#ifdef WIN16		
  (FARPROC&) lpfnMAPISaveMail = GetProcAddress(m_hInstMapi, "MAPISAVEMAIL"); 
#else
  (FARPROC&) lpfnMAPISaveMail = GetProcAddress(m_hInstMapi, "MAPISaveMail"); 
#endif
  
  if (!lpfnMAPISaveMail)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }          

  FLAGS   flFlags = 0;
  char    msg[512];
  char    file1[_MAX_PATH] = "";
  char    file2[_MAX_PATH] = "";
  char    file3[_MAX_PATH] = "";
  char    file4[_MAX_PATH] = "";
  char    toAddr[128];
  char    ccAddr[128];
  char    bccAddr[128];
  char    subject[128];
  char    noteText[4096];
  char    dateReceived[128] = "N/A";
  char    threadID[128] = "N/A";;
  char    origName[128] = "N/A";;
  char    origAddress[128] = "N/A";;

  GetDlgItemText(hWnd, ID_EDIT_TOADDRESS, toAddr, sizeof(toAddr));
  GetDlgItemText(hWnd, ID_EDIT_CCADDRESS, ccAddr, sizeof(ccAddr));
  GetDlgItemText(hWnd, ID_EDIT_BCCADDRESS, bccAddr, sizeof(bccAddr));
  GetDlgItemText(hWnd, ID_EDIT_SUBJECT, subject, sizeof(subject));
  GetDlgItemText(hWnd, ID_EDIT_NOTETEXT, noteText, sizeof(noteText));

  // Build the message to send off...
  lpMapiMessage  msgPtr = (MapiMessage *)malloc(sizeof(MapiMessage));
  if (msgPtr == NULL)
  {
    return;
  }
  
  memset(msgPtr, 0, sizeof(MapiMessage)); 

  //
  // At this point, we need to populate the structure of information
  // we are passing in via the *lppMessage
  //

  // Set all of the general information first!
  msgPtr->lpszSubject = strdup(subject);
  msgPtr->lpszNoteText = strdup(noteText);
  msgPtr->lpszDateReceived = strdup(dateReceived);
  msgPtr->lpszConversationID = strdup(threadID);
  msgPtr->flFlags = flFlags;

  // Now deal with the recipients of this message
  DWORD       realRecips = 0;
  if (toAddr[0] != '\0')    ++realRecips;
  if (ccAddr[0] != '\0')    ++realRecips;
  if (bccAddr[0] != '\0')   ++realRecips;

  msgPtr->lpRecips = (lpMapiRecipDesc) malloc((size_t) (sizeof(MapiRecipDesc) * realRecips));
  if (!msgPtr->lpRecips)
  {
    FreeMAPIMessage(msgPtr);
    return;
  }

  msgPtr->nRecipCount = realRecips;
  memset(msgPtr->lpRecips, 0, (size_t) (sizeof(MapiRecipDesc) * msgPtr->nRecipCount));

  DWORD       rCount = 0;
  if (toAddr[0] != '\0')
  {
    msgPtr->lpRecips[rCount].lpszName = strdup(toAddr);
    msgPtr->lpRecips[rCount].lpszAddress = strdup(toAddr);
    msgPtr->lpRecips[rCount].ulRecipClass = MAPI_TO;
    rCount++;
  }

  if (ccAddr[0] != '\0')
  {
    msgPtr->lpRecips[rCount].lpszName = strdup(ccAddr);
    msgPtr->lpRecips[rCount].lpszAddress = strdup(ccAddr);
    msgPtr->lpRecips[rCount].ulRecipClass = MAPI_CC;    
    rCount++;
  }

  if (bccAddr[0] != '\0')
  {
    msgPtr->lpRecips[rCount].lpszName = strdup(bccAddr);
    msgPtr->lpRecips[rCount].lpszAddress = strdup(bccAddr);
    msgPtr->lpRecips[rCount].ulRecipClass = MAPI_BCC;
    rCount++;
  }

  // Now get the names of the files to attach...
  GetDlgItemText(hWnd, ID_EDIT_ATTACH1, file1, sizeof(file1));
  GetDlgItemText(hWnd, ID_EDIT_ATTACH2, file2, sizeof(file2));
  GetDlgItemText(hWnd, ID_EDIT_ATTACH3, file3, sizeof(file3));
  GetDlgItemText(hWnd, ID_EDIT_ATTACH4, file4, sizeof(file4));

  DWORD       realFiles = 0;
  if (file1[0] != '\0')    ++realFiles;
  if (file2[0] != '\0')    ++realFiles;
  if (file3[0] != '\0')    ++realFiles;
  if (file4[0] != '\0')    ++realFiles;

  // Now deal with the list of attachments! Since the nFileCount should be set
  // correctly, this loop will automagically be correct
  //
  msgPtr->nFileCount = realFiles;
  if (realFiles > 0)
  {
    msgPtr->lpFiles = (lpMapiFileDesc) malloc((size_t) (sizeof(MapiFileDesc) * realFiles));
    if (!msgPtr->lpFiles)
    {
      FreeMAPIMessage(msgPtr);
      return;
    }

    memset(msgPtr->lpFiles, 0, (size_t) (sizeof(MapiFileDesc) * msgPtr->nFileCount));
  }

  rCount = 0;
  if (file1[0] != '\0')
  {
    msgPtr->lpFiles[rCount].lpszPathName = strdup((LPSTR)file1);
    msgPtr->lpFiles[rCount].lpszFileName = strdup((LPSTR)file1);
    ++rCount;
  }

  if (file2[0] != '\0')
  {
    msgPtr->lpFiles[rCount].lpszPathName = strdup((LPSTR)file2);
    msgPtr->lpFiles[rCount].lpszFileName = strdup((LPSTR)file2);
    ++rCount;
  }

  if (file3[0] != '\0')
  {
    msgPtr->lpFiles[rCount].lpszPathName = strdup((LPSTR)file3);
    msgPtr->lpFiles[rCount].lpszFileName = strdup((LPSTR)file3);
    ++rCount;
  }

  if (file4[0] != '\0')
  {
    msgPtr->lpFiles[rCount].lpszPathName = strdup((LPSTR)file4);
    msgPtr->lpFiles[rCount].lpszFileName = strdup((LPSTR)file4);
    ++rCount;
  }

  // Finally, make the call...
  LONG  rc = (*lpfnMAPISaveMail)
           (mapiSession, 
           (ULONG) hWnd, 
           msgPtr, 
           flFlags, 
           0, NULL);

  if (rc == SUCCESS_SUCCESS)
  {
    ShowMessage(hWnd, "Success with MAPISaveMail");
    SetFooter("MAPISaveMail success");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from MAPISaveMail\nError=[%s]", 
                      rc, GetMAPIError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("MAPISaveMail failed");
  }

  // Now cleanup and move on...
  FreeMAPIMessage(msgPtr);
}

void          
DoMAPIAddress(HWND hWnd)
{
  ULONG (FAR PASCAL *lpfnMAPIAddress) 
                            (LHANDLE lhSession,
                            ULONG ulUIParam,
                            LPSTR lpszCaption,
                            ULONG nEditFields,
                            LPSTR lpszLabels,
                            ULONG nRecips,
                            lpMapiRecipDesc lpRecips,
                            FLAGS flFlags,
                            ULONG ulReserved,
                            LPULONG lpnNewRecips,
                            lpMapiRecipDesc FAR *lppNewRecips);


#ifdef WIN16		
  (FARPROC&) lpfnMAPIAddress = GetProcAddress(m_hInstMapi, "MAPIADDRESS"); 
#else
  (FARPROC&) lpfnMAPIAddress = GetProcAddress(m_hInstMapi, "MAPIAddress"); 
#endif
  
  if (!lpfnMAPIAddress)
  {
    ShowMessage(hWnd, "Unable to locate MAPI function.");
    return;
  }          

  DWORD   i;
  FLAGS   flFlags = 0;
  DWORD   addrCount = 0;
  char    msg[512];
  char    toAddr[128];
  char    ccAddr[128];
  char    bccAddr[128];

  GetDlgItemText(hWnd, ID_EDIT_TOADDRESS, toAddr, sizeof(toAddr));
  GetDlgItemText(hWnd, ID_EDIT_CCADDRESS, ccAddr, sizeof(ccAddr));
  GetDlgItemText(hWnd, ID_EDIT_BCCADDRESS, bccAddr, sizeof(bccAddr));

  if (toAddr[0]) ++addrCount;
  if (ccAddr[0]) ++addrCount;
  if (bccAddr[0]) ++addrCount;

  lpMapiRecipDesc lpRecips = (lpMapiRecipDesc) malloc((size_t) (sizeof(MapiRecipDesc) * addrCount));
  if (!lpRecips)
  {
    return;
  }

  memset(lpRecips, 0, (size_t) (sizeof(MapiRecipDesc) * addrCount));

  DWORD       rCount = 0;
  if (toAddr[0] != '\0')
  {
    lpRecips[rCount].lpszName = strdup("To Address Name");
    lpRecips[rCount].lpszAddress = strdup(toAddr);
    lpRecips[rCount].ulRecipClass = MAPI_TO;
    rCount++;
  }

  if (ccAddr[0] != '\0')
  {
    lpRecips[rCount].lpszName = strdup("CC Address Name");
    lpRecips[rCount].lpszAddress = strdup(ccAddr);
    lpRecips[rCount].ulRecipClass = MAPI_CC;    
    rCount++;
  }

  if (bccAddr[0] != '\0')
  {
    lpRecips[rCount].lpszName = strdup("BCC Address Name");
    lpRecips[rCount].lpszAddress = strdup(bccAddr);
    lpRecips[rCount].ulRecipClass = MAPI_BCC;
    rCount++;
  }
  
  ULONG             newRecips;
  lpMapiRecipDesc   lpNewRecips;

  // Finally, make the call...
  LONG  rc = (*lpfnMAPIAddress)
                     (mapiSession,
                      0,
                      "MAPI Test Address Picker",
                      0,
                      NULL,
                      rCount,
                      lpRecips,
                      0,
                      0,
                      &newRecips,
                      &lpNewRecips);
  if (rc == SUCCESS_SUCCESS)
  {
    for (i=0; i<newRecips; i++)
    {
      char  tMsg[512];

      wsprintf(tMsg, "User %d\nName=[%s]\nEmail=[%s]\nType=[%d]", 
            i, 
            lpNewRecips[i].lpszName, 
            lpNewRecips[i].lpszAddress, 
            lpNewRecips[i].ulRecipClass);
      ShowMessage(hWnd, tMsg);
    }

    SetFooter("MAPIAddress success");

    DoMAPIFreeBuffer(hWnd, lpNewRecips, TRUE);
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from MAPIAddress\nError=[%s]", 
                      rc, GetMAPIError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("MAPIAddress failed");
  }

  // Now cleanup and move on...
  for (i=0; i<rCount; i++)
  {
    FreeMAPIRecipient(&(lpRecips[i]));
  }
}
