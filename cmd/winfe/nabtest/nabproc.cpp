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
#include <string.h>

#include "port.h"
#include "resource.h"
#include "nabapi.h"

//
// Variables...
// 
extern HINSTANCE  hInst;
HINSTANCE         m_hInstNAB;
NABConnectionID   nabConn;  

void    SetFooter(LPSTR msg);
LPSTR   GetNABError(LONG errorCode);

void    DoNAB_Open(HWND hWnd);
void    DoNAB_Close(HWND hWnd);
void    DoNAB_GetAddressBookList(HWND hWnd);
void    DoNAB_GetDefaultAddressBook(HWND hWnd);
void    DoNAB_CreatePersonalAddressBook(HWND hWnd);
void    DoNAB_FreeMemory(HWND hWnd, LPVOID buf, BOOL alert);
void    DoNAB_SaveAddressBookToHTML(HWND hWnd);
void    DoNAB_ImportLDIFFile(HWND hWnd);
void    DoNAB_ExportLDIFFile(HWND hWnd);
void    DoNAB_GetFirstAddressBookEntry(HWND hWnd);
void    DoNAB_FindAddressBookEntry(HWND hWnd);
void    DoNAB_InsertAddressBookEntry(HWND hWnd);
void    DoNAB_UpdateAddressBookEntry(HWND hWnd);
void    DoNAB_DeleteAddressBookEntry(HWND hWnd);
LPSTR   DoLDIFDiag(HWND hWnd);

void
HackIt(NABAddrBookDescType **arrayPtr[])
{
  *arrayPtr = (NABAddrBookDescType **) malloc(sizeof(NABAddrBookDescType));
  *arrayPtr[0] = (NABAddrBookDescType *) malloc(sizeof(NABAddrBookDescType));
}

void
DoTheHack(void)
{
  NABAddrBookDescType **descType; 

  HackIt(&descType);
  int x = 1;
}

void 
ProcessCommand(HWND hWnd, int id, HWND hCtl, UINT codeNotify) 
{ 
  switch (id) 
  {
  case ID_BUTTON_LDIFDIAG:
    {
      LPSTR ptr = DoLDIFDiag(hWnd);
      SetDlgItemText(hWnd, ID_EDIT_FINDLDIF, ptr);       
    }
    break;

  case ID_LIST_USERS:  
    switch(codeNotify) 
    {
    case LBN_SELCHANGE: 
      {
        char    abookInfo[512];
        DWORD selected = ListBox_GetCurSel(GetDlgItem(hWnd, ID_LIST_USERS));
        if (selected == LB_ERR)
          break;
        ListBox_GetText(GetDlgItem(hWnd, ID_LIST_USERS), selected, abookInfo);
        SetDlgItemText(hWnd, ID_EDIT_FINDLDIF, abookInfo);
        break;
      }
    default:
        break;        
    }
    break;

  case ID_BUTTON_OPEN:
    DoNAB_Open(hWnd);
    break;

  case ID_BUTTON_CLOSE:
    DoNAB_Close(hWnd);
    break;

  case ID_BUTTON_GETADDRLIST:
    DoNAB_GetAddressBookList(hWnd);
    break;

  case ID_BUTTON_GETDEF_ADDR:
    DoNAB_GetDefaultAddressBook(hWnd);
    break;

  case ID_BUTTON_CREATEABOOK:
    DoNAB_CreatePersonalAddressBook(hWnd);
    break;

  case ID_BUTTON_SAVETOHTML:
    DoNAB_SaveAddressBookToHTML(hWnd);
    break;

  case ID_BUTTON_EXPORTLDIF:
    DoNAB_ExportLDIFFile(hWnd);
    break;

  case ID_BUTTON_IMPORTLDIF:
    DoNAB_ImportLDIFFile(hWnd);
    break;

  case ID_BUTTON_LISTAB:
    DoNAB_GetFirstAddressBookEntry(hWnd);
    break;

  case ID_BUTTON_FINDAB:
    DoNAB_FindAddressBookEntry(hWnd);
    break;

  case ID_BUTTON_INSERT:
    DoNAB_InsertAddressBookEntry(hWnd);
    break;

  case ID_BUTTON_UPDATE:
    DoNAB_UpdateAddressBookEntry(hWnd);
    break;

  case ID_BUTTON_DELETE:
    DoNAB_DeleteAddressBookEntry(hWnd);
    break;

  case ID_MENU_MYEXIT:
	  DestroyWindow(hWnd);
	  break;

  case ID_BUTTON_CLEAR:
  case ID_MENU_CLEARRESULTS:
    ListBox_ResetContent(GetDlgItem(hWnd, ID_LIST_ABOOK));      
    ListBox_ResetContent(GetDlgItem(hWnd, ID_LIST_USERS));      
	  break;

  case ID_MENU_MYABOUT:
	  MessageBox(hWnd,	
			  "Netscape Address Book API Test Harness\nWritten by: Rich Pizzarro (rhp@netscape.com)",	
			  "About",
			  MB_ICONINFORMATION);
	  break;
  
  default:
    break;
  }
}

