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
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsIMsgCopyService.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailSession.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgFolder.h"
#include "nsIMsgAccountManager.h"
#include "nsIFolder.h"
#include "nsISupportsArray.h"
#include "nsIMsgIncomingServer.h"
#include "nsISupports.h"
#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsRDFCID.h"
#include "nsIURL.h"
#include "nsMsgComposeStringBundle.h"
#include "nsMsgCompUtils.h"

static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kMsgCopyServiceCID,NS_MSGCOPYSERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the copy operation. We have to create this class 
// to listen for message copy completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
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
  nsresult			rv;

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
    rv = GetUnsentMessagesFolder(aUserIdentity, getter_AddRefs(dstFolder));
    isDraft = PR_FALSE;
    if (!dstFolder || NS_FAILED(rv)) {
        return NS_MSG_UNABLE_TO_SEND_LATER;
    } 
  }
  else if (aMode == nsMsgSaveAsDraft)    // SaveAsDraft (Drafts)
  {
    rv = GetDraftsFolder(aUserIdentity, getter_AddRefs(dstFolder));
    isDraft = PR_TRUE;
    if (!dstFolder || NS_FAILED(rv)) {
	return NS_MSG_UNABLE_TO_SAVE_DRAFT;
    } 
  }
  else if (aMode == nsMsgSaveAsTemplate) // SaveAsTemplate (Templates)
  {
    rv = GetTemplatesFolder(aUserIdentity, getter_AddRefs(dstFolder));
    isDraft = PR_FALSE;
    if (!dstFolder || NS_FAILED(rv)) {
	return NS_MSG_UNABLE_TO_SAVE_TEMPLATE;
    } 
  }
  else // SaveInSentFolder (Sent) -  nsMsgDeliverNow
  {
    rv = GetSentFolder(aUserIdentity, getter_AddRefs(dstFolder));
    isDraft = PR_FALSE;
    if (!dstFolder || NS_FAILED(rv)) {
	return NS_MSG_COULDNT_OPEN_FCC_FOLDER;
    }
  }

  mMode = aMode;
  rv = DoCopy(aFileSpec, dstFolder, aMsgToReplace, isDraft, nsnull, aMsgSendObj);
  return rv;
}

nsresult 
nsMsgCopy::DoCopy(nsIFileSpec *aDiskFile, nsIMsgFolder *dstFolder,
                  nsIMessage *aMsgToReplace, PRBool aIsDraft,
                  nsIMsgWindow *msgWindow,
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

    mCopyListener = do_QueryInterface(tPtr, &rv);
    if (NS_FAILED(rv) || !mCopyListener)
      return NS_ERROR_OUT_OF_MEMORY;

    mCopyListener->SetMsgComposeAndSendObject(aMsgSendObj);
    rv = copyService->CopyFileMessage(aDiskFile, dstFolder, aMsgToReplace,
                                      aIsDraft, mCopyListener, msgWindow);
	}

	return rv;
}

nsresult
nsMsgCopy::GetUnsentMessagesFolder(nsIMsgIdentity   *userIdentity, nsIMsgFolder **folder)
{
  return LocateMessageFolder(userIdentity, nsMsgQueueForLater, mSavePref, folder);
}
 
nsresult 
nsMsgCopy::GetDraftsFolder(nsIMsgIdentity *userIdentity, nsIMsgFolder **folder)
{
  return LocateMessageFolder(userIdentity, nsMsgSaveAsDraft, mSavePref, folder);
}

nsresult 
nsMsgCopy::GetTemplatesFolder(nsIMsgIdentity *userIdentity, nsIMsgFolder **folder)
{
  return LocateMessageFolder(userIdentity, nsMsgSaveAsTemplate, mSavePref, folder);
}

nsresult 
nsMsgCopy::GetSentFolder(nsIMsgIdentity *userIdentity, nsIMsgFolder **folder)
{
  return LocateMessageFolder(userIdentity, nsMsgDeliverNow, mSavePref, folder);
}

