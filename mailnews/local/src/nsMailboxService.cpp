/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"    // precompiled header...
#include "nsCOMPtr.h"

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
#include "nsIWebShell.h"

static NS_DEFINE_CID(kCMailboxUrl, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

nsMailboxService::nsMailboxService()
{
    mPrintingOperation = PR_FALSE;
    NS_INIT_REFCNT();
}

nsMailboxService::~nsMailboxService()
{}

NS_IMPL_ISUPPORTS3(nsMailboxService, nsIMailboxService, nsIMsgMessageService, nsIProtocolHandler);

nsresult nsMailboxService::ParseMailbox(nsIMsgWindow *aMsgWindow, nsFileSpec& aMailboxPath, nsIStreamListener *aMailboxParser, 
										nsIUrlListener * aUrlListener, nsIURI ** aURL)
{
	nsCOMPtr<nsIMailboxUrl> mailboxurl;
	nsresult rv = NS_OK;

    rv = nsComponentManager::CreateInstance(kCMailboxUrl,
                                            nsnull,
                                            NS_GET_IID(nsIMailboxUrl),
                                            (void **) getter_AddRefs(mailboxurl));
	if (NS_SUCCEEDED(rv) && mailboxurl)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(mailboxurl);
		// okay now generate the url string
		nsFilePath filePath(aMailboxPath); // convert to file url representation...
		url->SetUpdatingFolder(PR_TRUE);
		url->SetMsgWindow(aMsgWindow);
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

	return rv;
}
						 
nsresult nsMailboxService::CopyMessage(const char * aSrcMailboxURI,
                              nsIStreamListener * aMailboxCopyHandler,
                              PRBool moveMessage,
                              nsIUrlListener * aUrlListener,
                              nsIURI **aURL)
{
    nsMailboxAction mailboxAction = nsIMailboxUrl::ActionMoveMessage;
    if (!moveMessage)
        mailboxAction = nsIMailboxUrl::ActionCopyMessage;
  return FetchMessage(aSrcMailboxURI, aMailboxCopyHandler, nsnull, aUrlListener, mailboxAction, aURL);
}

nsresult nsMailboxService::CopyMessages(nsMsgKeyArray *msgKeys,
							  nsIMsgFolder *srcFolder,
                              nsIStreamListener * aMailboxCopyHandler,
                              PRBool moveMessage,
                              nsIUrlListener * aUrlListener,
                              nsIURI **aURL)
{
	NS_ASSERTION(PR_FALSE, "not implemented yet");
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMailboxService::FetchMessage(const char* aMessageURI,
                                        nsISupports * aDisplayConsumer, 
                                        nsIMsgWindow * aMsgWindow,
										                    nsIUrlListener * aUrlListener,
                                        nsMailboxAction mailboxAction,
                                        nsIURI ** aURL)
{
  nsresult rv = NS_OK;
	nsCOMPtr<nsIMailboxUrl> mailboxurl;

  rv = PrepareMessageUrl(aMessageURI, aUrlListener, mailboxAction, getter_AddRefs(mailboxurl), aMsgWindow);

	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIURI> url = do_QueryInterface(mailboxurl);
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(url));
    msgUrl->SetMsgWindow(aMsgWindow);

		// instead of running the mailbox url like we used to, let's try to run the url in the webshell...
		nsCOMPtr<nsIWebShell> webshell = do_QueryInterface(aDisplayConsumer, &rv);
    // if we were given a webshell, run the url in the webshell..otherwise just run it normally.
    if (NS_SUCCEEDED(rv) && webshell)
	  rv = webshell->LoadURI(url, "view", nsnull, PR_TRUE);
    else
      rv = RunMailboxUrl(url, aDisplayConsumer); 
	}

	if (aURL)
		mailboxurl->QueryInterface(NS_GET_IID(nsIURI), (void **) aURL);

  return rv;
}


nsresult nsMailboxService::DisplayMessage(const char* aMessageURI,
                                          nsISupports * aDisplayConsumer,
                                          nsIMsgWindow * aMsgWindow,
										                      nsIUrlListener * aUrlListener,
                                          nsIURI ** aURL)
{
  return FetchMessage(aMessageURI, aDisplayConsumer, aMsgWindow,aUrlListener, nsIMailboxUrl::ActionDisplayMessage, aURL);
}

/* void OpenAttachment (in nsIURI aURI, in nsISupports aDisplayConsumer, in nsIMsgWindow aMsgWindow, in nsIUrlListener aUrlListener, out nsIURI aURL); */
NS_IMETHODIMP nsMailboxService::OpenAttachment(nsIURI *aURI, const char *aMessageURI, nsISupports *aDisplayConsumer, nsIMsgWindow *aMsgWindow, nsIUrlListener *aUrlListener, nsIURI **aURL)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP 
nsMailboxService::SaveMessageToDisk(const char *aMessageURI, 
                                    nsIFileSpec *aFile, 
                                    PRBool aAddDummyEnvelope, 
                                    nsIUrlListener *aUrlListener,
                                    nsIURI **aURL,
                                    PRBool canonicalLineEnding,
									nsIMsgWindow *aMsgWindow)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIMailboxUrl> mailboxurl;

	rv = PrepareMessageUrl(aMessageURI, aUrlListener, nsIMailboxUrl::ActionSaveMessageToDisk, getter_AddRefs(mailboxurl), aMsgWindow);

	if (NS_SUCCEEDED(rv))
	{
        nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(mailboxurl);
        if (msgUrl)
        {
		    msgUrl->SetMessageFile(aFile);
            msgUrl->SetAddDummyEnvelope(aAddDummyEnvelope);
            msgUrl->SetCanonicalLineEnding(canonicalLineEnding);
        }
		nsCOMPtr<nsIURI> url = do_QueryInterface(mailboxurl);
		rv = RunMailboxUrl(url);
	}

	if (aURL)
		mailboxurl->QueryInterface(NS_GET_IID(nsIURI), (void **) aURL);
	
	return rv;
}

