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

#include "nsSmtpService.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"

#include "nsSmtpUrl.h"
#include "nsSmtpProtocol.h"

static NS_DEFINE_CID(kCSmtpUrlCID, NS_SMTPURL_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

// foward declarations...

nsresult NS_MsgBuildMailtoUrl(const nsFilePath& aFilePath, const nsString& aHostName, const nsString& aSender, 
							  const nsString& aRecipients, nsIUrlListener *, nsIURI ** aUrl);
nsresult NS_MsgLoadMailtoUrl(nsIURI * aUrl, nsISupports * aConsumer);

nsSmtpService::nsSmtpService()
{
    NS_INIT_REFCNT();
}

nsSmtpService::~nsSmtpService()
{}

NS_IMPL_THREADSAFE_ADDREF(nsSmtpService);
NS_IMPL_THREADSAFE_RELEASE(nsSmtpService);

nsresult nsSmtpService::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;
    if (aIID.Equals(nsCOMTypeInfo<nsISmtpService>::GetIID()) || aIID.Equals(kISupportsIID))
	{
        *aInstancePtr = (void*) ((nsISmtpService*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
	if (aIID.Equals(nsCOMTypeInfo<nsIProtocolHandler>::GetIID()))
	{
		*aInstancePtr = (void *) ((nsIProtocolHandler*) this);
		NS_ADDREF_THIS();
		return NS_OK;
	}
    return NS_NOINTERFACE;
}

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

nsresult nsSmtpService::SendMailMessage(const nsFilePath& aFilePath, const nsString& aRecipients, 
										nsIUrlListener * aUrlListener, nsIURI ** aURL)
{
	nsIURI * urlToRun = nsnull;
	nsresult rv = NS_OK;

	NS_LOCK_INSTANCE();
	// get the current identity from the mail session....
	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv); 
	if (NS_SUCCEEDED(rv) && mailSession)
	{
		nsCOMPtr<nsIMsgIdentity> identity;
		rv = mailSession->GetCurrentIdentity(getter_AddRefs(identity));

		if (NS_SUCCEEDED(rv) && identity)
		{
			char * hostName = nsnull;
			char * senderName = nsnull;

			identity->GetSmtpHostname(&hostName);
			identity->GetSmtpUsername(&senderName);

            // todo:  are we leaking hostName and senderName

			rv = NS_MsgBuildMailtoUrl(aFilePath, hostName, senderName, aRecipients, aUrlListener, &urlToRun); // this ref counts urlToRun
			if (NS_SUCCEEDED(rv) && urlToRun)
			{	
				rv = NS_MsgLoadMailtoUrl(urlToRun, nsnull);
			}

			if (aURL) // does the caller want a handle on the url?
				*aURL = urlToRun; // transfer our ref count to the caller....
			else
				NS_IF_RELEASE(urlToRun);

			PR_FREEIF(hostName);
			PR_FREEIF(senderName);
		} // if we have an identity
		else
			NS_ASSERTION(0, "no current identity found for this user....");
	} // if we had a mail session

	NS_UNLOCK_INSTANCE();
	return rv;
}


// The following are two convience functions I'm using to help expedite building and running a mail to url...

#define TEMP_DEFAULT_HOST "nsmail-2.mcom.com:25"

// short cut function for creating a mailto url...
nsresult NS_MsgBuildMailtoUrl(const nsFilePath& aFilePath, const nsString& aHostName, const nsString& aSender, 
							  const nsString& aRecipients, nsIUrlListener * aUrlListener, nsIURI ** aUrl)
{
	// mscott: this function is a convience hack until netlib actually dispatches smtp urls.
	// in addition until we have a session to get a password, host and other stuff from, we need to use default values....
	// ..for testing purposes....
	
	nsresult rv = NS_OK;
	nsCOMPtr <nsISmtpUrl> smtpUrl;
	rv = nsComponentManager::CreateInstance(kCSmtpUrlCID, NULL, nsCOMTypeInfo<nsISmtpUrl>::GetIID(), getter_AddRefs(smtpUrl));

	if (NS_SUCCEEDED(rv) && smtpUrl)
	{
		// assemble a url spec...
		char * recipients = aRecipients.ToNewCString();
		char * hostName = aHostName.ToNewCString();
		char * urlSpec= PR_smprintf("mailto://%s:%d/%s", hostName && *hostName ? hostName : TEMP_DEFAULT_HOST, 25, recipients ? recipients : "");
		if (recipients)
			delete [] recipients;
		if (hostName)
			delete [] hostName;
		if (urlSpec)
		{
			nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(smtpUrl);
			url->SetSpec(urlSpec);
			smtpUrl->SetPostMessageFile(aFilePath);
			smtpUrl->SetUserEmailAddress(nsCAutoString(aSender));
			url->RegisterListener(aUrlListener);
			PR_Free(urlSpec);
		}
		rv = smtpUrl->QueryInterface(nsCOMTypeInfo<nsIURI>::GetIID(), (void **) aUrl);
	 }

	 return rv;
}

nsresult NS_MsgLoadMailtoUrl(nsIURI * aUrl, nsISupports * aConsumer)
{
	// mscott: this function is pretty clumsy right now...eventually all of the dispatching
	// and transport creation code will live in netlib..this whole function is just a hack
	// for our mail news demo....

	// for now, assume the url is a news url and load it....
	nsCOMPtr <nsISmtpUrl> smtpUrl;
	nsSmtpProtocol	*smtpProtocol = nsnull;
	nsresult rv = NS_OK;

	if (!aUrl)
		return rv;

    // turn the url into an smtp url...
	smtpUrl = do_QueryInterface(aUrl);
    if (smtpUrl)
    {
		const nsFilePath * fileName = nsnull;
		smtpUrl->GetPostMessageFile(&fileName);

		// almost there...now create a nntp protocol instance to run the url in...
		smtpProtocol = new nsSmtpProtocol(aUrl);
		if (smtpProtocol == nsnull)
			rv = NS_ERROR_OUT_OF_MEMORY;
		else
			smtpProtocol->LoadUrl(aUrl); // protocol will get destroyed when url is completed...
	}

	return rv;
}

NS_IMETHODIMP nsSmtpService::GetScheme(char * *aScheme)
{
	nsresult rv = NS_OK;
	if (aScheme)
		*aScheme = PL_strdup("mailto");
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 
}

NS_IMETHODIMP nsSmtpService::GetDefaultPort(PRInt32 *aDefaultPort)
{
	nsresult rv = NS_OK;
	if (aDefaultPort)
		*aDefaultPort = SMTP_PORT;
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 	
}

NS_IMETHODIMP nsSmtpService::MakeAbsolute(const char *aRelativeSpec, nsIURI *aBaseURI, char **_retval)
{
	// no such thing as relative urls for smtp.....
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}

NS_IMETHODIMP nsSmtpService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
	// i just haven't implemented this yet...I will be though....
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}

NS_IMETHODIMP nsSmtpService::NewChannel(const char *verb, nsIURI *aURI, nsIEventSinkGetter *eventSinkGetter, nsIEventQueue *eventQueue, nsIChannel **_retval)
{
	// mscott - right now, I don't like the idea of returning channels to the caller. They just want us
	// to run the url, they don't want a channel back...I'm going to be addressing this issue with
	// the necko team in more detail later on.
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}
