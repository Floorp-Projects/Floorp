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
//  Simple MAPI API
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//
#include <windows.h>  
#include <string.h> 

#include "xpapi.h"
#include "trace.h"
#include "mapiipc.h"
#include "mapiutl.h"
#include "nsstrseq.h"
#include "mapismem.h"
#include "mapimem.h"

#include "nscpmapi.h"

#ifndef MAPI_OLE	// Because MSFT doesn't do this for us :-(
#include "mapi.h"
#endif

//
// Necessary variables...
//
memTrackerType    memArray[MAX_POINTERS];

//
// For remembering memory...how ironic.
//
void
SetPointerArray(LPVOID ptr, BYTE type)
{
int i;
  
  for (i=0; i<MAX_POINTERS; i++)
  {
    if (memArray[i].lpMem == NULL)
    {
      memArray[i].lpMem = ptr;
      memArray[i].memType = type;
      break;
    }
  }
}

//
// To have MAPI work in MS-Word on Win95, you must have 
// this in win.ini:
//
// [Mail]
// MAPIX=1
//
// To turn logging of errors on, set:
// HKEY_LOCAL_MACHINE\Software\Netscape\Netscape Navigator\MAPI\Logging = 1

// 
// The MAPILogon function begins a Simple MAPI session, loading 
// the default message store and address book providers. 
//
MAPI_IMPLEMENT(ULONG) 
MAPILogon(
    ULONG ulUIParam,
    LPSTR lpszProfileName,
    LPSTR lpszPassword,
    FLAGS flFlags,
    ULONG ulReserved,
    LPLHANDLE lplhSession)
{
LRESULT       result;
HWND          hWndIPC;
MAPIIPCType   ipcInfo;
static BOOL   firstTime = TRUE;

	TRACE_FN("MAPILogon");
	TRACE("  lpszProfileName=%s, lpszPassword=%s\n", lpszProfileName, lpszPassword);  
#ifndef WIN16
	TRACE("  threadID = %ld\n", GetCurrentThreadId()); 
#endif

  // Init before we start...
  *lplhSession = 0;

  TRACE("MAPILogon: SET RETURN SESSION TO NULL\n"); 

  //
  // Check if diags should be turned on or off...
  //
  DWORD val = 0;
  if (GetConfigInfoNum(szMapiSection, "Logging", &val, HKEY_LOCAL_MACHINE))
  {
		SetLoggingEnabled((BOOL)val);
  }
  
  //
  // Init the stuff we will need for WM_COPYDATA calls and anything else
  //
  if (!nsMAPI_OpenAPI())
  {
    TRACE("MAPILogon: Can't open API\n"); 
    return(MAPI_E_FAILURE);
  }

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    TRACE("MAPILogon: Can't get Window\n"); 
    return(MAPI_E_FAILURE);
  }

  //
  // Reset the pointer arrary for future use.
  //
  if (firstTime)
  {
    TRACE("MAPILogon: Reset Pointer\n"); 
    memset(memArray, 0, sizeof(memTrackerType) * MAX_POINTERS);
    firstTime = FALSE;
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, ulUIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  LPSTR       strings[3];
  NSstringSeq strSeq;
  CSharedMem  *sMem;
  DWORD       memSize;
  LONG        strSize;
  HANDLE      hSharedMemory;
  MAPILogonType   logonInfo;

  strings[0] = CheckNullString(lpszProfileName);
  strings[1] = CheckNullString(lpszPassword);
  strings[2] = NULL;
  strSeq     = NSStrSeqNew(strings);
  strSize    = NSStrSeqSize(strSeq);
  memSize = sizeof(MAPILogonType) + strSize;

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      TRACE("MAPILogon: SMEM Failure\n"); 
      NSStrSeqDelete(strSeq);
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now populate a Logon structure with the passed in information
  //
  logonInfo.ulUIParam = ulUIParam;
  logonInfo.flFlags = flFlags;
  logonInfo.lhSession = 0;
  logonInfo.ipcWorked = 0;

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  memcpy(sMem->m_buf, &(logonInfo), sizeof(MAPILogonType));
  memcpy(sMem->m_buf + sizeof(MAPILogonType), strSeq, (size_t) strSize);

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPILogon, &ipcInfo);

  // Check for IPC completion
  MAPILogonType *logInfo = (MAPILogonType *) &(sMem->m_buf[0]);  
  if (!logInfo->ipcWorked)
  {
    result = MAPI_E_FAILURE;
  }

  // 
  // If successfull, then we need to get the information
  if (result == SUCCESS_SUCCESS)
  {
    *lplhSession = logInfo->lhSession;
  }

  //
  // Now time to do some cleanup...
  //
  NSStrSeqDelete(strSeq);
  NSCloseSharedMemory(sMem, hSharedMemory);

  CleanupMAPITempFiles();
	TRACE("  Returning lhSession=%lu\n", *lplhSession);
	LogString("--MAPILogon complete\r\n");

	return(result);
}

