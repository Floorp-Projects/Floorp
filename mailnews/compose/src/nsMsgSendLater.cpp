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
#include "nsMsgBaseCID.h"


static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);

#include "nsIMsgAccountManager.h"

nsIMsgFolder *
GetUnsentMessagesFolder(nsIMsgIdentity *userIdentity)
{
  nsresult                  rv = NS_OK;
  nsIMsgFolder              *msgFolder= nsnull;
  
  //******************************************************************
  // This should really be passed in, but for now, we are going
  // to use this hack call...should go away!
  //******************************************************************
	// get the current identity from the mail session....
/*****
	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv); 
	if (NS_SUCCEEDED(rv) && mailSession)
		rv = mailSession->GetCurrentIdentity(getter_AddRefs(identity));
  if (NS_FAILED(rv)) 
    return nsnull;
  //******************************************************************
  // This should really be passed in, but for now, we are going
  // to use this hack call...should go away!
  //******************************************************************

  nsCOMPtr<nsIMsgAccountManager> accountManager;
  rv = mailSession->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) 
    return nsnull;

  nsISupportsArray *retval;
  accountManager->GetServersForIdentity(identity, &retval);

  if (retval->ElementAt(0) != nsnull)
    (retval->ElementAt(0))->QueryInterface(nsIMsgFolder::GetIID(), (void **)&msgFolder);
**/
  nsCOMPtr<nsIMsgIdentity>  identity = nsnull;

    NS_WITH_SERVICE(nsIMsgMailSession, session, kCMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) return nsnull;
    nsCOMPtr<nsIMsgAccountManager> accountManager;
    rv = session->GetAccountManager(getter_AddRefs(accountManager));
    if (NS_FAILED(rv)) return nsnull;   
    rv = session->GetCurrentIdentity(getter_AddRefs(identity));
    if (NS_FAILED(rv)) return nsnull;   

    nsISupportsArray *retval;
    accountManager->GetServersForIdentity(identity, &retval);

/**

		nsIMsgIncomingServer * incomingServer = nsnull;
		rv = mailSession->GetCurrentServer(&incomingServer);
		if (NS_SUCCEEDED(rv) && incomingServer)
		{
			char * value = nsnull;
			incomingServer->GetPrettyName(&value);
			printf("Server pretty name: %s\n", value ? value : "");
			incomingServer->GetUsername(&value);
			printf("User Name: %s\n", value ? value : "");
			incomingServer->GetHostName(&value);
			printf("Pop Server: %s\n", value ? value : "");
			incomingServer->GetPassword(&value);
			printf("Pop Password: %s\n", value ? value : "");

      nsIFolder *folder;
      nsIFolder *aRootFolder;

      incomingServer->GetRootFolder(&aRootFolder);
      if (aRootFolder)
        aRootFolder->FindSubFolder("Inbox", &folder);

			NS_RELEASE(incomingServer);
		}

**/

  return msgFolder;
}

static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);

nsresult
SendIndividualMailFile()
{
  nsFileSpec    fileSpec;
  nsresult      rv;

//	nsIFileSpec fileSpec (m_msg_file_name ? m_msg_file_name : "");	//need to convert to unix path
//	nsFilePath filePath (fileSpec);

  NS_WITH_SERVICE(nsISmtpService, smtpService, kSmtpServiceCID, &rv);
	if (NS_FAILED(rv) || !smtpService)
    return NS_ERROR_FAILURE;

//  rv = smtpService->SaveMessageToDisk(filePath, buf, nsnull, nsnull);
//  SaveMessageToDisk(const char *aMessageURI, nsIFileSpec *aFile, PR_FALSE, 
                      // nsIUrlListener *aUrlListener, nsnull);
	if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;    

  char *buf = "rhp@netscape.com";
  nsFilePath    filePath ("c:\\temp\\nsmail01.txt");
  rv = smtpService->SendMailMessage(filePath, buf, nsnull, nsnull);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  
  return NS_OK;
}

