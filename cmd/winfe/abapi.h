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
// This is a header file for the Address Book API support within 
// Communicator. 
//
// Written by: Rich Pizzarro (rhp@netscape.com)
//
#ifndef __ABAPI
#define __ABAPI

#include "nabapi.h"   // for all address book defines

// These are very important for compatibility going forward...
#define   NAB_VERSION_MAJOR     1  /* Major version number; changes with major code release number */
#define   NAB_VERSION_MINOR     0  /* Minor version number; changes with point release number.     */

#ifdef WIN16
typedef unsigned char	UCHAR;
#endif 

#define           MAX_NAME_LEN            256
#define           MAX_ADDRBOOK_NAME_LEN   512
#define           MAX_CON                 4     // Maximum NABAPI sessions supported
#define           MAX_POINTERS            32
#define           MAX_LDIF_SIZE           16000 // Max size for address book LDIF allocation stuff
#define           MAX_LDIFSEARCH_SIZE     4096  // Max size for address book LDIF allocation stuff

#ifndef NABAPIDLL

#include "abcom.h"    // For MSG_ & AB_ stuff...
#include "msgcom.h"

//
// The Address Book class that will act as the internal mechanism for 
// Communicator to control multiple Address Book sessions.
//
class CNABConnection 
{
protected:
  LONG        m_ID;
  BOOL        m_defaultConnection;  
  LONG        m_sessionCount;
  LONG        m_nameIndex;
  LONG        m_nameTotalCount;

  // Methods
public:
  MSG_Pane		      *m_ContainerPane;      // Container Pane...
  AB_ContainerInfo  **m_ContainerArray;    // The array of container info pointers...
  LONG              m_ContainerArrayCount; // The count of the number of 
                                           // m_ContainerArray entries
  MSG_Pane          *m_AddressBookPane;    // Container info for a particular addr book

  CNABConnection  ( LONG );
  ~CNABConnection ( );

  // ID related methods
  LONG   GetID( ) { return m_ID; } ;

  // Dealing with the default session...
  BOOL    IsDefault( ) { return m_defaultConnection; } ;
  void    SetDefault( BOOL flag ) { m_defaultConnection = flag; } ;

  // For handling multiple sessions on a profile name...
  LONG    GetSessionCount( ) { return m_sessionCount; } ;
  void    IncrementSessionCount() { ++m_sessionCount; } ;
  void    DecrementSessionCount() { --m_sessionCount; } ;

  // For enumerating names...
  void    SetNameIndex( LONG mIndex ) { m_nameIndex = mIndex; } ;
  void    IncrementNameIndex( ) { ++m_nameIndex; } ;
  LONG    GetNameIndex( ) { return m_nameIndex; };

  void    SetNameTotalCount( LONG mTotal ) { m_nameTotalCount = mTotal; } ;
  LONG    GetNameTotalCount( ) { return m_nameTotalCount; };

  // For AB Calls...
  BOOL              RefreshABContainerList ( void );
  AB_ContainerInfo  *FindContainerInfo( LPSTR, LPSTR );


};

#endif // NABAPIDLL

//
// Defines needed for requests being made with the WM_COPYDATA call...
//
typedef enum {
    NSCP_NABStartRequestID = 200,
    NSCP_NAB_Open,
    NSCP_NAB_Close,
    NSCP_NAB_GetAddressBookList,
    NSCP_NAB_GetDefaultAddressBook,
    NSCP_NAB_CreatePersonalAddressBook,
    NSCP_NAB_FreeMemory,
    NSCP_NAB_SaveAddressBookToHTML,
    NSCP_NAB_ImportLDIFFile,
    NSCP_NAB_ExportLDIFFile,
    NSCP_NAB_GetFirstAddressBookEntry,
    NSCP_NAB_GetNextAddressBookEntry,
    NSCP_NAB_FindAddressBookEntry,
    NSCP_NAB_InsertAddressBookEntry,
    NSCP_NAB_UpdateAddressBookEntry,
    NSCP_NAB_DeleteAddressBookEntry,
    NSCP_NABEndRequestID       // Note: this is a marker for NAB IPC requests
} NSCP_NAB_IPC_REQUEST;

//
// This is to keep track of the pointers allocated in the MAPI DLL
// and deal with them correctly.
//
#define   NAB_FLAT_MEMORY         0
#define   NAB_ADDRBOOK_STRUCT     1

