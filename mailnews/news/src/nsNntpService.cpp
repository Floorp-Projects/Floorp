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

#include "nsString.h"

#include "nsNNTPNewsgroup.h"
#include "nsCOMPtr.h"
#include "nsMsgBaseCID.h"
#include "nsMsgNewsCID.h"

#include "nsIMessage.h"

#include "nsINetSupportDialogService.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kNntpUrlCID, NS_NNTPURL_CID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
static NS_DEFINE_CID(kNNTPNewsgroupCID, NS_NNTPNEWSGROUP_CID);
static NS_DEFINE_CID(kNNTPNewsgroupPostCID, NS_NNTPNEWSGROUPPOST_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);   

nsNntpService::nsNntpService()
{
    NS_INIT_REFCNT();
}

nsNntpService::~nsNntpService()
{
  // do nothing
}

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

NS_IMETHODIMP nsNntpService::SaveMessageToDisk(const char *aMessageURI, nsIFileSpec *aFile, PRBool aAppendToFile, nsIUrlListener *aUrlListener, nsIURL **aURL)
{
	// unimplemented for news right now....if we feel it would be useful to 
	// be able to spool a news article to disk then this is the method we need to implement.
	nsresult rv = NS_OK;
	return rv;
}

nsresult nsNntpService::DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, nsIUrlListener * aUrlListener, nsIURL ** aURL)
{
  nsresult rv = NS_OK;
  
  if (!aMessageURI) {
    return NS_ERROR_NULL_POINTER;
  }

#ifdef DEBUG_NEWS
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
  nsAutoString hostname (eOneByte);
  nsAutoString folder (eOneByte);
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

#ifdef DEBUG_NEWS
  printf("ConvertNewsMessageURI2NewsURI(%s,??) -> %s %u\n", messageURI, folder.GetBuffer(), key);
#endif

  nsNativeFileSpec pathResult;

  rv = nsNewsURI2Path(kNewsMessageRootURI, folder.GetBuffer(), pathResult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIMsgDatabase> newsDBFactory;
  nsCOMPtr<nsIMsgDatabase> newsDB;
  
  rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), getter_AddRefs(newsDBFactory));
  if (NS_FAILED(rv) || (!newsDBFactory)) {
    return rv;
  }

  rv = newsDBFactory->Open(pathResult, PR_TRUE, getter_AddRefs(newsDB), PR_FALSE);
    
  if (NS_FAILED(rv) || (!newsDB)) {
    return rv;
  }

  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  
  rv = newsDB->GetMsgHdrForKey((nsMsgKey) key, getter_AddRefs(msgHdr));
  if (NS_FAILED(rv) || (!msgHdr)) {
    return rv;
  }

  nsAutoString messageId;
  rv = msgHdr->GetMessageId(messageId);

#ifdef DEBUG_NEWS
  PRUint32 bytes;
  PRUint32 lines;
  rv = msgHdr->GetMessageSize(&bytes);
  rv = msgHdr->GetLineCount(&lines);

  printf("bytes = %u\n",bytes);
  printf("lines = %u\n",lines);
#endif

  if (NS_FAILED(rv)) {
    return rv;
  }

  newsURI = kNewsRootURI;
  newsURI += "/";
  newsURI += hostname;
  newsURI += "/";
  newsURI += messageId;

