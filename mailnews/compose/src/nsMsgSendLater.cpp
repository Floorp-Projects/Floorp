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
#include "nsCOMPtr.h"
#include "nsMsgCopy.h"
#include "nsIPref.h"
#include "nsMsgCompPrefs.h"
#include "nsMsgSendLater.h"
#include "nsIEnumerator.h"
#include "nsIFileSpec.h"
#include "nsISmtpService.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsMsgDeliveryListener.h"
#include "nsIMsgIncomingServer.h"
#include "nsICopyMessageListener.h"
#include "nsIMsgMessageService.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"
#include "nsMsgCompUtils.h"
#include "nsMsgUtils.h"
#include "nsMsgFolderFlags.h"
#include "nsIMessage.h"
#include "nsIRDFResource.h"
#include "nsISupportsArray.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_IID(kIMsgSendLater, NS_IMSGSENDLATER_IID);
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_CID(kMsgSendCID, NS_MSGSEND_CID); 
static NS_DEFINE_CID(kISupportsArrayCID, NS_SUPPORTSARRAY_CID);

// 
// This function will be used by the factory to generate the 
// nsMsgComposeAndSend Object....
//
nsresult NS_NewMsgSendLater(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgSendLater *pSendLater = new nsMsgSendLater();
		if (pSendLater)
			return pSendLater->QueryInterface(kIMsgSendLater, aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

NS_IMPL_ISUPPORTS(nsMsgSendLater, nsIMsgSendLater::GetIID())

nsMsgSendLater::nsMsgSendLater()
{
  mIdentity = nsnull;  
  mTempIFileSpec = nsnull;
  mTempFileSpec = nsnull;
  mEnumerator = nsnull;
  mFirstTime = PR_TRUE;
  mTotalSentSuccessfully = 0;
  mTotalSendCount = 0;
  mCompleteCallback = nsnull;
  mTagData = nsnull;
  mMessageFolder = nsnull;
  mMessage = nsnull;

  NS_INIT_REFCNT();
}

nsMsgSendLater::~nsMsgSendLater()
{
  if (mEnumerator)
    NS_RELEASE(mEnumerator);
  if (mTempIFileSpec)
    NS_RELEASE(mTempIFileSpec);
}

nsresult
SaveMessageCompleteCallback(nsIURI *aUrl, nsresult aExitCode, void *tagData)
{
  nsresult rv = NS_OK;

  if (tagData)
  {
    nsMsgSendLater *ptr = (nsMsgSendLater *) tagData;
    if (NS_SUCCEEDED(aExitCode))
    {
#ifdef NS_DEBUG
      printf("nsMsgSendLater: Success on the message save...\n");
#endif
      rv = ptr->CompleteMailFileSend();

      // If the send operation failed..try the next one...
      if (NS_FAILED(rv))
        rv = ptr->StartNextMailFileSend();

      NS_RELEASE(ptr);
    }
  }

  return rv;
}

nsresult
SendMessageCompleteCallback(nsresult aExitCode, void *tagData, nsFileSpec *returnFileSpec)
{
  nsresult                    rv = NS_OK;

  if (tagData)
  {
    nsMsgSendLater *ptr = (nsMsgSendLater *) tagData;
    if (NS_SUCCEEDED(aExitCode))
    {
#ifdef NS_DEBUG
      printf("nsMsgSendLater: Success on the message send operation!\n");
#endif

      PRBool    deleteMsgs = PR_FALSE;

      //
      // Now delete the message from the outbox folder.
      //
      NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
      if (NS_SUCCEEDED(rv) && prefs)
    		prefs->GetBoolPref("mail.really_delete_unsent_messages", &deleteMsgs);

      if (deleteMsgs)
        ptr->DeleteCurrentMessage();

      ++(ptr->mTotalSentSuccessfully);
      rv = ptr->StartNextMailFileSend();
      NS_RELEASE(ptr);
    }
  }

  return rv;
}

nsresult
nsMsgSendLater::CompleteMailFileSend()
{
nsresult                    rv;
nsString                    recips;
nsString                    ccList;
PRBool                      created;
nsCOMPtr<nsIMsgCompFields>  compFields = nsnull;
nsCOMPtr<nsIMsgSend>        pMsgSend = nsnull;

  // If for some reason the tmp file didn't get created, we've failed here
  mTempIFileSpec->exists(&created);
  if (!created)
    return NS_ERROR_FAILURE;

  // Get the recipients...
  if (NS_FAILED(mMessage->GetRecipients(recips)))
    return NS_ERROR_FAILURE;
  else
  	mMessage->GetCCList(ccList);

  // Get the composition fields interface
  nsresult res = nsComponentManager::CreateInstance(kMsgCompFieldsCID, NULL, nsIMsgCompFields::GetIID(), 
                                                    (void **) getter_AddRefs(compFields)); 
  if (NS_FAILED(res) || !compFields)
  {
    return NS_ERROR_FAILURE;
  }

  // Get the message send interface
  rv = nsComponentManager::CreateInstance(kMsgSendCID, NULL, nsIMsgSend::GetIID(), 
                                          (void **) getter_AddRefs(pMsgSend)); 
  if (NS_FAILED(res) || !pMsgSend)
  {
    return NS_ERROR_FAILURE;
  }

  char    *recipients = recips.ToNewCString();
  char    *cc = ccList.ToNewCString();

  compFields->SetTo(recipients, NULL);
  compFields->SetCc(cc, NULL);
  NS_ADDREF(this);  
  PR_FREEIF(recipients);
  PR_FREEIF(cc);
  rv = pMsgSend->SendMessageFile(compFields, // nsIMsgCompFields                  *fields,
                            mTempFileSpec,   // nsFileSpec                        *sendFileSpec,
                            PR_TRUE,         // PRBool                            deleteSendFileOnCompletion,
                            PR_FALSE,        // PRBool                            digest_p,
                            nsMsgDeliverNow, // nsMsgDeliverMode                  mode,
                            SendMessageCompleteCallback, // nsMsgSendCompletionCallback       completionCallback,
                            this);           // void                              *tagData);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsMsgSendLater::StartNextMailFileSend()
{
  nsFileSpec    fileSpec;
  nsresult      rv;
  char          *aMessageURI = nsnull;

  //
  // First, go to the next entry and check where we are in the 
  // enumerator!
  //
  if (mFirstTime)
  {
    mFirstTime = PR_FALSE;
    mEnumerator->First();
  }
  else
    mEnumerator->Next();

  if (mEnumerator->IsDone() == NS_OK)
  {
    // Call any caller based callbacks and then exit cleanly
#ifdef NS_DEBUG
    printf("nsMsgSendLater: Finished all \"Send Later\" operations successfully!\n");
#endif
    if (mCompleteCallback)
      mCompleteCallback(NS_OK, mTotalSendCount, mTotalSentSuccessfully, mTagData);
    return NS_OK;
  }

  nsCOMPtr<nsISupports>   currentItem;

  rv = mEnumerator->CurrentItem(getter_AddRefs(currentItem));
  if (NS_FAILED(rv))
  {
    return NS_ERROR_FAILURE;
  }

  mMessage = do_QueryInterface(currentItem); 
  if(!mMessage)
  {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRDFResource>  myRDFNode ;
  myRDFNode = do_QueryInterface(mMessage, &rv);
  if(NS_FAILED(rv) || (!myRDFNode))
  {
    return NS_ERROR_FAILURE;
  }

  myRDFNode->GetValue(&aMessageURI);

  char *tString = nsnull;
  nsString      subject;
  mMessage->GetSubject(subject);
  tString = subject.ToNewCString();
  printf("Sending message: [%s]\n", tString);
  PR_FREEIF(tString);

  mTempFileSpec = nsMsgCreateTempFileSpec("nsmail.tmp"); 
	if (!mTempFileSpec)
    return NS_ERROR_FAILURE;

  NS_NewFileSpecWithSpec(*mTempFileSpec, &mTempIFileSpec);
	if (!mTempIFileSpec)
    return NS_ERROR_FAILURE;

  nsIMsgMessageService * messageService = nsnull;
	rv = GetMessageServiceFromURI(aMessageURI, &messageService);
  if (NS_FAILED(rv) && !messageService)
    return NS_ERROR_FAILURE;

  ++mTotalSendCount;

  NS_ADDREF(this);
  nsMsgDeliveryListener *sendListener = new nsMsgDeliveryListener(SaveMessageCompleteCallback, 
                                                                  nsFileSaveDelivery, this);
  if (!sendListener)
  {
    ReleaseMessageServiceFromURI(aMessageURI, messageService);
    return NS_ERROR_FAILURE;
  }

  rv = messageService->SaveMessageToDisk(aMessageURI, mTempIFileSpec, PR_FALSE, sendListener, nsnull);
  ReleaseMessageServiceFromURI(aMessageURI, messageService);

	if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;    

  return NS_OK;
}

nsIMsgFolder *
nsMsgSendLater::GetUnsentMessagesFolder(nsIMsgIdentity *userIdentity)
{
  nsresult                  rv = NS_OK;
  nsIMsgFolder              *msgFolder= nsnull;
  
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
  
  nsISupportsArray *retval;
  accountManager->GetServersForIdentity(userIdentity, &retval);
  if (!retval) 
    return nsnull;

  // Now that we have the server...we need to get the named message folder
  nsCOMPtr<nsIMsgIncomingServer> inServer; 
  inServer = do_QueryInterface(retval->ElementAt(0));
  if(NS_FAILED(rv) || (!inServer))
    return nsnull;

  nsIFolder *aFolder;
  rv = inServer->GetRootFolder(&aFolder);
  if (NS_FAILED(rv) || (!aFolder)) 
    return nsnull;

  nsCOMPtr<nsIMsgFolder> rootFolder;
  rootFolder = do_QueryInterface(aFolder);
  if(NS_FAILED(rv) || (!aFolder))
    return nsnull;

  PRUint32 numFolders = 0;
  rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_QUEUE, &msgFolder, 1, &numFolders);

  if (NS_FAILED(rv) || (!msgFolder)) 
    return nsnull;
  return msgFolder;
}

nsresult 
nsMsgSendLater::SendUnsentMessages(nsIMsgIdentity                   *identity,
                                   nsMsgSendUnsentMessagesCallback  msgCallback,
                                   void                             *tagData)
{
 // RICHIE - for now hack it mIdentity = identity;

nsIMsgIdentity *GetHackIdentity();

  mIdentity = GetHackIdentity();
  if (!mIdentity)
    return NS_ERROR_FAILURE;

  mCompleteCallback = msgCallback;
  mTagData = tagData;

  mMessageFolder = GetUnsentMessagesFolder(mIdentity);
  if (!mMessageFolder)
    return NS_ERROR_FAILURE;

  nsresult ret = mMessageFolder->GetMessages(&mEnumerator);
	if (NS_FAILED(ret) || (!mEnumerator))
    return NS_ERROR_FAILURE;

  mFirstTime = PR_TRUE;
	return StartNextMailFileSend();
}

nsIMsgIdentity *
GetHackIdentity()
{
nsresult rv;

  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) 
  {
    printf("Failure on Mail Session Init!\n");
    return nsnull;
  }  

  nsCOMPtr<nsIMsgIdentity>        identity = nsnull;
  nsCOMPtr<nsIMsgAccountManager>  accountManager;

  rv = mailSession->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) 
  {
    printf("Failure getting account Manager!\n");
    return nsnull;
  }  

  rv = mailSession->GetCurrentIdentity(getter_AddRefs(identity));
  if (NS_FAILED(rv)) 
  {
    printf("Failure getting Identity!\n");
    return nsnull;
  }  

  return identity;
}

nsresult
nsMsgSendLater::DeleteCurrentMessage()
{
  nsCOMPtr<nsISupportsArray>  msgArray;

  // Get the composition fields interface
  nsresult res = nsComponentManager::CreateInstance(kISupportsArrayCID, NULL, nsISupportsArray::GetIID(), 
                                                    (void **) getter_AddRefs(msgArray)); 
  if (NS_FAILED(res) || !msgArray)
  {
    return NS_ERROR_FAILURE;
  }

  msgArray->InsertElementAt(mMessage, 0);
  res = mMessageFolder->DeleteMessages(msgArray, nsnull, PR_TRUE);
  if (NS_FAILED(res))
    return NS_ERROR_FAILURE;

  return NS_OK;
}
