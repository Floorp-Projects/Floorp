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
//  MAPI Hooks for Communicator
//  Note: THIS LIVES IN COMMUNICATOR THOUGH IT IS IN THE MAPIDLL FOR
//        BUILD REASONS
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997

#include <windows.h>
#include <trace.h>

#include <stdio.h>    
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "wfemsg.h"     // for WFE_MSGGetMaster()
#include "msgmapi.h"    // For enumerating messages...
#include "nsstrseq.h"
#include "mapihook.h"
#include "nscpmapi.h"
#include "mapismem.h"
#include "mapimail.h"
#include "hiddenfr.h"
#include "msgcom.h"

#define   DELIMIT         "-{|}-"    // DON'T I18N THIS!

//
// Static defines for the MAPI support...
//
static CMAPIConnection *mapiConnections[MAX_CON] = {NULL, NULL, NULL, NULL};

//
// Forward declarations...
//
LONG    ProcessMAPILogon(MAPILogonType *logonInfo);
LONG    ProcessMAPILogoff(MAPILogoffType *logoffInfo);
LONG    ProcessMAPISendMail(MAPISendMailType *sendMailPtr);
LONG    ProcessMAPISaveMail(MAPISendMailType *sendMailPtr);
LONG    ProcessMAPISendDocuments(MAPISendDocumentsType *sendDocPtr);
LONG    ProcessMAPIFindNext(MAPIFindNextType *findInfo);
LONG    ProcessMAPIDeleteMail(MAPIDeleteMailType *delInfo);
LONG    ProcessMAPIResolveName(MAPIResolveNameType *nameInfo);
LONG    ProcessMAPIDetails(MAPIDetailsType *detailInfo);
LONG    ProcessMAPIReadMail(MAPIReadMailType *readInfo);
LONG    ProcessMAPIAddress(MAPIAddressType *addrInfo);

//
// This will store the CFrameWnd we startup for MAPI and if we do start
// an instance of the browser, we will close it out when we leave.
//
static CFrameWnd  *pMAPIFrameWnd = NULL;

void 
StoreMAPIFrameWnd(CFrameWnd *pFrameWnd)
{
  pMAPIFrameWnd = pFrameWnd;
}