#ifdef DEBUG_NEWS
  printf("newsURI = %s\n", (const char *)nsAutoCString(newsURI));
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
nsresult nsNntpService::PostMessage(nsFilePath &pathToFile, const char *subject, const char *newsgroupName, nsIUrlListener * aUrlListener, nsIURL **_retval)
{
#ifdef DEBUG_NEWS
	printf("nsNntpService::PostMessage(%s,%s,%s,??,??)\n",(const char *)pathToFile,subject,newsgroupName);
#endif
	
	NS_LOCK_INSTANCE();
	
	// for now, assume the url is a news url and load it....
	nsINntpUrl		*nntpUrl = nsnull;
	nsNNTPProtocol	*nntpProtocol = nsnull;
	nsresult rv = NS_OK;
	
	rv = nsComponentManager::CreateInstance(kNntpUrlCID, nsnull, nsINntpUrl::GetIID(), (void **) &nntpUrl);
		
	if (NS_SUCCEEDED(rv) && nntpUrl) {
        printf("hardcoding the server name.  right now, we can only post to news.mozilla.org\n");
		char *urlstr = PR_smprintf("news://%s/%s","news.mozilla.org",newsgroupName);
		nntpUrl->SetSpec(urlstr);
		PR_FREEIF(urlstr);
		
        nsCOMPtr <nsINNTPNewsgroup> newsgroup;
        rv = nsComponentManager::CreateInstance(kNNTPNewsgroupCID, nsnull, nsINNTPNewsgroup::GetIID(), getter_AddRefs(newsgroup));
        
        if (NS_SUCCEEDED(rv) && newsgroup) {
          newsgroup->Initialize(nsnull /* line */, nsnull /* set */, PR_FALSE /* subscribed */);
          newsgroup->SetName((char *)newsgroupName);
        }
        else {
			return rv;
        }            
        nntpUrl->SetNewsgroup(newsgroup);
        
		const char * hostname = nsnull;
		PRUint32 port = NEWS_PORT;
		
		if (aUrlListener) // register listener if there is one...
			nntpUrl->RegisterListener(aUrlListener);
		
       
		// okay now create a transport to run the url in...
#ifdef DEBUG_NEWS
        printf("nsNntpService::RunNewsUrl(): hostname = %s port = %d\n", hostname, port);
#endif
		// almost there...now create a nntp protocol instance to run the url in...
		nntpProtocol = new nsNNTPProtocol();
		if (nntpProtocol) {
			rv = nntpProtocol->Initialize(nntpUrl);
            if (NS_FAILED(rv)) return rv;
			// get the current identity from the news session....
			NS_WITH_SERVICE(nsIMsgMailSession,newsSession,kCMsgMailSessionCID,&rv);

			if (NS_SUCCEEDED(rv) && newsSession) {
				nsCOMPtr<nsIMsgIdentity> identity;
				rv = newsSession->GetCurrentIdentity(getter_AddRefs(identity));
				if (NS_SUCCEEDED(rv) && identity) {
					char * fullname = nsnull;
					char * from = nsnull;
					char * org = nsnull;
						
					identity->GetFullName(&fullname);
					identity->GetEmail(&from);
					identity->GetOrganization(&org);
						
#ifdef DEBUG_NEWS
						printf("post message as: %s,%s,%s\n",fullname,from,org);
#endif
						
					nsCOMPtr <nsINNTPNewsgroupPost> post;
                    rv = nsComponentManager::CreateInstance(kNNTPNewsgroupPostCID, nsnull, nsINNTPNewsgroupPost::GetIID(), getter_AddRefs(post));
						
					if (NS_SUCCEEDED(rv)) {
						post->AddNewsgroup(newsgroupName);
						post->SetSubject((char *)subject);
						post->SetFrom((char *)from);
						post->SetOrganization((char *)org);
#ifdef DEBUG_NEWS
                        printf("set file to post to %s\n",(const char *)pathToFile);
#endif
                       rv = post->SetPostMessageFile(pathToFile);
                       if (NS_FAILED(rv)) {
                          return rv;
                       }

						rv = nntpUrl->SetMessageToPost(post);
                        if (NS_FAILED(rv)) {
                              return rv;
                        }
					}
						
					PR_FREEIF(fullname);
					PR_FREEIF(from);
					PR_FREEIF(org);
				}
				else {
						NS_ASSERTION(0, "no current identity found for this user....");
				}
			}

			if (NS_SUCCEEDED(rv)) {
				PRInt32 status = 0;
				rv = nntpProtocol->LoadUrl(nntpUrl, /* aConsumer */ nsnull, &status);
			}
		
			if (_retval)
				*_retval = nntpUrl; // transfer ref count
			else 
				NS_RELEASE(nntpUrl);
		}
	}
	
	NS_UNLOCK_INSTANCE();

	return rv;
}

nsresult 
nsNntpService::RunNewsUrl(nsString& urlString, nsISupports * aConsumer, nsIUrlListener *aUrlListener, nsIURL **_retval)
{
#ifdef DEBUG_NEWS
    printf("nsNntpService::RunNewsUrl(%s,...)\n", (const char *)nsAutoCString(urlString));
#endif
	// for now, assume the url is a news url and load it....
	nsINntpUrl		*nntpUrl = nsnull;
	nsNNTPProtocol	*nntpProtocol = nsnull;
	nsresult rv = NS_OK;

	rv = nsComponentManager::CreateInstance(kNntpUrlCID, nsnull, nsINntpUrl::GetIID(), (void **) &nntpUrl);

	if (NS_SUCCEEDED(rv) && nntpUrl) {
		nntpUrl->SetSpec(nsAutoCString(urlString));

		nsAutoString newsgroupName;
		nsNewsURI2Name(kNewsRootURI, nsAutoCString(urlString), newsgroupName);
			
        nsCOMPtr <nsINNTPNewsgroup> newsgroup;
        rv = nsComponentManager::CreateInstance(kNNTPNewsgroupCID, nsnull, nsINNTPNewsgroup::GetIID(), getter_AddRefs(newsgroup));
                         
        if (NS_SUCCEEDED(rv)) {
			rv = newsgroup->Initialize(nsnull /* line */, nsnull /* set */, PR_FALSE /* subscribed */);
            newsgroup->SetName((char *)((const char *)nsAutoCString(newsgroupName)));
        }
        else {
			return rv;
        }            
			
        nntpUrl->SetNewsgroup(newsgroup);         
	
		if (aUrlListener) // register listener if there is one...
			nntpUrl->RegisterListener(aUrlListener);

		// okay now create a transport to run the url in...

#ifdef DEBUG_NEWS
            printf("nsNntpService::RunNewsUrl(): hostname = %s port = %d\n", hostname, port);
#endif
		// almost there...now create a nntp protocol instance to run the url in...
		nntpProtocol = new nsNNTPProtocol();
		if (nntpProtocol) {
			PRInt32 status = 0;
            rv = nntpProtocol->Initialize(nntpUrl);
            if (NS_FAILED(rv)) return rv;
			rv = nntpProtocol->LoadUrl(nntpUrl, aConsumer, &status);
            if (NS_FAILED(rv)) return rv;
         }

		if (_retval)
			*_retval = nntpUrl; // transfer ref count
		else 
			NS_RELEASE(nntpUrl);
	}
	
	return rv;
}