//
// The MAPILogoff function ends a session with the messaging system. 
//
MAPI_IMPLEMENT(ULONG) 
MAPILogoff(
    LHANDLE lhSession,
    ULONG ulUIParam,
    FLAGS flFlags,
    ULONG ulReserved)
{
LRESULT       result;
HWND          hWndIPC;
MAPIIPCType   ipcInfo;

	TRACE_FN("MAPILogoff");
	TRACE("lhSession Session ID = %lu\n", lhSession);
	LogString("--MAPILogoff\r\n");

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    return(MAPI_E_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, ulUIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem      *sMem;
  DWORD           memSize;
  HANDLE          hSharedMemory;
  MAPILogoffType  logoffInfo;
   
  memSize = sizeof(MAPILogoffType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now populate a Logon structure with the passed in information
  //
  logoffInfo.lhSession  = lhSession;
  logoffInfo.ulUIParam  = ulUIParam;
  logoffInfo.flFlags    = flFlags;
  logoffInfo.ipcWorked  = 0;

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  memcpy(sMem->m_buf, &(logoffInfo), sizeof(MAPILogoffType));

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPILogoff, &ipcInfo);

  // Check for IPC completion
  MAPILogoffType *logInfo = (MAPILogoffType *) &(sMem->m_buf[0]);
  if (!logInfo->ipcWorked)
  {
      result = MAPI_E_FAILURE;
  }

  //
  // Now do cleanup on the way out...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);

  //
  // Close the stuff for WM_COPYDATA calls and anything else
  //
  nsMAPI_CloseAPI();

	LogString("--MAPILogoff complete\r\n");

  CleanupMAPITempFiles();

  // Now just return!
  return(result);
}

//
// The MAPIFreeBuffer function frees a memory buffer allocated with a call to 
// the MAPIAllocateBuffer function or the MAPIAllocateMore function. 
//
MAPI_IMPLEMENT(ULONG) 
MAPIFreeBuffer(LPVOID pv)
{
int   i;

  TRACE_FN("MAPIFreeBuffer");

  if (!pv)
#ifdef WIN16     
  	return(SUCCESS_SUCCESS);
#else
  	return(S_OK);
#endif

  for (i=0; i<MAX_POINTERS; i++)
  {
    if (pv == memArray[i].lpMem)
    {
      if (memArray[i].memType == MAPI_MESSAGE_TYPE)
      {
        FreeMAPIMessage((MapiMessage *)pv);
        memArray[i].lpMem = NULL;
      }
      else if (memArray[i].memType == MAPI_RECIPIENT_TYPE)
      {
        FreeMAPIRecipient((MapiRecipDesc *)pv);
        memArray[i].lpMem = NULL;
      }
    }
  }

  pv = NULL;
#ifdef WIN16     
	return(SUCCESS_SUCCESS);
#else
  	return(S_OK);
#endif
}

//
// The MAPISendMail function sends a message. This function differs 
// from the MAPISendDocuments function in that it allows greater 
// flexibility in message generation. 
//
MAPI_IMPLEMENT(ULONG) 
MAPISendMail(
             LHANDLE lhSession, 
             ULONG UIParam, 
             lpMapiMessage lpMessage, 
             FLAGS flags, 
             ULONG ulReserved)
{  
#ifdef _DEBUG
	TRACE_FN("MapiSendMail");
	TRACE("  lhSession=%lu\n", lhSession);
	if (lpMessage)
	{
		TRACE("  lpMessage->lpszSubject = '%s'\n", lpMessage->lpszSubject);
		TRACE("  lpMessage->nRecipCount = %lu\n  lpMessage->nFileCount=%lu\n", lpMessage->nRecipCount, lpMessage->nFileCount);
		lpMapiFileDesc pFiles = lpMessage->lpFiles;
		for (ULONG i = 0; i < lpMessage->nFileCount; i++)
		{
			TRACE("    lpszPathName='%s', lpszFileName='%s'\n", pFiles[i].lpszPathName, pFiles[i].lpszFileName);
		}
	}
#endif
  CleanupMAPITempFiles();

  //
  // Needed variables...
  //
  BOOL          isTempSession = FALSE;
  LONG          result = SUCCESS_SUCCESS;
  HWND          hWndIPC;
  MAPIIPCType   ipcInfo;

	if (!lpMessage)
	{
		return(MAPI_E_FAILURE);
	}

  //
  // Sanity Check the attachments if any here...
  //
  if (lpMessage->nFileCount > 0)
  {
    DWORD rc = SanityCheckAttachmentFiles(lpMessage);  // return of zero is ok
    if (rc == 1)
    {
      return(MAPI_E_ATTACHMENT_NOT_FOUND);
    }
    else if (rc == 2)
    {
      return(MAPI_E_ATTACHMENT_OPEN_FAILURE);
    }
  }

  //
  // If lhSession is ZERO, then we have to get a temporary session and
  // go from there.
  //
  if (lhSession == 0)
  {
    LHANDLE    lhTempSession;
    if (MAPILogon(0, NULL, NULL, 0, 0, &lhTempSession) != SUCCESS_SUCCESS)
    {
      isTempSession = TRUE;
      if (MAPILogon(0, "dummy", "dummy", 0, 0, &lhTempSession) != SUCCESS_SUCCESS)
      {
        return(MAPI_E_LOGIN_FAILURE);
      }
    }

    lhSession = lhTempSession;
  }

  //
  // Now create the big chunk of memory that we will pass across which
  // represents the message
  //
  DWORD               totalSize;
  MAPISendMailType    *memToPass = NULL;

  memToPass = (MAPISendMailType *) FlattenMAPIMessageStructure(lpMessage, &totalSize);
  if (!memToPass)
  {
      if (isTempSession)
      {
        MAPILogoff(lhSession, 0, 0, 0);
      }
      
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  memToPass->lhSession = lhSession;
  memToPass->ulUIParam = UIParam;
  memToPass->flFlags = flags;
  memToPass->ipcWorked = 0;

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    free(memToPass);
    if (isTempSession)
    {
      MAPILogoff(lhSession, 0, 0, 0);
    }
    return(MAPI_E_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, UIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for send message call.
  //
  CSharedMem      *sMem;
  HANDLE          hSharedMemory;

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(totalSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      free(memToPass);
      if (isTempSession)
      {
        MAPILogoff(lhSession, 0, 0, 0);
      }
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = totalSize;
  memcpy(sMem->m_buf, memToPass, (size_t) totalSize);

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = totalSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPISendMail, &ipcInfo);

  // Check for IPC completion
  MAPISendMailType *ptr = (MAPISendMailType *) &(sMem->m_buf[0]);
  if (!ptr->ipcWorked)
  {
    result = MAPI_E_FAILURE;
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);
  free(memToPass);

	LogString("--MAPISendMail complete\r\n");

  //
  // If we created a temp session, then we need to trash it...
  //
  if (isTempSession)
  {
    MAPILogoff(lhSession, 0, 0, 0);
  }

  return(result);
}

//
// The MAPISendDocuments function sends a standard message with one or more attached 
// files and a cover note. The cover note is a dialog box that allows the user to 
// enter a list of recipients and an optional message. MAPISendDocuments differs 
// from the MAPISendMail function in that it allows less flexibility in message generation. 
//
void
CleanupStringArray(LPSTR *strArray, DWORD arraySize)
{
  DWORD   i;

  if (strArray == NULL)
    return;

  for (i=0; i<arraySize; i++)
  {
    if (strArray[i] != NULL)
    {
      free(strArray[i]);
      strArray[i] = NULL;
    }
  }
  
  free(strArray);
  strArray = NULL;
}

MAPI_IMPLEMENT(ULONG) MAPISendDocuments(
    ULONG ulUIParam,
    LPSTR lpszDelimChar,
    LPSTR lpszFilePaths,
    LPSTR lpszFileNames,
    ULONG ulReserved)
{
  TRACE_FN("MAPISendDocuments");
  TRACE("MAPI: MAPISendDocuments() FilePaths: %s\n", lpszFilePaths);
  TRACE("MAPI: MAPISendDocuments() FileNames: %s\n", lpszFileNames);
  TRACE("MAPI: MAPISendDocuments() Delim    : [%s]\n", lpszDelimChar);

  CleanupMAPITempFiles();

  //
  // Needed variables...
  //
  LONG          result = SUCCESS_SUCCESS;
  HWND          hWndIPC;

	if (!lpszFilePaths) 
	{
		return(MAPI_E_FAILURE);
	}

  //
  // Sanity Check the attachments here...
  //
  DWORD aCount = GetFileCount(lpszFilePaths, lpszDelimChar);
  DWORD reallyFound = 0;
  DWORD i;
  BYTE  szFileName[_MAX_PATH];
  BYTE  szNewFileName[_MAX_PATH];
  BYTE  szReadableName[_MAX_PATH];

  LPSTR *strArray = (LPSTR *) malloc((size_t) (sizeof(LPSTR) * ((aCount * 2) + 1)));
  if (!strArray)
  {
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  memset(strArray, 0, (size_t) (sizeof(LPSTR) * ((aCount * 2) + 1)));
  for (i=0; i<aCount; i++)
  {
    if (ExtractFile(lpszFilePaths, lpszDelimChar, i, (LPSTR)szFileName))  // true=found
    {
      if (ValidateFile((LPSTR)szFileName) != 0)
      {
        CleanupStringArray(strArray, reallyFound);
        return(MAPI_E_ATTACHMENT_OPEN_FAILURE);
      }

      //
      // Now get the matching name for this file!
      //
      if (!ExtractFile(lpszFileNames, lpszDelimChar, i, (LPSTR)szReadableName))  // true=found
      {
        lstrcpy((char *)szReadableName, (char *)szFileName);
      }

      if (GetTempMailNameWithExtension((char *)szNewFileName, (char *)szReadableName) == 0)
      {
        CleanupStringArray(strArray, reallyFound);
        return(MAPI_E_ATTACHMENT_OPEN_FAILURE);
      }

      if (!XP_CopyFile((char *)szFileName, (char *)szNewFileName, TRUE))
      {
        CleanupStringArray(strArray, reallyFound);
        return(MAPI_E_ATTACHMENT_WRITE_FAILURE);
      }

      AddTempFile((LPSTR) szNewFileName);

      strArray[reallyFound++] = strdup((LPSTR)szNewFileName);
      strArray[reallyFound++] = strdup((LPSTR)szReadableName);
    }
  }

  // Set last entry to null...again
  strArray[reallyFound] = NULL;

  // Check that we created an even matching pair, otherwise bail!
  if ((reallyFound % 2) != 0)
  {
    CleanupStringArray(strArray, reallyFound);
    return(MAPI_E_ATTACHMENT_OPEN_FAILURE);
  }

  MAPISendDocumentsType   docType;
  MAPISendDocumentsType   *memToPass = NULL;
  NSstringSeq             strSeq;
  DWORD                   memSize;
  LONG                    strSize;
  MAPIIPCType             ipcInfo;

  // Start setting up the MAPISendDocumentsType structure to pass to communicator
  docType.nFileCount   = reallyFound/2;
  docType.ulUIParam    = ulUIParam;
  docType.ipcWorked	   = 0;
  strSeq               = NSStrSeqNew(strArray);
  if (!strSeq)
  {
    CleanupStringArray(strArray, reallyFound);
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  // Cleanup strArray... 
  CleanupStringArray(strArray, reallyFound);

  strSize = NSStrSeqSize(strSeq);
  memSize = sizeof(MAPISendDocumentsType) + strSize; 
 
  //
  // Now create the big chunk of memory that we will pass across which
  // represents attached files...
  //
  memToPass = (MAPISendDocumentsType *)malloc((size_t) memSize);
  if (!memToPass)
  {
    NSStrSeqDelete(strSeq);
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }
            
  // Null out the new memory...
  memset(memToPass, 0, (size_t) memSize);
            
  // Copy in the actual data to the real chunk of memory...
  memcpy(memToPass, &docType, sizeof(MAPISendDocumentsType));
  memcpy(&(memToPass->dataBuf[0]), strSeq, (size_t) strSize);

  // Cleanup sequence...
  NSStrSeqDelete(strSeq);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    free(memToPass);
    return(MAPI_E_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, ulUIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for send message call.
  //
  CSharedMem      *sMem;
  HANDLE          hSharedMemory;

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      free(memToPass);
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  memcpy(sMem->m_buf, memToPass, (size_t) memSize);

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPISendDocuments, &ipcInfo);

  MAPISendDocumentsType *ptr = (MAPISendDocumentsType *) &(sMem->m_buf[0]);
  if (!ptr->ipcWorked)
  {
    result = MAPI_E_FAILURE;
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);
  free(memToPass);

	LogString("--MAPISendDocuments() complete\r\n");
  return(result);
}

MAPI_IMPLEMENT(ULONG) 
MAPIFindNext(
    LHANDLE lhSession,
    ULONG ulUIParam,
    LPSTR lpszMessageType,
    LPSTR lpszSeedMessageID,
    FLAGS flFlags,
    ULONG ulReserved,
    LPSTR lpszMessageID)
{
LRESULT       result;
HWND          hWndIPC;
MAPIIPCType   ipcInfo;
LPSTR         tempSeedString = "";

	TRACE_FN("MAPIFindNext");
  if (lhSession == 0)
  {
    return(MAPI_E_INVALID_SESSION);
  }

  if (lpszSeedMessageID)
  {
    if (strlen(lpszSeedMessageID) > 511)
    {
      return(MAPI_E_INSUFFICIENT_MEMORY);
    }
  }
  else
  {
    lpszSeedMessageID = tempSeedString;
  }

	TRACE("MAPIFindNext lhSession Session ID = %lu\n", lhSession);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    return(MAPI_E_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, ulUIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem        *sMem;
  DWORD             memSize;
  HANDLE            hSharedMemory;
  MAPIFindNextType  findInfo;
   
  memSize = sizeof(MAPIFindNextType);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now populate a Logon structure with the passed in information
  //
  findInfo.lhSession  = lhSession;
  findInfo.ulUIParam  = ulUIParam;
  findInfo.flFlags    = flFlags;
  findInfo.ipcWorked  = 0;

  //
  // Copy the seed value in the structure
  //
  strcpy((LPSTR) findInfo.lpszSeedMessageID, lpszSeedMessageID);

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  memcpy(sMem->m_buf, &(findInfo), sizeof(MAPIFindNextType));

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPIFindNext, &ipcInfo);

  // Check for IPC completion
  MAPIFindNextType *findNextInfo = (MAPIFindNextType *) &(sMem->m_buf[0]);
  if (!findNextInfo->ipcWorked)
  {
    result = MAPI_E_FAILURE;
  }

  //
  // Now return the value to the caller...
  //
  if (result == SUCCESS_SUCCESS)
  {
    strcpy(lpszMessageID, (LPSTR) findNextInfo->lpszMessageID);
  }

  //
  // Now do cleanup on the way out...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);
	LogString("--MAPIFindNext complete\r\n");
	return(result);
}

MAPI_IMPLEMENT(ULONG) 
MAPIReadMail(
    LHANDLE lhSession,
    ULONG ulUIParam,
    LPSTR lpszMessageID,
    FLAGS flFlags,
    ULONG ulReserved,
    lpMapiMessage FAR *lppMessage)
{
LRESULT       result;
HWND          hWndIPC;
MAPIIPCType   ipcInfo;

	TRACE_FN("MAPIReadMail");
  //
  // This one has to have a valid session handle, or we bail!
  // 
  if (lhSession == 0)
  {
    return(MAPI_E_INVALID_SESSION);
  }

  if (strlen(lpszMessageID) > 511)
  {
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    return(MAPI_E_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, ulUIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem        *sMem;
  DWORD             memSize;
  HANDLE            hSharedMemory;
  MAPIReadMailType  readMailInfo;
   
  memSize = sizeof(MAPIReadMailType);
  memset(&readMailInfo, 0, (size_t) memSize);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now populate a Logon structure with the passed in information
  //
  readMailInfo.lhSession  = lhSession;
  readMailInfo.ulUIParam  = ulUIParam;
  readMailInfo.flFlags    = flFlags;
  readMailInfo.ipcWorked  = 0;

  //
  // Copy the seed value in the structure
  //
  strcpy((LPSTR) readMailInfo.lpszMessageID, lpszMessageID);

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  memcpy(sMem->m_buf, &(readMailInfo), sizeof(MAPIReadMailType));

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPIReadMail, &ipcInfo);

  // Check the IPC status...
  MAPIReadMailType *rMailPtr = (MAPIReadMailType *) &(sMem->m_buf[0]);

  //
  // Now return the value to the caller if we bombed...
  //
  if ((result != SUCCESS_SUCCESS) || (!(rMailPtr->ipcWorked)))
  {
    NSCloseSharedMemory(sMem, hSharedMemory);
	  LogString("--MAPIReadMail failed\r\n");
	  return(result);
  }

  // Now get a hold of the various values for the message we need to 
  // return in the lpMapiMessage structure

  if (rMailPtr->MSG_nRecipCount == 0)
  {
    NSCloseSharedMemory(sMem, hSharedMemory);
	  LogString("--MAPIReadMail failed - ZERO recipients\r\n");
	  return(MAPI_E_FAILURE);
  }

  //
  // Get the blob loaded into memory...
  // 
  NSstringSeq blobSequence = (NSstringSeq) LoadBlobToMemory((LPSTR) rMailPtr->lpszBlob);

  // Now check on the blob
  if (!blobSequence)
  {	  
    // Cleanup the shared memory...
    NSCloseSharedMemory(sMem, hSharedMemory);
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  lpMapiMessage  msgPtr = (MapiMessage *)malloc(sizeof(MapiMessage));
  if (msgPtr == NULL)
  {
    // Cleanup the shared memory...
    NSCloseSharedMemory(sMem, hSharedMemory);
    NSStrSeqDelete(blobSequence);
	  LogString("--MAPIReadMail failed\r\n");
	  return(MAPI_E_INSUFFICIENT_MEMORY);
  }
  
  memset(msgPtr, 0, sizeof(MapiMessage)); 

  //
  // At this point, we need to populate the structure of information
  // we are passing back via the *lppMessage and then start parsing through the
  // String sequence for each of needed fields. After populating the entire 
  // structure, return this pointer and our job on this side is done!
  //
  LPSTR   subject = NSStrSeqGet(blobSequence, 0);
  LPSTR   noteText = NSStrSeqGet(blobSequence, 1);
  LPSTR   dateReceived = NSStrSeqGet(blobSequence, 2);
  LPSTR   threadID = NSStrSeqGet(blobSequence, 3);
  LPSTR   origName = NSStrSeqGet(blobSequence, 4);
  LPSTR   origAddress = NSStrSeqGet(blobSequence, 5);

  UINT    i;
  ULONG   stringCount = 6;      // This is to start with the rest of the string seq

  // Set all of the general header information first!
  msgPtr->lpszSubject = strdup(CheckNullString(subject));
  msgPtr->lpszDateReceived = strdup(CheckNullString(dateReceived));
  msgPtr->lpszConversationID = strdup(CheckNullString(threadID));
  msgPtr->flFlags = rMailPtr->MSG_flFlags;
  
  // Setup the originator information!
  msgPtr->lpOriginator = (lpMapiRecipDesc) malloc(sizeof(MapiRecipDesc));
  if (!msgPtr->lpOriginator)
  {
    // Cleanup the shared memory...
    NSCloseSharedMemory(sMem, hSharedMemory);

    FreeMAPIMessage(msgPtr);
    NSStrSeqDelete(blobSequence);
	  LogString("--MAPIReadMail failed\r\n");
	  return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  // memset the area just allocated to null
  memset(msgPtr->lpOriginator, 0, sizeof(MapiRecipDesc));

  msgPtr->lpOriginator->lpszName = strdup(CheckNullString(origName));
  msgPtr->lpOriginator->lpszAddress = strdup(CheckNullString(origAddress));
  msgPtr->lpOriginator->ulRecipClass = rMailPtr->MSG_ORIG_ulRecipClass;

  // Now deal with the recipients of this message
  msgPtr->lpRecips = (lpMapiRecipDesc) malloc((size_t) (sizeof(MapiRecipDesc) * 
                                                    rMailPtr->MSG_nRecipCount));
  if (!msgPtr->lpRecips)
  {
    // Cleanup the shared memory...
    NSCloseSharedMemory(sMem, hSharedMemory);

    FreeMAPIMessage(msgPtr);
    NSStrSeqDelete(blobSequence);
	  LogString("--MAPIReadMail failed\r\n");
	  return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  msgPtr->nRecipCount = rMailPtr->MSG_nRecipCount;
  memset(msgPtr->lpRecips, 0, (size_t) (sizeof(MapiRecipDesc) * msgPtr->nRecipCount));

  for (i=0; i<msgPtr->nRecipCount; i++)
  {
  //      String x: LPSTR lpszName;             // Recipient N name                           
  //      String x: LPSTR lpszAddress;          // Recipient N address (optional)             
  //      String x: LPSTR lpszRecipClass        // recipient class - sprintf of ULONG

    msgPtr->lpRecips[i].lpszName = 
        strdup( CheckNullString(NSStrSeqGet(blobSequence, stringCount++)) );

    msgPtr->lpRecips[i].lpszAddress = 
        strdup( CheckNullString(NSStrSeqGet(blobSequence, stringCount++)) );
      
    msgPtr->lpRecips[i].ulRecipClass = 
        (ULONG) atoi( CheckNullString(NSStrSeqGet(blobSequence, stringCount++)) );
  }

  //
  // MAPI_ENVELOPE_ONLY 
  // Indicates MAPIReadMail should read the message header only. File attachments are 
  // not copied to temporary files, and neither temporary file names nor message text 
  // is written. Setting this flag enhances performance. 
  //
  // MAPI_SUPPRESS_ATTACH 
  // Indicates MAPIReadMail should not copy file attachments but should write message 
  // text into the MapiMessage structure. MAPIReadMail ignores this flag if the 
  // calling application has set the MAPI_ENVELOPE_ONLY flag. Setting the 
  // MAPI_SUPPRESS_ATTACH flag enhances performance. 
  //
  if (flFlags & MAPI_ENVELOPE_ONLY)
  {
    noteText = NULL;
    rMailPtr->MSG_nFileCount = 0;
  }
  else if ( (flFlags & MAPI_BODY_AS_FILE) &&
            (flFlags & MAPI_SUPPRESS_ATTACH))
  {
    rMailPtr->MSG_nFileCount = 1;
  }
  else if (flFlags & MAPI_BODY_AS_FILE) 
  {
    ++(rMailPtr->MSG_nFileCount);
  }

  // Now deal with the list of attachments! Since the nFileCount should be set
  // correctly, this loop will automagically be correct
  //
  if (rMailPtr->MSG_nFileCount > 0)
  {
    msgPtr->lpFiles = (lpMapiFileDesc) malloc((size_t)  (sizeof(MapiFileDesc) * 
                                                    rMailPtr->MSG_nFileCount) );
    if (!msgPtr->lpFiles)
    {
      // Cleanup the shared memory...
      NSCloseSharedMemory(sMem, hSharedMemory);

      FreeMAPIMessage(msgPtr);
      NSStrSeqDelete(blobSequence);
      LogString("--MAPIReadMail failed\r\n");
      return(MAPI_E_INSUFFICIENT_MEMORY);
    }

    memset(msgPtr->lpFiles, 0, (size_t) (sizeof(MapiFileDesc) * rMailPtr->MSG_nFileCount));
  }

  UCHAR   fNameBody[_MAX_PATH];

  // Now do body and attachment stuff only if we haven't been told
  // not to do it by the user.
  if (!(flFlags & MAPI_ENVELOPE_ONLY))
  {
    if (flFlags & MAPI_BODY_AS_FILE)
    {
      LPSTR tempNoteText = CheckNullString(noteText);
      
      if (GetTempAttachmentName((LPSTR) fNameBody) != 0)
      {
        // Cleanup the shared memory...
        NSCloseSharedMemory(sMem, hSharedMemory);
        
        FreeMAPIMessage(msgPtr);
        NSStrSeqDelete(blobSequence);
        LogString("--MAPIReadMail failed\r\n");
        return(MAPI_E_ATTACHMENT_WRITE_FAILURE);
      }
      
      if (WriteMemoryBufferToDisk((LPSTR) fNameBody, 
        (strlen(tempNoteText) + 1), tempNoteText) != 0)
      {
        // Cleanup the shared memory...
        NSCloseSharedMemory(sMem, hSharedMemory);
        
        FreeMAPIMessage(msgPtr);
        NSStrSeqDelete(blobSequence);
        LogString("--MAPIReadMail failed\r\n");
        return(MAPI_E_ATTACHMENT_WRITE_FAILURE);
      }
    }
    else
    {
      msgPtr->lpszNoteText = strdup(noteText);
    }
    
    msgPtr->nFileCount = rMailPtr->MSG_nFileCount;
    for (i=0; i<msgPtr->nFileCount; i++)
    {
      //
      // MAPI_BODY_AS_FILE 
      // Indicates MAPIReadMail should write the message text to a temporary file 
      // and add it as the first attachment in the attachment list. 
      if ( (i == 0) && (flFlags & MAPI_BODY_AS_FILE))
      {
        msgPtr->lpFiles[i].lpszPathName = strdup((LPSTR)fNameBody);
        msgPtr->lpFiles[i].lpszFileName = strdup((LPSTR)fNameBody);
        continue;
      }
      
      msgPtr->lpFiles[i].lpszPathName = 
        strdup( CheckNullString(NSStrSeqGet(blobSequence, stringCount++)) );
      msgPtr->lpFiles[i].lpszFileName =
        strdup( CheckNullString(NSStrSeqGet(blobSequence, stringCount++)) );
    }
  }

  //
  // RICHIE - For now, let's do a little cleanup on the message itself
  // and strip the following inline tags:
  //
  // &nbsp; &lt; 
  //
  msgPtr->lpszNoteText = StripHTML(msgPtr->lpszNoteText);

  //
  // Now that we are here, set the pointer array and move on with life...
  //
  *lppMessage = msgPtr;
  SetPointerArray(msgPtr, MAPI_MESSAGE_TYPE);

  // Cleanup the shared memory...
  NSCloseSharedMemory(sMem, hSharedMemory);
  if (blobSequence)
    NSStrSeqDelete(blobSequence);

  //
  // Now, get out...
  //
	LogString("--MAPIReadMail complete\r\n");
	return(result);
}

MAPI_IMPLEMENT(ULONG) 
MAPISaveMail(
    LHANDLE lhSession,
    ULONG ulUIParam,
    lpMapiMessage lpMessage,
    FLAGS flFlags,
    ULONG ulReserved,
    LPSTR lpszMessageID
)
{
#ifdef _DEBUG
	TRACE_FN("MapiSaveMail");
	TRACE("  lhSession=%lu\n", lhSession);
	if (lpMessage)
	{
		TRACE("  lpMessage->lpszSubject = '%s'\n  lpMessage->lpszNoteText='%s'\n", lpMessage->lpszSubject, lpMessage->lpszNoteText);
		TRACE("  lpMessage->nRecipCount = %lu\n  lpMessage->nFileCount=%lu\n", lpMessage->nRecipCount, lpMessage->nFileCount);
		lpMapiFileDesc pFiles = lpMessage->lpFiles;
		for (ULONG i = 0; i < lpMessage->nFileCount; i++)
		{
			TRACE("    lpszPathName='%s', lpszFileName='%s'\n", pFiles[i].lpszPathName, pFiles[i].lpszFileName);
		}
	}
#endif

  CleanupMAPITempFiles();

  //
  // Needed variables...
  //
  BOOL          isTempSession = FALSE;
  LONG          result = SUCCESS_SUCCESS;
  HWND          hWndIPC;
  MAPIIPCType   ipcInfo;

	if (!lpMessage)
	{
		return(MAPI_E_FAILURE);
	}

  //
  // Sanity Check the attachments if any here...
  //
  if (lpMessage->nFileCount > 0)
  {
    DWORD rc = SanityCheckAttachmentFiles(lpMessage);  // return of zero is ok
    if (rc == 1)
    {
      return(MAPI_E_ATTACHMENT_NOT_FOUND);
    }
    else if (rc == 2)
    {
      return(MAPI_E_ATTACHMENT_OPEN_FAILURE);
    }
  }

  //
  // If lhSession is ZERO, then we have to get a temporary session and
  // go from there.
  //
  if ( (lhSession == 0) || (flFlags & MAPI_NEW_SESSION))
  {
    LHANDLE    lhTempSession;
    if (MAPILogon(0, NULL, NULL, 0, 0, &lhTempSession) != SUCCESS_SUCCESS)
    {
      isTempSession = TRUE;
      if (MAPILogon(0, "dummy", "dummy", 0, 0, &lhTempSession) != SUCCESS_SUCCESS)
      {
        return(MAPI_E_LOGIN_FAILURE);
      }
    }

    lhSession = lhTempSession;
  }

  //
  // Now create the big chunk of memory that we will pass across which
  // represents the message
  //
  DWORD               totalSize;
  MAPISendMailType    *memToPass = NULL;

  memToPass = (MAPISendMailType *) FlattenMAPIMessageStructure(lpMessage, &totalSize);
  if (!memToPass)
  {
      if (isTempSession)
      {
        MAPILogoff(lhSession, 0, 0, 0);
      }
      
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  memToPass->lhSession = lhSession;
  memToPass->ulUIParam = ulUIParam;
  memToPass->flFlags = flFlags;
  memToPass->ipcWorked = 0;

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    free(memToPass);
    if (isTempSession)
    {
      MAPILogoff(lhSession, 0, 0, 0);
    }
    return(MAPI_E_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, ulUIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for send message call.
  //
  CSharedMem      *sMem;
  HANDLE          hSharedMemory;

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(totalSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      free(memToPass);
      if (isTempSession)
      {
        MAPILogoff(lhSession, 0, 0, 0);
      }
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = totalSize;
  memcpy(sMem->m_buf, memToPass, (size_t) totalSize);

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = totalSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPISaveMail, &ipcInfo);

  // Check for IPC completion
  MAPISendMailType *ptr = (MAPISendMailType *) &(sMem->m_buf[0]);
  if (!ptr->ipcWorked)
  {
    result = MAPI_E_FAILURE;
  }

  //
  // Now time to do some cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);
  free(memToPass);

	LogString("--MAPISaveMail complete\r\n");

  //
  // If we created a temp session, then we need to trash it...
  //
  if (isTempSession)
  {
    MAPILogoff(lhSession, 0, 0, 0);
  }

  return(result);
}

//
// The MAPIDeleteMail function deletes a message. 
//
MAPI_IMPLEMENT(ULONG) 
MAPIDeleteMail(
    LHANDLE lhSession,
    ULONG ulUIParam,
    LPSTR lpszMessageID,
    FLAGS flFlags,
    ULONG ulReserved)
{
LRESULT       result;
HWND          hWndIPC;
MAPIIPCType   ipcInfo;

	TRACE_FN("MAPIDeleteMail");
  if (lhSession == 0)
  {
    return(MAPI_E_INVALID_SESSION);
  }

  // Make sure we have something valid on the message ID front!
  if ((!lpszMessageID) || (!*lpszMessageID))
  {
    return(MAPI_E_INVALID_MESSAGE);
  }

  if (strlen(lpszMessageID) > 511)
  {
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }

	TRACE("MAPIDeleteMail lhSession Session ID = %lu\n", lhSession);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    return(MAPI_E_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, ulUIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the login call.
  //
  CSharedMem          *sMem;
  DWORD               memSize;
  HANDLE              hSharedMemory;
  MAPIDeleteMailType  delInfo;
   
  memSize = sizeof(MAPIDeleteMailType);
  lstrcpy((LPSTR)delInfo.lpszMessageID, lpszMessageID);

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now populate a Logon structure with the passed in information
  //
  delInfo.lhSession   = lhSession;
  delInfo.ulUIParam   = ulUIParam;
  delInfo.ipcWorked   = 0;

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  memcpy(sMem->m_buf, &(delInfo), sizeof(MAPIDeleteMailType));

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPIDeleteMail, &ipcInfo);

  // Check for IPC completion
  MAPIDeleteMailType *ptr = (MAPIDeleteMailType *) &(sMem->m_buf[0]);
  if (!ptr->ipcWorked)
  {
    result = MAPI_E_FAILURE;
  }

  //
  // Now do cleanup on the way out and return the value to the caller...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);
	LogString("--MAPIDeleteMail complete\r\n");
	return(result);
}

// RICHIE - MAPI ADDRESS HERE

//
// The MAPIDetails function displays a dialog box containing the details 
// of a selected address list entry
//
MAPI_IMPLEMENT(ULONG) 
MAPIDetails(
    LHANDLE lhSession,
    ULONG ulUIParam,
    lpMapiRecipDesc lpRecip,
    FLAGS flFlags,
    ULONG ulReserved)
{
	TRACE_FN("MAPIDetails");
  //
  // Needed variables...
  //
  BOOL          isTempSession = FALSE;
  LONG          result = SUCCESS_SUCCESS;
  HWND          hWndIPC;
  MAPIIPCType   ipcInfo;

  // Make sure we have something valid on the query name front!
  if (!lpRecip) 
  {
    return(MAPI_E_AMBIGUOUS_RECIPIENT);
  }

  if ((lpRecip->ulEIDSize == 0) || (lpRecip->ulEIDSize > MAX_NAME_LEN))
  {
    return(MAPI_E_AMBIGUOUS_RECIPIENT);
  }

  //
  // If lhSession is ZERO, then we have to get a temporary session and
  // go from there.
  //
  if (lhSession == 0)
  {
    LHANDLE    lhTempSession;
    if (MAPILogon(0, NULL, NULL, 0, 0, &lhTempSession) != SUCCESS_SUCCESS)
    {
      isTempSession = TRUE;
      if (MAPILogon(0, "dummy", "dummy", 0, 0, &lhTempSession) != SUCCESS_SUCCESS)
      {
        return(MAPI_E_LOGIN_FAILURE);
      }
    }

    lhSession = lhTempSession;
  }

  //
  // Now create the big chunk of memory that we will pass across which
  // represents the message
  //
  MAPIDetailsType        detailInfo;

  memset(&detailInfo, 0, sizeof(MAPIDetailsType));
  detailInfo.lhSession = lhSession;
  detailInfo.ulUIParam = ulUIParam;
  detailInfo.flFlags   = flFlags;
  detailInfo.ipcWorked = 0;
  memcpy(detailInfo.lpszABookID, lpRecip->lpEntryID, (size_t) lpRecip->ulEIDSize);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    if (isTempSession)
    {
      MAPILogoff(lhSession, 0, 0, 0);
    }
    return(MAPI_E_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, ulUIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for send message call.
  //
  CSharedMem      *sMem;
  HANDLE          hSharedMemory;
  
  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(sizeof(MAPIDetailsType), (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      if (isTempSession)
      {
        MAPILogoff(lhSession, 0, 0, 0);
      }
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = sizeof(MAPIDetailsType);
  memcpy(sMem->m_buf, &detailInfo, sizeof(MAPIDetailsType));

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = sizeof(MAPIDetailsType);

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPIDetails, &ipcInfo);

  // Check for IPC completion
  MAPIDetailsType *ptr = (MAPIDetailsType *) &(sMem->m_buf[0]);
  if (!ptr->ipcWorked)
  {
    result = MAPI_E_FAILURE;
  }

  //
  // Now time to do some real cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);

  //
  // If we created a temp session, then we need to trash it...
  //
  if (isTempSession)
  {
    MAPILogoff(lhSession, 0, 0, 0);
  }

  LogString("--MAPIDetails complete\r\n");
  return(result);
}

//
// The MAPIResolveName function transforms a message recipient's name as entered by a 
// user to an unambiguous address list entry. 
//
MAPI_IMPLEMENT(ULONG) 
MAPIResolveName(
    LHANDLE lhSession,
    ULONG ulUIParam,
    LPSTR lpszName,
    FLAGS flFlags,
    ULONG ulReserved,
    lpMapiRecipDesc FAR *lppRecip)
{
	TRACE_FN("MAPIResolveName");
  //
  // Needed variables...
  //
  BOOL          isTempSession = FALSE;
  LONG          result = SUCCESS_SUCCESS;
  HWND          hWndIPC;
  MAPIIPCType   ipcInfo;

  // Make sure we have something valid on the query name front!
  if ((!lpszName) || (!*lpszName))
  {
    return(MAPI_E_UNKNOWN_RECIPIENT);
  }

  if (strlen(lpszName) > 255)
  {
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // If lhSession is ZERO, then we have to get a temporary session and
  // go from there.
  //
  if (lhSession == 0)
  {
    LHANDLE    lhTempSession;
    if (MAPILogon(0, NULL, NULL, 0, 0, &lhTempSession) != SUCCESS_SUCCESS)
    {
      isTempSession = TRUE;
      if (MAPILogon(0, "dummy", "dummy", 0, 0, &lhTempSession) != SUCCESS_SUCCESS)
      {
        return(MAPI_E_LOGIN_FAILURE);
      }
    }

    lhSession = lhTempSession;
  }

  //
  // Now create the big chunk of memory that we will pass across which
  // represents the message
  //
  MAPIResolveNameType   nameInfo;

  nameInfo.lhSession = lhSession;
  nameInfo.ulUIParam = ulUIParam;
  nameInfo.flFlags   = flFlags;
  nameInfo.ipcWorked = 0;
  lstrcpy((LPSTR) nameInfo.lpszName, lpszName);

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    if (isTempSession)
    {
      MAPILogoff(lhSession, 0, 0, 0);
    }
    return(MAPI_E_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, ulUIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for send message call.
  //
  CSharedMem      *sMem;
  HANDLE          hSharedMemory;

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(sizeof(MAPIResolveNameType), (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      if (isTempSession)
      {
        MAPILogoff(lhSession, 0, 0, 0);
      }
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = sizeof(MAPIResolveNameType);
  memcpy(sMem->m_buf, &nameInfo, sizeof(MAPIResolveNameType));

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = sizeof(MAPIResolveNameType);

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPIResolveName, &ipcInfo);

  // Check for IPC completion
  MAPIResolveNameType *ptr = (MAPIResolveNameType *) &(sMem->m_buf[0]);
  if (!ptr->ipcWorked)
  {
    result = MAPI_E_FAILURE;
  }

  //
  // IF FOUND: create the new structure for the matched address info and set the
  // memory tracking array correctly for cleanup later down the road.
  // 
  if (result == SUCCESS_SUCCESS)
  {
    *lppRecip = (MapiRecipDesc FAR *)malloc(sizeof(MapiRecipDesc));
    if (lppRecip == NULL)
    {
        if (isTempSession)
        {
          MAPILogoff(lhSession, 0, 0, 0);
        }

        return(MAPI_E_INSUFFICIENT_MEMORY);
    }

    // Null out the new memory...
    memset((*lppRecip), 0, sizeof(MapiRecipDesc));
    MAPIResolveNameType *resultInfo = (MAPIResolveNameType *) &(sMem->m_buf[0]);

    (*lppRecip)->ulRecipClass = 0;   // This field is meaningless in this call
    (*lppRecip)->lpszName = strdup((LPSTR) resultInfo->lpszABookName);
    (*lppRecip)->lpszAddress = strdup((LPSTR) resultInfo->lpszABookAddress);

    // We will be using a string made up of the first and last name of the
    // person as a unique ID for that individual. The string will be delimited
    // like this:
    //
    //                <first_name>:=LAST=:<last_name>
    //
    (*lppRecip)->ulEIDSize = strlen((LPSTR) resultInfo->lpszABookID) + 1;
    (*lppRecip)->lpEntryID = strdup((LPSTR) resultInfo->lpszABookID);

    SetPointerArray(*lppRecip, MAPI_RECIPIENT_TYPE);
  }  

  //
  // Now time to do some real cleanup...
  //
  NSCloseSharedMemory(sMem, hSharedMemory);

  //
  // If we created a temp session, then we need to trash it...
  //
  if (isTempSession)
  {
    MAPILogoff(lhSession, 0, 0, 0);
  }

  LogString("--MAPIResolveName complete\r\n");
  return(result);
}

MAPI_IMPLEMENT(ULONG) 
MAPIAddress(
    LHANDLE lhSession,
    ULONG ulUIParam,
    LPSTR lpszCaption,
    ULONG nEditFields,
    LPSTR lpszLabels,
    ULONG nRecips,
    lpMapiRecipDesc lpRecips,
    FLAGS flFlags,
    ULONG ulReserved,
    LPULONG lpnNewRecips,
    lpMapiRecipDesc FAR *lppNewRecips)
{
  TRACE_FN("MAPIAddress");
  TRACE("MAPIAddress: lhSession=%lu\n", lhSession);
  //
  // Needed variables...
  //
  BOOL          isTempSession = FALSE;
  LONG          result = SUCCESS_SUCCESS;
  HWND          hWndIPC;
  MAPIIPCType   ipcInfo;

  *lppNewRecips = NULL;

  //
  // If lhSession is ZERO, then we have to get a temporary session and
  // go from there.
  //
  if ( (lhSession == 0) || (flFlags & MAPI_NEW_SESSION))
  {
    LHANDLE    lhTempSession;
    if (MAPILogon(0, NULL, NULL, 0, 0, &lhTempSession) != SUCCESS_SUCCESS)
    {
      isTempSession = TRUE;
      if (MAPILogon(0, "dummy", "dummy", 0, 0, &lhTempSession) != SUCCESS_SUCCESS)
      {
        return(MAPI_E_LOGIN_FAILURE);
      }
    }

    lhSession = lhTempSession;
  }

  //
  // This call locates a Communicator Window, if one is not running,
  // it will start up an instance of Communicator and grab that window.
  //
  hWndIPC = GetCommunicatorIPCWindow();
  if (NULL == hWndIPC)
  {
    return(MAPI_E_FAILURE);
  }

  // 
  // Build the IPC structure of information that needs to be provided
  // to get information into Communicator
  //
  BuildMemName((LPSTR)ipcInfo.smemName, ulUIParam);

  //
  // Build the shared memory segment of information to pass into 
  // Communicator for the address call.
  //
  ULONG             i;
  CSharedMem        *sMem;
  DWORD             memSize;
  HANDLE            hSharedMemory;
  MAPIAddressType   addrInfo;
  LONG              strSeqSize = 0;
  NSstringSeq       strSeq = NULL;
  DWORD             currentString = 0;
   
  //
  // First populate a address structure with the passed in information
  //
  memset(&addrInfo, 0, (size_t) sizeof(MAPIAddressType));

  addrInfo.lhSession  = lhSession;
  addrInfo.ulUIParam  = ulUIParam;
  addrInfo.flFlags    = flFlags;
  addrInfo.ipcWorked  = 0;
  addrInfo.nNewRecips = 0;        // number of recips returned...
  addrInfo.lpszBlob[0] = '\0';    // file name for blob of information 

  addrInfo.nRecips = nRecips;     // number of recips to start with...

  //
  // Copy the caption into the structure
  //
  if ((lpszCaption) && (*lpszCaption))    // check for no null
  {
    strcpy((LPSTR) addrInfo.lpszCaption, lpszCaption);
  }

  // Now build the glob of memory of addresses to start with...
  if (nRecips > 0)
  {
    LPSTR *strArray = (LPSTR *) malloc((size_t) (sizeof(LPSTR) * ((nRecips * 3) + 1)));
    if (!strArray)
    {
      if (isTempSession)
      {
        MAPILogoff(lhSession, 0, 0, 0);
      }
      return(MAPI_E_INSUFFICIENT_MEMORY);
    }
  
    LPSTR toString  = "1";
    LPSTR ccString  = "2";
    LPSTR bccString = "3";

    for (i=0; i<nRecips; i++)
    {
      // rhp - need message class
      if (lpRecips[i].ulRecipClass == MAPI_BCC)
        strArray[currentString++] = CheckNullString(bccString);
      else if (lpRecips[i].ulRecipClass == MAPI_CC)
        strArray[currentString++] = CheckNullString(ccString);
      else
        strArray[currentString++] = CheckNullString(toString);

      strArray[currentString++] = CheckNullString(lpRecips[i].lpszName);
      strArray[currentString++] = CheckNullString(lpRecips[i].lpszAddress);  
    }

    strArray[currentString] = NULL;

    strSeq     = NSStrSeqNew(strArray);
    free(strArray);
    if (!strSeq)
    {
      if (isTempSession)
      {
        MAPILogoff(lhSession, 0, 0, 0);
      }
      return(MAPI_E_INSUFFICIENT_MEMORY);
    }

    strSeqSize = NSStrSeqSize(strSeq);
  }
  
  memSize = sizeof(MAPIAddressType) + strSeqSize;

  //
  // Create the shared memory...
  //
  sMem = NSCreateSharedMemory(memSize, (LPSTR) ipcInfo.smemName, &hSharedMemory);
  if (!sMem)
  {
      if (isTempSession)
      {
        MAPILogoff(lhSession, 0, 0, 0);
      }
      return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  //
  // Now copy the information into the memory segment.
  //
  sMem->m_dwSize = memSize;
  memcpy(sMem->m_buf, &(addrInfo), sizeof(MAPIAddressType));

  // Now if we have recipients...copy in the sequence!
  if (nRecips > 0)  
  {
    MAPIAddressType *ptr = (MAPIAddressType *)&(sMem->m_buf[0]);

    memcpy(ptr->dataBuf, strSeq, strSeqSize);
    NSStrSeqDelete(strSeq);
  }

  //
  // Now set the size of the shared memory segment for the 
  // WM_COPYDATA argument that will be sent across.
  //
  ipcInfo.smemSize = memSize;

  // For Win16, we actually pass in the pointer to the structure,
  // so just assign the variable here.
#ifdef WIN16
  ipcInfo.lpsmem = sMem;
#else
  ipcInfo.lpsmem = NULL;
#endif

  //
  // Ok, time to do the SendMessage() to Communicator...
  //
  result = SendMAPIRequest(hWndIPC, NSCP_MAPIAddress, &ipcInfo);

  // Check the IPC status...
  MAPIAddressType *addrPtr = (MAPIAddressType *) &(sMem->m_buf[0]);

  //
  // Now return the value to the caller if we bombed...
  //
  if ((result != SUCCESS_SUCCESS) || (!(addrPtr->ipcWorked)))
  {
    NSCloseSharedMemory(sMem, hSharedMemory);
    if (isTempSession)
    {
      MAPILogoff(lhSession, 0, 0, 0);
    }
	  LogString("--MAPIAddress failed\r\n");
	  return(result);
  }

  // Now deal with the addresses selected and move on...
  if (addrPtr->nNewRecips == 0)
  {
    NSCloseSharedMemory(sMem, hSharedMemory);
    if (isTempSession)
    {
      MAPILogoff(lhSession, 0, 0, 0);
    }

	  LogString("--MAPIAddress - ZERO recipients\r\n");
	  return(result);
  }

  //
  // Get the blob loaded into memory...
  // 
  NSstringSeq blobSequence = (NSstringSeq) LoadBlobToMemory((LPSTR) addrPtr->lpszBlob);

  // Now check on the blob
  if (!blobSequence)
  {	  
    // Cleanup the shared memory...
    NSCloseSharedMemory(sMem, hSharedMemory);

    if (isTempSession)
    {
      MAPILogoff(lhSession, 0, 0, 0);
    }
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  ULONG           stringCount = 0; // This is to start with the rest of the string seq
  lpMapiRecipDesc lpNewRecips;

  // Now deal with the recipients of this message
  lpNewRecips = (lpMapiRecipDesc) malloc((size_t) (sizeof(MapiRecipDesc) * 
                                                    addrPtr->nNewRecips));
  if (!lpNewRecips)
  {
    // Cleanup the shared memory...
    NSCloseSharedMemory(sMem, hSharedMemory);

    NSStrSeqDelete(blobSequence);
	  LogString("--MAPIAddress failed\r\n");
    if (isTempSession)
    {
      MAPILogoff(lhSession, 0, 0, 0);
    }
	  return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  memset(lpNewRecips, 0, (size_t) (sizeof(MapiRecipDesc) * addrPtr->nNewRecips));

  for (i=0; i<addrPtr->nNewRecips; i++)
  {
    // String x: LPSTR lpszName;             // Recipient N name                           
    // String x: LPSTR lpszAddress;          // Recipient N address (optional)             
    // String x: LPSTR lpszRecipClass        // recipient class - sprintf of ULONG
    lpNewRecips[i].lpszName = 
        strdup( CheckNullString(NSStrSeqGet(blobSequence, stringCount++)) );

    lpNewRecips[i].lpszAddress = 
        strdup( CheckNullString(NSStrSeqGet(blobSequence, stringCount++)) );
      
    lpNewRecips[i].ulRecipClass = 
        (ULONG) atoi( CheckNullString(NSStrSeqGet(blobSequence, stringCount++)) );
  }

  //
  // Now that we are here, set the pointer array and move on with life...
  //
  *lppNewRecips = lpNewRecips;
  *lpnNewRecips = addrPtr->nNewRecips;
  SetPointerArray(lpNewRecips, MAPI_RECIPIENT_TYPE);
  
  // Cleanup the shared memory...
  NSCloseSharedMemory(sMem, hSharedMemory);
  if (blobSequence)
    NSStrSeqDelete(blobSequence);

  //
  // If we created a temp session, then we need to trash it...
  //
  if (isTempSession)
  {
    MAPILogoff(lhSession, 0, 0, 0);
  }

  TRACE("MAPIAddress Complete");
	return(result);
}

MAPI_IMPLEMENT(ULONG) 
MAPIGetNetscapeVersion(void)
{
  return(1);
}