//
// This is all needed to process a MAPIReadMail operation
//
static LPSTR
CheckNull(LPSTR inStr)
{
  static LPSTR  str = "<Null>";

  str[0] = '\0';
  if (inStr == NULL)
    return((LPSTR)str);
  else
    return(inStr);
}

void
SetFooter(LPSTR msg)
{
  extern HWND   hWnd;

  SetDlgItemText(hWnd, ID_STATIC_RESULT, msg);
}

LPSTR
GetNABError(LONG errorCode)
{
  static char FAR msg[128];

  switch (errorCode) 
  {
  case NAB_SUCCESS:
    lstrcpy(msg, "The API call succeeded.");
    break;
  case NAB_FAILURE:
    lstrcpy(msg, "This is a generic NAB API failure.");
    break;
  case NAB_INVALID_CONNID:
    lstrcpy(msg, "Invalid Connection ID for request");
    break;
  case NAB_NOT_OPEN: 
    lstrcpy(msg, "This will be returned when a call is made to the API without NAB_Open() being called first.");
    break;
  case NAB_NOT_FOUND:
    lstrcpy(msg, "The requested information was not found.");
    break;
  case NAB_PERMISSION_DENIED:
    lstrcpy(msg, "User does not have write permission to perform the operation");
    break;
  case NAB_DISK_FULL:
    lstrcpy(msg, "Disk full condition encountered attempting to perform the operation");
    break;
  case NAB_INVALID_NAME:
    lstrcpy(msg, "The name specified for the operation is invalid");
    break;
  case NAB_INVALID_ABOOK:
    lstrcpy(msg, "The address book specified is invalid");
    break;
  case NAB_FILE_NOT_FOUND:
    lstrcpy(msg, "The disk file for the operation was not found");
    break;
  case NAB_ALREADY_EXISTS:
    lstrcpy(msg, "The user entry already exists in the address book being updated");
    break;
  case NAB_MAXCON_EXCEEDED:
    lstrcpy(msg, "The maximum number of API connections has been exceeded");
    break;
  case NAB_MEMORY_FAILURE:
    lstrcpy(msg, "Memory condition, such as malloc() failures");
    break;
  case NAB_FILE_ERROR:
    lstrcpy(msg, "Error on file creation");
    break;
  case NAB_INVALID_ENTRY:
    lstrcpy(msg, "Invalid entry for operation");
    break;
  case NAB_MULTIPLE_USERS_FOUND:
    lstrcpy(msg, "More than one person found with current search criteria");
    break;
  case NAB_INVALID_SEARCH_ATTRIB: 
    lstrcpy(msg, "Invalid search criteria");
    break;
  default:
    strcpy(msg, "Unknown NAB Return Code");
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
OpenNAB(void)
{
#ifdef WIN16
	m_hInstNAB = LoadLibrary("Y:\\ns\\cmd\\winfe\\nabapi\\NABAPI.DLL");	
#else
  m_hInstNAB = LoadLibrary("Y:\\ns\\cmd\\winfe\\nabapi\\x86Dbg\\NABAPI32.DLL");
  // m_hInstNAB = LoadLibrary("NABAPI32.DLL");
#endif

  if (!m_hInstNAB)
  {
    ShowMessage(NULL, "Error Loading the Address Book DLL...Probably not found!");
    return(FALSE);
  }

  return(TRUE);
}

void
CloseNAB(void)
{
  if(m_hInstNAB)
  {
    FreeLibrary(m_hInstNAB);
  }
}

int
GetCurrentABook(HWND hWnd, NABAddrBookDescType *abInfo)
{
  char abookInfo[512];

  DWORD selected = ListBox_GetCurSel(GetDlgItem(hWnd, ID_LIST_ABOOK));
  if (selected == LB_ERR)
  {
    ShowMessage(hWnd, "Select a valid address book for this operation\nYou may have to perform a Get AB List operation.");
    return(0);
  }

  ListBox_GetText(GetDlgItem(hWnd, ID_LIST_ABOOK), selected, abookInfo);
  char *ptr = strchr(abookInfo, '|');
  if (!ptr)
    return(0);

  --ptr;
  *ptr = '\0';
  abInfo->description = strdup(abookInfo);
  *ptr = ' ';
  ptr += 3;
  abInfo->fileName = strdup(ptr);
  return(1);
}

void
DoNAB_Open(HWND hWnd)
{
  char  msg[256];

  UINT_16           majorVerNumber;
  UINT_16           minorVerNumber;

  // Get Address of NAB function...
  NABError (FAR PASCAL *lpfnNAB_Open)(NABConnectionID *id, UINT_16 *majorVerNumber, UINT_16 *minorVerNumber);  

#ifdef WIN16		
  (FARPROC&) lpfnNAB_Open = GetProcAddress(m_hInstNAB, "NAB_OPEN"); 
#else
  (FARPROC&) lpfnNAB_Open = GetProcAddress(m_hInstNAB, "NAB_Open"); 
#endif
  
  if (!lpfnNAB_Open)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  LONG  rc = (*lpfnNAB_Open)(  &nabConn, &majorVerNumber, &minorVerNumber );
  if (rc == NAB_SUCCESS)
  {
    wsprintf(msg, "Success with connection id = %d", nabConn);
    ShowMessage(hWnd, msg);
    SetFooter("Logon success");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from Logon\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("Open failed");
  }
}

void
DoNAB_Close(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_Close) ( NABConnectionID id );  

#ifdef WIN16		
  (FARPROC&) lpfnNAB_Close = GetProcAddress(m_hInstNAB, "NAB_CLOSE"); 
#else
  (FARPROC&) lpfnNAB_Close = GetProcAddress(m_hInstNAB, "NAB_Close"); 
#endif
  
  if (!lpfnNAB_Close)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char  msg[256];
  NABError rc = (*lpfnNAB_Close)( nabConn );
  if (rc == NAB_SUCCESS)
  {
    wsprintf(msg, "Successful Close");
    ShowMessage(hWnd, msg);
    SetFooter(msg);
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from Close\nError=[%s]", 
                      rc, GetNABError(rc));

    ShowMessage(hWnd, msg);
    SetFooter("Close failed");
  }

  nabConn = 0;
}

