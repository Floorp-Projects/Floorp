/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsXPIDLString.h"
#include "nsMsgCopy.h"
#include "nsIPref.h"
#include "nsIMsgCopyService.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailSession.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgFolder.h"

static NS_DEFINE_CID(kMsgCopyServiceCID,		NS_MSGCOPYSERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the copy operation. We have to create this class 
// to listen for message copy completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
// RICHIE SHERRY NS_IMPL_ISUPPORTS(CopyListener, nsCOMTypeInfo<nsIMsgCopyServiceListener>::GetIID());
NS_IMPL_ADDREF(CopyListener);
NS_IMPL_RELEASE(CopyListener);
NS_IMPL_QUERY_INTERFACE(CopyListener,nsCOMTypeInfo<nsIMsgCopyServiceListener>::GetIID());

CopyListener::CopyListener(void) 
{ 
  mComposeAndSend = nsnull;
  NS_INIT_REFCNT(); 
}

CopyListener::~CopyListener(void) 
{
  this;
}

nsresult
CopyListener::OnStartCopy()
{
#ifdef NS_DEBUG
  printf("CopyListener::OnStartCopy()\n");
#endif

  if (mComposeAndSend)
    mComposeAndSend->NotifyListenersOnStartCopy();
  return NS_OK;
}
  
nsresult
CopyListener::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
#ifdef NS_DEBUG
  printf("CopyListener::OnProgress() %d of %d\n", aProgress, aProgressMax);
#endif

  if (mComposeAndSend)
    mComposeAndSend->NotifyListenersOnProgressCopy(aProgress, aProgressMax);

  return NS_OK;
}

nsresult
CopyListener::SetMessageKey(PRUint32 aMessageKey)
{
    if (mComposeAndSend)
        mComposeAndSend->SetMessageKey(aMessageKey);
    return NS_OK;
}

nsresult
CopyListener::GetMessageId(nsCString* aMessageId)
{
    if (mComposeAndSend)
        mComposeAndSend->GetMessageId(aMessageId);
    return NS_OK;
}

nsresult
CopyListener::OnStopCopy(nsresult aStatus)
{
  if (NS_SUCCEEDED(aStatus))
  {
#ifdef NS_DEBUG
    printf("CopyListener: SUCCESSFUL ON THE COPY OPERATION!\n");
#endif
  }
  else
  {
#ifdef NS_DEBUG
    printf("CopyListener: COPY OPERATION FAILED!\n");
#endif
  }

  if (mComposeAndSend)
    mComposeAndSend->NotifyListenersOnStopCopy(aStatus);

  return NS_OK;
}

