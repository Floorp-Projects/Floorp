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
#include "port.h"
#include "resource.h"

//
// Variables...
// 
extern HINSTANCE    hInst;
LPSTR               ldifRetPtr = NULL;
extern HINSTANCE    m_hInstNAB;

// 
// Forward declarations...
//
LPSTR         DoLDIFDiag(HWND hWnd);
extern void   ShowMessage(HWND hWnd, LPSTR msg);
extern void   SetFooter(LPSTR msg);
void          ProcessOK(HWND hWnd);

void
SetDefaults(HWND hWndDiag)
{
  // Start with the basics...
  SetDlgItemText(hWndDiag, FIRSTNAME, "firstName");
  SetDlgItemText(hWndDiag, LASTNAME, "lastName");
  SetDlgItemText(hWndDiag, NOTES, "generalNotes"); 
  SetDlgItemText(hWndDiag, CITY, "city");
  SetDlgItemText(hWndDiag, STATE, "state");
  SetDlgItemText(hWndDiag, EMAIL, "email");
  SetDlgItemText(hWndDiag, TITLE, "title");
  SetDlgItemText(hWndDiag, ADDR1, "addrLine1");
  SetDlgItemText(hWndDiag, ADDR2, "addrLine2");
  SetDlgItemText(hWndDiag, ZIPCODE, "zipCode");
  SetDlgItemText(hWndDiag, COUNTRY, "country");
  SetDlgItemText(hWndDiag, BUSINESSPHONE, "businessPhone");
  SetDlgItemText(hWndDiag, FAXPHONE, "faxPhone");
  SetDlgItemText(hWndDiag, HOMEPHONE, "homePhone");
  SetDlgItemText(hWndDiag, ORG, "organization");
  SetDlgItemText(hWndDiag, NICKNAME, "nickname");
  SetDlgItemText(hWndDiag, USEHTML, "TRUE");
}

void 
ProcessLDIFCommand(HWND hWnd, int id, HWND hCtl, UINT codeNotify) 
{ 
  switch (id) 
  {
  case IDCANCEL:
    ldifRetPtr = NULL;
    EndDialog(hWnd, 0);
    break;

  case IDDEFAULTS:
    SetDefaults(hWnd);
    break;

  case IDOK:
    ProcessOK(hWnd);
    break;

  default:
    break;
  }
}

BOOL CALLBACK LOADDS 
LDIFDlgProc(HWND hWndMail, UINT wMsg, WPARAM wParam, LPARAM lParam) 
{
  switch (wMsg) 
  {
  case WM_INITDIALOG: 
    {
    }
    break;

  case WM_COMMAND:
    HANDLE_WM_COMMAND(hWndMail, wParam, lParam, ProcessLDIFCommand);
    break;

  default:
    return FALSE;
  }
  
  return TRUE;
}

LPSTR 
DoLDIFDiag(HWND hWnd)
{
  if (!ldifRetPtr)
    free(ldifRetPtr);

  DialogBox(hInst, MAKEINTRESOURCE(ID_DIALOG_LDIF), hWnd, (DLGPROC)LDIFDlgProc);
  return ldifRetPtr;
}

void
ProcessOK(HWND hWndDiag)
{
  char     *(FAR PASCAL *lpfnNAB_FormatLDIFLine) 
            ( char *firstName, char *lastName, char *generalNotes, char *city, 
              char *state, char *email, char *title, char *addrLine1, char *addrLine2, 
              char *zipCode, char *country, char *businessPhone, char *faxPhone, 
              char *homePhone, char *organization, char *nickname, char *useHTML); 
#ifdef WIN16		
  (FARPROC&) lpfnNAB_FormatLDIFLine = GetProcAddress(m_hInstNAB, "NAB_FORMATLDIFLINE"); 
#else
  (FARPROC&) lpfnNAB_FormatLDIFLine = GetProcAddress(m_hInstNAB, "NAB_FormatLDIFLine"); 
#endif
  
  if (!lpfnNAB_FormatLDIFLine)
  {
    ShowMessage(hWndDiag, "Unable to locate NAB function.");
    return;
  }

#define VARSIZE   64

char firstName[VARSIZE]; 
char lastName[VARSIZE]; 
char generalNotes[VARSIZE]; 
char city[VARSIZE]; 
char state[VARSIZE]; 
char email[VARSIZE]; 
char title[VARSIZE]; 
char addrLine1[VARSIZE]; 
char addrLine2[VARSIZE]; 
char zipCode[VARSIZE]; 
char country[VARSIZE]; 
char businessPhone[VARSIZE]; 
char faxPhone[VARSIZE]; 
char homePhone[VARSIZE]; 
char organization[VARSIZE]; 
char nickname[VARSIZE]; 
char useHTML[VARSIZE]; 

  GetDlgItemText(hWndDiag, FIRSTNAME, firstName, VARSIZE);
  GetDlgItemText(hWndDiag, LASTNAME, lastName, VARSIZE);
  GetDlgItemText(hWndDiag, NOTES, generalNotes, VARSIZE); 
  GetDlgItemText(hWndDiag, CITY, city, VARSIZE);
  GetDlgItemText(hWndDiag, STATE, state, VARSIZE);
  GetDlgItemText(hWndDiag, EMAIL, email, VARSIZE);
  GetDlgItemText(hWndDiag, TITLE, title, VARSIZE);
  GetDlgItemText(hWndDiag, ADDR1, addrLine1, VARSIZE);
  GetDlgItemText(hWndDiag, ADDR2, addrLine2, VARSIZE);
  GetDlgItemText(hWndDiag, ZIPCODE, zipCode, VARSIZE);
  GetDlgItemText(hWndDiag, COUNTRY, country, VARSIZE);
  GetDlgItemText(hWndDiag, BUSINESSPHONE, businessPhone, VARSIZE);
  GetDlgItemText(hWndDiag, FAXPHONE, faxPhone, VARSIZE);
  GetDlgItemText(hWndDiag, HOMEPHONE, homePhone, VARSIZE);
  GetDlgItemText(hWndDiag, ORG, organization, VARSIZE);
  GetDlgItemText(hWndDiag, NICKNAME, nickname, VARSIZE);
  GetDlgItemText(hWndDiag, USEHTML, useHTML, VARSIZE);
  
  ldifRetPtr = (*lpfnNAB_FormatLDIFLine)
            ( firstName, lastName, generalNotes, city, 
              state, email, title, addrLine1, addrLine2, 
              zipCode, country, businessPhone, faxPhone, 
              homePhone, organization, nickname, useHTML); 
  EndDialog(hWndDiag, 0);
}