typedef struct {
  LPVOID    lpMem;
  char      memType;
} nabMemTrackerType;

//
// This is the generic message that WM_COPYDATA will send to the
// Communicator product to allow it to attach to shared memory.
// NOTE: On Win16, this will simply reference a pointer.
//
typedef struct {
  char    smemName[64]; // Name of shared memory
  DWORD   smemSize;     // Size of shared memory
  LPVOID  lpsmem;       // Will be really used in Win16 only
} NABIPCType;

//
// These are message specific structures that will be used for 
// the various Address Book operations.
//
typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  UINT_16         majorVerNumber;
  UINT_16         minorVerNumber;
} NAB_OpenType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
} NAB_CloseType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  UINT_32         abookCount;
  char            blobFileName[_MAX_PATH];    // file name N abooks on disk
} NAB_GetAddressBookListType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  BOOL            defaultFound;
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  char            abookDesc[MAX_ADDRBOOK_NAME_LEN]; // description of address book
  char            abookFileName[_MAX_PATH];         // file name of abooks on disk
} NAB_GetDefaultAddressBookType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  char            abookName[MAX_ADDRBOOK_NAME_LEN]; // description of address book
  char            abookFileName[_MAX_PATH];         // file name of abooks on disk
} NAB_CreatePersonalAddressBookType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  char            abookName[MAX_ADDRBOOK_NAME_LEN]; // description of address book
  char            abookFileName[_MAX_PATH];         // file name of abooks on disk
  // Return stuff
  char            HTMLFileName[_MAX_PATH];          // saved HTML file name
} NAB_SaveAddressBookToHTMLType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  BOOL            deleteImportFile;
  char            LDIFFileName[_MAX_PATH];          // LDIF file to import
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  char            abookName[MAX_ADDRBOOK_NAME_LEN]; // description of address book
  char            abookFileName[_MAX_PATH];         // file name of abooks on disk
} NAB_ImportLDIFFileType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  char            LDIFFileName[_MAX_PATH];          // LDIF file to export
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  char            abookName[MAX_ADDRBOOK_NAME_LEN]; // description of address book
  char            abookFileName[_MAX_PATH];         // file name of abooks on disk
} NAB_ExportLDIFFileType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  char            abookName[MAX_ADDRBOOK_NAME_LEN]; // description of address book
  char            abookFileName[_MAX_PATH];         // file name of abooks on disk
  // Returned Values
  NABUserID       userID;
  NABUpdateTime   updateTime;
  char            userInfo[MAX_LDIF_SIZE];
} NAB_GetFirstAddressBookEntryType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  // Returned Values
  NABUserID       userID;
  NABUpdateTime   updateTime;
  char            userInfo[MAX_LDIF_SIZE];
} NAB_GetNextAddressBookEntryType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  char            abookName[MAX_ADDRBOOK_NAME_LEN]; // description of address book
  char            abookFileName[_MAX_PATH];         // file name of abooks on disk
  // Input Values
  char            ldifSearchAttribute[MAX_LDIFSEARCH_SIZE];
  // Output values
  NABUserID       userID;
  NABUpdateTime   updateTime;
  char            userInfo[MAX_LDIF_SIZE];
} NAB_FindAddressBookEntryType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  char            abookName[MAX_ADDRBOOK_NAME_LEN]; // description of address book
  char            abookFileName[_MAX_PATH];         // file name of abooks on disk
  // Insert Value
  char            userInfo[MAX_LDIF_SIZE];
  // Update Value
  NABUserID       userID;
} NAB_InsertAddressBookEntryType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  char            abookName[MAX_ADDRBOOK_NAME_LEN]; // description of address book
  char            abookFileName[_MAX_PATH];         // file name of abooks on disk
  // Update Value
  NABUserID       userID;
  char            userInfo[MAX_LDIF_SIZE];
} NAB_UpdateAddressBookEntryType;

typedef struct {
  DWORD           ipcWorked;      // Necessary for IPC check with Communicator
  NABConnectionID abConnection;
  // NABAddrBookDescType flattened
  char            reserved[16];                     // future use
  char            abookName[MAX_ADDRBOOK_NAME_LEN]; // description of address book
  char            abookFileName[_MAX_PATH];         // file name of abooks on disk
  // Delete user Value
  NABUserID       userID;
} NAB_DeleteAddressBookEntryType;

#endif    // __ABAPI