////////////////////////////////////////////////////////////////////////////////////
// Utility Functions for MsgFolders
////////////////////////////////////////////////////////////////////////////////////
nsresult
LocateMessageFolder(nsIMsgIdentity   *userIdentity, 
                    nsMsgDeliverMode aFolderType,
                    const char       *aFolderURI,
		    nsIMsgFolder     **msgFolder)
{
  nsresult                  rv = NS_OK;

  if (!msgFolder) return NS_ERROR_NULL_POINTER;

  if (!userIdentity || !aFolderURI || (PL_strlen(aFolderURI) == 0)) {
    return NS_ERROR_INVALID_ARG;
  }
  
  // get the current mail session....
  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv); 
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIMsgAccountManager> accountManager;
  rv = mailSession->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) return rv;

  // as long as it doesn't start with anyfolder://
  if (PL_strncasecmp(ANY_SERVER, aFolderURI, PL_strlen(aFolderURI)) != 0) {
    NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // get the corresponding RDF resource
    // RDF will create the folder resource if it doesn't already exist
    nsCOMPtr<nsIRDFResource> resource;
    rv = rdf->GetResource(aFolderURI, getter_AddRefs(resource));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr <nsIMsgFolder> folderResource;
    folderResource = do_QueryInterface(resource, &rv);
    if (NS_SUCCEEDED(rv) && folderResource) {
	*msgFolder = folderResource;
	NS_ADDREF(*msgFolder);
	return NS_OK;
    }
    else {
	return NS_ERROR_FAILURE;
    }
  }
  else {
    PRUint32                  cnt = 0;
    PRUint32                  i;
    
    // if anyfolder will do, go look for one.
    nsCOMPtr<nsISupportsArray> retval; 
    accountManager->GetServersForIdentity(userIdentity, getter_AddRefs(retval)); 
    if (!retval) return NS_ERROR_FAILURE;
    
    // Ok, we have to look through the servers and try to find the server that
    // has a valid folder of the type that interests us...
    rv = retval->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    
    for (i=0; i<cnt; i++) {
      // Now that we have the server...we need to get the named message folder
      nsCOMPtr<nsIMsgIncomingServer> inServer; 
      nsCOMPtr<nsISupports>ptr;
      ptr = retval->ElementAt(i);
      
      inServer = do_QueryInterface(ptr, &rv);
      if(NS_FAILED(rv) || (!inServer))
        continue;
      
      //
      // If aFolderURI is passed in, then the user has chosen a specific
      // mail folder to save the message, but if it is null, just find the
      // first one and make that work. The folder is specified as a URI, like
      // the following:
      //
      //                  mailbox://rhp@netscape.com/Sent
      //                  imap://rhp@nsmail-2/Drafts
      //                  newsgroup://news.mozilla.org/netscape.test
      //
      char *serverURI = nsnull;
      rv = inServer->GetServerURI(&serverURI);
      if ( NS_FAILED(rv) || (!serverURI) || !(*serverURI) )
        continue;
      
      nsCOMPtr <nsIFolder> folder;
      rv = inServer->GetRootFolder(getter_AddRefs(folder));
      if (NS_FAILED(rv) || (!folder))
        continue;
      
      nsCOMPtr<nsIMsgFolder> rootFolder;
      rootFolder = do_QueryInterface(folder, &rv);
      
      if(NS_FAILED(rv) || (!rootFolder))
        continue;
      
      PRUint32 numFolders = 0;
      
      // use the defaults by getting the folder by flags
      if (aFolderType == nsMsgQueueForLater)       // QueueForLater (Outbox)
        {
          rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_QUEUE, msgFolder, 1, &numFolders);
        }
      else if (aFolderType == nsMsgSaveAsDraft)    // SaveAsDraft (Drafts)
        {
          rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_DRAFTS, msgFolder, 1, &numFolders);
        }
      else if (aFolderType == nsMsgSaveAsTemplate) // SaveAsTemplate (Templates)
        {
          rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_TEMPLATES, msgFolder, 1, &numFolders);
        }
      else // SaveInSentFolder (Sent) -  nsMsgDeliverNow
        {
          rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_SENTMAIL, msgFolder, 1, &numFolders);
        }

      if (NS_SUCCEEDED(rv) && *msgFolder) {
	return NS_OK;
      }
    }
  }
  
  return NS_ERROR_FAILURE;
}

//
// Figure out if a folder is local or not and return a boolean to 
// say so.
//
nsresult
MessageFolderIsLocal(nsIMsgIdentity   *userIdentity, 
                     nsMsgDeliverMode aFolderType,
                     const char       *aFolderURI,
		     PRBool 	      *aResult)
{
  nsresult rv;
  nsXPIDLCString scheme;

  if (!aFolderURI) return NS_ERROR_NULL_POINTER;

  nsCOMPtr <nsIURL> url;
  rv = nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, nsCOMTypeInfo<nsIURL>::GetIID(), getter_AddRefs(url));
  if (NS_FAILED(rv)) return rv;

  rv = url->SetSpec(aFolderURI);
  if (NS_FAILED(rv)) return rv;
 
  rv = url->GetScheme(getter_Copies(scheme));
  if (NS_FAILED(rv)) return rv;

  /* mailbox:/ means its local (on disk) */
  if (PL_strcmp("mailbox", (const char *)scheme) == 0) {
	*aResult = PR_TRUE;
  }
  else {
	*aResult = PR_FALSE;
  }
  return NS_OK;
}

