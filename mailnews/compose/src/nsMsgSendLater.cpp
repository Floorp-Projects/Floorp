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
#include "nsMsgCompUtils.h"
#include "nsMsgUtils.h"
#include "nsMsgFolderFlags.h"
#include "nsIMessage.h"
#include "nsIRDFResource.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_IID(kIMsgSendLater, NS_IMSGSENDLATER_IID);

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
  mTempFileSpec = nsnull;
  mEnumerator = nsnull;
  mFirstTime = PR_TRUE;

  NS_INIT_REFCNT();
}

nsMsgSendLater::~nsMsgSendLater()
{
  NS_RELEASE(mEnumerator);
  if (mTempFileSpec)
    delete mTempFileSpec;
}

nsresult
SaveMessageCompleteCallback(nsIURL *aUrl, nsresult aExitCode, void *tagData)
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
    }
  }

  return rv;
}

nsresult
SendMessageCompleteCallback(nsIURL *aUrl, nsresult aExitCode, void *tagData)
{
  nsresult rv = NS_OK;

  if (tagData)
  {
    nsMsgSendLater *ptr = (nsMsgSendLater *) tagData;
    if (NS_SUCCEEDED(aExitCode))
    {
#ifdef NS_DEBUG
      printf("nsMsgSendLater: Success on the message send operation!\n");
#endif

      rv = ptr->StartNextMailFileSend();
    }
  }

  return rv;
}

nsresult
nsMsgSendLater::CompleteMailFileSend()
{
nsresult rv;

  NS_WITH_SERVICE(nsISmtpService, smtpService, kSmtpServiceCID, &rv);
	if (NS_FAILED(rv) || !smtpService)
    return NS_ERROR_FAILURE;

  char      *aUnixStyleFilePath;

  nsString      recips;
  mMessage->GetRecipients(recips);
  printf("SEND THIS : [%s]\n", recips.GetBuffer());

  mMessage->GetRecipients(recips);
  char      *buf = recips.ToNewCString();

  mTempFileSpec->GetUnixStyleFilePath(&aUnixStyleFilePath);
  if (!aUnixStyleFilePath)
    return NS_ERROR_FAILURE;

  nsFilePath    filePath (aUnixStyleFilePath);

  nsMsgDeliveryListener *sendListener = new nsMsgDeliveryListener(SendMessageCompleteCallback, 
                                                                  nsMailDelivery, this);
  if (!sendListener)
    return NS_ERROR_FAILURE;

  rv = smtpService->SendMailMessage(filePath, buf, nsnull, nsnull);
  PR_FREEIF(buf);
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
  printf("Fun subject %s\n", tString);
  PR_FREEIF(tString);

  nsFileSpec *tSpec = nsMsgCreateTempFileSpec("nsmail.tmp"); 
	if (!tSpec)
    return NS_ERROR_FAILURE;

//  NS_NewFileSpecWithSpec(*tSpec, &mTempFileSpec);
  delete tSpec;
	if (!mTempFileSpec)
    return NS_ERROR_FAILURE;

  nsIMsgMessageService * messageService = nsnull;
	rv = GetMessageServiceFromURI(aMessageURI, &messageService);
  if (NS_FAILED(rv) && !messageService)
    return NS_ERROR_FAILURE;

  nsMsgDeliveryListener *sendListener = new nsMsgDeliveryListener(SaveMessageCompleteCallback, 
                                                                  nsFileSaveDelivery, this);
  if (!sendListener)
  {
    ReleaseMessageServiceFromURI(aMessageURI, messageService);
    return NS_ERROR_FAILURE;
  }

  rv = messageService->SaveMessageToDisk(aMessageURI, mTempFileSpec, PR_FALSE, sendListener, nsnull);
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
nsMsgSendLater::SendUnsentMessages(nsIMsgIdentity *identity)
{
  if (!identity)
    return NS_ERROR_FAILURE;

  mIdentity = identity;

  mMessageFolder = GetUnsentMessagesFolder(identity);
  if (!mMessageFolder)
    return NS_ERROR_FAILURE;

  nsresult ret = mMessageFolder->GetMessages(&mEnumerator);
	if (NS_FAILED(ret) || (!mEnumerator))
    return NS_ERROR_FAILURE;

  mFirstTime = PR_TRUE;
	return StartNextMailFileSend();
}




#include "nsIComponentManager.h" 
#include "nsMsgCompCID.h"
#include "nsIMsgCompose.h"
#include "nsIMsgCompFields.h"
#include "nsIMsgSend.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nscore.h"
#include "nsIMsgMailSession.h"
#include "nsINetSupportDialogService.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsISmtpService.h"
#include "nsISmtpUrl.h"
#include "nsIUrlListener.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsIFileLocator.h"
#include "MsgCompGlue.h"
#include "nsCRT.h"
#include "prmem.h"
#include "nsIMimeURLUtils.h"
#include "nsMsgSendLater.h"
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
static NS_DEFINE_CID(kMimeURLUtilsCID, NS_IMIME_URLUTILS_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kIAppShellServiceIID, NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIMsgSendLaterIID, NS_IMSGSENDLATER_IID); 
static NS_DEFINE_CID(kMsgSendLaterCID, NS_MSGSENDLATER_CID); 

void DoItDude(void)
{
nsresult rv;

  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) 
  {
    printf("Failure on Mail Session Init!\n");
    return ;
  }  

  nsCOMPtr<nsIMsgIdentity>  identity = nsnull;
  
  NS_WITH_SERVICE(nsIMsgMailSession, session, kCMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) 
  {
    printf("Failure getting Mail Session!\n");
    return ;
  }  

  nsCOMPtr<nsIMsgAccountManager> accountManager;
  rv = session->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) 
  {
    printf("Failure getting account Manager!\n");
    return ;
  }  
  rv = session->GetCurrentIdentity(getter_AddRefs(identity));
  if (NS_FAILED(rv)) 
  {
    printf("Failure getting Identity!\n");
    return ;
  }  
  
  nsIMsgSendLater *pMsgSendLater;
  rv = nsComponentManager::CreateInstance(kMsgSendLaterCID, NULL, kIMsgSendLaterIID, (void **) &pMsgSendLater); 
  if (rv == NS_OK && pMsgSendLater) 
  { 
    printf("We succesfully obtained a nsIMsgSendLater interface....\n");    
    pMsgSendLater->SendUnsentMessages(identity);
  }
}
