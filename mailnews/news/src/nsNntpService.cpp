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

#include "nsString.h"

#include "nsNewsUtils.h"

#include "nsNewsDatabase.h"
#include "nsMsgDBCID.h"

#include "nsString2.h"

#include "nsNNTPNewsgroup.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kNntpUrlCID, NS_NNTPURL_CID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);

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
  nsresult rv = NS_OK;

  if (aMessageURI == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

#ifdef DEBUG_sspitzer
  printf("nsNntpService::DisplayMessage(%s,...)\n",aMessageURI);
#endif

  nsString uri = aMessageURI;

  if (PL_strncmp(aMessageURI, kNewsRootURI, kNewsRootURILen) == 0) {
    uri = aMessageURI;
  }
  else if (PL_strncmp(aMessageURI, kNewsMessageRootURI, kNewsMessageRootURILen) == 0) {
	rv = ConvertNewsMessageURI2NewsURI(aMessageURI, uri);
  }
  else {
    return NS_ERROR_UNEXPECTED;
  }

  if (NS_FAILED(rv)) {
    return rv;
  }
  else {
    return RunNewsUrl(uri, aDisplayConsumer, aUrlListener, aURL);
  }
}

nsresult nsNntpService::ConvertNewsMessageURI2NewsURI(const char *messageURI, nsString &newsURI)
{
  nsString hostname;
  nsString folder;
  nsresult rv = NS_OK;
  PRUint32 key;

  // messageURI is of the form:  news_message://news.mcom.com/mcom.linux#1

  rv = nsParseNewsMessageURI(messageURI, folder, &key);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // cut news_message://hostname/group -> hostname/group
  folder.Right(hostname, folder.Length() - kNewsMessageRootURILen - 1);

  // cut hostname/group -> hostname
  PRInt32 hostEnd = hostname.Find('/');
  if (hostEnd >0) {
    hostname.Truncate(hostEnd);
  }

#ifdef DEBUG_sspitzer
  printf("ConvertNewsMessageURI2NewsURI(%s,??) -> %s %u\n", messageURI, folder.ToNewCString(), key);
#endif

  nsNativeFileSpec pathResult;

  rv = nsNewsURI2Path(kNewsMessageRootURI, folder.ToNewCString(), pathResult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsIMsgDatabase *newsDBFactory = nsnull;
  nsIMsgDatabase *newsDB = nsnull;
  
  rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &newsDBFactory);
  if (NS_FAILED(rv) || (newsDBFactory == nsnull)) {
    return rv;
  }

  rv = newsDBFactory->Open(pathResult, PR_TRUE, (nsIMsgDatabase **) &newsDB, PR_FALSE);
    
  NS_RELEASE(newsDBFactory);
  newsDBFactory = nsnull;

  if (NS_FAILED(rv) || (newsDB == nsnull)) {
    return rv;
  }

  nsIMsgDBHdr *msgHdr = nsnull;

  rv = newsDB->GetMsgHdrForKey((nsMsgKey) key, &msgHdr);
  NS_IF_RELEASE(newsDB);
  newsDB = nsnull;

  if (NS_FAILED(rv) || (msgHdr == nsnull)) {
    return rv;
  }

  nsString messageId;
  rv = msgHdr->GetMessageId(messageId);

#ifdef DEBUG_sspitzer
  PRUint32 bytes;
  PRUint32 lines;
  rv = msgHdr->GetMessageSize(&bytes);
  rv = msgHdr->GetLineCount(&lines);

  printf("bytes = %u\n",bytes);
  printf("lines = %u\n",lines);
#endif

  NS_IF_RELEASE(msgHdr);
  msgHdr = nsnull;

  if (NS_FAILED(rv)) {
    return rv;
  }

  newsURI = kNewsRootURI;
  newsURI += "/";
  newsURI += hostname;
  newsURI += "/";
  newsURI += messageId;

#ifdef DEBUG_sspitzer
  printf("newsURI = %s\n", newsURI.ToNewCString());
#endif

  return NS_OK;
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

nsresult 
nsNntpService::RunNewsUrl(const nsString& urlString, nsISupports * aConsumer, 
										nsIUrlListener *aUrlListener, nsIURL ** aURL)
{
#ifdef DEBUG_sspitzer
    printf("nsNntpService::RunNewsUrl(%s,...)\n",urlString.ToNewCString());
#endif
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

		if (NS_SUCCEEDED(rv) && nntpUrl) {
			char * urlSpec = urlString.ToNewCString();
			nntpUrl->SetSpec(urlSpec);
			if (urlSpec) {
				delete [] urlSpec;
			}

			const char * hostname = nsnull;
			PRUint32 port = NEWS_PORT;
			
			if (aUrlListener) // register listener if there is one...
				nntpUrl->RegisterListener(aUrlListener);

			nntpUrl->GetHostPort(&port);
			nntpUrl->GetHost(&hostname);
			// okay now create a transport to run the url in...

#ifdef DEBUG_sspitzer
            printf("nsNntpService::RunNewsUrl(): hostname = %s port = %d\n", hostname, port);
#endif
			pNetService->CreateSocketTransport(&transport, port, hostname);
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