void
DoNAB_FreeMemory(HWND hWnd, LPVOID buf, BOOL alert)
{
  NABError (FAR PASCAL *lpfnNABFreeMemory) (void *ptr);

#ifdef WIN16		
  (FARPROC&) lpfnNABFreeMemory = GetProcAddress(m_hInstNAB, "NAB_FREEMEMORY"); 
#else
  (FARPROC&) lpfnNABFreeMemory = GetProcAddress(m_hInstNAB, "NAB_FreeMemory"); 
#endif
  
  if (!lpfnNABFreeMemory)
  {
    ShowMessage(hWnd, "Unable to locate NAB Function.");
    return;
  }

  char  msg[256];
  LONG  rc = (*lpfnNABFreeMemory)(buf);
  if (rc == NAB_SUCCESS)
  {
    wsprintf(msg, "Successful Free Memory Operation");
    if (alert)
      ShowMessage(hWnd, msg);
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from NAB_FreeMemory\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
  }
}

void
DoNAB_GetAddressBookList(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_GetAddressBookList) 
                             (NABConnectionID id, UINT_32 *abookCount,
                              NABAddrBookDescType **aBooks[]);  
#ifdef WIN16		
  (FARPROC&) lpfnNAB_GetAddressBookList = GetProcAddress(m_hInstNAB, "NAB_GETADDRESSBOOKLIST"); 
