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

#include "nsIPref.h"

#include "nsCRT.h"  // for nsCRT::strtok

#define PREF_NETWORK_HOSTS_NNTP_SERVER	"network.hosts.nntp_server"

#ifdef DEBUG_sspitzer_
#define DEBUG_NEWS 1
#endif

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kCNntpUrlCID, NS_NNTPURL_CID);
static NS_DEFINE_CID(kCNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
static NS_DEFINE_CID(kCNNTPNewsgroupCID, NS_NNTPNEWSGROUP_CID);
static NS_DEFINE_CID(kCNNTPNewsgroupPostCID, NS_NNTPNEWSGROUPPOST_CID);
static NS_DEFINE_CID(kCNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);   
static NS_DEFINE_CID(kCPrefServiceCID, NS_PREF_CID); 
                     
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

  nsString uri(aMessageURI, eOneByte);
  nsString newsgroupName("", eOneByte);
  nsMsgKey key = nsMsgKey_None;
    
  if (PL_strncmp(aMessageURI, kNewsMessageRootURI, kNewsMessageRootURILen) == 0) {
	rv = ConvertNewsMessageURI2NewsURI(aMessageURI, uri, newsgroupName, &key);
  }
  else {
    return NS_ERROR_UNEXPECTED;
  }

  if (NS_FAILED(rv)) {
    return rv;
  }
 
  return RunNewsUrl(uri, newsgroupName, key, aDisplayConsumer, aUrlListener, aURL);
}

