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

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsSmtpService.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"

#include "nsSmtpUrl.h"
#include "nsSmtpProtocol.h"

// foward declarations...

nsresult NS_MsgBuildMailtoUrl(const nsFilePath& aFilePath, const nsString& aHostName, const nsString& aSender, 
							  const nsString& aRecipients, nsIUrlListener *, nsIURL ** aUrl);
nsresult NS_MsgLoadMailtoUrl(nsIURL * aUrl, nsISupports * aConsumer);

nsSmtpService::nsSmtpService()
{
    NS_INIT_REFCNT();
}

nsSmtpService::~nsSmtpService()
{}

NS_IMPL_THREADSAFE_ISUPPORTS(nsSmtpService, nsISmtpService::GetIID());


static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

nsresult nsSmtpService::SendMailMessage(const nsFilePath& aFilePath, const nsString& aRecipients, 
										nsIUrlListener * aUrlListener, nsIURL ** aURL)
{
	nsIURL * urlToRun = nsnull;
	nsresult rv = NS_OK;

	NS_LOCK_INSTANCE();
	// get the current identity from the mail session....
	nsIMsgMailSession * mailSession = nsnull;
	rv = nsServiceManager::GetService(kCMsgMailSessionCID,
	    							  nsIMsgMailSession::GetIID(),
                                      (nsISupports **) &mailSession);
	if (NS_SUCCEEDED(rv) && mailSession)
	{
		nsIMsgIdentity * identity = nsnull;
		rv = mailSession->GetCurrentIdentity(&identity);
		// now release the mail service because we are done with it
		nsServiceManager::ReleaseService(kCMsgMailSessionCID, mailSession);
		if (NS_SUCCEEDED(rv) && identity)
		{
			char * hostName = nsnull;
			char * senderName = nsnull;

			identity->GetSmtpHostname(&hostName);
			identity->GetSmtpUsername(&senderName);
			rv = NS_MsgBuildMailtoUrl(aFilePath, hostName, senderName, aRecipients, aUrlListener, &urlToRun); // this ref counts urlToRun
			if (NS_SUCCEEDED(rv) && urlToRun)
			{	
				rv = NS_MsgLoadMailtoUrl(urlToRun, nsnull);
			}

			if (aURL) // does the caller want a handle on the url?
				*aURL = urlToRun; // transfer our ref count to the caller....
			else
				NS_IF_RELEASE(urlToRun);

			// release the identity
			NS_IF_RELEASE(identity);
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
							  const nsString& aRecipients, nsIUrlListener * aUrlListener, nsIURL ** aUrl)
{
	// mscott: this function is a convience hack until netlib actually dispatches smtp urls.
	// in addition until we have a session to get a password, host and other stuff from, we need to use default values....
	// ..for testing purposes....
	
	nsresult rv = NS_OK;
	nsSmtpUrl * smtpUrl = new nsSmtpUrl(nsnull, nsnull);

	if (smtpUrl)
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
			smtpUrl->ParseURL(urlSpec);  // load the spec we were given...
			smtpUrl->SetPostMessageFile(aFilePath);
			smtpUrl->SetUserEmailAddress(aSender);
			smtpUrl->RegisterListener(aUrlListener);
			PR_Free(urlSpec);
		}
		rv = smtpUrl->QueryInterface(nsISmtpUrl::GetIID(), (void **) aUrl);
	 }

	 return rv;
}

nsresult NS_MsgLoadMailtoUrl(nsIURL * aUrl, nsISupports * aConsumer)
{
	// mscott: this function is pretty clumsy right now...eventually all of the dispatching
	// and transport creation code will live in netlib..this whole function is just a hack
	// for our mail news demo....

	// for now, assume the url is a news url and load it....
	nsISmtpUrl		*smtpUrl = nsnull;
	nsSmtpProtocol	*smtpProtocol = nsnull;
	nsresult rv = NS_OK;

	if (!aUrl)
		return rv;

    // turn the url into an smtp url...
    rv = aUrl->QueryInterface(nsISmtpUrl::GetIID(), (void **) &smtpUrl);
    if (NS_SUCCEEDED(rv) && smtpUrl)
    {
      const nsFilePath * fileName = nsnull;
      smtpUrl->GetPostMessageFile(&fileName);

      // almost there...now create a nntp protocol instance to run the url in...
      smtpProtocol = new nsSmtpProtocol(smtpUrl);
      if (smtpProtocol == nsnull)
		  rv = NS_ERROR_OUT_OF_MEMORY;
	  else
		  smtpProtocol->LoadURL(smtpUrl); // protocol will get destroyed when url is completed...
	}

    NS_IF_RELEASE(smtpUrl);
	
	return rv;
}