#else
  (FARPROC&) lpfnNAB_GetAddressBookList = GetProcAddress(m_hInstNAB, "NAB_GetAddressBookList"); 
#endif
  
  if (!lpfnNAB_GetAddressBookList)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char      msg[512];
  UINT_32   abookCount;
  NABAddrBookDescType **descType; 

  // Clear the list before we start...
  ListBox_ResetContent(GetDlgItem(hWnd, ID_LIST_ABOOK));
  NABError rc = (*lpfnNAB_GetAddressBookList)( nabConn, &abookCount, &descType );

  if (rc == NAB_SUCCESS)
  {
    for (DWORD i=0; i<abookCount; i++)
    {
      wsprintf(msg, "%s | %s", CheckNull(descType[i]->description), 
                               CheckNull(descType[i]->fileName));
      ListBox_InsertString(GetDlgItem(hWnd, ID_LIST_ABOOK), 0, msg);
      ListBox_SetHorizontalExtent(GetDlgItem(hWnd, ID_LIST_ABOOK), 2048);      
      DoNAB_FreeMemory(hWnd, descType[i], FALSE);
    }    

    ShowMessage(hWnd, "Successful Call to Address Book List.");
    SetFooter("Successful Call to Address Book List");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from Address Book List\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("GetAddressBookList failed");
  }
}

void
DoNAB_GetDefaultAddressBook(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_GetDefaultAddressBook) (NABConnectionID id, NABAddrBookDescType **aBook);  
#ifdef WIN16		
  (FARPROC&) lpfnNAB_GetDefaultAddressBook = GetProcAddress(m_hInstNAB, "NAB_GETDEFAULTADDRESSBOOK"); 
#else
  (FARPROC&) lpfnNAB_GetDefaultAddressBook = GetProcAddress(m_hInstNAB, "NAB_GetDefaultAddressBook"); 
#endif
  
  if (!lpfnNAB_GetDefaultAddressBook)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char      msg[512];
  NABAddrBookDescType *descType; 

  NABError rc = (*lpfnNAB_GetDefaultAddressBook)( nabConn, &descType );
  if (rc == NAB_SUCCESS)
  {
    wsprintf(msg, "%s | %s", CheckNull(descType->description), 
                             CheckNull(descType->fileName));
    ListBox_InsertString(GetDlgItem(hWnd, ID_LIST_ABOOK), 0, msg);
    ListBox_SetHorizontalExtent(GetDlgItem(hWnd, ID_LIST_ABOOK), 2048);

    wsprintf(msg, "Default Address Book:\nName=[%s]\nFile=%s", 
              CheckNull(descType->description), 
              CheckNull(descType->fileName));    
    DoNAB_FreeMemory(hWnd, descType, FALSE);
    ShowMessage(hWnd, msg);
    SetFooter("Found Default");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from Default Address Book\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("GetDefaultAddressBook failed");
  }
}

void
DoNAB_CreatePersonalAddressBook(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_CreatePersonalAddressBook) (NABConnectionID id, char *addrBookName, NABAddrBookDescType **addrBook); 

#ifdef WIN16		
  (FARPROC&) lpfnNAB_CreatePersonalAddressBook = GetProcAddress(m_hInstNAB, "NAB_CREATEPERSONALADDRESSBOOK"); 
#else
  (FARPROC&) lpfnNAB_CreatePersonalAddressBook = GetProcAddress(m_hInstNAB, "NAB_CreatePersonalAddressBook"); 