NS_IMETHODIMP nsNntpService::GetNewNews(nsINntpIncomingServer *nntpServer, const char *uri, nsIUrlListener * aUrlListener, nsIURL **_retval)
{
  if (!uri) {
	return NS_ERROR_NULL_POINTER;
  }

#ifdef DEBUG_NEWS
  printf("nsNntpService::GetNewNews(%s)\n", uri);
#endif
  
  NS_LOCK_INSTANCE();
  nsresult rv = NS_OK;
  char * nntpHostName = nsnull;
  
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsCOMPtr<nsINntpUrl> nntpUrl;
  
  server = do_QueryInterface(nntpServer);
  
  // convert normal host to nntp host.
  // XXX - this doesn't handle QI failing very well
  if (server) {
    // load up required server information
    server->GetHostName(&nntpHostName);
  }
#ifdef DEBUG_NEWS
  else {
	  printf("server == nsnull\n");
  }
#endif
  
#ifdef DEBUG_sspitzer
  if (nntpHostName) {
    printf("get news from news://%s\n", nntpHostName);
  }
  else {
    printf("nntpHostName is null\n");
  }
#endif

  nsString uriStr(uri);
  rv = RunNewsUrl(uriStr, nsnull, aUrlListener, _retval);
  
  NS_UNLOCK_INSTANCE();
  return rv;
}

NS_IMETHODIMP nsNntpService::CancelMessages(nsISupportsArray *messages, nsIUrlListener * aUrlListener)
{
  nsresult rv = NS_OK;
  PRUint32 count = 0;

  NS_WITH_SERVICE(nsINetSupportDialogService,dialog,kNetSupportDialogCID,&rv);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  
  if (!messages) {
     nsAutoString alertText("No articles are selected.");
     if (dialog)
      rv = dialog->Alert(alertText);
     
     return NS_ERROR_NULL_POINTER;
  }

  rv = messages->Count(&count);
  if (NS_FAILED(rv)) {
#ifdef DEBUG_sspitzer
    printf("Count failed\n");
#endif
    return rv;
  }

  if (count != 1) {
    nsAutoString alertText("You can only cancel one article at a time.");
    if (dialog)
      rv = dialog->Alert(alertText);
    return NS_ERROR_FAILURE;
  }

  // we've got an article.  check that the we are the poster.
  // nsAutoString alertText("This message does not appear to be from you.  You may only cancel your own posts, not those made by others.");

  // we're the sender, give them one last chance to cancel the cancel
  PRInt32 result;

  nsAutoString confirmText("Are you sure you want to cancel this message?");
  rv = dialog->Confirm(confirmText, &result);
#ifdef DEBUG_sspitzer
  printf("OK or CANCEL? %d\n",result);
#endif

  if (result != 1) {
    // they canceled the cancel
    return NS_ERROR_FAILURE;
  }
  
  nsString2 messageIds("", eOneByte);
  PRUint32 i;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> msgSupports = getter_AddRefs(messages->ElementAt(i));
    nsCOMPtr<nsIMessage> message(do_QueryInterface(msgSupports));
    if (message) {
      nsMsgKey key;
      rv = message->GetMessageKey(&key);
      if (NS_SUCCEEDED(rv)) {
        if (messageIds.Length() > 0)
          messageIds.Append(',');
        messageIds.Append((PRInt32)key);
      }
    }
  }
  
#ifdef DEBUG_sspitzer
  printf("attempt to cancel the following IDs: %s\n", messageIds.GetBuffer());
#endif  

  rv = NS_OK;
  
  if (NS_SUCCEEDED(rv)) {
    // the CANCEL succeeded.

    nsAutoString alertText("Message cancelled.");
    rv = dialog->Alert(alertText);
    
    return NS_OK;
  }
  else {
    // the CANCEL failed.
    return NS_ERROR_FAILURE;
  }
}
