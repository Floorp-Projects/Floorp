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
//  Address Book API Hooks for Communicator
//  Note: THIS LIVES IN COMMUNICATOR THOUGH IT IS IN THE ABAPI FOR
//        BUILD REASONS
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  March 1998

#ifdef NSCPNABAPIDLL    // Is this part of the DLL or Communicator?
#include <windows.h>
#include <trace.h>
#else
#include "stdafx.h"
#endif

#include <stdio.h>    
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "wfemsg.h"     // for WFE_MSGGetMaster()
#include "nsstrseq.h"
#include "abapi.h"
#include "abhook.h"
#include "nabapi.h"
#include "mapismem.h"
#include "hiddenfr.h"
//#include "msgcom.h"
#include "abcom.h"
#include "abutils.h"

extern CNetscapeApp theApp;

//
// Static defines for the Address Book support...
//
static CNABConnection *nabConnections[MAX_CON] = {NULL, NULL, NULL, NULL};

//
// Forward declarations...
//
LONG ProcessNAB_Open(NAB_OpenType *openPtr);
LONG ProcessNAB_Close(NAB_CloseType *closePtr);
LONG ProcessNAB_GetAddressBookList(NAB_GetAddressBookListType *getPtr);
LONG ProcessNAB_GetDefaultAddressBook(NAB_GetDefaultAddressBookType *getPtr);
LONG ProcessNAB_CreatePersonalAddressBook(NAB_CreatePersonalAddressBookType *createPtr);
LONG ProcessNAB_SaveAddressBookToHTML(NAB_SaveAddressBookToHTMLType *savePtr);
LONG ProcessNAB_ImportLDIFFile(NAB_ImportLDIFFileType *importPtr);
LONG ProcessNAB_ExportLDIFFile(NAB_ExportLDIFFileType *exportPtr);
LONG ProcessNAB_GetFirstAddressBookEntry(NAB_GetFirstAddressBookEntryType *getPtr);
LONG ProcessNAB_GetNextAddressBookEntry(NAB_GetNextAddressBookEntryType *getPtr);
LONG ProcessNAB_FindAddressBookEntry(NAB_FindAddressBookEntryType *findPtr);
LONG ProcessNAB_InsertAddressBookEntry(NAB_InsertAddressBookEntryType *insertPtr);
LONG ProcessNAB_UpdateAddressBookEntry(NAB_UpdateAddressBookEntryType *updatePtr);
LONG ProcessNAB_DeleteAddressBookEntry(NAB_DeleteAddressBookEntryType *deletePtr);

//
// This will store the CFrameWnd we startup for the Address Book API and if we 
// do start an instance of the browser, we will close it out when we leave.
//
static CFrameWnd  *pNABFrameWnd = NULL;

void 
StoreNABFrameWnd(CFrameWnd *pFrameWnd)
{
  pNABFrameWnd = pFrameWnd;
}