// This is the result of a WM_COPYDATA message. This will be used to send requests
// into Communicator for Simple MAPI commands.
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
//                        // Simple MAPI Calls.
//		} COPYDATASTRUCT; 
//
// Returns: The MAPI Return code for the operation:
//
//
LONG ProcessNetscapeMAPIHook(WPARAM wParam, LPARAM lParam)
{
  PCOPYDATASTRUCT	  pcds = (PCOPYDATASTRUCT) lParam;
  MAPIIPCType       *ipcInfo; 
#ifdef WIN32   
  HANDLE            hSharedMemory;
#endif

	if (lParam == NULL)
	{
		return(MAPI_E_FAILURE);
	}

  //
  // Get shared memory info structure pointer...
  //
  ipcInfo = (MAPIIPCType *)pcds->lpData;
  if (ipcInfo == NULL)
	{
		return(MAPI_E_FAILURE);
	}

  //
  // Now connect to shared memory...or just set the
  // pointer for Win16
  //
#ifdef WIN32
  CSharedMem *sMem = NSOpenExistingSharedMemory((LPCTSTR) ipcInfo->smemName, &hSharedMemory);
  if (!sMem)
	{
		return(MAPI_E_FAILURE);
	}
#else
  if (!ipcInfo->lpsmem)
  {
    return(MAPI_E_FAILURE);
  }
#endif

  TRACE("MAPI: MAPIHook Message ID = %d\n", pcds->dwData);
	switch (pcds->dwData) 
	{
  //////////////////////////////////////////////////////////////////////
  // MAPILogon
  //////////////////////////////////////////////////////////////////////
  case NSCP_MAPILogon:
    {
     MAPILogonType     *logonInfo;
#ifdef WIN32
    logonInfo = (MAPILogonType *) &(sMem->m_buf[0]);
#else    
    logonInfo = (MAPILogonType *) ipcInfo->lpsmem;
#endif

    if (!logonInfo)
    {
      return(MAPI_E_FAILURE);
    }
  
    logonInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPILogon(logonInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // MAPILogoff
  //////////////////////////////////////////////////////////////////////
  case NSCP_MAPILogoff:
    {
    MAPILogoffType     *logoffInfo;
 
#ifdef WIN32
    logoffInfo = (MAPILogoffType *) &(sMem->m_buf[0]);
#else    
    logoffInfo = (MAPILogoffType *) ipcInfo->lpsmem;
#endif

    if (!logoffInfo)
    {
      return(MAPI_E_FAILURE);
    }

    logoffInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPILogoff(logoffInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NSCP_MAPISendMail
  //////////////////////////////////////////////////////////////////////
  case NSCP_MAPISendMail:
     {
     MAPISendMailType     *sendMailPtr;
#ifdef WIN32
    sendMailPtr = (MAPISendMailType *) &(sMem->m_buf[0]);
#else    
    sendMailPtr = (MAPISendMailType *) ipcInfo->lpsmem;
#endif

    if (!sendMailPtr)
    {
      return(MAPI_E_FAILURE);
    }

    sendMailPtr->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPISendMail(sendMailPtr));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // MAPISendDocuments
  //////////////////////////////////////////////////////////////////////
  case NSCP_MAPISendDocuments:
     {
     MAPISendDocumentsType     *sendDocPtr;
#ifdef WIN32
    sendDocPtr = (MAPISendDocumentsType *) &(sMem->m_buf[0]);
#else    
    sendDocPtr = (MAPISendDocumentsType *) ipcInfo->lpsmem;
#endif

    if (!sendDocPtr)
    {
      return(MAPI_E_FAILURE);
    }

    sendDocPtr->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPISendDocuments(sendDocPtr));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // MAPIFindNext
  //////////////////////////////////////////////////////////////////////
  case NSCP_MAPIFindNext:
    {
    MAPIFindNextType    *findInfo;
 
#ifdef WIN32
    findInfo = (MAPIFindNextType *) &(sMem->m_buf[0]);
#else    
    findInfo = (MAPIFindNextType *) ipcInfo->lpsmem;
#endif

    if (!findInfo)
    {
      return(MAPI_E_FAILURE);
    }

    findInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPIFindNext(findInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // MAPIDeleteMail
  //////////////////////////////////////////////////////////////////////
  case NSCP_MAPIDeleteMail:
    {
    MAPIDeleteMailType    *delInfo;
 
#ifdef WIN32
    delInfo = (MAPIDeleteMailType *) &(sMem->m_buf[0]);
#else    
    delInfo = (MAPIDeleteMailType *) ipcInfo->lpsmem;
#endif

    if (!delInfo)
    {
      return(MAPI_E_FAILURE);
    }

    delInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPIDeleteMail(delInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // MAPIResolveName
  //////////////////////////////////////////////////////////////////////
  case NSCP_MAPIResolveName:
    {
    MAPIResolveNameType    *nameInfo;
 
#ifdef WIN32
    nameInfo = (MAPIResolveNameType *) &(sMem->m_buf[0]);
#else    
    nameInfo = (MAPIResolveNameType *) ipcInfo->lpsmem;
#endif

    if (!nameInfo)
    {
      return(MAPI_E_FAILURE);
    }

    nameInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPIResolveName(nameInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // MAPIDetails
  //////////////////////////////////////////////////////////////////////
  case NSCP_MAPIDetails:
    {
    MAPIDetailsType    *detailInfo;
 
#ifdef WIN32
    detailInfo = (MAPIDetailsType *) &(sMem->m_buf[0]);
#else    
    detailInfo = (MAPIDetailsType *) ipcInfo->lpsmem;
#endif

    if (!detailInfo)
    {
      return(MAPI_E_FAILURE);
    }

    detailInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPIDetails(detailInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // MAPIReadMail
  //////////////////////////////////////////////////////////////////////  
  case NSCP_MAPIReadMail:
    {
    MAPIReadMailType    *readInfo;
 
#ifdef WIN32
    readInfo = (MAPIReadMailType *) &(sMem->m_buf[0]);
#else    
    readInfo = (MAPIReadMailType *) ipcInfo->lpsmem;
#endif

    if (!readInfo)
    {
      return(MAPI_E_FAILURE);
    }

    readInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPIReadMail(readInfo));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // NSCP_MAPISaveMail
  //////////////////////////////////////////////////////////////////////
  case NSCP_MAPISaveMail:
     {
     MAPISendMailType     *sendMailPtr;
#ifdef WIN32
    sendMailPtr = (MAPISendMailType *) &(sMem->m_buf[0]);
#else    
    sendMailPtr = (MAPISendMailType *) ipcInfo->lpsmem;
#endif

    if (!sendMailPtr)
    {
      return(MAPI_E_FAILURE);
    }

    sendMailPtr->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPISaveMail(sendMailPtr));
    break;
    }

  //////////////////////////////////////////////////////////////////////
  // MAPIAddress
  //////////////////////////////////////////////////////////////////////
  case NSCP_MAPIAddress:
    {
    MAPIAddressType    *addrInfo;
 
#ifdef WIN32
    addrInfo = (MAPIAddressType *) &(sMem->m_buf[0]);
#else    
    addrInfo = (MAPIAddressType *) ipcInfo->lpsmem;
#endif

    if (!addrInfo)
    {
      return(MAPI_E_FAILURE);
    }

    addrInfo->ipcWorked = 1;         // Set the worked flag
    return(ProcessMAPIAddress(addrInfo));
    }
  
  case NSCP_MAPIFree:   // This should never hit Communicator, but if it does
#ifdef WIN16
    return(SUCCESS_SUCCESS);       // Just return
#else
    return(S_OK);       // Just return 
#endif

	default:
    return(MAPI_E_NOT_SUPPORTED);     // Should never hit here!
		break;
	}

	return(SUCCESS_SUCCESS);
}

//
// Find the default session for Communicator...
//
CMAPIConnection 
*GetDefaultSession(void)
{
  int i;
  
  for (i=0; i < MAX_CON; i++)
  {
    if (mapiConnections[i] != NULL)
    {
      if (mapiConnections[i]->IsDefault())
      {
        return mapiConnections[i];
      }
    }
  }

  return(NULL);
}

//
// Find an open session slot...
//
LONG
GetOpenSessionSlot( void )
{
  int i;
  
  for (i=0; i < MAX_CON; i++)
  {
    if (mapiConnections[i] == NULL)
    {
      return(i);
    }
  }

  return(-1);
}

//
// Returns TRUE if it was reassigned and FALSE if not.
//
BOOL
TryDefaultReassignment(void)
{
  int   i;
  int   loc = -1;

  // Find any sessions left?
  for (i=0; i < MAX_CON; i++)
  {
    if (mapiConnections[i] != NULL)
    {
      loc = i;
      break;
    }
  }

  // Set default state on the object to true
  if (loc >= 0)
  {
    mapiConnections[loc]->SetDefault(TRUE);
    return(TRUE);
  }
  else    
  {
    return(FALSE);
  }
}

LONG
ProcessMAPILogon(MAPILogonType *logonInfo)
{
  CMAPIConnection *defSession = GetDefaultSession();
  NSstringSeq strSeq = (LPSTR) logonInfo + sizeof(MAPILogonType);
  LPSTR lpszProfileName = NSStrSeqGet(strSeq, 0);
  LPSTR lpszPassword    = NSStrSeqGet(strSeq, 1);

  TRACE("MAPI: ProcessMAPILogon() ProfileName = [%s] Password = [%s]\n", 
                            lpszProfileName, lpszPassword);
  //
  // This is a query if lpszProfileName is NULL, lpszPassword is NULL and 
  // the MAPI_LOGON_UI is not set 
  //
  if ((lpszProfileName[0] == '\0') && (lpszPassword[0] == '\0') && 
    (!(logonInfo->flFlags & MAPI_LOGON_UI)) )
  {
    if (defSession == NULL)
    {
      return(MAPI_E_FAILURE);
    }
    else
    {
      defSession->IncrementSessionCount();
      logonInfo->lhSession = defSession->GetID();
      return(SUCCESS_SUCCESS);
    }
  }

  //
  // Only allow for sessions by a single profile name...
  //
  if ( ( defSession != NULL ) &&
       (
        (lstrcmp(lpszProfileName, defSession->GetProfileName() ) != 0) ||
        (lstrcmp(lpszPassword, defSession->GetPassword() ) != 0)
       )
     )
  {
    return(MAPI_E_TOO_MANY_SESSIONS);
  }

  //
  // Now create the new MAPI session if we have to...
  //
  BOOL createNew = FALSE;
  if ( 
      ( defSession == NULL ) ||
      // Indicates an attempt should be made to create a new session rather than acquire 
      // the environment's shared session. If the MAPI_NEW_SESSION flag is not set, MAPILogon 
      // uses an existing shared session.
      (logonInfo->flFlags & MAPI_NEW_SESSION) ||
      (defSession == NULL)      // No default session exists!
     ) 
  {
    createNew = TRUE;
  }

  //
  // Make sure only 4 max are allowed...
  //
  LONG  slot; 
  if ((createNew) && ((slot = GetOpenSessionSlot()) == -1))
  {
    return(MAPI_E_TOO_MANY_SESSIONS);
  }

  //
  // If we don't have to create a new session, just return the ID and
  // move on with life
  //
  if (!createNew)
  {
    if (defSession == NULL)
    {
      return(MAPI_E_FAILURE);
    }

    defSession->IncrementSessionCount();
    logonInfo->lhSession = defSession->GetID();
    return(SUCCESS_SUCCESS);
  }

  //
  // Finally, create a new session!
  //
  mapiConnections[slot] = new CMAPIConnection(slot, lpszProfileName, lpszPassword);
  if (defSession == NULL)
  {
    mapiConnections[slot]->SetDefault(TRUE);
  }

  logonInfo->lhSession = mapiConnections[slot]->GetID();

  //
  // Process Flags...
  //
  // Indicates an attempt should be made to download all of the user's messages before 
  // returning. If the MAPI_FORCE_DOWNLOAD flag is not set, messages can be downloaded 
  // in the background after the function call returns.
  if (logonInfo->flFlags & MAPI_FORCE_DOWNLOAD) 
  {
    MAPIGetNewMessagesInBackground();
  }

  // Indicates that a logon dialog box should be displayed to prompt the user for 
  // logon information. If the user needs to provide a password and profile name 
  // to enable a successful logon, MAPI_LOGON_UI must be set.
  if (logonInfo->flFlags & MAPI_LOGON_UI) 
  {
    TRACE("MAPI: ProcessMAPILogon() MAPI_LOGON_UI Unsupported.\n");
  }

#ifdef WIN32
  // Indicates that MAPILogon should only prompt for a password and not allow the user 
  // to change the profile name. Either MAPI_PASSWORD_UI or MAPI_LOGON_UI should not be 
  // set, since the intent is to select between two different dialog boxes for logon.
  if (logonInfo->flFlags & MAPI_PASSWORD_UI)
  {
    TRACE("MAPI: ProcessMAPILogon() MAPI_PASSWORD_UI Unsupported.\n");
  }         
#endif  
  
  return(SUCCESS_SUCCESS);
}

LONG    
ProcessMAPILogoff(MAPILogoffType *logoffInfo)
{
  TRACE("MAPI: ProcessMAPILogoff() Session ID = [%d]\n", logoffInfo->lhSession);
  //
  // Verify the session handle...
  //
  if (( (logoffInfo->lhSession-1) >= MAX_CON) || ( (logoffInfo->lhSession-1) < 0))
  {
    return(MAPI_E_INVALID_SESSION);
  }

  CMAPIConnection *logoffSession = mapiConnections[(logoffInfo->lhSession - 1)];
  if (logoffSession == NULL)
  {
    return(MAPI_E_INVALID_SESSION);
  }
  
  //
  // Decrement the session count, then if this is the last one 
  // connected to this session, then kill it...
  //
  logoffSession->DecrementSessionCount();
  if (logoffSession->GetSessionCount() <= 0)
  {
    //
    // If this was the default session "holder", then we need to 
    // assign that task to a new one if it exists, if not, then
    // it is all back to null.
    //
    BOOL needToReassign = logoffSession->IsDefault();

    delete logoffSession;
    mapiConnections[logoffInfo->lhSession - 1] = NULL;

    if (needToReassign)
    {
      TRACE("MAPI: ProcessMAPILogoff() Need to reassign default\n");
      if (!TryDefaultReassignment())
      {
        if (pMAPIFrameWnd != NULL)
        {
          pMAPIFrameWnd->PostMessage(WM_CLOSE);
          pMAPIFrameWnd = NULL;
        }
      }
    }
  }

  return(SUCCESS_SUCCESS);
}

//
// Actually process the send mail operation...
//
LONG    
ProcessMAPISendMail(MAPISendMailType *sendMailPtr)
{
  CMAPIConnection *mySession;
  NSstringSeq     mailInfoSeq;

  TRACE("MAPI: ProcessMAPISendMail() Session ID = [%d]\n", sendMailPtr->lhSession);
  //
  // Verify the session handle...
  //
  if (((sendMailPtr->lhSession-1) >= MAX_CON) || ((sendMailPtr->lhSession-1) < 0))
  {
    return(MAPI_E_FAILURE);
  }

  mySession = mapiConnections[(sendMailPtr->lhSession - 1)];
  if (mySession == NULL)
  {
    return(MAPI_E_FAILURE);
  }

  //
  // Before we start, make sure things make sense...
  //
  if ( (!(sendMailPtr->flFlags & MAPI_DIALOG)) && (sendMailPtr->MSG_nRecipCount == 0) )
  {
    return(MAPI_E_UNKNOWN_RECIPIENT);
  }

  mailInfoSeq = (NSstringSeq) &(sendMailPtr->dataBuf[0]);
  TRACE("MAPI: ProcessMAPISendMail() Session ID = [%d]\n", sendMailPtr->lhSession);

  //
  // Now set the show window flag...
  //
  BOOL showWindow = (( sendMailPtr->flFlags & MAPI_DIALOG ) != 0);
  LONG rc = DoFullMAPIMailOperation(sendMailPtr, 
                                    NSStrSeqGet(mailInfoSeq, 1),
                                    showWindow);
  return(rc);
}

//
// Actually process the send mail operation...
//
LONG    
ProcessMAPISaveMail(MAPISendMailType *sendMailPtr)
{
  CMAPIConnection *mySession;
  NSstringSeq     mailInfoSeq;

  TRACE("MAPI: ProcessMAPISaveMail() Session ID = [%d]\n", sendMailPtr->lhSession);
  //
  // Verify the session handle...
  //
  if (((sendMailPtr->lhSession-1) >= MAX_CON) || ((sendMailPtr->lhSession-1) < 0))
  {
    return(MAPI_E_FAILURE);
  }

  mySession = mapiConnections[(sendMailPtr->lhSession - 1)];
  if (mySession == NULL)
  {
    return(MAPI_E_FAILURE);
  }

  mailInfoSeq = (NSstringSeq) &(sendMailPtr->dataBuf[0]);
  TRACE("MAPI: ProcessMAPISaveMail() Session ID = [%d]\n", sendMailPtr->lhSession);

  //
  // Now process the save mail operation...
  //
  LONG rc = DoMAPISaveMailOperation(sendMailPtr, 
                                    NSStrSeqGet(mailInfoSeq, 1));
  return(rc);
}

//
// Actually process the send documents operation...
//
LONG    
ProcessMAPISendDocuments(MAPISendDocumentsType *sendDocPtr)
{
  LONG rc = DoPartialMAPIMailOperation(sendDocPtr);    
  return(rc);
}

//
// This is an ENUM procedure for messages that are in the system
//
LONG    
ProcessMAPIFindNext(MAPIFindNextType *findInfo)
{
  CMAPIConnection *mySession;

  //
  // Verify the session handle...
  //
  if (((findInfo->lhSession-1) >= MAX_CON) || ((findInfo->lhSession-1) < 0))
  {
    return(MAPI_E_INVALID_SESSION);
  }

  mySession = mapiConnections[(findInfo->lhSession - 1)];
  if (mySession == NULL)
  {
    return(MAPI_E_INVALID_SESSION);
  }

  //
  // If this is true, then this is the first call to this FindNext function
  // and we should start the enumeration operation.
  //
	MSG_FolderInfo  *inboxFolder = NULL;
	int32           folderRC = MSG_GetFoldersWithFlag(WFE_MSGGetMaster(),
        										MSG_FOLDER_FLAG_INBOX, &inboxFolder, 1);
  if (folderRC <= 0)
  {
    return(MAPI_E_NO_MESSAGES);
  }

  // This is the first message
  if (mySession->GetMessageIndex() < 0)
  {
    MSG_MapiListContext *cookie;

    MessageKey keyFound = MSG_GetFirstKeyInFolder(inboxFolder, &cookie);
    if (keyFound == MSG_MESSAGEKEYNONE)
    {
      mySession->SetMapiListContext(NULL);
      mySession->SetMessageIndex(-1); // Reset to -1...
      return(MAPI_E_NO_MESSAGES);
    }

    TRACE("MAPI: ProcessMAPIFindNext() Found message id = %d\n", keyFound);

    wsprintf((LPSTR) findInfo->lpszMessageID, "%d", keyFound);
    mySession->SetMapiListContext(cookie);
    mySession->SetMessageIndex(keyFound); // Just set to 1 and then increment from there...
  }
  else
  {
    MSG_MapiListContext *cookie = (MSG_MapiListContext *)mySession->GetMapiListContext();
    
    // Sanity check the cookie value...
    if (cookie == NULL)
    {
      mySession->SetMapiListContext(NULL);
      mySession->SetMessageIndex(-1); // Reset to -1...
      return(MAPI_E_NO_MESSAGES);
    }

    MessageKey nextKey = MSG_GetNextKeyInFolder(cookie);
    if (nextKey == MSG_MESSAGEKEYNONE)
    {
      mySession->SetMapiListContext(NULL);
      mySession->SetMessageIndex(-1); // Reset to -1...
      return(MAPI_E_NO_MESSAGES);
    }

    TRACE("MAPI: ProcessMAPIFindNext() Found message id = %d\n", nextKey);

    wsprintf((LPSTR) findInfo->lpszMessageID, "%d", nextKey);
    mySession->SetMessageIndex(nextKey); // Set the key to the found one and move on...
  }

  return(SUCCESS_SUCCESS);
}

//
// Actually process the delete mail operation!
//
LONG    
ProcessMAPIDeleteMail(MAPIDeleteMailType *delInfo)
{
  CMAPIConnection *mySession;

  //
  // Verify the session handle...
  //
  if (((delInfo->lhSession-1) >= MAX_CON) || ((delInfo->lhSession-1) < 0))
  {
    return(MAPI_E_INVALID_SESSION);
  }

  mySession = mapiConnections[(delInfo->lhSession - 1)];
  if (mySession == NULL)
  {
    return(MAPI_E_INVALID_SESSION);
  }

  BOOL    msgValid = TRUE;

  TRACE("MAPI: RICHIE TODO: ProcessMAPIDeleteMail() TRY TO FIND THE MESSAGE FROM THE IDENTIFIER:\n");
  TRACE("MAPI: RICHIE TODO: ProcessMAPIDeleteMail() [%s]\n", delInfo->lpszMessageID);

  if (msgValid)
  {
    TRACE("MAPI: RICHIE TODO: ProcessMAPIDeleteMail() DELETE THE MESSAGE!!!\n");

    return(SUCCESS_SUCCESS);
  }
  else
  {
    return(MAPI_E_INVALID_MESSAGE);
  }
}

// 
// Process the name resolution for the address passed in.
//
LONG    
ProcessMAPIResolveName(MAPIResolveNameType *nameInfo)
{
  CMAPIConnection *mySession;

  //
  // Verify the session handle...
  //
  if (((nameInfo->lhSession-1) >= MAX_CON) || ((nameInfo->lhSession-1) < 0))
  {
    return(MAPI_E_INVALID_SESSION);
  }

  mySession = mapiConnections[(nameInfo->lhSession - 1)];
  if (mySession == NULL)
  {
    return(MAPI_E_INVALID_SESSION);
  }

  TRACE("MAPI: ProcessMAPIResolveName() TRY TO IDENTIFY THE ADDRESS INFO FOR THE NAME:\n");
  TRACE("MAPI: ProcessMAPIResolveName() [%s]\n", nameInfo->lpszName);

  DWORD numFound;
  LPSTR retFullName = NULL;
  LPSTR retAddr = NULL;
  
  AB_ContainerInfo  *ctr;
  ABID              id;

  numFound = AB_MAPI_ResolveName ( 
                (char *)nameInfo->lpszName, // i.e. a nickname or part of the person's name to look up. Assumes First Last 
                &ctr,                       // caller allocates the ctr ptr. BE fills Caller must close container when done with it
                &id);                       // caller allocates the ABID, BE fills 

  if (numFound > 1)
  {
    return(MAPI_E_AMBIGUOUS_RECIPIENT);
  }
  else if (numFound <= 0)
  {
    return(MAPI_E_UNKNOWN_RECIPIENT);
  }
  else    // Here we found a single entry!
  {
    // Get the full name and email address...
    retFullName = AB_MAPI_GetFullName(ctr, id);
    retAddr = AB_MAPI_GetEmailAddress(ctr, id);

    // null out return values just in case...
    nameInfo->lpszABookName[0] = '\0';
    nameInfo->lpszABookAddress[0] = '\0';

    // if valid, copy the strings over...
    if (retFullName)
      lstrcpy((LPSTR) nameInfo->lpszABookName, retFullName);
    if (retAddr)
      lstrcpy((LPSTR) nameInfo->lpszABookAddress, retAddr);

    // Now get the string to use for an ID later on...
    LPSTR  abookIDString =  AB_MAPI_ConvertToDescription(ctr); 

    // build that string ID to pass back through the MAPI API
    wsprintf((LPSTR) nameInfo->lpszABookID, "%d%s", id, DELIMIT);
    lstrcat((LPSTR) nameInfo->lpszABookID, abookIDString);
    
    // Be nice and cleanup after ourselves...
    if (retAddr)
      XP_FREE(retAddr);
    if (retFullName)
      XP_FREE(retFullName);
    if (abookIDString)
      XP_FREE(abookIDString);

    return(SUCCESS_SUCCESS);
  }
}

//
// This deals with the MAPIDetails call
//
LONG    
ProcessMAPIDetails(MAPIDetailsType *detailInfo)
{
  CMAPIConnection *mySession;

  //
  // Verify the session handle...
  //
  if (((detailInfo->lhSession-1) >= MAX_CON) || ((detailInfo->lhSession-1) < 0))
  {
    return(MAPI_E_INVALID_SESSION);
  }

  mySession = mapiConnections[(detailInfo->lhSession - 1)];
  if (mySession == NULL)
  {
    return(MAPI_E_INVALID_SESSION);
  }

  TRACE("MAPI: ProcessMAPIDetails() NEED TO LOCATE THE ENTRY FOR:\n");
  TRACE("MAPI: ProcessMAPIDetails() [%s]\n", detailInfo->lpszABookID);

  char    workString[MAX_NAME_LEN];
  char    *idString;
  char    *containerString;

  lstrcpy((LPSTR) workString, (LPSTR) detailInfo->lpszABookID);

  //
  // Now parse out the first and last names of the person...
  //
  int       totLen = lstrlen((LPSTR) workString);
  int       loc = 0;
  idString = &(workString[0]);

  while ( ( lstrlen((LPSTR) DELIMIT) + loc) < totLen)
  {
    if (strncmp((LPSTR)(workString + loc), DELIMIT, lstrlen(DELIMIT)) == 0)
    {
      workString[loc] = '\0';
      containerString = &(workString[loc + lstrlen(DELIMIT)]);
      break;
    }

    ++loc;
  }
    
  if ( (lstrlen((LPSTR) DELIMIT) + loc) >= totLen )
  {
    return(MAPI_E_INVALID_RECIPS);
  }

  // Now, convert the stuff to real values for the API...
  ABID              id = atoi((const char *) idString);
  AB_ContainerInfo  *container = AB_MAPI_ConvertToContainer(containerString);
  MSG_Pane          *personPane = NULL;
  extern MWContext         *GetUsableContext(void);

  // Need to first create a person entry pane and then call
  // FE_ShowPropertySheetAB2 on that pane - return 0 is problem
  int rc = AB_MAPI_CreatePropertySheetPane( 
            GetUsableContext(),
            WFE_MSGGetMaster(), 
            container, 
            id, 
            &personPane);  // BE will allocate a personPane and return a ptr to it here 
  if (!rc)
  {
    return(MAPI_E_INVALID_RECIPS);
  }

  // Now display the address book entry for this user. If the parameter
  // detailInfo->ulUIParam  is NULL, just doit and return, but if it is
  // not NULL, then we should do it as application modal.
  
  // RICHIE - THIS IS NOT IN THE BUILD YET! MUST WAIT
  //(void) FE_ShowPropertySheetForAB2(personPane, AB_Person);

  // cleanup the container and person pane...
  AB_MAPI_CloseContainer(&container);
  AB_ClosePane(personPane); 

  return(SUCCESS_SUCCESS);
}

//
// CMAPIConnections object...should be in their own file...
//
CMAPIConnection::CMAPIConnection  ( LONG id, LPSTR name, LPSTR pw)
{
  m_sessionCount = 1;
  m_defaultConnection = FALSE;
  m_ID = (id + 1);      // since zero is invalid, but have to make sure we 
                        // decrement by one when it is passed in again.
  m_messageIndex = -1;  // For tracing our way through the FindNext operation
  m_cookie = NULL;      // For doing the enumeration of messages
  m_messageFindInfo[0] = '\0';
  lstrcpy((LPSTR) m_profileName, name);
  lstrcpy((LPSTR) m_password, pw);  
}

CMAPIConnection::~CMAPIConnection ( )
{
}

//
// This is all needed to process a MAPIReadMail operation
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

LONG
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

LONG    
ProcessMAPIReadMail(MAPIReadMailType *readInfo)
{
  CMAPIConnection *mySession;
  lpMapiMessage   message;      // this is the message we will receive

  //
  // Verify the session handle...
  //
  if (((readInfo->lhSession-1) >= MAX_CON) || ((readInfo->lhSession-1) < 0))
  {
    return(MAPI_E_INVALID_SESSION);
  }

  mySession = mapiConnections[(readInfo->lhSession - 1)];
  if (mySession == NULL)
  {
    return(MAPI_E_INVALID_SESSION);
  }

  // First, gotta get the inbox folder!
  MSG_FolderInfo  *inboxFolder = NULL;
	int32           folderRC = MSG_GetFoldersWithFlag(WFE_MSGGetMaster(),
        										MSG_FOLDER_FLAG_INBOX, &inboxFolder, 1);
  if (folderRC <= 0)
  {
    return(MAPI_E_INVALID_MESSAGE);
  }

  // Convert the message id to a number!
  MessageKey lookupKey = atoi((const char *)readInfo->lpszMessageID);
  TRACE("MAPI: ProcessMAPIReadMail() Find this message = [%s] - [%d]\n", 
                          readInfo->lpszMessageID, lookupKey);

  // Now see if we can find this message!
  BOOL msgFound = MSG_GetMapiMessageById(inboxFolder, lookupKey, &message);

  // If we didn't find the message, then we need to bail!
  if (!msgFound)
  {
    TRACE("MAPI: ProcessMAPIReadMail() MESSAGE NOT FOUND\n");
    return(MAPI_E_INVALID_MESSAGE);
  }

  //
  // Now that we have found the message, we need to populate the chunk of
  // memory that we will write to disk and then move on. For now, we need
  // to setup all of the variables for the information we will need for the
  // message.
  //
  ULONG     i;
  DWORD     arrayCount = 0;         // Counter for array entries!
  DWORD     arrayTotal = 0;         // Total for array entries!
  FLAGS     msgFlags;               // unread, return or receipt                  
  ULONG     msgRecipCount = message->nRecipCount; // Number of recipients                   
  ULONG     msgFileAttachCount = message->nFileCount; // # of file attachments                  

  LPSTR     msgSubject = message->lpszSubject;          // Message Subject
  LPSTR     msgNoteText = NULL;                         // Message Text will be
  LPSTR     msgDateReceived = message->lpszDateReceived;// in YYYY/MM/DD HH:MM format
  LPSTR     msgThreadID = message->lpszConversationID;  // conversation thread ID

  //
  // The following are for the originator of the message. Only ONE of these.
  //
  LPSTR     origName = NULL;    // Originator name                           
  LPSTR     origAddress = NULL; // Originator address (optional)             
  ULONG     origRecipClass = MAPI_TO;               // Recipient class - MAPI_TO, MAPI_CC, MAPI_BCC, MAPI_ORIG

  // Now we have to make sure there is an originator and set the vars...
  if (message->lpOriginator)
  {
    origName    = message->lpOriginator->lpszName;
    origAddress = message->lpOriginator->lpszAddress;
  }

  if ( ! (readInfo->flFlags & MAPI_ENVELOPE_ONLY))
  {
      TRACE("MAPI: ProcessMAPIReadMail() GET NOTE TEXT - ONLY IF WE HAVE TO!\n"); 
      msgNoteText = message->lpszNoteText;
  }

  // First, figure out how many entries we need to have in a string array...
  arrayTotal = 6 + 1; // 6 for the first 6 strings and 1 for the final NULL    

  // Add the recipient information
  arrayTotal += (msgRecipCount * 3);

  // Now check to see if we are going to pass back attachments:
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
  if ((readInfo->flFlags & MAPI_SUPPRESS_ATTACH) ||
      (readInfo->flFlags & MAPI_ENVELOPE_ONLY) )
  {
    msgFileAttachCount = 0;
  }

  arrayTotal += (msgFileAttachCount * 2);
  
  // now allocate the memory for the string array 
  LPSTR *strArray = (LPSTR *) malloc((size_t) (sizeof(LPSTR) * arrayTotal));
  if (!strArray)
  {
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  // Now assign the easy return values to the shared memory structure
  readInfo->MSG_flFlags     = msgFlags;           // unread, return or receipt                  
  readInfo->MSG_nRecipCount = msgRecipCount;      // Number of recipients                   
  readInfo->MSG_nFileCount  = msgFileAttachCount; // # of file attachments                  
  readInfo->MSG_ORIG_ulRecipClass = origRecipClass; //  Recipient class - MAPI_TO, MAPI_CC, MAPI_BCC, MAPI_ORIG

  // Now go through and fill out the array to create the string sequence
  strArray[0] = CheckNull(msgSubject);
  strArray[1] = CheckNull(msgNoteText);
  strArray[2] = CheckNull(msgDateReceived);
  strArray[3] = CheckNull(msgThreadID);
  strArray[4] = CheckNull(origName);
  strArray[5] = CheckNull(origAddress);

  arrayCount = 6;   // counter for the rest of the needed attachments

  // Now add all of the recipients
  for (i=0; i<msgRecipCount; i++)
  {
    UCHAR     tempString[128];
    ULONG     messageClass = message->lpRecips[i].ulRecipClass; // Get the recipient class...

    // Now get the following 3 items:
    //      String x: LPSTR lpszName;             // Recipient N name                           
    //      String x: LPSTR lpszAddress;          // Recipient N address (optional)             
    //      String x: LPSTR lpszRecipClass        // recipient class - sprintf of ULONG
    strArray[arrayCount++] = CheckNull(message->lpRecips[i].lpszName);
    strArray[arrayCount++] = CheckNull(message->lpRecips[i].lpszAddress);

    // 0	MAPI_ORIG	Indicates the original sender of the message. 
    // 1	MAPI_TO	Indicates a primary message recipient. 
    // 2	MAPI_CC	Indicates a recipient of a message copy. 
    // 3	MAPI_BCC	Indicates a recipient of a blind copy. 
    wsprintf((LPSTR) tempString, "%d", messageClass);
    strArray[arrayCount++] = CheckNull((LPSTR) tempString);
  }

  // Now, add all of the attachments to this blob
  for (i=0; i<msgFileAttachCount; i++)
  {
    //      String x: LPSTR lpszPathName // Fully qualified path of the attached file. 
    //                                   // This path should include the disk drive letter and directory name.
    //      String x: LPSTR lpszFileName // The display name for the attached file
    //
    strArray[arrayCount++] = CheckNull(message->lpFiles[i].lpszPathName);
    strArray[arrayCount++] = CheckNull(message->lpFiles[i].lpszFileName);
  }

  strArray[arrayCount] = NULL;

  // Create the string sequence
  NSstringSeq strSeq = NSStrSeqNew(strArray);

  // Now just cleanup the array....
  free(strArray);
 
  if (!strSeq)
  {
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }
  
  // Now get the name of the file to write the blob to!
  if (GetTempAttachmentName((LPSTR)readInfo->lpszBlob) < 0)
  {
    NSStrSeqDelete(strSeq);
    return(MAPI_E_ATTACHMENT_WRITE_FAILURE);
  }

  // Now write the blob to disk and move on...
  if (WriteTheMemoryBufferToDisk((LPSTR) readInfo->lpszBlob, 
                      (UINT) NSStrSeqSize(strSeq), strSeq) != 0)
  {
    NSStrSeqDelete(strSeq);
    return(MAPI_E_ATTACHMENT_WRITE_FAILURE);
  }

  //
  // MAPI_PEEK 
  // Indicates MAPIReadMail does not mark the message as read. Marking a message as 
  // read affects its appearance in the user interface and generates a read receipt. 
  // If the messaging system does not support this flag, MAPIReadMail always marks 
  // the message as read. If MAPIReadMail encounters an error, it leaves the 
  // message unread.  
  if (!(readInfo->flFlags & MAPI_PEEK))
  {
    MSG_MarkMapiMessageRead (inboxFolder, lookupKey, TRUE);
  }

  // Now we have to free the message and the chunk of memory...
  MSG_FreeMapiMessage(message);
  NSStrSeqDelete(strSeq);

  return(SUCCESS_SUCCESS);
}

// necessary variables for 
int           arrayCount   = 0;         // Counter for array entries!
int           addressCount = 0;         // Counter for array entries!
LPSTR         *addrArray;
LPSTR         toString  = "1";
LPSTR         ccString  = "2";
LPSTR         bccString = "3";
 
BOOL 
AddressCallback(int totalCount, int currentIndex, 
                int addrType, LPSTR addrString)
{
  LPSTR     messageClass; 

  TRACE("TOTAL COUNT   = %d\n", totalCount);
  TRACE("CURRENT COUNT = %d\n", currentIndex);
  TRACE("CURRENT ADDR  = %s\n", addrString);

  // If this is the first entry, then we need to allocate the array...
  if (currentIndex == 0)
  {
    addressCount = totalCount;
    addrArray = (LPSTR *) malloc( (size_t) 
                            sizeof(LPSTR) * ((addressCount * 3) + 1) 
                          );
    if (!addrArray)
    {
      addressCount = 0;
      return FALSE;
    }
  }

  // if the array was not allocated...
  if (!addrArray)
  {
    addressCount = 0;
    return FALSE;
  }

  // Deal with the address type...
  if (addrType == MSG_CC_HEADER_MASK)
    messageClass = ccString;
  else if (addrType == MSG_BCC_HEADER_MASK)
    messageClass = bccString;
  else 
    messageClass = toString;

  // Now get the following 3 items:
  //      String x: LPSTR lpszName;             // Recipient N name                           
  //      String x: LPSTR lpszAddress;          // Recipient N address (optional)             
  //      String x: LPSTR lpszRecipClass        // recipient class - sprintf of ULONG

  char *ptr = strchr(addrString, '<');
  if (ptr)
  {
    addrString[strlen(addrString) - 1] = '\0';
    *(ptr-1) = '\0';
  }
  addrArray[arrayCount++] = CheckNull(addrString);

  if (ptr)
  {
    ptr++;
  }
  else
  {
    ptr = (char *) addrString;
  }

  addrArray[arrayCount++] = CheckNull(ptr);

  // 0	MAPI_ORIG	Indicates the original sender of the message. 
  // 1	MAPI_TO	Indicates a primary message recipient. 
  // 2	MAPI_CC	Indicates a recipient of a message copy. 
  // 3	MAPI_BCC	Indicates a recipient of a blindcopy. 
  addrArray[arrayCount++] = CheckNull(messageClass);

  return(TRUE);
}

int               getAddressCount = 0;
MAPIAddressType   *getAddressPtr = NULL;
NSstringSeq       addrSeq = NULL;

// rhp - for MAPI calls...
BOOL
GetNextAddress(LPSTR *name, LPSTR *address, int *addrType)
{
  if ( getAddressCount >= (getAddressPtr->nRecips * 3) )
  {
    return FALSE;
  }

  ULONG   aType = atoi(NSStrSeqGet(addrSeq, getAddressCount++));

  // return which type of address this is?
  if (aType == MAPI_CC)
    *addrType = MSG_CC_HEADER_MASK;
  else if (aType == MAPI_BCC)
    *addrType = MSG_BCC_HEADER_MASK;
  else
    *addrType = MSG_TO_HEADER_MASK;
      
  *name    = (LPSTR) NSStrSeqGet(addrSeq, getAddressCount++);
  *address = (LPSTR) NSStrSeqGet(addrSeq, getAddressCount++);

  return TRUE;
}

LONG
ProcessMAPIAddress(MAPIAddressType *addrInfo)
{
  CMAPIConnection *mySession;

  TRACE("MAPI: ProcessMAPIAddress() Incomming Addrs = %d\nCaption = [%s]\n",
                    addrInfo->nRecips,
                    addrInfo->lpszCaption);
  //
  // Verify the session handle...
  //
  if (((addrInfo->lhSession-1) >= MAX_CON) || ((addrInfo->lhSession-1) < 0))
  {
    return(MAPI_E_INVALID_SESSION);
  }

  mySession = mapiConnections[(addrInfo->lhSession - 1)];
  if (mySession == NULL)
  {
    return(MAPI_E_INVALID_SESSION);
  }

  // 
  // Put up the address dialog and do the right thing :-)
  // 
  addrSeq = (NSstringSeq) &(addrInfo->dataBuf[0]);
  getAddressCount = 0;  // Make sure to reset the counter for address info...
  getAddressPtr = addrInfo; // Address pointer for callback...
  arrayCount = 0;       // counter for the rest of the entries...
  theApp.m_pHiddenFrame->AddressDialog((LPSTR) (addrInfo->lpszCaption), 
                                            AddressCallback, GetNextAddress);

  // If the address count is zero, then we can return early...
  addrInfo->nNewRecips = addressCount;
  if (addressCount == 0)
  {
    return(SUCCESS_SUCCESS);
  }

  addrArray[arrayCount] = NULL;  

  // Create the string sequence
  NSstringSeq strSeq = NSStrSeqNew(addrArray);
  
  // Now just cleanup the array....
  free(addrArray);
  
  if (!strSeq)
  {
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }
  
  // Now get the name of the file to write the blob to!
  if (GetTempAttachmentName((LPSTR)addrInfo->lpszBlob) < 0)
  {
    NSStrSeqDelete(strSeq);
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  // Now write the blob to disk and move on...
  if (WriteTheMemoryBufferToDisk((LPSTR) addrInfo->lpszBlob, 
                      (UINT) NSStrSeqSize(strSeq), strSeq) != 0)
  {
    NSStrSeqDelete(strSeq);
    return(MAPI_E_INSUFFICIENT_MEMORY);
  }

  NSStrSeqDelete(strSeq);
  return(SUCCESS_SUCCESS);
}
