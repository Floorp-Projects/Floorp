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

#include "msgCore.h"    // precompiled header...
#include "nsCOMPtr.h"

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsMailboxService.h"
#include "nsIMsgMailSession.h"
#include "nsMailboxUrl.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsMailboxProtocol.h"
#include "nsIMsgDatabase.h"
#include "nsMsgDBCID.h"
#include "nsMsgKeyArray.h"
#include "nsLocalUtils.h"
#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kCMailboxUrl, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

nsMailboxService::nsMailboxService()
{
    NS_INIT_REFCNT();
}

nsMailboxService::~nsMailboxService()
{}

NS_IMPL_THREADSAFE_ADDREF(nsMailboxService);
NS_IMPL_THREADSAFE_RELEASE(nsMailboxService);

nsresult nsMailboxService::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (nsnull == aInstancePtr)
        return NS_ERROR_NULL_POINTER;
 
    if (aIID.Equals(nsIMailboxService::GetIID()) || aIID.Equals(kISupportsIID)) 
	{
        *aInstancePtr = (void*) ((nsIMailboxService*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIMsgMessageService::GetIID())) 
	{
        *aInstancePtr = (void*) ((nsIMsgMessageService*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
	if (aIID.Equals(nsIProtocolHandler::GetIID()))
	{
        *aInstancePtr = (void*) ((nsIProtocolHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
	}

#if defined(NS_DEBUG)
    /*
     * Check for the debug-only interface indicating thread-safety
     */
    static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);
    if (aIID.Equals(kIsThreadsafeIID))
        return NS_OK;
#endif
 
    return NS_NOINTERFACE;
}

nsresult nsMailboxService::ParseMailbox(nsFileSpec& aMailboxPath, nsIStreamListener *aMailboxParser, 
										nsIUrlListener * aUrlListener, nsIURI ** aURL)
{
	nsCOMPtr<nsIMailboxUrl> mailboxurl;
	nsresult rv = NS_OK;
	NS_LOCK_INSTANCE();

	rv = nsComponentManager::CreateInstance(kCMailboxUrl,
                                            nsnull,
                                            nsIMailboxUrl::GetIID(),
                                            (void **) getter_AddRefs(mailboxurl));
	if (NS_SUCCEEDED(rv) && mailboxurl)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(mailboxurl);
		// okay now generate the url string
		nsFilePath filePath(aMailboxPath); // convert to file url representation...
		char * urlSpec = PR_smprintf("mailbox://%s", (const char *) filePath);
		url->SetSpec(urlSpec);
		PR_FREEIF(urlSpec);
		mailboxurl->SetMailboxParser(aMailboxParser);
		if (aUrlListener)
			url->RegisterListener(aUrlListener);

		RunMailboxUrl(url, nsnull);

		if (aURL)
		{
			*aURL = url;
			NS_IF_ADDREF(*aURL);
		}
	}

	NS_UNLOCK_INSTANCE();

	return rv;
}
						 
nsresult nsMailboxService::CopyMessage(const char * aSrcMailboxURI,
                              nsIStreamListener * aMailboxCopyHandler,
                              PRBool moveMessage,
                              nsIUrlListener * aUrlListener,
                              nsIURI **aURL)
{
	nsCOMPtr<nsIMailboxUrl> mailboxurl;
	nsresult rv = NS_OK;
	NS_LOCK_INSTANCE();

	nsMailboxAction mailboxAction = nsIMailboxUrl::ActionMoveMessage;

	rv = PrepareMessageUrl(aSrcMailboxURI, aUrlListener, mailboxAction, getter_AddRefs(mailboxurl));

	if (NS_SUCCEEDED(rv) && mailboxurl)
	{

		if (!moveMessage)
			mailboxurl->SetMailboxAction(nsIMailboxUrl::ActionCopyMessage);
		mailboxurl->SetMailboxCopyHandler(aMailboxCopyHandler);
		nsCOMPtr<nsIURI> url = do_QueryInterface(mailboxurl);
		rv = RunMailboxUrl(url);
	}

	if (aURL)
		mailboxurl->QueryInterface(nsIURI::GetIID(), (void **) aURL);

	NS_UNLOCK_INSTANCE();

	return rv;
}

nsresult nsMailboxService::DisplayMessage(const char* aMessageURI,
                                          nsISupports * aDisplayConsumer, 
										  nsIUrlListener * aUrlListener,
                                          nsIURI ** aURL)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIMailboxUrl> mailboxurl;
	NS_LOCK_INSTANCE();

	rv = PrepareMessageUrl(aMessageURI, aUrlListener, nsIMailboxUrl::ActionDisplayMessage, getter_AddRefs(mailboxurl));

	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIURI> url = do_QueryInterface(mailboxurl);
		rv = RunMailboxUrl(url, aDisplayConsumer);
	}

	if (aURL)
		mailboxurl->QueryInterface(nsIURI::GetIID(), (void **) aURL);
	NS_UNLOCK_INSTANCE();

	return rv;
}

NS_IMETHODIMP nsMailboxService::SaveMessageToDisk(const char *aMessageURI, nsIFileSpec *aFile, 
												  PRBool aAppendToFile, nsIUrlListener *aUrlListener, nsIURI **aURL)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIMailboxUrl> mailboxurl;
	NS_LOCK_INSTANCE();

	rv = PrepareMessageUrl(aMessageURI, aUrlListener, nsIMailboxUrl::ActionSaveMessageToDisk, getter_AddRefs(mailboxurl));

	if (NS_SUCCEEDED(rv))
	{
		mailboxurl->SetMessageFile(aFile);
		nsCOMPtr<nsIURI> url = do_QueryInterface(mailboxurl);
		rv = RunMailboxUrl(url);
	}

	if (aURL)
		mailboxurl->QueryInterface(nsIURI::GetIID(), (void **) aURL);
	
	NS_UNLOCK_INSTANCE();
	return rv;
}

