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
#include "nntpCore.h"

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsNntpService.h"

#include "nsINetService.h"

#include "nsINntpUrl.h"
#include "nsNNTPProtocol.h"

#include "nsNNTPNewsgroupPost.h"
#include "nsINetService.h"

#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"

#include "nsString2.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kNntpUrlCID, NS_NNTPURL_CID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

nsNntpService::nsNntpService()
{
    NS_INIT_REFCNT();
}

nsNntpService::~nsNntpService()
{}

NS_IMPL_THREADSAFE_ADDREF(nsNntpService);
NS_IMPL_THREADSAFE_RELEASE(nsNntpService);

nsresult nsNntpService::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (nsnull == aInstancePtr)
        return NS_ERROR_NULL_POINTER;
 
    if (aIID.Equals(nsINntpService::GetIID()) 
        || aIID.Equals(kISupportsIID)) 
	{
        *aInstancePtr = (void*) ((nsINntpService*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIMsgMessageService::GetIID())) 
	{
        *aInstancePtr = (void*) ((nsIMsgMessageService*)this);
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

////////////////////////////////////////////////////////////////////////////////////////
// nsIMsgMessageService support
////////////////////////////////////////////////////////////////////////////////////////
nsresult nsNntpService::DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, 
										  nsIUrlListener * aUrlListener, nsIURL ** aURL)
{
  // this function is just a shell right now....eventually we'll implement displaymessage such
  // that we break down the URI and extract the news host and article number. We'll then
  // build up a url that represents that action, create a connection to run the url
  // and load the url into the connection. 
  
  // HACK ALERT: For now, the only news url we run is a display message url. So just forward
  // this URI to RunNewUrl
  nsString uri = aMessageURI;

  return RunNewsUrl(uri, aDisplayConsumer, aUrlListener, aURL);
}

nsresult nsNntpService::CopyMessage(const char * aSrcMailboxURI, nsIStreamListener * aMailboxCopyHandler, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIURL **aURL)
{
	NS_ASSERTION(0, "unimplemented feature");
	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
// nsINntpService support
////////////////////////////////////////////////////////////////////////////////////////
nsresult nsNntpService::PostMessage(nsFilePath &pathToFile, const char *subject, const char *newsgroup, nsIUrlListener * aUrlListener, nsIURL ** aURL)
{
  nsresult rv = NS_OK;

#ifdef DEBUG_sspitzer
  printf("nsNntpService::PostMessage(%s,%s,%s,??,??)\n",(const char *)pathToFile,subject,newsgroup);
#endif

  NS_LOCK_INSTANCE();

  // get the current identity from the news session....
  NS_WITH_SERVICE(nsIMsgMailSession,newsSession,kCMsgMailSessionCID,&rv);

  if (NS_SUCCEEDED(rv) && newsSession)
	{
      nsIMsgIdentity * identity = nsnull;
      rv = newsSession->GetCurrentIdentity(&identity);

      if (NS_SUCCEEDED(rv) && identity)
		{
          char * fullname = nsnull;
          char * email = nsnull;
          char * organization = nsnull;

          identity->GetFullName(&fullname);
          identity->GetEmail(&email);
          identity->GetOrganization(&organization);

#ifdef DEBUG_sspitzer
          printf("post message as: %s,%s,%s\n",fullname,email,organization);
#endif

          // todo:  are we leaking fullname, email and organization?

          // release the identity
          NS_IF_RELEASE(identity);
		} // if we have an identity
      else
        NS_ASSERTION(0, "no current identity found for this user....");
	} // if we had a news session
  
  NS_UNLOCK_INSTANCE();
  return rv;
}

nsresult nsNntpService::RunNewsUrl(const nsString& urlString, nsISupports * aConsumer, 
										nsIUrlListener *aUrlListener, nsIURL ** aURL)
{
	// for now, assume the url is a news url and load it....
	nsINntpUrl		*nntpUrl = nsnull;
	nsINetService	*pNetService = nsnull;
	nsITransport	*transport = nsnull;
	nsNNTPProtocol	*nntpProtocol = nsnull;
	nsresult rv = NS_OK;

	// make sure we have a netlib service around...
	rv = NS_NewINetService(&pNetService, nsnull); 

	if (NS_SUCCEEDED(rv) && pNetService)
	{

		rv = nsComponentManager::CreateInstance(kNntpUrlCID, nsnull, nsINntpUrl::GetIID(), (void **)
												&nntpUrl);

		if (NS_SUCCEEDED(rv) && nntpUrl)
		{
			char * urlSpec = urlString.ToNewCString();
			nntpUrl->SetSpec(urlSpec);
			if (urlSpec)
				delete [] urlSpec;
			
			const char * host;
			PRUint32 port = NEWS_PORT;
			
			if (aUrlListener) // register listener if there is one...
				nntpUrl->RegisterListener(aUrlListener);

			nntpUrl->GetHostPort(&port);
			nntpUrl->GetHost(&host);
			// okay now create a transport to run the url in...
			pNetService->CreateSocketTransport(&transport, port, host);
			if (NS_SUCCEEDED(rv) && transport)
			{
				// almost there...now create a nntp protocol instance to run the url in...
				nntpProtocol = new nsNNTPProtocol(nntpUrl, transport);
				if (nntpProtocol)
					nntpProtocol->LoadURL(nntpUrl, aConsumer);

                //delete nntpProtocol;
			}

			if (aURL)
				*aURL = nntpUrl; // transfer ref count
			else
				NS_RELEASE(nntpUrl);
		} // if nntpUrl

		NS_RELEASE(pNetService);
	} // if pNetService
	
	return rv;
}