nsresult
CopyListener::SetMsgComposeAndSendObject(nsMsgComposeAndSend *obj)
{
  if (obj)
    mComposeAndSend = obj;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// END  END  END  END  END  END  END  END  END  END  END  END  END  END  END 
// This is the listener class for the copy operation. We have to create this class 
// to listen for message copy completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsMsgCopy, nsMsgCopy::GetIID());

nsMsgCopy::nsMsgCopy()
{
  mCopyListener = nsnull;
  mFileSpec = nsnull;
  mMode = nsMsgDeliverNow;
  mSavePref = nsnull;

  NS_INIT_REFCNT(); 
}

nsMsgCopy::~nsMsgCopy()
{
  PR_FREEIF(mSavePref);
}

nsresult
nsMsgCopy::StartCopyOperation(nsIMsgIdentity       *aUserIdentity,
                              nsIFileSpec          *aFileSpec, 
                              nsMsgDeliverMode     aMode,
                              nsMsgComposeAndSend  *aMsgSendObj,
                              const char           *aSavePref,
                              nsIMessage           *aMsgToReplace)
{
  nsCOMPtr<nsIMsgFolder>  dstFolder;
  PRBool                  isDraft = PR_FALSE;

  if (!aMsgSendObj)
    return NS_ERROR_INVALID_ARG;

  // Store away the server location...
  if (aSavePref)
    mSavePref = PL_strdup(aSavePref);

  //
  // Vars for implementation...
  //
  if (aMode == nsMsgQueueForLater)       // QueueForLater (Outbox)
  {
    dstFolder = GetUnsentMessagesFolder(aUserIdentity);
    isDraft = PR_FALSE;
  }
  else if (aMode == nsMsgSaveAsDraft)    // SaveAsDraft (Drafts)
  {
    dstFolder = GetDraftsFolder(aUserIdentity);
    isDraft = PR_TRUE;
  }
  else if (aMode == nsMsgSaveAsTemplate) // SaveAsTemplate (Templates)
  {
    dstFolder = GetTemplatesFolder(aUserIdentity);
    isDraft = PR_FALSE;
  }
  else // SaveInSentFolder (Sent) -  nsMsgDeliverNow
  {
    dstFolder = GetSentFolder(aUserIdentity);
    isDraft = PR_FALSE;
  }

  mMode = aMode;
  nsresult rv = DoCopy(aFileSpec, dstFolder, aMsgToReplace, isDraft, nsnull, aMsgSendObj);
  return rv;
}

nsresult 
nsMsgCopy::DoCopy(nsIFileSpec *aDiskFile, nsIMsgFolder *dstFolder,
                  nsIMessage *aMsgToReplace, PRBool aIsDraft,
                  nsITransactionManager *txnMgr,
                  nsMsgComposeAndSend   *aMsgSendObj)
{
  nsresult rv = NS_OK;

  // Check sanity
  if ((!aDiskFile) || (!dstFolder))
    return NS_ERROR_INVALID_ARG;

	//Call copyservice with dstFolder, disk file, and txnManager
	NS_WITH_SERVICE(nsIMsgCopyService, copyService, kMsgCopyServiceCID, &rv); 
	if(NS_SUCCEEDED(rv))
	{
    CopyListener    *tPtr = new CopyListener();
    if (!tPtr)
      return NS_ERROR_OUT_OF_MEMORY;

    mCopyListener = do_QueryInterface(tPtr);
    if (!mCopyListener)
      return NS_ERROR_OUT_OF_MEMORY;

    mCopyListener->SetMsgComposeAndSendObject(aMsgSendObj);
    rv = copyService->CopyFileMessage(aDiskFile, dstFolder, aMsgToReplace,
                                      aIsDraft, mCopyListener, txnMgr);
	}

	return rv;
}

nsIMsgFolder *
nsMsgCopy::GetUnsentMessagesFolder(nsIMsgIdentity   *userIdentity)
{
  return LocateMessageFolder(userIdentity, nsMsgQueueForLater, mSavePref);
}
 
nsIMsgFolder *
nsMsgCopy::GetDraftsFolder(nsIMsgIdentity *userIdentity)
{
  return LocateMessageFolder(userIdentity, nsMsgSaveAsDraft, mSavePref);
}

nsIMsgFolder *
nsMsgCopy::GetTemplatesFolder(nsIMsgIdentity *userIdentity)
{
  return LocateMessageFolder(userIdentity, nsMsgSaveAsTemplate, mSavePref);
}

nsIMsgFolder * 
nsMsgCopy::GetSentFolder(nsIMsgIdentity *userIdentity)
{
  return LocateMessageFolder(userIdentity, nsMsgDeliverNow, mSavePref);
}

////////////////////////////////////////////////////////////////////////////////////
// Utility Functions for MsgFolders
////////////////////////////////////////////////////////////////////////////////////
nsIMsgFolder *
LocateMessageFolder(nsIMsgIdentity   *userIdentity, 
                    nsMsgDeliverMode aFolderType,
                    const char       *aFolderURI)
{
  nsresult                  rv = NS_OK;
  nsIMsgFolder              *msgFolder= nsnull;
  PRUint32                  cnt = 0;
  PRUint32                  i;
  PRBool                    fixed = PR_FALSE;
  char                      *savePref = "";

  //
  // get the current mail session....
  //
	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv); 
  if (NS_FAILED(rv)) 
    return nsnull;

  nsCOMPtr<nsIMsgAccountManager> accountManager;
  rv = mailSession->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) 
    return nsnull;
  
  nsCOMPtr<nsISupportsArray> retval; 
  accountManager->GetServersForIdentity(userIdentity, getter_AddRefs(retval)); 
  if (!retval) 
    return nsnull; 

  // Ok, we have to look through the servers and try to find the server that
  // has a valid folder of the type that interests us...
  rv = retval->Count(&cnt);
  if (NS_FAILED(rv))
    return nsnull;

  // Make a working copy to avoid compiler problems with const...
  if ( (aFolderURI) && (*aFolderURI) )
    savePref = PL_strdup(aFolderURI);

  for (i=0; i<cnt; i++)
  {
    // Now that we have the server...we need to get the named message folder
    nsCOMPtr<nsIMsgIncomingServer> inServer; 
    nsISupports                    *ptr = retval->ElementAt(i);

    inServer = do_QueryInterface(ptr);
    NS_IF_RELEASE(ptr);
    if(NS_FAILED(rv) || (!inServer))
    {
      if (*savePref)
        PR_FREEIF(savePref);
      return nsnull;
    }

    //
    // If aFolderURI is passed in, then the user has chosen a specific
    // mail folder to save the message, but if it is null, just find the
    // first one and make that work. The folder is specified as a URI, like
    // the following:
    //
    //                  mailbox://rhp@netscape.com/Sent
    //                  newsgroup://news.mozilla.org/Outbox
    //
    if ( (savePref) && (*savePref) )
    {
      char *anyServer = "anyfolder://";
      if (PL_strncasecmp(anyServer, savePref, PL_strlen(savePref)) != 0)
      {
        char *serverURI = nsnull;
        nsresult res = inServer->GetServerURI(&serverURI);
        if ( NS_FAILED(res) || (!serverURI) || !(*serverURI) )
          continue;

        // First, make sure that savePref is only the protocol://server
        // for this comparison...not the file path if any...
        //
        if (!fixed)
        {
          char *ptr1 = PL_strstr(savePref, "//");
          if ( (ptr1) && (*ptr1) )
          {
            ptr1 = ptr1 + 2;
            char *ptr2 = PL_strchr(ptr1, '/');
            if ( (ptr2) && (*ptr2) )
              *ptr2 = '\0';
          }
          fixed = PR_TRUE;
        }
        // Now check to see if this URI is the same as the
        // server pref
        if (PL_strncasecmp(serverURI, savePref, PL_strlen(savePref)) != 0)
          continue;
      }
    }

    nsIFolder *aFolder;
    rv = inServer->GetRootFolder(&aFolder);
    if (NS_FAILED(rv) || (!aFolder))
      continue;

    nsCOMPtr<nsIMsgFolder> rootFolder;
    rootFolder = do_QueryInterface(aFolder);
    NS_RELEASE(aFolder);

    if(NS_FAILED(rv) || (!rootFolder))
      continue;

    PRUint32 numFolders = 0;
    msgFolder = nsnull;

    //
    // First, just try to match the aFolderURI which is a URI with the GetChildWithURI
    // call. If that fails, then fall back to the defaults, but if it works, then just
    // return what is found
    //
    if ( (aFolderURI) && (*aFolderURI) )
    {
      rv = rootFolder->GetChildWithURI(aFolderURI, PR_TRUE, &msgFolder);
      if (NS_SUCCEEDED(rv) && (msgFolder)) 
        break;
    }

    // 
    // If we haven't found the msgFolder, then just use the defaults
    // by getting the folder by flags
    //
    if (!msgFolder)
    {
      if (aFolderType == nsMsgQueueForLater)       // QueueForLater (Outbox)
      {
        rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_QUEUE, &msgFolder, 1, &numFolders);
      }
      else if (aFolderType == nsMsgSaveAsDraft)    // SaveAsDraft (Drafts)
      {
        rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_DRAFTS, &msgFolder, 1, &numFolders);
      }
      else if (aFolderType == nsMsgSaveAsTemplate) // SaveAsTemplate (Templates)
      {
        rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_TEMPLATES, &msgFolder, 1, &numFolders);
      }
      else // SaveInSentFolder (Sent) -  nsMsgDeliverNow
      {
        rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_SENTMAIL, &msgFolder, 1, &numFolders);
      }
    }
    
    if (NS_SUCCEEDED(rv) && (msgFolder)) 
      break;
  }

  if (*savePref)
    PR_FREEIF(savePref);

  return msgFolder;
}

//
// Figure out if a folder is local or not and return a boolean to 
// say so.
//
PRBool
MessageFolderIsLocal(nsIMsgIdentity   *userIdentity, 
                     nsMsgDeliverMode aFolderType,
                     const char       *aFolderURI)
{
  nsresult                        rv;
  nsXPIDLCString                  aType;
  nsCOMPtr<nsIMsgFolder>          dstFolder = nsnull;
  nsCOMPtr<nsIMsgIncomingServer>  dstServer = nsnull;

  dstFolder = LocateMessageFolder(userIdentity, aFolderType, aFolderURI);
  if (!dstFolder)
    return PR_TRUE;

  rv = dstFolder->GetServer(getter_AddRefs(dstServer));
  if (NS_FAILED(rv))
    return PR_TRUE;

  rv = dstServer->GetType(getter_Copies(aType));
  if (NS_FAILED(rv) || !aType)
    return PR_TRUE;

  if ((aType) && (*aType))
  {
    if (PL_strcasecmp(aType, "POP3") == 0)
      return PR_TRUE;
  }

  return PR_FALSE;
}