// This is the result of a WM_COPYDATA message. This will be used to send requests
// into Communicator for Address Book API requests.
//
// The description of the parameters coming into this call are:
//
//    wParam = (WPARAM) (HWND) hwnd;            // handle of sending window 
//    lParam = (LPARAM) (PCOPYDATASTRUCT) pcds; // pointer to structure with data 
//    context= needed for the context we will use in Communicator
//
//		typedef struct tagCOPYDATASTRUCT {  // cds  
//			DWORD dwData;			// the ID of the request
//			DWORD cbData;			// the size of the argument
//			PVOID lpData;			// Chunk of information defined specifically for each of the
//                        // Address Book API Calls.
//		} COPYDATASTRUCT; 
//
// Returns: The Address Book API Return code for the operation:
//
//
LONG 
ProcessNetscapeNABHook(WPARAM wParam, LPARAM lParam)
{
  PCOPYDATASTRUCT	  pcds = (PCOPYDATASTRUCT) lParam;
  NABIPCType       *ipcInfo; 
#ifdef WIN32   
  HANDLE            hSharedMemory;
#endif

	if (lParam == NULL)
	{
		return(NAB_FAILURE);
	}

  //
  // Get shared memory info structure pointer...
  //
  ipcInfo = (NABIPCType *)pcds->lpData;
  if (ipcInfo == NULL)
	{
		return(NAB_FAILURE);
	}

  //
  // Now connect to shared memory...or just set the
  // pointer for Win16
  //
#ifdef WIN32
  CSharedMem *sMem = NSOpenExistingSharedMemory((LPCTSTR) ipcInfo->smemName, &hSharedMemory);
  if (!sMem)
	{
		return(NAB_FAILURE);
	}
#else
  if (!ipcInfo->lpsmem)
  {
    return(NAB_FAILURE);
  }
#endif

  TRACE("NAB: NABHook Message ID = %d\n", pcds->dwData);
	switch (pcds->dwData) 
	{
  //////////////////////////////////////////////////////////////////////
  // NAB_Open
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_Open:
    {
     NAB_OpenType *openInfo;
#ifdef WIN32
    openInfo = (NAB_OpenType *) &(sMem->m_buf[0]);
#else    
    openInfo = (NAB_OpenType *) ipcInfo->lpsmem;
#endif

    if (!openInfo)
    {
      return(NAB_FAILURE);
    }
  
    openInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_Open(openInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NAB_Close
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_Close:
    {
     NAB_CloseType *closeInfo;
#ifdef WIN32
    closeInfo = (NAB_CloseType *) &(sMem->m_buf[0]);
#else    
    closeInfo = (NAB_CloseType *) ipcInfo->lpsmem;
#endif

    if (!closeInfo)
    {
      return(NAB_FAILURE);
    }
  
    closeInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_Close(closeInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NAB_GetAddressBookList
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_GetAddressBookList:
    {
     NAB_GetAddressBookListType *getInfo;
#ifdef WIN32
    getInfo = (NAB_GetAddressBookListType *) &(sMem->m_buf[0]);
#else    
    getInfo = (NAB_GetAddressBookListType *) ipcInfo->lpsmem;
#endif

    if (!getInfo)
    {
      return(NAB_FAILURE);
    }
  
    getInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_GetAddressBookList(getInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NAB_GetDefaultAddressBook
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_GetDefaultAddressBook:
    {
     NAB_GetDefaultAddressBookType *getInfo;
#ifdef WIN32
    getInfo = (NAB_GetDefaultAddressBookType *) &(sMem->m_buf[0]);
#else    
    getInfo = (NAB_GetDefaultAddressBookType *) ipcInfo->lpsmem;
#endif

    if (!getInfo)
    {
      return(NAB_FAILURE);
    }
  
    getInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_GetDefaultAddressBook(getInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NAB_CreatePersonalAddressBook
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_CreatePersonalAddressBook:
    {
     NAB_CreatePersonalAddressBookType *createInfo;
#ifdef WIN32
    createInfo = (NAB_CreatePersonalAddressBookType *) &(sMem->m_buf[0]);
#else    
    createInfo = (NAB_CreatePersonalAddressBookType *) ipcInfo->lpsmem;
#endif

    if (!createInfo)
    {
      return(NAB_FAILURE);
    }
  
    createInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_CreatePersonalAddressBook(createInfo));
    break;
    }
  
  //////////////////////////////////////////////////////////////////////
  // NAB_SaveAddressBookToHTML
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_SaveAddressBookToHTML:
    {
     NAB_SaveAddressBookToHTMLType *saveInfo;
#ifdef WIN32
    saveInfo = (NAB_SaveAddressBookToHTMLType *) &(sMem->m_buf[0]);
#else    
    saveInfo = (NAB_SaveAddressBookToHTMLType *) ipcInfo->lpsmem;
#endif

    if (!saveInfo)
    {
      return(NAB_FAILURE);
    }
  
    saveInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_SaveAddressBookToHTML(saveInfo));
    break;
    }
 
  //////////////////////////////////////////////////////////////////////
  // NAB_ImportLDIFFile
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_ImportLDIFFile:
    {
     NAB_ImportLDIFFileType *importInfo;
#ifdef WIN32
    importInfo = (NAB_ImportLDIFFileType *) &(sMem->m_buf[0]);
#else    
    importInfo = (NAB_ImportLDIFFileType *) ipcInfo->lpsmem;
#endif

    if (!importInfo)
    {
      return(NAB_FAILURE);
    }
  
    importInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_ImportLDIFFile(importInfo));
    break;
    }
    
  //////////////////////////////////////////////////////////////////////
  // NAB_ExportLDIFFile
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_ExportLDIFFile:
    {
     NAB_ExportLDIFFileType *exportInfo;
#ifdef WIN32
    exportInfo = (NAB_ExportLDIFFileType *) &(sMem->m_buf[0]);
#else    
    exportInfo = (NAB_ExportLDIFFileType *) ipcInfo->lpsmem;
#endif

    if (!exportInfo)
    {
      return(NAB_FAILURE);
    }
  
    exportInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_ExportLDIFFile(exportInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NAB_GetFirstAddressBookEntry
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_GetFirstAddressBookEntry:
    {
     NAB_GetFirstAddressBookEntryType *getInfo;
#ifdef WIN32
    getInfo = (NAB_GetFirstAddressBookEntryType *) &(sMem->m_buf[0]);
#else    
    getInfo = (NAB_GetFirstAddressBookEntryType *) ipcInfo->lpsmem;
#endif

    if (!getInfo)
    {
      return(NAB_FAILURE);
    }
  
    getInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_GetFirstAddressBookEntry(getInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NAB_GetNextAddressBookEntry
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_GetNextAddressBookEntry:
    {
     NAB_GetNextAddressBookEntryType *getInfo;
#ifdef WIN32
    getInfo = (NAB_GetNextAddressBookEntryType *) &(sMem->m_buf[0]);
#else    
    getInfo = (NAB_GetNextAddressBookEntryType *) ipcInfo->lpsmem;
#endif

    if (!getInfo)
    {
      return(NAB_FAILURE);
    }
  
    getInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_GetNextAddressBookEntry(getInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NAB_FindAddressBookEntry
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_FindAddressBookEntry:
    {
     NAB_FindAddressBookEntryType *findInfo;
#ifdef WIN32
    findInfo = (NAB_FindAddressBookEntryType *) &(sMem->m_buf[0]);
#else    
    findInfo = (NAB_FindAddressBookEntryType *) ipcInfo->lpsmem;
#endif

    if (!findInfo)
    {
      return(NAB_FAILURE);
    }
  
    findInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_FindAddressBookEntry(findInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NAB_InsertAddressBookEntry
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_InsertAddressBookEntry:
    {
     NAB_InsertAddressBookEntryType *insertInfo;
#ifdef WIN32
    insertInfo = (NAB_InsertAddressBookEntryType *) &(sMem->m_buf[0]);
#else    
    insertInfo = (NAB_InsertAddressBookEntryType *) ipcInfo->lpsmem;
#endif

    if (!insertInfo)
    {
      return(NAB_FAILURE);
    }
  
    insertInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_InsertAddressBookEntry(insertInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NAB_UpdateAddressBookEntry
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_UpdateAddressBookEntry:
    {
     NAB_UpdateAddressBookEntryType *updateInfo;
#ifdef WIN32
    updateInfo = (NAB_UpdateAddressBookEntryType *) &(sMem->m_buf[0]);
#else    
    updateInfo = (NAB_UpdateAddressBookEntryType *) ipcInfo->lpsmem;
#endif

    if (!updateInfo)
    {
      return(NAB_FAILURE);
    }
  
    updateInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_UpdateAddressBookEntry(updateInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NAB_DeleteAddressBookEntry
  //////////////////////////////////////////////////////////////////////
  case NSCP_NAB_DeleteAddressBookEntry:
    {
     NAB_DeleteAddressBookEntryType *deleteInfo;
#ifdef WIN32
    deleteInfo = (NAB_DeleteAddressBookEntryType *) &(sMem->m_buf[0]);
#else    
    deleteInfo = (NAB_DeleteAddressBookEntryType *) ipcInfo->lpsmem;
#endif

    if (!deleteInfo)
    {
      return(NAB_FAILURE);
    }
  
    deleteInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessNAB_DeleteAddressBookEntry(deleteInfo));
    break;
    }

  case NSCP_NAB_FreeMemory: // This should never hit Communicator, but if it does
    return(NAB_SUCCESS);    // Just return 

	default:
    return(NAB_FAILURE);    // Should never hit here!
		break;
	}

	return(NAB_SUCCESS);
}

//
// CNABConnections object...should be in their own file...
//
CNABConnection::CNABConnection  ( LONG id )
{
int   result;

  m_nameIndex = 0;
  m_nameTotalCount = 0;
  m_sessionCount = 1;
  m_defaultConnection = FALSE;
  m_ID = (id + 1);      // since zero is invalid, but have to make sure we 
                        // decrement by one when it is passed in again.
  m_nameIndex = -1;  // For tracing our way through the FindNext operation

  m_ContainerPane = NULL;   // Container Pane...
  m_ContainerArray = NULL; // The array of container info pointers...
  m_AddressBookPane = NULL;
  m_ContainerArrayCount = 0;

  result = AB_CreateContainerPane(&m_ContainerPane,
			                theApp.m_pAddressContext,
                			WFE_MSGGetMaster()) ;
  if (result)
    return;
  
  result = AB_InitializeContainerPane(m_ContainerPane);
	if (result) 
  {
		AB_ClosePane (m_ContainerPane);
		m_ContainerPane = NULL;    
    return;
	}

  RefreshABContainerList();
}

CNABConnection::~CNABConnection ( )
{
  AB_ClosePane( m_ContainerPane );
  m_ContainerPane = NULL;

  if (m_ContainerArray != NULL)
  {
    free(m_ContainerArray);
    m_ContainerArray = NULL;
  }

  m_ContainerArrayCount = 0;

  if (m_AddressBookPane != NULL)
  {
    AB_ClosePane(m_AddressBookPane);
    m_AddressBookPane = NULL;
  }
}

BOOL
CNABConnection::RefreshABContainerList ( void )
{
int result;

  if (m_ContainerArray != NULL)
  {
    free(m_ContainerArray);
    m_ContainerArray = NULL;
    m_ContainerArrayCount = 0;
  }

  // Do the querying stuff for AB API!
  result = AB_GetNumRootContainers(m_ContainerPane, &m_ContainerArrayCount);  // FE allocated. BE fills with the number of root containers 
  if (result) 
  {
		AB_ClosePane (m_ContainerPane);
    m_ContainerPane = NULL;    
		return FALSE;
	}

  if (m_ContainerArrayCount <= 0)
  {
    m_ContainerArrayCount = 0;
    if (m_ContainerArray != NULL)
    {
      free(m_ContainerArray);
      m_ContainerArray = NULL;
    }
    return TRUE;
  }

  m_ContainerArray = (AB_ContainerInfo **)
                  malloc(sizeof(AB_ContainerInfo *) * m_ContainerArrayCount);
  if (!m_ContainerArray) 
  {    
    m_ContainerArrayCount = 0;
		return FALSE;
	}

  result = AB_GetOrderedRootContainers(m_ContainerPane, m_ContainerArray, 
                                       &m_ContainerArrayCount);   
  if (result) 
  {
		AB_ClosePane (m_ContainerPane);
    m_ContainerPane = NULL;
    free(m_ContainerArray);
		return FALSE;
	}

  return TRUE;
}

AB_ContainerInfo *
CNABConnection::FindContainerInfo( LPSTR containerName, LPSTR containerFileName)
{
  int                     result;
  AB_ContainerAttribValue *value;

  // Check the input name...
  if ((!containerName) || (*containerName == '\0'))
    return(NULL);

  if ((!containerFileName) || (*containerFileName == '\0'))
    return(NULL);

  // Check object state for AB...
  if ( (!m_ContainerPane) || (m_ContainerArrayCount <= 0) || (!m_ContainerArray) )
    return(NULL);

  for (int j=0; j<m_ContainerArrayCount; j++)
  {
    // First check the type for the container...
    result = AB_GetContainerAttribute(m_ContainerArray[j], attribContainerType, &value);
    if (result) 
      continue;

    if (value->u.containerType != AB_PABContainer) // AB_LDAPContainer, AB_MListContainer,  a mailing list, AB_PABContainer 
    {
      AB_FreeContainerAttribValue(value);
      continue;
    }

    AB_FreeContainerAttribValue(value);

    // This is valid! Now get the info!
    result = AB_GetContainerAttribute(m_ContainerArray[j], attribName, &value);
    if (result) 
      continue;

    if ( (!value->u.string) || (!*(value->u.string)) )
    {
      AB_FreeContainerAttribValue(value);
      continue;
    }

    TRACE("CURRENT NAME IS = %s\n", (char *)value->u.string);
    if (strcmp(containerName, (char *)value->u.string) == 0)       
    {
      DIR_Server *dir = AB_GetDirServerForContainer(m_ContainerArray[j]);
      if ( (dir) && (dir->fileName != NULL) && (dir->fileName[0] != '\0') )
      {
        char *fullName;
        
        DIR_GetServerFileName(&fullName, dir->fileName);      
        if ( (fullName != NULL) && (fullName[0] != '\0') )
        {
          if ( strcmp(fullName, containerFileName) == 0)
          {
            XP_FREE(fullName);
            AB_FreeContainerAttribValue(value);
            return( m_ContainerArray[j] );
          }
          else
          {
            XP_FREE(fullName);
          }
        }
      }
    }

    AB_FreeContainerAttribValue(value);
  }

  return(NULL);
}

//
// Various utility functions...
//
static LPSTR
CheckNull(LPSTR inStr)
{
  static UCHAR  str[1];

  str[0] = '\0';
  if (inStr == NULL)
    return((LPSTR)str);
  else
    return(inStr);
}

static LONG
WriteTheMemoryBufferToDisk(LPSTR fName, UINT bufSize, LPSTR buf)
{
  if (!buf)
  {
    return(-1);
  }

  HFILE hFile = _lcreat(fName, 0);
  if (hFile == HFILE_ERROR)
  {
    return(-1);
  }

  UINT writeCount = _lwrite(hFile, buf, bufSize);
  _lclose(hFile);

  if (writeCount != bufSize)
  {
    _unlink(fName);
    return(-1);
  }

  return(0);
}

static LPSTR
GetTheTempDirectoryOnTheSystem(void)
{
 static UCHAR retPath[_MAX_PATH];
  
  if (getenv("TEMP"))
  {
    lstrcpy((LPSTR) retPath, getenv("TEMP"));  // environmental variable
  } 
  else if (getenv("TMP"))
  {
    lstrcpy((LPSTR) retPath, getenv("TMP"));  // How about this environmental variable?
  }
  else
  {
    GetWindowsDirectory((LPSTR) retPath, sizeof(retPath));
  }

  return((LPSTR) &(retPath[0]));
}

#ifdef WIN16
static int ISGetTempFileName(LPCSTR a_pDummyPath,  LPCSTR a_pPrefix, UINT a_uUnique, LPSTR a_pResultName)
{
#ifdef GetTempFileName	// we need the real thing comming up next...
#undef GetTempFileName
#endif
	return GetTempFileName(0, a_pPrefix, a_uUnique, a_pResultName);
}
#endif

static DWORD 
ValidateFile(LPCSTR szFile) 
{
  	struct _stat buf;
    int result;

    result = _stat( szFile, &buf );
    if (result != 0)
      return(1);

    if (!(buf.st_mode & S_IREAD))
      return(2);

    return(0);
}

static LONG
GetTempAttachmentName(LPSTR fName) 
{
  UINT        res;
  static UINT uUnique = 1;

  if (!fName)
    return(-1);

  LPSTR szTempPath = GetTheTempDirectoryOnTheSystem();

TRYAGAIN:
#ifdef WIN32
  res = GetTempFileName(szTempPath, "MAPI", uUnique++, fName);
#else
  res = ISGetTempFileName(szTempPath, "MAPI", uUnique++, fName);
#endif

  if (ValidateFile(fName) != 1) 
  {
    if (uUnique < 32000)
    {
      goto TRYAGAIN;
    }
    else
    {
      return(-1);
    }
  }

  return 0;
}

//
// Find the default session for Communicator...
//
static CNABConnection 
*GetDefaultSession(void)
{
  int i;
  
  for (i=0; i < MAX_CON; i++)
  {
    if (nabConnections[i] != NULL)
    {
      if (nabConnections[i]->IsDefault())
      {
        return nabConnections[i];
      }
    }
  }

  return(NULL);
}

//
// Find an open session slot...
//
static LONG
GetOpenSessionSlot( void )
{
  int i;
  
  for (i=0; i < MAX_CON; i++)
  {
    if (nabConnections[i] == NULL)
    {
      return(i);
    }
  }

  return(-1);
}

//
// Returns TRUE if it was reassigned and FALSE if not.
//
static BOOL
TryDefaultReassignment(void)
{
  int   i;
  int   loc = -1;

  // Find any sessions left?
  for (i=0; i < MAX_CON; i++)
  {
    if (nabConnections[i] != NULL)
    {
      loc = i;
      break;
    }
  }

  // Set default state on the object to true
  if (loc >= 0)
  {
    nabConnections[loc]->SetDefault(TRUE);
    return(TRUE);
  }
  else    
  {
    return(FALSE);
  }
}

//
// APISTART - Where it all begins...
//
LONG
ProcessNAB_Open(NAB_OpenType *openPtr)
{
  CNABConnection *defSession = GetDefaultSession();
  TRACE("MAPI: ProcessNAB_Open()\n");

  // Reset values...
  openPtr->abConnection = 0;
  openPtr->majorVerNumber = 0; 
  openPtr->minorVerNumber = 0;

  //
  // Make sure only MAX_CONN are allowed...
  //
  LONG  slot; 
  if ((slot = GetOpenSessionSlot()) == -1)
  {
    return(NAB_MAXCON_EXCEEDED);
  }

  //
  // Create a new session!
  //
  nabConnections[slot] = new CNABConnection(slot);
  if (!nabConnections[slot])
  {
    return(NAB_MEMORY_FAILURE);
  }

  // Assign the default session...
  if (defSession == NULL)
  {
    nabConnections[slot]->SetDefault(TRUE);
  }

  // Set the values the API needs to return...
  openPtr->abConnection = nabConnections[slot]->GetID();
  openPtr->majorVerNumber = NAB_VERSION_MAJOR; 
  openPtr->minorVerNumber = NAB_VERSION_MINOR;

  return(NAB_SUCCESS);
}

LONG    
ProcessNAB_Close(NAB_CloseType *closePtr)
{
  TRACE("MAPI: ProcessNAB_Close() Connection ID = [%d]\n", closePtr->abConnection);
  //
  // Verify the session handle...
  //
  if (( (closePtr->abConnection-1) >= MAX_CON) || ( (closePtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  CNABConnection *closeSession = nabConnections[(closePtr->abConnection - 1)];
  if (closeSession == NULL)
  {
    return(NAB_INVALID_CONNID);
  }
  
  //
  // Decrement the session count, then if this is the last one 
  // connected to this session, then kill it...
  //
  closeSession->DecrementSessionCount();
  if (closeSession->GetSessionCount() <= 0)
  {
    //
    // If this was the default session "holder", then we need to 
    // assign that task to a new one if it exists, if not, then
    // it is all back to null.
    //
    BOOL needToReassign = closeSession->IsDefault();

    delete closeSession;
    nabConnections[closePtr->abConnection - 1] = NULL;

    if (needToReassign)
    {
      TRACE("NAB: ProcessNAB_Close() Need to reassign default\n");
      if (!TryDefaultReassignment())
      {
        if (pNABFrameWnd != NULL)
        {
          pNABFrameWnd->PostMessage(WM_CLOSE);
          pNABFrameWnd = NULL;
        }
      }
    }
  }

  return(NAB_SUCCESS);
}

//
// Actually process the get address book list request...
//
LONG 
ProcessNAB_GetAddressBookList(NAB_GetAddressBookListType *getPtr)
{
  CNABConnection *mySession;

  TRACE("NAB: ProcessNAB_GetAddressBookList() Connection ID = [%d]\n", getPtr->abConnection);

  // Reset some values...
  getPtr->abookCount = 0;
  getPtr->blobFileName[0] = '\0';
  
  //
  // Verify the session handle...
  //
  if (((getPtr->abConnection-1) >= MAX_CON) || ((getPtr->abConnection-1) < 0))
    return(NAB_INVALID_CONNID);

  mySession = nabConnections[(getPtr->abConnection - 1)];
  if (mySession == NULL)
    return(NAB_FAILURE);

  TRACE("NAB: ProcessNAB_GetAddressBookList() Temp File = [%s]\n", getPtr->blobFileName); 
  LPSTR         *abookArray;
  LONG          realAbookCount = 0;
  int           result;

  if (mySession->RefreshABContainerList ( ) == FALSE)
		return(NAB_FAILURE);

  if (mySession->m_ContainerArrayCount <= 0)
    return(NAB_SUCCESS);

  if (!(mySession->m_ContainerArray)) 
		return(NAB_MEMORY_FAILURE);

  if (GetTempAttachmentName((LPSTR) getPtr->blobFileName) < 0)
    return(NAB_FILE_ERROR);

  int                     j;
  AB_ContainerAttribValue *value;
  LONG                    currentLocation = 0;

  abookArray = (LPSTR *) malloc( (size_t) sizeof(LPSTR) * ((mySession->m_ContainerArrayCount * 2) + 1) );
  if (!abookArray)
    return(NAB_MEMORY_FAILURE);

  for (j=0; j < mySession->m_ContainerArrayCount; j++)
  {
    result = AB_GetContainerAttribute(mySession->m_ContainerArray[j], attribContainerType, &value);
    if (result) 
      continue;

    if (value->u.containerType != AB_PABContainer) // AB_LDAPContainer, AB_MListContainer,  a mailing list, AB_PABContainer 
    {
      AB_FreeContainerAttribValue(value);
      continue;
    }

    AB_FreeContainerAttribValue(value);

    TRACE("THIS IS A PERSONAL ADDRESS BOOK\n");
    result = AB_GetContainerAttribute(mySession->m_ContainerArray[j], attribName, &value);
    if (result) 
      continue;

    TRACE("NAME OF THE VALUE IS = %s\n", (char *)value->u.string);
    abookArray[currentLocation++] = strdup((char *)value->u.string);
    AB_FreeContainerAttribValue(value);

    // Now get the file name...
    DIR_Server *dir = AB_GetDirServerForContainer(mySession->m_ContainerArray[j]);
    if ( (dir) && (dir->fileName != NULL) && (dir->fileName[0] != '\0') )
    {
      char *fullName;
      DIR_GetServerFileName(&fullName, dir->fileName);      
      if ( (fullName != NULL) && (fullName[0] != '\0') )
      {
        abookArray[currentLocation++] = fullName;
      }
      else
      {
        abookArray[currentLocation++] = "";      
      } 
    }
    else
      abookArray[currentLocation++] = "";

    ++realAbookCount;
  }

  abookArray[currentLocation] = NULL;

  // Create the string sequence
  NSstringSeq strSeq = NSStrSeqNew(abookArray);

  // Now just cleanup the array....
  for (int i=0; i<currentLocation; i++)
  {
    if ((abookArray[i] != NULL) && (*(abookArray[i]) != '\0'))
      free(abookArray[i]);
  }

  free(abookArray);

  if (!strSeq)
    return(NAB_MEMORY_FAILURE);

  // Now write the blob to disk and move on...
  if (WriteTheMemoryBufferToDisk((LPSTR) getPtr->blobFileName, 
                      (UINT) NSStrSeqSize(strSeq), strSeq) != 0)
  {
    NSStrSeqDelete(strSeq);
    return(NAB_MEMORY_FAILURE);
  }

  NSStrSeqDelete(strSeq);
  getPtr->abookCount = realAbookCount;
  return(NAB_SUCCESS);
}

//
// Get the default address book type...
//
LONG 
ProcessNAB_GetDefaultAddressBook(NAB_GetDefaultAddressBookType *getPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_GetDefaultAddressBook() Connection ID = [%d]\n", getPtr->abConnection);

  // Reset some values...  
  getPtr->abookDesc[0] = '\0';
  getPtr->abookFileName[0] = '\0';
  
  //
  // Verify the session handle...
  //
  if (((getPtr->abConnection-1) >= MAX_CON) || ((getPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(getPtr->abConnection - 1)];
  if (mySession == NULL)
    return(NAB_FAILURE);

  int                     j;
  int                     result;
  AB_ContainerAttribValue *value;

  if (mySession->RefreshABContainerList ( ) == FALSE)
		return(NAB_FAILURE);

  if (mySession->m_ContainerArrayCount <= 0)
    return(NAB_SUCCESS);

  if (!(mySession->m_ContainerArray)) 
		return(NAB_MEMORY_FAILURE);

  for (j=0; j < mySession->m_ContainerArrayCount; j++)
  {
    result = AB_GetContainerAttribute(mySession->m_ContainerArray[j], attribContainerType, &value);
    if (result) 
      continue;

    if (value->u.containerType != AB_PABContainer) // AB_LDAPContainer, AB_MListContainer,  a mailing list, AB_PABContainer 
    {
      AB_FreeContainerAttribValue(value);
      continue;
    }

    AB_FreeContainerAttribValue(value);

    TRACE("THIS IS A PERSONAL ADDRESS BOOK\n");
    result = AB_GetContainerAttribute(mySession->m_ContainerArray[j], attribName, &value);
    if (result) 
      continue;

    getPtr->defaultFound = TRUE;
    TRACE("NAME OF THE DEFAULT IS = %s\n", (char *)value->u.string);
    lstrcpy((LPSTR) getPtr->abookDesc, (char *)value->u.string);

    // Now get the file name...
    DIR_Server *dir = AB_GetDirServerForContainer(mySession->m_ContainerArray[j]);
    if (dir)
    {
      char *fullName;
      DIR_GetServerFileName(&fullName, dir->fileName);      
      if ( (fullName != NULL) && (fullName[0] != '\0') )
      {
        lstrcpy((LPSTR) getPtr->abookFileName, fullName);
        XP_FREE(fullName);
      }
    }

    AB_FreeContainerAttribValue(value);
    break;
  }

  if (getPtr->defaultFound)
    return(NAB_SUCCESS);
  else
    return(NAB_NOT_FOUND);
}

NAB_CreatePersonalAddressBookType *createPtr = NULL;

int 
MyCallbackForCreateAddressBook(
                DIR_Server *server,       /* BE will create a DIR_Server and pass it here or pass in an existing DIR_Server if AB_PropertiesCmd */
                MWContext  *context,
                MSG_Pane   *srcPane,      /* the pane that caused this call back */
                XP_Bool    newDirectory)  /* BE will set this argument to TRUE if the FE is showing a new DIR_Server and FALSE otherwise */
{
  int   result;
  if (!createPtr)
    return(1);

  // Set the description in the DIR_Server structure...
  server->description = strdup(createPtr->abookName);
 
  result = AB_UpdateDIRServerForContainerPane(srcPane, server);

  if ( (server->fileName != NULL) && (server->fileName[0] != '\0') )
  {
    char *fullName;
    DIR_GetServerFileName(&fullName, server->fileName);

    if ( (fullName != NULL) && (fullName[0] != '\0') )
    {
      lstrcpy((LPSTR) createPtr->abookFileName, fullName);
    }
    else
    {
      lstrcpy((LPSTR) createPtr->abookFileName, (LPSTR)server->fileName);
    } 
  }

  return(0);
}

//
// Create the address book...
//
LONG 
ProcessNAB_CreatePersonalAddressBook(NAB_CreatePersonalAddressBookType *shmPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_CreatePersonalAddressBook() Connection ID = [%d]\n", shmPtr->abConnection);
  TRACE("NAB: Address Book Name [%s]\n", shmPtr->abookName);

  // Reset some values...  
  shmPtr->abookFileName[0] = '\0';
  
  //
  // Verify the session handle...
  //
  if (((shmPtr->abConnection-1) >= MAX_CON) || ((shmPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(shmPtr->abConnection - 1)];
  if (mySession == NULL)
  {
    return(NAB_FAILURE);
  }

  int   result;
  createPtr = shmPtr;
  result = AB_SetShowPropertySheetForDirFunc(mySession->m_ContainerPane,
                                             MyCallbackForCreateAddressBook);
  if (result)
  {
    return(NAB_FAILURE);
  }

  // Create the address book!
  result = AB_CommandAB2(mySession->m_ContainerPane, AB_NewAddressBook, 0, 0); 
  if (result)
  {
    return(NAB_FAILURE);
  }
  else
  {
    AB_InitializeContainerPane(mySession->m_ContainerPane);
    return(NAB_SUCCESS);
  }
}

//
// Save the address book to HTML...
//
LONG 
ProcessNAB_SaveAddressBookToHTML(NAB_SaveAddressBookToHTMLType *shmPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_SaveAddressBookToHTML() Connection ID = [%d]\n", shmPtr->abConnection);
  TRACE("NAB: HTML File Name [%s]\n", shmPtr->HTMLFileName);
  //
  // Verify the session handle...
  //
  if (((shmPtr->abConnection-1) >= MAX_CON) || ((shmPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(shmPtr->abConnection - 1)];
  if (mySession == NULL)
  {
    return(NAB_FAILURE);
  }
  
  TRACE("ADDRESS BOOK NAME    : [%s]\n", shmPtr->abookName);
  TRACE("HTML FILE NAME       : [%s]\n", shmPtr->abookFileName);
  LONG          result;
  MSG_Pane      *addressBookPane;    // Container info for a particular addr book
  LONG          currentLocation;

  TRACE("HTML FILENAME: [%s]\n", shmPtr->HTMLFileName);
  if ( ValidateFile(shmPtr->HTMLFileName) != 1)
    return(NAB_FILE_ERROR);

  if (! OpenNABAPIHTMLFile(shmPtr->HTMLFileName, shmPtr->abookName) )
    return(NAB_FILE_ERROR);

  AB_ContainerInfo *conInfo = mySession->FindContainerInfo(shmPtr->abookName, 
                                                           shmPtr->abookFileName);
  if (!conInfo)
  {
    (void) CloseNABAPIHTMLFile();
    unlink(shmPtr->abookFileName);
    return(NAB_INVALID_ABOOK);
  }

  result = AB_CreateABPane(&addressBookPane, theApp.m_pAddressContext, WFE_MSGGetMaster());
  if (result)
  {
    (void) CloseNABAPIHTMLFile();
    unlink(shmPtr->abookFileName);
    return(NAB_FAILURE);
  }

  result = AB_InitializeABPane(addressBookPane, conInfo);
  if (result)
  {
    (void) CloseNABAPIHTMLFile();
    AB_ClosePane(addressBookPane);
    unlink(shmPtr->abookFileName);
		return(NAB_INVALID_ABOOK);
  }

  LONG  lineCount = MSG_GetNumLines(addressBookPane);
  currentLocation = 0;

  while ( currentLocation < lineCount )    
  {
    (void) DumpHTMLTableLineForUser(addressBookPane, currentLocation);
    // Increment for next call...
    currentLocation++;
  }
  
  AB_ClosePane(addressBookPane);
  if (CloseNABAPIHTMLFile())
  {
    return(NAB_SUCCESS);
  }
  else
  {
    unlink(shmPtr->abookFileName);
    return(NAB_FAILURE);
  }
}

//
// Import LDIF file...
//
LONG 
ProcessNAB_ImportLDIFFile(NAB_ImportLDIFFileType *shmPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_ImportLDIFFile() Connection ID = [%d]\n", shmPtr->abConnection);
  TRACE("NAB: LDIF File to Import [%s]\n", shmPtr->LDIFFileName);
  //
  // Verify the session handle...
  //
  if (((shmPtr->abConnection-1) >= MAX_CON) || ((shmPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(shmPtr->abConnection - 1)];
  if (mySession == NULL)
  {
    return(NAB_FAILURE);
  }

  TRACE("LDIF Import File: [%s]\n", shmPtr->LDIFFileName);
  if ( ValidateFile(shmPtr->LDIFFileName) != 0)
  {
    return(NAB_FILE_NOT_FOUND);
  }

  TRACE("SPECIFY ADDRESS BOOK NAME: [%s]\n", shmPtr->abookName);
  AB_ContainerInfo *conInfo = mySession->FindContainerInfo(shmPtr->abookName,
                                                           shmPtr->abookFileName);
  if (!conInfo)
    return(NAB_INVALID_ABOOK);

  char  *tPtr = shmPtr->LDIFFileName;
  int failed = AB_ImportData(conInfo, tPtr, 0, AB_Filename); 
  if (!failed)
  {
    if (shmPtr->deleteImportFile)
    {
      unlink(shmPtr->abookFileName);
    }
    return(NAB_SUCCESS);
  }
  else
  {
    return(NAB_FAILURE);
  }
}

//
// Export LDIF file...
//
LONG 
ProcessNAB_ExportLDIFFile(NAB_ExportLDIFFileType *shmPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_ExportLDIFFile() Connection ID = [%d]\n", shmPtr->abConnection);
  TRACE("NAB: LDIF File Name [%s]\n", shmPtr->LDIFFileName);
  //
  // Verify the session handle...
  //
  if (((shmPtr->abConnection-1) >= MAX_CON) || ((shmPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(shmPtr->abConnection - 1)];
  if (mySession == NULL)
  {
    return(NAB_FAILURE);
  }

  TRACE("ADDRESS BOOK FILENAME: [%s]\n", shmPtr->abookFileName);
  if ( ValidateFile(shmPtr->abookFileName) != 1)
  {
    return(NAB_FILE_ERROR);
  }

  TRACE("SPECIFY ADDRESS BOOK NAME: [%s]\n", shmPtr->abookName);
  AB_ContainerInfo *conInfo = mySession->FindContainerInfo(shmPtr->abookName,
                                                           shmPtr->abookFileName);
  if (!conInfo)
    return(NAB_INVALID_ABOOK);

  char  *tPtr = shmPtr->LDIFFileName;
  int failed = AB_ExportData(conInfo, &(tPtr), 0, AB_Filename);
  if (!failed)
  {
    return(NAB_SUCCESS);
  }
  else
  {
    return(NAB_FILE_ERROR);
  }
}

//
// Get first address book entry...
//
LONG 
ProcessNAB_GetFirstAddressBookEntry(NAB_GetFirstAddressBookEntryType *shmPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_GetFirstAddressBookEntry() Connection ID = [%d]\n", shmPtr->abConnection);

  // Reset some values...
  shmPtr->userID = 0;
  shmPtr->updateTime = 0;
  shmPtr->userInfo[0] = '\0';
  
  //
  // Verify the session handle...
  //
  if (((shmPtr->abConnection-1) >= MAX_CON) || ((shmPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(shmPtr->abConnection - 1)];
  if (mySession == NULL)
  {
    return(NAB_FAILURE);
  }

  TRACE("ADDRESS BOOK NAME    : [%s]\n", shmPtr->abookName);
  TRACE("ADDRESS BOOK FILENAME: [%s]\n", shmPtr->abookFileName);
  NABUserID     userID;
  NABUpdateTime updtTime;
  LONG          result;

  if (mySession->m_AddressBookPane != NULL)
  {
    AB_ClosePane(mySession->m_AddressBookPane);
    mySession->m_AddressBookPane = NULL;
  }

  TRACE("RICHIE-TODO: GOTTA GET UPDATE TIME\n");
  AB_ContainerInfo *conInfo = mySession->FindContainerInfo(shmPtr->abookName,
                                                           shmPtr->abookFileName);
  if (!conInfo)
    return(NAB_INVALID_ABOOK);

  result = AB_CreateABPane(&(mySession->m_AddressBookPane), theApp.m_pAddressContext, WFE_MSGGetMaster());
  if (result)
    return(NAB_FAILURE);

  result = AB_InitializeABPane(mySession->m_AddressBookPane, conInfo);
  if (result)
		return(NAB_FAILURE);

  LONG  lineCount = MSG_GetNumLines(mySession->m_AddressBookPane);
  mySession->SetNameIndex( 0 );
  mySession->SetNameTotalCount( lineCount ); 

  BOOL found = FALSE;

  while ( mySession->GetNameIndex() < mySession->GetNameTotalCount() )
  {
    found = GetLDIFLineForUser(mySession->m_AddressBookPane, 
                                mySession->GetNameIndex(), 
                                shmPtr->userInfo, &userID, 
                                &updtTime);
    // Increment for next call...
    mySession->IncrementNameIndex();

    if (found)
      break;
  }
  
  if (found)
  {
    shmPtr->userID = userID;
    shmPtr->updateTime = updtTime;
    return(NAB_SUCCESS);
  }
  else
    return(NAB_NOT_FOUND);    
}

//
// Get next address book entry...
//
LONG 
ProcessNAB_GetNextAddressBookEntry(NAB_GetNextAddressBookEntryType *shmPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_GetNextAddressBookEntry() Connection ID = [%d]\n", shmPtr->abConnection);

  // Reset some values...
  shmPtr->userID = 0;
  shmPtr->updateTime = 0;
  shmPtr->userInfo[0] = '\0';
  
  //
  // Verify the session handle...
  //
  if (((shmPtr->abConnection-1) >= MAX_CON) || ((shmPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(shmPtr->abConnection - 1)];
  if (mySession == NULL)
  {
    return(NAB_FAILURE);
  }

  TRACE("RICHIE-TODO: GOTTA GET UPDATE TIME\n");
  NABUserID     userID;
  NABUpdateTime updtTime;

  if (mySession->m_AddressBookPane == NULL)
    return(NAB_INVALID_ABOOK);

  LONG myIndex = mySession->GetNameIndex();
  if ( myIndex >= mySession->GetNameTotalCount() )  
  {
    if (mySession->m_AddressBookPane != NULL)
    {
      AB_ClosePane(mySession->m_AddressBookPane);
      mySession->m_AddressBookPane = NULL;
    }

    return(NAB_NOT_FOUND);
  }

  BOOL    found = FALSE;
  while ( mySession->GetNameIndex() < mySession->GetNameTotalCount() )
  {
    found = GetLDIFLineForUser(mySession->m_AddressBookPane, 
                                mySession->GetNameIndex(), 
                                shmPtr->userInfo, &userID, 
                                &updtTime);
    // Increment for next call...
    mySession->IncrementNameIndex();

    if (found)
      break;
  }
  
  if (found)
  {
    shmPtr->userID = userID;
    shmPtr->updateTime = updtTime;
    return(NAB_SUCCESS);
  }
  else
    return(NAB_NOT_FOUND);    
}

//
// Find a particular address book entry...
//
LONG 
ProcessNAB_FindAddressBookEntry(NAB_FindAddressBookEntryType *shmPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_FindAddressBookEntry() Connection ID = [%d]\n", shmPtr->abConnection);

  // Reset some values...
  shmPtr->userInfo[0] = '\0';
  shmPtr->updateTime = 0;
  
  if (shmPtr->ldifSearchAttribute[0] != '\0')
    shmPtr->userID = 0;
  
  //
  // Verify the session handle...
  //
  if (((shmPtr->abConnection-1) >= MAX_CON) || ((shmPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(shmPtr->abConnection - 1)];
  if (mySession == NULL)
  {
    return(NAB_FAILURE);
  }

  int                     result;
  AB_ContainerAttribValue *value;
  BOOL                    found = FALSE;

  // Check object state for AB...
  if ( (!(mySession->m_ContainerPane)) || 
       ((mySession->m_ContainerArrayCount) <= 0) || 
       (!(mySession->m_ContainerArray)) )
    return(NAB_INVALID_ABOOK);

  // Now either search a specific address book, OR ALL ADDRESS BOOKS!
  if (shmPtr->abookName[0] == '\0')
  {
    for (int j=0; j<mySession->m_ContainerArrayCount; j++)
    {
      // First check the type for the container...
      result = AB_GetContainerAttribute(mySession->m_ContainerArray[j], attribContainerType, &value);
      if (result) 
        continue;

      if (value->u.containerType != AB_PABContainer) // AB_LDAPContainer, AB_MListContainer,  a mailing list, AB_PABContainer 
      {
        AB_FreeContainerAttribValue(value);
        continue;
      }
      AB_FreeContainerAttribValue(value);

      if (SearchABForAttrib( mySession->m_ContainerArray[j], 
                                    shmPtr->ldifSearchAttribute,
                                    shmPtr->userInfo,
                                    &(shmPtr->userID), 
                                    &(shmPtr->updateTime)) )
      {
        found = TRUE;      
        break;
      }
    }
  }
  else
  {
    TRACE("SEARCHING ADDRESS BOOK NAME    : [%s]\n", shmPtr->abookName);
    AB_ContainerInfo *conInfo = mySession->FindContainerInfo(shmPtr->abookName,
                                                             shmPtr->abookFileName);
    if (!conInfo)
      return(NAB_INVALID_ABOOK);

    if (SearchABForAttrib( conInfo, shmPtr->ldifSearchAttribute,
              shmPtr->userInfo, &(shmPtr->userID), &(shmPtr->updateTime)) )
    {
      found = TRUE;      
    }
  }

  if (found)
    return(NAB_SUCCESS);
  else
    return(NAB_NOT_FOUND);
/*
 NAB_FAILURE - General failure performing the query  
 NAB_NOT_FOUND - The requested entry was not found  
 NAB_NOT_OPEN - This will be returned when a call is made to the API without NAB_Open() being called first.  
 NAB_INVALID_ABOOK - Address book specified is invalid   
 NAB_INVALID_CONNID - Invalid connection ID  
 NAB_MULTIPLE_USERS_FOUND - More than one person found with current search criteria  
 NAB_INVALID_SEARCH_ATTRIB - Invalid search criteria
*/
}

//
// Insert an address book entry...
//
LONG 
ProcessNAB_InsertAddressBookEntry(NAB_InsertAddressBookEntryType *shmPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_InsertAddressBookEntry() Connection ID = [%d]\n", shmPtr->abConnection);

  // Check for SOME input
  if (shmPtr->userInfo[0] == '\0')
    return NAB_FAILURE;

  //
  // Verify the session handle...
  //
  if (((shmPtr->abConnection-1) >= MAX_CON) || ((shmPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(shmPtr->abConnection - 1)];
  if (mySession == NULL)
  {
    return(NAB_FAILURE);
  }

  TRACE("ADDRESS BOOK NAME    : [%s]\n", shmPtr->abookName);
  AB_ContainerInfo *conInfo = mySession->FindContainerInfo(shmPtr->abookName,
                                                           shmPtr->abookFileName);  
  if (!conInfo)
  {
    return(NAB_INVALID_ABOOK);
  }

  ABID    userID;

  TRACE("INSERT THE ADDRESS BOOK ENTRY: [%s]\n", shmPtr->userInfo);
  NABError result = InsertEntryToAB(conInfo, shmPtr->userInfo, 
                                    FALSE, &userID);
  shmPtr->userID = userID;
  return(result);
}

//
// Updating an address book entry...
//
LONG 
ProcessNAB_UpdateAddressBookEntry(NAB_UpdateAddressBookEntryType *shmPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_UpdateAddressBookEntry() Connection ID = [%d]\n", shmPtr->abConnection);
  
  //
  // Verify the session handle...
  //
  if (((shmPtr->abConnection-1) >= MAX_CON) || ((shmPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(shmPtr->abConnection - 1)];
  if (mySession == NULL)
  {
    return(NAB_FAILURE);
  }

  TRACE("ADDRESS BOOK NAME    : [%s]\n", shmPtr->abookName);
  TRACE("ADDRESS BOOK FILENAME: [%s]\n", shmPtr->abookFileName);
  TRACE("UPDATING ADDRESS BOOK ENTRY: [%d]\n", shmPtr->userID);
  LONG          result;
  MSG_Pane      *addressBookPane;    // Container info for a particular addr book

  AB_ContainerInfo *conInfo = mySession->FindContainerInfo(shmPtr->abookName,
                                                           shmPtr->abookFileName);  
  if (!conInfo)
  {
    return(NAB_INVALID_ABOOK);
  }

  result = AB_CreateABPane(&addressBookPane, theApp.m_pAddressContext, WFE_MSGGetMaster());
  if (result)
  {
    return(NAB_FAILURE);
  }

  result = AB_InitializeABPane(addressBookPane, conInfo);
  if (result)
  {
    AB_ClosePane(addressBookPane);
		return(NAB_INVALID_ABOOK);
  }

  DWORD           lineCount = MSG_GetNumLines(addressBookPane);
  MSG_ViewIndex   currentLocation = 0;
  ABID            id;
  BOOL            found = FALSE;

  while ( currentLocation < lineCount )    
  {
    result = AB_GetABIDForIndex(addressBookPane, currentLocation, &id);
    if (result)
      continue;

    if (id == (ABID) shmPtr->userID)
    {
      found = TRUE;
      break;
    }

    // Increment for next call...
    currentLocation++;
  }

  if (!found)
    return(NAB_NOT_FOUND);

  result = InsertEntryToAB(conInfo, shmPtr->userInfo, TRUE, &id);
  return(result);
}

//
// Deleting an address book entry...
//
LONG 
ProcessNAB_DeleteAddressBookEntry(NAB_DeleteAddressBookEntryType *shmPtr)
{
  CNABConnection *mySession;
  TRACE("NAB: ProcessNAB_DeleteAddressBookEntry() Connection ID = [%d]\n", shmPtr->abConnection);
  
  //
  // Verify the session handle...
  //
  if (((shmPtr->abConnection-1) >= MAX_CON) || ((shmPtr->abConnection-1) < 0))
  {
    return(NAB_INVALID_CONNID);
  }

  mySession = nabConnections[(shmPtr->abConnection - 1)];
  if (mySession == NULL)
  {
    return(NAB_FAILURE);
  }

  TRACE("ADDRESS BOOK NAME    : [%s]\n", shmPtr->abookName);
  TRACE("ADDRESS BOOK FILENAME: [%s]\n", shmPtr->abookFileName);
  TRACE("DELETING ADDRESS BOOK ENTRY: [%d]\n", shmPtr->userID);
  LONG          result;
  MSG_Pane      *addressBookPane;    // Container info for a particular addr book

  AB_ContainerInfo *conInfo = mySession->FindContainerInfo(shmPtr->abookName,
                                                           shmPtr->abookFileName);  
  if (!conInfo)
  {
    return(NAB_INVALID_ABOOK);
  }

  result = AB_CreateABPane(&addressBookPane, theApp.m_pAddressContext, WFE_MSGGetMaster());
  if (result)
  {
    return(NAB_FAILURE);
  }

  result = AB_InitializeABPane(addressBookPane, conInfo);
  if (result)
  {
    AB_ClosePane(addressBookPane);
		return(NAB_INVALID_ABOOK);
  }

  DWORD           lineCount = MSG_GetNumLines(addressBookPane);
  MSG_ViewIndex   currentLocation = 0;
  ABID            id;
  BOOL            found = FALSE;

  while ( currentLocation < lineCount )    
  {
    result = AB_GetABIDForIndex(addressBookPane, currentLocation, &id);
    if (result)
      continue;

    if (id == (ABID) shmPtr->userID)
    {
      found = TRUE;
      break;
    }

    // Increment for next call...
    currentLocation++;
  }

  if (!found)
    return(NAB_NOT_FOUND);

  MSG_ViewIndex indices[1];

  indices[0] = currentLocation;
  if (AB_CommandAB2( addressBookPane, AB_DeleteCmd, indices, 1) )
  {
    AB_ClosePane(addressBookPane);
    return(NAB_FAILURE);
  }
  else
  {
    AB_ClosePane(addressBookPane);
    return(NAB_SUCCESS);
  }
}