nsresult nsMailboxService::DisplayMessageNumber(const char *url,
                                                PRUint32 aMessageNumber,
                                                nsISupports * aDisplayConsumer,
                                                nsIUrlListener * aUrlListener,
                                                nsIURI ** aURL)
{
	// mscott - this function is no longer supported...
	NS_ASSERTION(0, "deprecated method");
	return NS_OK;
}

// Takes a mailbox url, this method creates a protocol instance and loads the url
// into the protocol instance.
nsresult nsMailboxService::RunMailboxUrl(nsIURI * aMailboxUrl, nsISupports * aDisplayConsumer)
{
	// create a protocol instance to run the url..
	nsresult rv = NS_OK;
	nsMailboxProtocol * protocol = new nsMailboxProtocol(aMailboxUrl);

	if (protocol)
	{
		NS_ADDREF(protocol);
		rv = protocol->LoadUrl(aMailboxUrl, aDisplayConsumer);
		NS_RELEASE(protocol); // after loading, someone else will have a ref cnt on the mailbox
	}
		
	return rv;
}

// This function takes a message uri, converts it into a file path & msgKey 
// pair. It then turns that into a mailbox url object. It also registers a url
// listener if appropriate. AND it can take in a mailbox action and set that field
// on the returned url as well.
nsresult nsMailboxService::PrepareMessageUrl(const char * aSrcMsgMailboxURI, nsIUrlListener * aUrlListener,
											 nsMailboxAction aMailboxAction, nsIMailboxUrl ** aMailboxUrl)
{
	nsresult rv = NS_OK;
	rv = nsComponentManager::CreateInstance(kCMailboxUrl,
                                            nsnull,
                                            nsIMailboxUrl::GetIID(),
                                            (void **) aMailboxUrl);

	if (NS_SUCCEEDED(rv) && aMailboxUrl && *aMailboxUrl)
	{
		// okay now generate the url string
		char * urlSpec;
		nsAutoString folderURI (eOneByte);
		nsFileSpec folderPath;
		nsMsgKey msgKey;
		
		rv = nsParseLocalMessageURI(aSrcMsgMailboxURI, folderURI, &msgKey);
		rv = nsLocalURI2Path(kMailboxMessageRootURI, folderURI.GetBuffer(), folderPath);

		if (NS_SUCCEEDED(rv))
		{
			// set up the url spec and initialize the url with it.
			nsFilePath filePath(folderPath); // convert to file url representation...
			urlSpec = PR_smprintf("mailboxMessage://%s?number=%d", (const char *) filePath, msgKey);
			nsCOMPtr <nsIMsgMailNewsUrl> url = do_QueryInterface(*aMailboxUrl);
			url->SetSpec(urlSpec);
			PR_FREEIF(urlSpec);

			// set up the mailbox action
			(*aMailboxUrl)->SetMailboxAction(aMailboxAction);

			// set up the url listener
			if (aUrlListener)
				rv = url->RegisterListener(aUrlListener);

			// set progress feedback...eventually, we'll need to pass this into all methods in
			// the mailbox service...this is just a temp work around to get things going...
			NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv); 
			if (NS_FAILED(rv)) return rv;
			nsCOMPtr<nsIMsgStatusFeedback> status;
			session->GetTemporaryMsgStatusFeedback(getter_AddRefs(status));
			url->SetStatusFeedback(status);

		} // if we got a url
	} // if we got a url

	return rv;
}

NS_IMETHODIMP nsMailboxService::GetScheme(char * *aScheme)
{
	nsresult rv = NS_OK;
	if (aScheme)
		*aScheme = PL_strdup("mailbox");
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 
}

NS_IMETHODIMP nsMailboxService::GetDefaultPort(PRInt32 *aDefaultPort)
{
	nsresult rv = NS_OK;
	if (aDefaultPort)
		*aDefaultPort = -1;  // mailbox doesn't use a port!!!!!
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 	
}

NS_IMETHODIMP nsMailboxService::MakeAbsolute(const char *aRelativeSpec, nsIURI *aBaseURI, char **_retval)
{
	// no such thing as relative urls for smtp.....
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}

NS_IMETHODIMP nsMailboxService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
	// i just haven't implemented this yet...I will be though....
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}

NS_IMETHODIMP nsMailboxService::NewChannel(const char *verb, nsIURI *aURI, nsIEventSinkGetter *eventSinkGetter, nsIChannel **_retval)
{
	// mscott - right now, I don't like the idea of returning channels to the caller. They just want us
	// to run the url, they don't want a channel back...I'm going to be addressing this issue with
	// the necko team in more detail later on.
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}