#endif
  
  if (!lpfnNAB_CreatePersonalAddressBook)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char                  abookName[256];
  char                  msg[256];
  NABAddrBookDescType   *abookDesc; 

  GetDlgItemText(hWnd, ID_EDIT_ABOOKNAME, abookName, sizeof(abookName));
  if (abookName[0] == '\0')
  {
    ShowMessage(hWnd, "Enter a name for the new address book!");
    return;
  }

  // Clear the list before we start...
  NABError rc = (*lpfnNAB_CreatePersonalAddressBook)
                         (nabConn, abookName, &abookDesc); 
  if (rc == NAB_SUCCESS)
  {
    wsprintf(msg, "New Address Book:\nName=[%s]\nFile=%s", 
              CheckNull(abookDesc->description), 
              CheckNull(abookDesc->fileName));    
    DoNAB_FreeMemory(hWnd, abookDesc, TRUE);
    ShowMessage(hWnd, msg);
    SetFooter("Created successfully");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from NAB_CreatePersonalAddressBook\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("NAB_CreatePersonalAddressBook failed");
  }
}

void
DoNAB_SaveAddressBookToHTML(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_SaveAddressBookToHTML) (NABConnectionID id, NABAddrBookDescType *addrBook, 
                                           char *fileName); 
#ifdef WIN16		
  (FARPROC&) lpfnNAB_SaveAddressBookToHTML = GetProcAddress(m_hInstNAB, "NAB_SAVEADDRESSBOOKTOHTML"); 
#else
  (FARPROC&) lpfnNAB_SaveAddressBookToHTML = GetProcAddress(m_hInstNAB, "NAB_SaveAddressBookToHTML"); 
#endif
  
  if (!lpfnNAB_SaveAddressBookToHTML)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char                  fname[256];
  char                  msg[256];
  NABAddrBookDescType   abookDesc; 

  GetDlgItemText(hWnd, ID_EDIT_FNAME, fname, sizeof(fname));
  if (fname[0] == '\0')
  {
    ShowMessage(hWnd, "Enter a valid file name!");
    return;
  }

  // Get the selected address book...
  if (!GetCurrentABook(hWnd, &abookDesc))
  {
    return;
  }

  NABError rc = (*lpfnNAB_SaveAddressBookToHTML)
                        (nabConn, &abookDesc, fname);

  if (rc == NAB_SUCCESS)
  {
    ShowMessage(hWnd, "Created HTML file successfully.");
    SetFooter("Created HTML file successfully");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from NAB_SaveAddressBookToHTML\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("NAB_SaveAddressBookToHTML failed");
  }
}

void
DoNAB_ExportLDIFFile(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_ExportLDIFFile) 
          (NABConnectionID id, NABAddrBookDescType *addrBook, char *fileName);    

#ifdef WIN16		
  (FARPROC&) lpfnNAB_ExportLDIFFile = GetProcAddress(m_hInstNAB, "NAB_EXPORTLDIFFILE"); 
#else
  (FARPROC&) lpfnNAB_ExportLDIFFile = GetProcAddress(m_hInstNAB, "NAB_ExportLDIFFile"); 
#endif
  
  if (!lpfnNAB_ExportLDIFFile)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char                  fname[256];
  char                  msg[256];
  NABAddrBookDescType   abookDesc; 

  GetDlgItemText(hWnd, ID_EDIT_FNAME, fname, sizeof(fname));
  if (fname[0] == '\0')
  {
    ShowMessage(hWnd, "Enter a valid file name!");
    return;
  }

  // Get the selected address book...
  if (!GetCurrentABook(hWnd, &abookDesc))
  {
    return;
  }

  NABError rc = (*lpfnNAB_ExportLDIFFile)
                        (nabConn, &abookDesc, fname);

  if (rc == NAB_SUCCESS)
  {
    ShowMessage(hWnd, "Created LDIF file successfully.");
    SetFooter("Created LDIF file successfully");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from NAB_ExportLDIFFile\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("NAB_ExportLDIFFile failed");
  }
}

void
DoNAB_ImportLDIFFile(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_ImportLDIFFile) 
           (NABConnectionID id, NABAddrBookDescType *addrBook, char *fileName,
            NABBool deleteFileWhenFinished);    

