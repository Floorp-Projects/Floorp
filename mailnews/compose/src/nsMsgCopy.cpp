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
#include "nsMsgCopy.h"
#include "nsIPref.h"
#include "nsMsgCompPrefs.h"
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
NS_IMPL_ISUPPORTS(CopyListener, nsCOMTypeInfo<nsIMsgCopyServiceListener>::GetIID());

CopyListener::CopyListener(void) 
{ 
  mComposeAndSend = nsnull;
  NS_INIT_REFCNT(); 
}

CopyListener::~CopyListener(void) 
{
}

nsresult
CopyListener::OnStartCopy(nsISupports *listenerData)
{
#ifdef NS_DEBUG
  printf("CopyListener::OnStartCopy()\n");
#endif

  if (mComposeAndSend)
    mComposeAndSend->NotifyListenersOnStartCopy(listenerData);
  return NS_OK;
}
  
nsresult
CopyListener::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax, nsISupports *listenerData)
{
#ifdef NS_DEBUG
  printf("CopyListener::OnProgress() %d of %d\n", aProgress, aProgressMax);
#endif

  if (mComposeAndSend)
    mComposeAndSend->NotifyListenersOnProgressCopy(aProgress, aProgressMax, listenerData);

  return NS_OK;
}

nsresult
CopyListener::OnStopCopy(nsresult aStatus, nsISupports *listenerData)
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
    mComposeAndSend->NotifyListenersOnStopCopy(aStatus, listenerData);

  return NS_OK;
}

nsresult
CopyListener::SetMsgComposeAndSendObject(nsMsgComposeAndSend *obj)
{
  if (obj)
  {
    mComposeAndSend = obj;
    NS_ADDREF(mComposeAndSend);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// END  END  END  END  END  END  END  END  END  END  END  END  END  END  END 
// This is the listener class for the copy operation. We have to create this class 
// to listen for message copy completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////

nsMsgCopy::nsMsgCopy()
{
  mCopyListener = nsnull;
  mFileSpec = nsnull;
  mMsgSendObj = nsnull;
  mMode = nsMsgDeliverNow;
}

nsMsgCopy::~nsMsgCopy()
{
  if (mMsgSendObj)
    NS_RELEASE(mMsgSendObj);
  if (mCopyListener)
    NS_RELEASE(mCopyListener);
}

nsresult
nsMsgCopy::StartCopyOperation(nsIMsgIdentity       *aUserIdentity,
                              nsIFileSpec          *aFileSpec, 
                              nsMsgDeliverMode     aMode,
                              nsMsgComposeAndSend  *aMsgSendObj)
{
  nsCOMPtr<nsIMsgFolder>  dstFolder;
  nsIMessage              *msgToReplace = nsnull;
  PRBool                  isDraft = PR_FALSE;

  if (!aMsgSendObj)
    return NS_ERROR_FAILURE;

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
  nsresult rv = DoCopy(aFileSpec, dstFolder, msgToReplace, isDraft, nsnull);

  if (NS_SUCCEEDED(rv))
  {
    mMsgSendObj = aMsgSendObj;
    NS_ADDREF(mMsgSendObj);
  }

  return rv;
}


nsresult 
nsMsgCopy::DoCopy(nsIFileSpec *aDiskFile, nsIMsgFolder *dstFolder,
                  nsIMessage *aMsgToReplace, PRBool aIsDraft,
                  nsITransactionManager *txnMgr)
{
  nsresult rv = NS_OK;

  // Check sanity
  if ((!aDiskFile) || (!dstFolder))
    return NS_ERROR_FAILURE;

	//Call copyservice with dstFolder, disk file, and txnManager
	NS_WITH_SERVICE(nsIMsgCopyService, copyService, kMsgCopyServiceCID, &rv); 
	if(NS_SUCCEEDED(rv))
	{
    mCopyListener = new CopyListener();
    if (!mCopyListener)
      return MK_OUT_OF_MEMORY;

    NS_ADDREF(mCopyListener);
    mCopyListener->SetMsgComposeAndSendObject(mMsgSendObj);
    rv = copyService->CopyFileMessage(aDiskFile, dstFolder, aMsgToReplace, aIsDraft, 
                                      mCopyListener, nsnull, txnMgr);
	}

	return rv;
}

nsIMsgFolder *
LocateMessageFolder(nsIMsgIdentity   *userIdentity, 
                    nsMsgDeliverMode aFolderType)
{
  nsresult                  rv = NS_OK;
  nsIMsgFolder              *msgFolder= nsnull;
  PRUint32                  cnt = 0;
  PRUint32                  subCnt = 0;
  PRUint32                  i;

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

  for (i=0; i<cnt; i++)
  {
    // Now that we have the server...we need to get the named message folder
    nsCOMPtr<nsIMsgIncomingServer> inServer; 
    inServer = do_QueryInterface(retval->ElementAt(i));
    if(NS_FAILED(rv) || (!inServer))
      return nsnull;

    nsIFolder *aFolder;
    rv = inServer->GetRootFolder(&aFolder);
    if (NS_FAILED(rv) || (!aFolder))
      continue;

    nsCOMPtr<nsIMsgFolder> rootFolder;
    rootFolder = do_QueryInterface(aFolder);
    NS_RELEASE(aFolder);

    if(NS_FAILED(rv) || (!rootFolder))
      return nsnull;

    PRUint32 numFolders = 0;
    msgFolder = nsnull;
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
    
    if (NS_SUCCEEDED(rv) && (msgFolder)) 
      break;
  }

  return msgFolder;
}

nsIMsgFolder *
nsMsgCopy::GetUnsentMessagesFolder(nsIMsgIdentity   *userIdentity)
{
  return LocateMessageFolder(userIdentity, nsMsgQueueForLater);
}
 
nsIMsgFolder *
nsMsgCopy::GetDraftsFolder(nsIMsgIdentity *userIdentity)
{
  return LocateMessageFolder(userIdentity, nsMsgSaveAsDraft);
}

nsIMsgFolder *
nsMsgCopy::GetTemplatesFolder(nsIMsgIdentity *userIdentity)
{
  return LocateMessageFolder(userIdentity, nsMsgSaveAsTemplate);
}

nsIMsgFolder * 
nsMsgCopy::GetSentFolder(nsIMsgIdentity *userIdentity)
{
  return LocateMessageFolder(userIdentity, nsMsgDeliverNow);
}