nsresult nsNntpService::ConvertNewsMessageURI2NewsURI(const char *messageURI, nsString &newsURI, nsString &newsgroupName, nsMsgKey *key)
{
  nsString hostname("", eOneByte);
  nsString messageUriWithoutKey("", eOneByte);
  nsresult rv = NS_OK;

  // messageURI is of the form:  "news_message://news.mcom.com/mcom.linux#1"
  // if successful, we should get
  // messageUriWithoutKey = "news_message://news.mcom.com/mcom.linux"
  // key = 1
  rv = nsParseNewsMessageURI(messageURI, messageUriWithoutKey, key);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // turn news_message://news.mcom.com/mcom.linux -> news.mcom.com/mcom.linux
  // stick "news.mcom.com/mcom.linux" in hostname.
  messageUriWithoutKey.Right(hostname, messageUriWithoutKey.Length() - kNewsMessageRootURILen - 1);

  // take news.mcom.com/mcom.linux (in hostname) and put
  // "mcom.linux" into newsgroupName and truncate to leave
  // "news.mcom.com" in hostname
  PRInt32 hostEnd = hostname.Find('/');
  if (hostEnd > 0) {
    hostname.Right(newsgroupName, hostname.Length() - hostEnd - 1);
    hostname.Truncate(hostEnd);
  }
  else {
    // error!
    // we didn't find a "/" in something we thought looked like this:
    // news.mcom.com/mcom.linux
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG_NEWS
  printf("ConvertNewsMessageURI2NewsURI(%s,??) -> %s %u\n", messageURI, newsgroupName.GetBuffer(), *key);
#endif

  nsNativeFileSpec pathResult;

  rv = nsNewsURI2Path(kNewsMessageRootURI, messageUriWithoutKey.GetBuffer(), pathResult);
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
  
  rv = newsDB->GetMsgHdrForKey((nsMsgKey) *key, getter_AddRefs(msgHdr));
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

nsresult nsNntpService::FindHostFromGroup(nsString &host, nsString &groupName)
{
  nsresult rv = NS_OK;
  if (0) {
    // we have "group"
    // todo:  look for "group" in the newsrc files of all the hosts, starting with the default nntp host.
    // for now, leave host blank so we'll just use the default nntp host.
  }
  
  if (host == "") {
    NS_WITH_SERVICE(nsIPref, prefs, kCPrefServiceCID, &rv);
    if (NS_FAILED(rv) || (!prefs)) {
      return rv;
    } 
    
    char *default_nntp_server = nsnull; 
    // if we get here, we know prefs is not null
    rv = prefs->CopyCharPref(PREF_NETWORK_HOSTS_NNTP_SERVER, &default_nntp_server);
    if (NS_FAILED(rv) || (!default_nntp_server)) {
      // if all else fails, use "news" as the default_nntp_server
      default_nntp_server = PR_smprintf("news");
    }
    host = default_nntp_server;
    PR_FREEIF(default_nntp_server);
  }
  
  if (host == "")
    return NS_ERROR_FAILURE;
  else 
    return NS_OK;
}

nsresult nsNntpService::DetermineHostForPosting(nsString &host, const char *newsgroupNames)
{
  nsresult rv = NS_OK;
  
  if (!newsgroupNames) return NS_ERROR_NULL_POINTER;
  if (PL_strlen(newsgroupNames) == 0) return NS_ERROR_FAILURE;

#ifdef DEBUG_sspitzer
  printf("newsgroupNames == %s\n",newsgroupNames);
#endif
  
  // newsgroupNames can be a comma seperated list of these:
  // news://host/group
  // news://group
  // host/group
  // group
  //
  // we are not going to allow the user to cross post to multiple hosts.
  // so as soon as we determine the host from newsgroupNames, we can stop.
  //
  // 1) explict (news://host/group or host/group)
  // 2) context (if you reply to a message when reading it on host x, reply to that host.
  // 3) no context look up news://group or group in the various newsrc files to determine host, with PREF_NETWORK_HOSTS_NNTP_SERVER being the first newrc file searched each time
  // 4) use the default nntp server

  //nsCRT::strtok is going destroy what we pass to it, so we need to make a copy of newsgroupNames.
  char *list = PL_strdup(newsgroupNames);
  char *token = nsnull;
  char *rest = list;
  nsString str("", eOneByte);

  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) {
    str = token;
    str.StripWhitespace();

    if (str != "") {
#ifdef DEBUG_sspitzer
      printf("value = %s\n", str.GetBuffer());
#endif
      nsString theRest("",eOneByte);

      // does str start with "news:/"?
      if (str.Find(kNewsRootURI) == 0) {
        // we have news://group or news://host/group
        // set theRest to what's after news://
        str.Right(theRest, str.Length() - kNewsRootURILen /* for news:/ */ - 1 /* for the slash */);
      }
      else {
        theRest = str;
      }
      
#ifdef DEBUG_sspitzer
      printf("theRest == %s\n",theRest.GetBuffer());
#endif
      
      // theRest is "group" or "host/group"
      PRInt32 slashpos = theRest.Find('/');
      if (slashpos > 0 ) {
        // theRest is "host/group"
        theRest.Left(host, slashpos);
#ifdef DEBUG_sspitzer
        printf("host == %s\n", host.GetBuffer());
#endif
      }
      else {
        // theRest is "group"
        rv = FindHostFromGroup(host, str);
        if (NS_FAILED(rv)) return rv;
      }  
      str = "";
    }
#ifdef DEBUG_sspitzer
    else {
        printf("nothing between two commas. ignore and keep going...\n");
    }
#endif
    token = nsCRT::strtok(rest, ",", &rest);
    
    //we have our host, we're done.
    //if (host != "") break;
  }    
  PR_Free(list);
  
  if (host != "")
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}
////////////////////////////////////////////////////////////////////////////////////////
// nsINntpService support
////////////////////////////////////////////////////////////////////////////////////////
nsresult nsNntpService::PostMessage(nsFilePath &pathToFile, const char *newsgroupNames, nsIUrlListener * aUrlListener, nsIURL **_retval)
{
#ifdef DEBUG_sspitzer
  printf("nsNntpService::PostMessage(%s,%s,??,??)\n",(const char *)pathToFile,newsgroupNames);
#endif
  if (!newsgroupNames) return NS_ERROR_NULL_POINTER;
  if (PL_strlen(newsgroupNames) == 0) return NS_ERROR_FAILURE;
    
  NS_LOCK_INSTANCE();
  
  nsCOMPtr <nsINntpUrl> nntpUrl;
  nsresult rv = NS_OK;
  
  rv = nsComponentManager::CreateInstance(kCNntpUrlCID, nsnull, nsINntpUrl::GetIID(), getter_AddRefs(nntpUrl));
  if (NS_FAILED(rv) || !nntpUrl) return rv;

  nsString host("",eOneByte);

  rv = DetermineHostForPosting(host, newsgroupNames);
  if (NS_FAILED(rv) || (host == "")) return rv;

#ifdef DEBUG_sspitzer
  printf("post to this host: %s\n",host.GetBuffer());
#endif

  char *urlstr = PR_smprintf("%s/%s",kNewsRootURI,host.GetBuffer());
  nntpUrl->SetSpec(urlstr);
  PR_FREEIF(urlstr);
  
  if (aUrlListener) // register listener if there is one...
    nntpUrl->RegisterListener(aUrlListener);
  
  // almost there...now create a nntp protocol instance to run the url in...
  nsNNTPProtocol *nntpProtocol = nsnull;

  nntpProtocol = new nsNNTPProtocol();
  if (!nntpProtocol) return NS_ERROR_OUT_OF_MEMORY;;
  
  rv = nntpProtocol->Initialize(nntpUrl);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr <nsINNTPNewsgroupPost> post;
  rv = nsComponentManager::CreateInstance(kCNNTPNewsgroupPostCID, nsnull, nsINNTPNewsgroupPost::GetIID(), getter_AddRefs(post));
  if (NS_FAILED(rv) || !post) return rv;

#ifdef DEBUG_sspitzer
  printf("set file to post to %s\n",(const char *)pathToFile);
#endif
  
  rv = post->SetPostMessageFile(pathToFile);
  if (NS_FAILED(rv)) return rv;
  
  rv = nntpUrl->SetMessageToPost(post);
  if (NS_FAILED(rv)) return rv;
            
  rv = nntpProtocol->LoadUrl(nntpUrl, /* aConsumer */ nsnull);
		
  if (_retval)
    *_retval = nntpUrl; // transfer ref count
    
  NS_UNLOCK_INSTANCE();

  return rv;
}

nsresult 
nsNntpService::RunNewsUrl(nsString& urlString, nsString &newsgroupName, nsMsgKey key, nsISupports * aConsumer, nsIUrlListener *aUrlListener, nsIURL **_retval)
{
#ifdef DEBUG_NEWS
  printf("nsNntpService::RunNewsUrl(%s,...)\n", (const char *)nsAutoCString(urlString));
#endif
  
  nsCOMPtr <nsINntpUrl> nntpUrl;
  nsresult rv = NS_OK;

  rv = nsComponentManager::CreateInstance(kCNntpUrlCID, nsnull, nsINntpUrl::GetIID(), getter_AddRefs(nntpUrl));
  if (NS_FAILED(rv) || !nntpUrl) return rv;
  
  nntpUrl->SetSpec(nsAutoCString(urlString));
  
  nsCOMPtr <nsINNTPNewsgroup> newsgroup;
  rv = nsComponentManager::CreateInstance(kCNNTPNewsgroupCID, nsnull, nsINNTPNewsgroup::GetIID(), getter_AddRefs(newsgroup));
  if (NS_FAILED(rv) || !newsgroup) return rv;                       
       
  rv = newsgroup->Initialize(nsnull /* line */, nsnull /* set */, PR_FALSE /* subscribed */);
  
  nsString newsgroupNameStr(newsgroupName,eOneByte);
  
  newsgroup->SetName((char *)(newsgroupNameStr.GetBuffer()));
  nntpUrl->SetNewsgroup(newsgroup);

  // if we are running a news url to display a message, these
  // will be used later, to mark the message as read after we finish loading
  nntpUrl->SetMessageKey(key);
  nntpUrl->SetNewsgroupName((char *)(newsgroupNameStr.GetBuffer()));
       
  if (aUrlListener) // register listener if there is one...
    nntpUrl->RegisterListener(aUrlListener);

  // almost there...now create a nntp protocol instance to run the url in...
  nsNNTPProtocol *nntpProtocol = nsnull;

  nntpProtocol = new nsNNTPProtocol();
  if (!nntpProtocol) return NS_ERROR_OUT_OF_MEMORY;
  
  rv = nntpProtocol->Initialize(nntpUrl);
  if (NS_FAILED(rv)) return rv;
  
  rv = nntpProtocol->LoadUrl(nntpUrl, aConsumer);
  if (NS_FAILED(rv)) return rv;
  
  if (_retval)
    *_retval = nntpUrl; // transfer ref count
	
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
  
#ifdef DEBUG_NEWS
  if (nntpHostName) {
    printf("get news from news://%s\n", nntpHostName);
  }
  else {
    printf("nntpHostName is null\n");
  }
#endif

  nsString uriStr(uri, eOneByte);
  nsString newsgroupName("", eOneByte);
  
  NS_ASSERTION((uriStr.Find(kNewsRootURI) == 0), "uriStr didn't start with news:/");
  if (uriStr.Find(kNewsRootURI) == 0) {
    // uriStr look like this:  "news://news.mcom.com/mcom.linux"
    // 
    uriStr.Right(newsgroupName, uriStr.Length() - kNewsRootURILen /* for news:/ */ - 1 /* for the slash */ - PL_strlen(nntpHostName) /* for the hostname */ -1 /* for the next slash */);
    
    rv = RunNewsUrl(uriStr, newsgroupName, nsMsgKey_None, nsnull, aUrlListener, _retval);
  }
  else {
    rv = NS_ERROR_FAILURE;
  }
      
  NS_UNLOCK_INSTANCE();
  return rv;
}

NS_IMETHODIMP nsNntpService::CancelMessages(nsISupportsArray *messages, nsIUrlListener * aUrlListener)
{
  nsresult rv = NS_OK;
  PRUint32 count = 0;

  NS_WITH_SERVICE(nsINetSupportDialogService,dialog,kCNetSupportDialogCID,&rv);
  if (NS_FAILED(rv)) return rv;
    
  if (!messages) {
    nsAutoString alertText("No articles are selected.");
    if (dialog)
      rv = dialog->Alert(alertText);
    
    return NS_ERROR_NULL_POINTER;
  }

  rv = messages->Count(&count);
  if (NS_FAILED(rv)) {
#ifdef DEBUG_NEWS
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
#ifdef DEBUG_NEWS
  printf("OK or CANCEL? %d\n",result);
#endif

  if (result != 1) {
    // they canceled the cancel
    return NS_ERROR_FAILURE;
  }
  
  nsString messageIds("", eOneByte);
  PRUint32 i;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> msgSupports = getter_AddRefs(messages->ElementAt(i));
    nsCOMPtr<nsIMessage> message(do_QueryInterface(msgSupports));
    if (message) {
      nsMsgKey key;
      rv = message->GetMessageKey(&key);
      if (NS_SUCCEEDED(rv)) {
        if (messageIds.Length() > 0) {
          messageIds.Append(',');
        }
        messageIds.Append((PRInt32)key);
      }
    }
  }
  
#ifdef DEBUG_NEWS
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