#ifdef WIN16		
  (FARPROC&) lpfnNAB_ImportLDIFFile = GetProcAddress(m_hInstNAB, "NAB_IMPORTLDIFFILE"); 
#else
  (FARPROC&) lpfnNAB_ImportLDIFFile = GetProcAddress(m_hInstNAB, "NAB_ImportLDIFFile"); 
#endif
  
  if (!lpfnNAB_ImportLDIFFile)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char                  fname[256];
  char                  msg[256];
  NABAddrBookDescType   abookDesc; 

  GetDlgItemText(hWnd, ID_EDIT_FNAME, fname, sizeof(fname));
  if (fname[0] == '\0')
  {
    ShowMessage(hWnd, "Enter a valid file name!");
    return;
  }

  // Get the selected address book...
  if (!GetCurrentABook(hWnd, &abookDesc))
  {
    return;
  }

  NABError rc = (*lpfnNAB_ImportLDIFFile)
                        (nabConn, &abookDesc, fname, FALSE);

  if (rc == NAB_SUCCESS)
  {
    ShowMessage(hWnd, "Imported LDIF file successfully.");
    SetFooter("Imported LDIF file successfully");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from NAB_ImportLDIFFile\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("NAB_ImportLDIFFile failed");
  }
}

void    
DoNAB_GetFirstAddressBookEntry(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_GetFirstAddressBookEntry) 
           (NABConnectionID id, NABAddrBookDescType *addrBook, NABUserID *userID, 
            char **ldifEntry, NABUpdateTime *updTime);    
  NABError (FAR PASCAL *lpfnNAB_GetNextAddressBookEntry) 
           (NABConnectionID id, NABUserID *userID, 
            char **ldifEntry, NABUpdateTime *updTime);    

#ifdef WIN16		
  (FARPROC&) lpfnNAB_GetFirstAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_GETFIRSTADDRESSBOOKENTRY"); 
  (FARPROC&) lpfnNAB_GetNextAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_GETNEXTADDRESSBOOKENTRY"); 
#else
  (FARPROC&) lpfnNAB_GetFirstAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_GetFirstAddressBookEntry"); 
  (FARPROC&) lpfnNAB_GetNextAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_GetNextAddressBookEntry"); 
#endif
  
  if ((!lpfnNAB_GetFirstAddressBookEntry) || (!lpfnNAB_GetNextAddressBookEntry))
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char                  msg[128];
  NABAddrBookDescType   abookDesc; 
  NABUserID             userID;
  char                  *ldifInfo;
  NABUpdateTime         updTime;
  DWORD                 totalCount = 0;

  // Get the selected address book...
  if (!GetCurrentABook(hWnd, &abookDesc))
  {
    return;
  }

  // Reset the list...
  ListBox_ResetContent(GetDlgItem(hWnd, ID_LIST_USERS));      

  NABError rc = (*lpfnNAB_GetFirstAddressBookEntry)
                (nabConn, &abookDesc, &userID, &ldifInfo, &updTime);    
  while (rc == NAB_SUCCESS)
  {
    char *tmp;

    if (strlen(ldifInfo) <= 0)
      goto NEXT;

    tmp = (char *)malloc(strlen(ldifInfo) + 128);
    if (!tmp)
      goto NEXT;

    wsprintf(tmp, "U=%d T=%d|%s", userID, updTime, ldifInfo);
    ListBox_InsertString(GetDlgItem(hWnd, ID_LIST_USERS), 0, tmp);
    ListBox_SetHorizontalExtent(GetDlgItem(hWnd, ID_LIST_USERS), 2048);
    free(tmp);
    ++totalCount;

NEXT:
    rc = (*lpfnNAB_GetNextAddressBookEntry)
                  (nabConn, &userID, &ldifInfo, &updTime);    
  }

  
  wsprintf(msg, "%d total entries found.", totalCount); 
  ShowMessage(hWnd, msg);  
}

void    
DoNAB_FindAddressBookEntry(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_FindAddressBookEntry) 
                  (NABConnectionID id, NABAddrBookDescType *addrBook, 
                   NABUserID *userID, char *ldifSearchAttribute, 
                   char **ldifEntry, NABUpdateTime *updTime);    