//
// If you pass in the user identity, then the "Outbox" will be 
// sent at this time.
//
nsresult
SendUnsentMessages(nsIMsgIdentity *userIdentity)
{
  nsresult            ret;
  nsIMsgFolder        *outboxFolder;
  nsIEnumerator       *enumerator = nsnull;

  outboxFolder = GetUnsentMessagesFolder(userIdentity);
  if (!outboxFolder)
    return NS_ERROR_FAILURE;

  //
  // Now, enumerate over the messages and send each via
  // SMTP...
  //
  ret = outboxFolder->GetMessages(&enumerator);
	if (NS_SUCCEEDED(ret) && (enumerator))
  {
    for (enumerator->First(); enumerator->IsDone() != NS_OK; enumerator->Next()) 
    {
      nsIMsgDBHdr *pMessage = nsnull;
      ret = enumerator->CurrentItem((nsISupports**)&pMessage);

      NS_ASSERTION(NS_SUCCEEDED(ret), "nsMsgDBEnumerator broken");
      if (NS_FAILED(ret)) 
        break;

      if (pMessage)
      {
        nsMsgKey      key;
        nsString      subject;

        (void)pMessage->GetMessageKey(&key);
        pMessage->GetSubject(subject);
        
        printf("message in thread %u %s\n", key, (const char *) &nsAutoString(subject));
      }

      pMessage = nsnull;
    }

    NS_RELEASE(enumerator);
  }

  return NS_OK;



/* 
*/


  
  
  /****
  identity
server (nsIMsgIncomingServer)
folder (nsIMsgFolder)
GetFolderWithFlags() - nsMsgFolderFlags.h (public)

GetMessages() on folder

nsIMessages returned
get URI, key, header (nsIMsgHdr - )
get message key

nsMsg

get POP service - extract from Berkely mail folder

nsIPop3Service 


smtptest.cpp - setup as a url listener






nsresult OnIdentityCheck()
{
	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &result); 
	if (NS_SUCCEEDED(result) && mailSession)
	{
		// mscott: we really don't check an identity, we check
		// for an outgoing 
		result = mailSession->GetCurrentServer(&incomingServer);
		if (NS_SUCCEEDED(result) && incomingServer)
		{
			char * value = nsnull;
			incomingServer->GetPrettyName(&value);
			printf("Server pretty name: %s\n", value ? value : "");
			incomingServer->GetUsername(&value);
			printf("User Name: %s\n", value ? value : "");
			incomingServer->GetHostName(&value);
			printf("Pop Server: %s\n", value ? value : "");
			incomingServer->GetPassword(&value);
			printf("Pop Password: %s\n", value ? value : "");

			NS_RELEASE(incomingServer);
		}
		else
			printf("Unable to retrieve the outgoing server interface....\n");
	}
	else
		printf("Unable to retrieve the mail session service....\n");

	return result;
}






















  NS_IMETHODIMP
nsMsgAppCore::CopyMessages(nsIDOMXULElement *srcFolderElement, nsIDOMXULElement *dstFolderElement,
						   nsIDOMNodeList *nodeList, PRBool isMove)
	nsresult rv;

	if(!srcFolderElement || !dstFolderElement || !nodeList)
		return NS_ERROR_NULL_POINTER;

	nsIRDFResource *srcResource, *dstResource;
	nsICopyMessageListener *dstFolder;
	nsIMsgFolder *srcFolder;
	nsISupportsArray *resourceArray;

	if(NS_FAILED(rv = dstFolderElement->GetResource(&dstResource)))
		return rv;

	if(NS_FAILED(rv = dstResource->QueryInterface(nsICopyMessageListener::GetIID(), (void**)&dstFolder)))
		return rv;

	if(NS_FAILED(rv = srcFolderElement->GetResource(&srcResource)))
		return rv;

	if(NS_FAILED(rv = srcResource->QueryInterface(nsIMsgFolder::GetIID(), (void**)&srcFolder)))
		return rv;

	if(NS_FAILED(rv =ConvertDOMListToResourceArray(nodeList, &resourceArray)))
		return rv;

	//Call the mailbox service to copy first message.  In the future we should call CopyMessages.
	//And even more in the future we need to distinguish between the different types of URI's, i.e.
	//local, imap, and news, and call the appropriate copy function.

	PRUint32 cnt;
  rv = resourceArray->Count(&cnt);
  if (NS_SUCCEEDED(rv) && cnt > 0)
	{
		nsIRDFResource * firstMessage = (nsIRDFResource*)resourceArray->ElementAt(0);
		char *uri;
		firstMessage->GetValue(&uri);
		nsCopyMessageStreamListener* copyStreamListener = new nsCopyMessageStreamListener(srcFolder, dstFolder, nsnull);

		nsIMsgMessageService * messageService = nsnull;
		rv = GetMessageServiceFromURI(uri, &messageService);

		if (NS_SUCCEEDED(rv) && messageService)
		{
			nsIURL * url = nsnull;
			messageService->CopyMessage(uri, copyStreamListener, isMove, nsnull, &url);
			ReleaseMessageServiceFromURI(uri, messageService);
		}

	}

	NS_RELEASE(srcResource);
	NS_RELEASE(srcFolder);
	NS_RELEASE(dstResource);
	NS_RELEASE(dstFolder);
	NS_RELEASE(resourceArray);
	return rv;
  **********/
return NS_OK;
}





































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
  mSmtpServer = nsnull;

  NS_INIT_REFCNT();
}

nsMsgSendLater::~nsMsgSendLater()
{
  PR_FREEIF(mSmtpServer);
}

nsresult 
nsMsgSendLater::SendUnsentMessages(nsIMsgIdentity *identity, char *unsentFolder, 
                                   char *smtpServer)
{
  if ( (!identity) || (!smtpServer) || (!*smtpServer))
    return NS_ERROR_FAILURE;

  mIdentity = identity;
  mSmtpServer = PL_strdup(smtpServer);
  if (!mSmtpServer)
    return NS_ERROR_FAILURE;
  
	return NS_OK;
}

