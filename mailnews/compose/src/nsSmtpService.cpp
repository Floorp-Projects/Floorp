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

#include "nsINetService.h"
#include "nsSmtpService.h"

#include "nsSmtpUrl.h"
#include "nsSmtpProtocol.h"

// foward declarations...

nsresult NS_MsgBuildMailtoUrl(const nsFilePath& aFilePath, const nsString& aSender, const nsString& aRecipients, nsIURL ** aUrl);
nsresult NS_MsgLoadMailtoUrl(nsIURL * aUrl, nsISupports * aConsumer);

nsSmtpService::nsSmtpService()
{
    NS_INIT_REFCNT();
}

nsSmtpService::~nsSmtpService()
{}

NS_IMPL_THREADSAFE_ISUPPORTS(nsSmtpService, nsISmtpService::IID());

nsresult nsSmtpService::SendMailMessage(const nsFilePath& aFilePath,  const nsString& aSender, const nsString& aRecipients, nsIURL ** aURL)
{
	nsIURL * urlToRun = nsnull;
	nsresult rv = NS_OK;

	NS_LOCK_INSTANCE();
	rv = NS_MsgBuildMailtoUrl(aFilePath, aSender, aRecipients, &urlToRun); // this ref counts urlToRun
	if (NS_SUCCEEDED(rv) && urlToRun)
	{	
		rv = NS_MsgLoadMailtoUrl(urlToRun, nsnull);
	}

	if (aURL) // does the caller want a handle on the url?
		*aURL = urlToRun; // transfer our ref count to the caller....
	else
		NS_IF_RELEASE(urlToRun);

	NS_UNLOCK_INSTANCE();
	return rv;
}


// The following are two convience functions I'm using to help expedite building and running a mail to url...

#define TEMP_DEFAULT_HOST "nsmail-2.mcom.com:25"

// short cut function for creating a mailto url...
nsresult NS_MsgBuildMailtoUrl(const nsFilePath& aFilePath, const nsString& aSender, const nsString& aRecipients, nsIURL ** aUrl)
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
		char * urlSpec= PR_smprintf("mailto://%s/%s", TEMP_DEFAULT_HOST, recipients ? recipients : "");
		if (recipients)
			delete [] recipients;
		if (urlSpec)
		{
			smtpUrl->ParseURL(urlSpec);  // load the spec we were given...
			smtpUrl->SetPostMessageFile(aFilePath);
			smtpUrl->SetUserEmailAddress(aSender);
			PR_Free(urlSpec);
		}
		rv = smtpUrl->QueryInterface(nsISmtpUrl::IID(), (void **) aUrl);
	 }

	 return rv;
}

static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

nsresult NS_MsgLoadMailtoUrl(nsIURL * aUrl, nsISupports * aConsumer)
{
	// mscott: this function is pretty clumsy right now...eventually all of the dispatching
	// and transport creation code will live in netlib..this whole function is just a hack
	// for our mail news demo....

	// for now, assume the url is a news url and load it....
	nsISmtpUrl		*smtpUrl = nsnull;
	nsINetService	*pNetService = nsnull;
	nsITransport	*transport = nsnull;
	nsSmtpProtocol	*smtpProtocol = nsnull;
	nsresult rv = NS_OK;

	if (!aUrl)
		return rv;

    rv = nsServiceManager::GetService(kNetServiceCID, nsINetService::IID(), (nsISupports **)&pNetService);

	if (NS_SUCCEEDED(rv) && pNetService)
	{
		// turn the url into an smtp url...
		rv = aUrl->QueryInterface(nsISmtpUrl::IID(), (void **) &smtpUrl);
		if (NS_SUCCEEDED(rv) && smtpUrl)
		{
			const nsFilePath * fileName = nsnull;
			smtpUrl->GetPostMessageFile(&fileName);

			// okay now create a transport to run the url in...
			const char * hostName = nsnull;
			PRUint32 port = 25;

			smtpUrl->GetHost(&hostName);
			smtpUrl->GetHostPort(&port);

			pNetService->CreateSocketTransport(&transport, port, hostName);
			if (NS_SUCCEEDED(rv) && transport)
			{
				// almost there...now create a nntp protocol instance to run the url in...
				smtpProtocol = new nsSmtpProtocol(smtpUrl, transport);
				if (smtpProtocol)
					smtpProtocol->LoadURL(smtpUrl);
			}

			NS_RELEASE(smtpUrl);
		} // if nntpUrl

		nsServiceManager::ReleaseService(kNetServiceCID, pNetService);
	} // if pNetService
	
	return rv;
}