#ifdef WIN16		
  (FARPROC&) lpfnNAB_FindAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_FINDADDRESSBOOKENTRY"); 
#else
  (FARPROC&) lpfnNAB_FindAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_FindAddressBookEntry"); 
#endif
  
  if (!lpfnNAB_FindAddressBookEntry)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char                  ldif[256] = "";
  char                  msg[256];
  NABAddrBookDescType   abookDesc; 
  char                  abid[32] = "";

  NABUserID       userID;
  char            *ldifEntry;
  NABUpdateTime   updTime;    
  NABError        rc;


  GetDlgItemText(hWnd, ID_EDIT_FINDID, abid, sizeof(abid));
  if (abid[0] != '\0')
  {
    userID = atoi(abid);  
  }
  else
  {
    GetDlgItemText(hWnd, ID_EDIT_FINDLDIF, ldif, sizeof(ldif));
    if (ldif[0] == '\0')
    {
      ShowMessage(hWnd, "Enter an LDIF search criteria!");
      return;
    }
  }

  ListBox_ResetContent(GetDlgItem(hWnd, ID_LIST_USERS));      

  // Get the selected address book...
  if (!GetCurrentABook(hWnd, &abookDesc))
  {
    rc = (*lpfnNAB_FindAddressBookEntry)
                  (nabConn, NULL, &userID, ldif,
                  &ldifEntry, &updTime);  
  }
  else
  {
    rc = (*lpfnNAB_FindAddressBookEntry)
                  (nabConn, &abookDesc, &userID, ldif,
                  &ldifEntry, &updTime);
  }

  if (rc == NAB_SUCCESS)
  {
    char *tmp;

    if (strlen(ldifEntry) <= 0)
    {
      ShowMessage(hWnd, "Memory allocation problem");
      return;
    }

    tmp = (char *)malloc(strlen(ldifEntry) + 128);
    if (!tmp)
    {
      ShowMessage(hWnd, "Memory allocation problem");
      return;
    }

    wsprintf(tmp, "U=%d T=%d|%s", userID, updTime, ldifEntry);
    ListBox_InsertString(GetDlgItem(hWnd, ID_LIST_USERS), 0, tmp);
    ListBox_SetHorizontalExtent(GetDlgItem(hWnd, ID_LIST_USERS), 2048);    
    free(tmp);
    SetFooter("NAB_FindAddressBookEntry succeeded!");
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from FindEntry\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("NAB_FindAddressBookEntry failed");
  }
}

void    
DoNAB_InsertAddressBookEntry(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_InsertAddressBookEntry) 
                (NABConnectionID id, NABAddrBookDescType *addrBook, 
                 char *ldifEntry, NABUserID *userID);
#ifdef WIN16		
  (FARPROC&) lpfnNAB_InsertAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_INSERTADDRESSBOOKENTRY"); 
#else
  (FARPROC&) lpfnNAB_InsertAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_InsertAddressBookEntry"); 
#endif
  
  if (!lpfnNAB_InsertAddressBookEntry)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char                  ldif[256];
  char                  msg[256];
  NABAddrBookDescType   abookDesc; 

  GetDlgItemText(hWnd, ID_EDIT_FINDLDIF, ldif, sizeof(ldif));
  if (ldif[0] == '\0')
  {
    ShowMessage(hWnd, "Enter an LDIF insert line!");
    return;
  }

  // Get the selected address book...
  if (!GetCurrentABook(hWnd, &abookDesc))
  {
    return;
  }
  
  NABUserID   id;
  NABError rc = (*lpfnNAB_InsertAddressBookEntry)
                        (nabConn, &abookDesc, ldif, &id);
  if (rc == NAB_SUCCESS)
  {
    ShowMessage(hWnd, "NAB_InsertAddressBookEntry succeeded");
    DoNAB_GetFirstAddressBookEntry(hWnd);    
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from NAB_InsertAddressBookEntry\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("NAB_InsertAddressBookEntry failed");
  }
}