NS_IMETHODIMP nsMailboxService::GetUrlForUri(const char *aMessageURI, nsIURI **aURL, nsIMsgWindow *aMsgWindow)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMailboxUrl> mailboxurl;
  rv = PrepareMessageUrl(aMessageURI, nsnull, nsIMailboxUrl::ActionDisplayMessage, getter_AddRefs(mailboxurl), aMsgWindow);
  if (NS_SUCCEEDED(rv) && mailboxurl)
    rv = mailboxurl->QueryInterface(NS_GET_IID(nsIURI), (void **) aURL);
  return rv;
}

/* readonly attribute canFetchMimeParts; */
NS_IMETHODIMP nsMailboxService::GetCanFetchMimeParts(PRBool *canFetchMimeParts)
{
  if (!canFetchMimeParts) return NS_ERROR_NULL_POINTER;
  *canFetchMimeParts = PR_FALSE;
  return NS_OK;
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
	nsMailboxProtocol * protocol = new nsMailboxProtocol(aMailboxUrl, aMailboxUrl);

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
											 nsMailboxAction aMailboxAction, nsIMailboxUrl ** aMailboxUrl,
											 nsIMsgWindow *msgWindow)
{
	nsresult rv = NS_OK;
	rv = nsComponentManager::CreateInstance(kCMailboxUrl,
                                            nsnull,
                                            NS_GET_IID(nsIMailboxUrl),
                                            (void **) aMailboxUrl);

	if (NS_SUCCEEDED(rv) && aMailboxUrl && *aMailboxUrl)
	{
		// okay now generate the url string
		char * urlSpec;
		nsCAutoString folderURI;
		nsFileSpec folderPath;
		nsMsgKey msgKey;
		
		rv = nsParseLocalMessageURI(aSrcMsgMailboxURI, folderURI, &msgKey);
		rv = nsLocalURI2Path(kMailboxMessageRootURI, folderURI, folderPath);

		if (NS_SUCCEEDED(rv))
		{
			// set up the url spec and initialize the url with it.
			nsFilePath filePath(folderPath); // convert to file url representation...

      if (mPrintingOperation)
        urlSpec = PR_smprintf("mailbox://%s?number=%d&header=print", (const char *) filePath, msgKey);
      else
  			urlSpec = PR_smprintf("mailbox://%s?number=%d", (const char *) filePath, msgKey);

			nsCOMPtr <nsIMsgMailNewsUrl> url = do_QueryInterface(*aMailboxUrl);
			url->SetSpec(urlSpec);
			PR_FREEIF(urlSpec);

			// set up the mailbox action
			(*aMailboxUrl)->SetMailboxAction(aMailboxAction);

			// set up the url listener
			if (aUrlListener)
				rv = url->RegisterListener(aUrlListener);

			url->SetMsgWindow(msgWindow);

		} // if we got a url
	} // if we got a url

	return rv;
}

NS_IMETHODIMP nsMailboxService::GetScheme(char * *aScheme)
{
	nsresult rv = NS_OK;
	if (aScheme)
		*aScheme = nsCRT::strdup("mailbox");
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

NS_IMETHODIMP nsMailboxService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
	nsCOMPtr<nsIMailboxUrl> aMsgUrl;
	nsresult rv = NS_OK;
	rv = nsComponentManager::CreateInstance(kCMailboxUrl,
                                            nsnull,
                                            NS_GET_IID(nsIMailboxUrl),
                                            getter_AddRefs(aMsgUrl));

	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIURL> aUrl = do_QueryInterface(aMsgUrl);
		aUrl->SetSpec((char *) aSpec);
		aMsgUrl->SetMailboxAction(nsIMailboxUrl::ActionDisplayMessage);
		aMsgUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) _retval);
	}

	return rv;
}

NS_IMETHODIMP nsMailboxService::NewChannel(const char *verb, 
                                           nsIURI *aURI, 
                                           nsILoadGroup* aLoadGroup,
                                           nsIInterfaceRequestor* notificationCallbacks,
                                           nsLoadFlags loadAttributes,
                                           nsIURI* originalURI,
                                           PRUint32 bufferSegmentSize,
                                           PRUint32 bufferMaxSize,
                                           nsIChannel **_retval)
{
	nsresult rv = NS_OK;
	nsMailboxProtocol * protocol = new nsMailboxProtocol(aURI, originalURI);
	if (protocol)
	{
        rv = protocol->SetLoadAttributes(loadAttributes);
        if (NS_FAILED(rv)) return rv;
        rv = protocol->SetLoadGroup(aLoadGroup);
        if (NS_FAILED(rv)) return rv;
        rv = protocol->SetNotificationCallbacks(notificationCallbacks);
        if (NS_FAILED(rv)) return rv;
		rv = protocol->QueryInterface(NS_GET_IID(nsIChannel), (void **) _retval);
	}
	else
		rv = NS_ERROR_NULL_POINTER;

	return rv;
}

nsresult nsMailboxService::DisplayMessageForPrinting(const char* aMessageURI,
                                                      nsISupports * aDisplayConsumer,
                                                      nsIMsgWindow * aMsgWindow,
										                                  nsIUrlListener * aUrlListener,
                                                      nsIURI ** aURL)
{
  mPrintingOperation = PR_TRUE;
  nsresult rv = FetchMessage(aMessageURI, aDisplayConsumer, aMsgWindow,aUrlListener, 
                      nsIMailboxUrl::ActionDisplayMessage, aURL);
  mPrintingOperation = PR_FALSE;
  return rv;
}