NABUserID
GetUserID(HWND hWnd)
{
  char      userInfo[512];
  NABUserID userID;

  DWORD selected = ListBox_GetCurSel(GetDlgItem(hWnd, ID_LIST_USERS));
  if (selected == LB_ERR)
  {
    ShowMessage(hWnd, "Select an entry to update for this operation.");
    return 0;
  }
  ListBox_GetText(GetDlgItem(hWnd, ID_LIST_USERS), selected, userInfo);

  char  *begPtr, *endPtr;
  begPtr = strchr(userInfo, '=');
  endPtr = strchr(userInfo, ' ');
  if (!endPtr)
  {
    ShowMessage(hWnd, "Unable to locate the User ID for operation.");
    return 0;
  }

  *endPtr = '\0';
  ++begPtr;
  userID = atoi( begPtr );
  *endPtr = ' ';
  return(userID);
}

void    
DoNAB_UpdateAddressBookEntry(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_UpdateAddressBookEntry) 
              (NABConnectionID id, NABAddrBookDescType *addrBook, 
               NABUserID userID, char *ldifEntry);  

#ifdef WIN16		
  (FARPROC&) lpfnNAB_UpdateAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_UPDATEADDRESSBOOKENTRY"); 
#else
  (FARPROC&) lpfnNAB_UpdateAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_UpdateAddressBookEntry"); 
#endif
  
  if (!lpfnNAB_UpdateAddressBookEntry)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char                  ldif[512];
  char                  msg[256];
  NABAddrBookDescType   abookDesc; 

  GetDlgItemText(hWnd, ID_EDIT_FINDLDIF, ldif, sizeof(ldif));
  if (ldif[0] == '\0')
  {
    ShowMessage(hWnd, "Enter an LDIF update line!");
    return;
  }

  // Get the selected address book...
  if (!GetCurrentABook(hWnd, &abookDesc))
  {
    return;
  }

  NABUserID userID = GetUserID(hWnd);
  if (userID == 0)
    return;
  NABError rc = (*lpfnNAB_UpdateAddressBookEntry)
                        (nabConn, &abookDesc, userID, ldif);
  if (rc == NAB_SUCCESS)
  {
    ShowMessage(hWnd, "NAB_InsertAddressBookEntry succeeded");
    DoNAB_GetFirstAddressBookEntry(hWnd);    
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from NAB_InsertAddressBookEntry\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("NAB_InsertAddressBookEntry failed");
  }
}

void    
DoNAB_DeleteAddressBookEntry(HWND hWnd)
{
  NABError (FAR PASCAL *lpfnNAB_DeleteAddressBookEntry) 
                  (NABConnectionID id, NABAddrBookDescType *addrBook, 
                   NABUserID userID); 
#ifdef WIN16		
  (FARPROC&) lpfnNAB_DeleteAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_DELETEADDRESSBOOKENTRY"); 
#else
  (FARPROC&) lpfnNAB_DeleteAddressBookEntry = GetProcAddress(m_hInstNAB, "NAB_DeleteAddressBookEntry"); 
#endif
  
  if (!lpfnNAB_DeleteAddressBookEntry)
  {
    ShowMessage(hWnd, "Unable to locate NAB function.");
    return;
  }

  char                  msg[256];
  NABAddrBookDescType   abookDesc; 

  // Get the selected address book...
  if (!GetCurrentABook(hWnd, &abookDesc))
  {
    return;
  }

  NABUserID userID = GetUserID(hWnd);
  if (userID < 0)
  {
    ShowMessage(hWnd, "Unable to locate ABID");
    return;
  }

  NABError rc = (*lpfnNAB_DeleteAddressBookEntry)
                        (nabConn, &abookDesc, userID);
  if (rc == NAB_SUCCESS)
  {
    ShowMessage(hWnd, "NAB_DeleteAddressBookEntry succeeded");
    DoNAB_GetFirstAddressBookEntry(hWnd);
  }
  else
  {
    wsprintf(msg, "FAILURE: Return code %d from NAB_DeleteAddressBookEntry\nError=[%s]", 
                      rc, GetNABError(rc));
    ShowMessage(hWnd, msg);
    SetFooter("NAB_DeleteAddressBookEntry failed");
  }
}