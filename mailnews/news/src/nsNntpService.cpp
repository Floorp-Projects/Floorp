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
#include "nsMsgNewsCID.h"

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsNntpService.h"
#include "nsINntpUrl.h"
#include "nsNNTPProtocol.h"
#include "nsNNTPNewsgroupPost.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsString.h"
#include "nsNewsUtils.h"
#include "nsNewsDatabase.h"
#include "nsMsgDBCID.h"
#include "nsNNTPNewsgroup.h"
#include "nsCOMPtr.h"
#include "nsMsgBaseCID.h"
#include "nsMsgNewsCID.h"
#include "nsIMessage.h"
#include "nsINetSupportDialogService.h"
#include "nsIPref.h"
#include "nsCRT.h"  // for nsCRT::strtok

#define PREF_NETWORK_HOSTS_NNTP_SERVER	"network.hosts.nntp_server"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kCNntpUrlCID, NS_NNTPURL_CID);
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
	if (aIID.Equals(nsIProtocolHandler::GetIID()))
	{
		*aInstancePtr = (void *) ((nsIProtocolHandler*) this);
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

NS_IMETHODIMP nsNntpService::SaveMessageToDisk(const char *aMessageURI, nsIFileSpec *aFile, PRBool aAppendToFile, nsIUrlListener *aUrlListener, nsIURI **aURL)
{
	// unimplemented for news right now....if we feel it would be useful to 
	// be able to spool a news article to disk then this is the method we need to implement.

  //
  // Returning success causes us much grief with the editor because we wait until this is
  // complete to do anything...so changing this to failure.
  //
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

nsresult nsNntpService::DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, nsIUrlListener * aUrlListener, nsIURI ** aURL)
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
  PRInt32 hostEnd = hostname.FindChar('/');
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

  nsFileSpec pathResult;

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

  nsCOMPtr <nsIFileSpec> dbFileSpec;
  NS_NewFileSpecWithSpec(pathResult, getter_AddRefs(dbFileSpec));
  rv = newsDBFactory->Open(dbFileSpec, PR_TRUE, PR_FALSE, getter_AddRefs(newsDB));
    
  if (NS_FAILED(rv) || (!newsDB)) {
    return rv;
  }

  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  
  rv = newsDB->GetMsgHdrForKey((nsMsgKey) *key, getter_AddRefs(msgHdr));
  if (NS_FAILED(rv) || (!msgHdr)) {
    return rv;
  }

  nsAutoString messageId;
  rv = msgHdr->GetMessageId(&messageId);

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
						   nsIUrlListener * aUrlListener, nsIURI **aURL)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsISupports> streamSupport;
    if (!aSrcMailboxURI || !aMailboxCopyHandler) return rv;
    streamSupport = do_QueryInterface(aMailboxCopyHandler, &rv);
    if (NS_SUCCEEDED(rv))
        rv = DisplayMessage(aSrcMailboxURI, streamSupport, aUrlListener, aURL);
	return rv;
}

nsresult nsNntpService::FindHostFromGroup(nsString &host, nsString &groupName)
{
  nsresult rv = NS_OK;
  // host always comes in as ""
  NS_ASSERTION(host == "", "host is not empty");
  if (host != "") return NS_ERROR_FAILURE;
  
  if (0) {
    // groupName is "foobar"
    // todo:  look for "foobar" in the newsrc files of all the hosts,
    // starting with the default nntp host.
    //
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

nsresult nsNntpService::DetermineHostForPosting(nsString &host, const char *newsgroupsNames)
{
  nsresult rv = NS_OK;
  
  if (!newsgroupsNames) return NS_ERROR_NULL_POINTER;
  if (PL_strlen(newsgroupsNames) == 0) return NS_ERROR_FAILURE;

#ifdef DEBUG_NEWS
  printf("newsgroupsNames == %s\n",newsgroupsNames);
#endif
  
  // newsgroupsNames can be a comma seperated list of these:
  // news://host/group
  // news://group
  // host/group
  // group
  //
  // we are not going to allow the user to cross post to multiple hosts.
  // so as soon as we determine the host from newsgroupsNames, we can stop.
  //
  // 1) explict (news://host/group or host/group)
  // 2) context (if you reply to a message when reading it on host x, reply to that host.
  // 3) no context look up news://group or group in the various newsrc files to determine host, with PREF_NETWORK_HOSTS_NNTP_SERVER being the first newrc file searched each time
  // 4) use the default nntp server

  //nsCRT::strtok is going destroy what we pass to it, so we need to make a copy of newsgroupsNames.
  char *list = PL_strdup(newsgroupsNames);
  char *token = nsnull;
  char *rest = list;
  nsString str("", eOneByte);
  
  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) {
    str = token;
    str.StripWhitespace();

    if (str != "") {
#ifdef DEBUG_NEWS
      printf("value = %s\n", str.GetBuffer());
#endif
      nsString theRest("",eOneByte);
      nsString currentHost("", eOneByte);
      
      // does str start with "news:/"?
      if (str.Find(kNewsRootURI) == 0) {
        // we have news://group or news://host/group
        // set theRest to what's after news://
        str.Right(theRest, str.Length() - kNewsRootURILen /* for news:/ */ - 1 /* for the slash */);
      }
      else if (str.Find(":/") != -1) {
#ifdef DEBUG_NEWS
	printf("we have x:/y where x != news. this is bad, return failure\n");
#endif
        PR_FREEIF(list);
        return NS_ERROR_FAILURE;
      }
      else {
        theRest = str;
      }
      
#ifdef DEBUG_NEWS
      printf("theRest == %s\n",theRest.GetBuffer());
#endif
      
      // theRest is "group" or "host/group"
      PRInt32 slashpos = theRest.FindChar('/');
      if (slashpos > 0 ) {
        // theRest is "host/group"
        theRest.Left(currentHost, slashpos);
#ifdef DEBUG_NEWS
        printf("currentHost == %s\n", currentHost.GetBuffer());
#endif
      }
      else {
        // theRest is "group"
        rv = FindHostFromGroup(currentHost, str);
        PR_FREEIF(list);
        if (NS_FAILED(rv)) return rv;
      }

      if (host == "") {
        host = currentHost;

        //we have our host, we're done.
        //break;
      }
      else {
        if (host != currentHost) {
          printf("no cross posting to multiple hosts!\n");
          PR_FREEIF(list);
          return NS_ERROR_FAILURE;
        }
      }
      
      str = "";
      currentHost = "";
    }
#ifdef DEBUG_NEWS
    else {
        printf("nothing between two commas. ignore and keep going...\n");
    }
#endif
    token = nsCRT::strtok(rest, ",", &rest);
  }    
  PR_FREEIF(list);
  
  if (host != "")
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}
////////////////////////////////////////////////////////////////////////////////////////
// nsINntpService support
////////////////////////////////////////////////////////////////////////////////////////
nsresult nsNntpService::ConvertNewsgroupsString(const char *newsgroupsNames, char **_retval)
{
  nsresult rv = NS_OK;
  
  if (!newsgroupsNames) return NS_ERROR_NULL_POINTER;
  if (PL_strlen(newsgroupsNames) == 0) return NS_ERROR_FAILURE;

#ifdef DEBUG_NEWS
  printf("newsgroupsNames == %s\n",newsgroupsNames);
#endif
  
  // newsgroupsNames can be a comma seperated list of these:
  // news://host/group
  // news://group
  // host/group
  // group
  //
  // we are not going to allow the user to cross post to multiple hosts.
  // if we detect that, we stop and return error.

  // nsCRT::strtok is going destroy what we pass to it, so we need to make a copy of newsgroupsNames.
  char *list = PL_strdup(newsgroupsNames);
  char *token = nsnull;
  char *rest = list;
  nsString host("", eOneByte);
  nsString str("", eOneByte);
  nsString retvalStr("", eOneByte);
    
  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) {
    str = token;
    str.StripWhitespace();

    if (str != "") {
#ifdef DEBUG_NEWS
      printf("value = %s\n", str.GetBuffer());
#endif
      nsString currentHost("", eOneByte);
      nsString theRest("",eOneByte);

      // does str start with "news:/"?
      if (str.Find(kNewsRootURI) == 0) {
        // we have news://group or news://host/group
        // set theRest to what's after news://
        str.Right(theRest, str.Length() - kNewsRootURILen /* for news:/ */ - 1 /* for the slash */);
      }
      else if (str.Find(":/") != -1) {
#ifdef DEBUG_NEWS
	printf("we have x:/y where x != news. this is bad, return failure\n");
#endif
        PR_FREEIF(list);
        return NS_ERROR_FAILURE;
      }
      else {
        theRest = str;
      }
      
#ifdef DEBUG_NEWS
      printf("theRest == %s\n",theRest.GetBuffer());
#endif
      
      // theRest is "group" or "host/group"
      PRInt32 slashpos = theRest.FindChar('/');
      if (slashpos > 0 ) {
        nsString currentGroup("",eOneByte);
        
        // theRest is "host/group"
        theRest.Left(currentHost, slashpos);
        
#ifdef DEBUG_NEWS
        printf("currentHost == %s\n", currentHost.GetBuffer());
#endif
        // from "host/group", put "group" into currentGroup;
        theRest.Right(currentGroup, theRest.Length() - currentHost.Length() - 1);

        NS_ASSERTION(currentGroup != "", "currentGroup is empty");
        if (currentGroup == "") {
          PR_FREEIF(list);
          return NS_ERROR_FAILURE;
        }
        
        // build up the retvalStr;
        if (retvalStr != "") {
          retvalStr += ",";
        }
        retvalStr += currentGroup;
      }
      else {
        // theRest is "group"
        rv = FindHostFromGroup(currentHost, str);
        if (NS_FAILED(rv)) {
            PR_FREEIF(list);
            return rv;
        }

        // build up the retvalStr;
        if (retvalStr != "") {
          retvalStr += ",";
        }
        retvalStr += str;
      }

      if (currentHost == "") {
        printf("empty current host!\n");
        PR_FREEIF(list);
        return NS_ERROR_FAILURE;
      }
      
      if (host == "") {
        printf("got a host, set it\n");
        host = currentHost;
      }
      else {
        if (host != currentHost) {
          printf("no cross posting to multiple hosts!\n");
          PR_FREEIF(list);
          return NS_ERROR_FAILURE;
        }
      }

      str = "";
      currentHost = "";
    }
#ifdef DEBUG_NEWS
    else {
        printf("nothing between two commas. ignore and keep going...\n");
    }
#endif
    token = nsCRT::strtok(rest, ",", &rest);
  }
  PR_FREEIF(list);
  
  // caller will free with PR_FREEIF()
  *_retval = PL_strdup(retvalStr.GetBuffer());
  if (!*_retval) return NS_ERROR_OUT_OF_MEMORY;
  
#ifdef DEBUG_NEWS
  printf("Newsgroups header = %s\n", *_retval);
#endif

  return NS_OK;
}

nsresult nsNntpService::PostMessage(nsFilePath &pathToFile, const char *newsgroupsNames, nsIUrlListener * aUrlListener, nsIURI **_retval)
{
#ifdef DEBUG_NEWS
  printf("nsNntpService::PostMessage(%s,%s,??,??)\n",(const char *)pathToFile,newsgroupsNames);
#endif
  if (!newsgroupsNames) return NS_ERROR_NULL_POINTER;
  if (PL_strlen(newsgroupsNames) == 0) return NS_ERROR_FAILURE;
    
  NS_LOCK_INSTANCE();
  
  nsCOMPtr <nsINntpUrl> nntpUrl;
  nsresult rv = NS_OK;
  
  rv = nsComponentManager::CreateInstance(kCNntpUrlCID, nsnull, nsINntpUrl::GetIID(), getter_AddRefs(nntpUrl));
  if (NS_FAILED(rv) || !nntpUrl) return rv;

  nsString host("",eOneByte);
  rv = DetermineHostForPosting(host, newsgroupsNames);
  
  if (NS_FAILED(rv) || (host == "")) return rv;

#ifdef DEBUG_NEWS
  printf("post to this host: %s\n",host.GetBuffer());
#endif

  char *urlstr = PR_smprintf("%s/%s",kNewsRootURI,host.GetBuffer());
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(nntpUrl);
  if (mailnewsurl)
	mailnewsurl->SetSpec(urlstr);
  
  PR_FREEIF(urlstr);
  
  if (aUrlListener) // register listener if there is one...
    mailnewsurl->RegisterListener(aUrlListener);
  
  // almost there...now create a nntp protocol instance to run the url in...
  nsNNTPProtocol *nntpProtocol = nsnull;

  nntpProtocol = new nsNNTPProtocol();
  if (!nntpProtocol) return NS_ERROR_OUT_OF_MEMORY;;
  
  rv = nntpProtocol->Initialize(mailnewsurl);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr <nsINNTPNewsgroupPost> post;
  rv = nsComponentManager::CreateInstance(kCNNTPNewsgroupPostCID, nsnull, nsINNTPNewsgroupPost::GetIID(), getter_AddRefs(post));
  if (NS_FAILED(rv) || !post) return rv;

#ifdef DEBUG_NEWS
  printf("set file to post to %s\n",(const char *)pathToFile);
#endif
  
  rv = post->SetPostMessageFile(pathToFile);
  if (NS_FAILED(rv)) return rv;
  
  rv = nntpUrl->SetMessageToPost(post);
  if (NS_FAILED(rv)) return rv;
            
  rv = nntpProtocol->LoadUrl(mailnewsurl, /* aConsumer */ nsnull);
		
  if (_retval)
	  nntpUrl->QueryInterface(nsIURI::GetIID(), (void **) _retval);
    
  NS_UNLOCK_INSTANCE();

  return rv;
}

nsresult 
nsNntpService::RunNewsUrl(nsString& urlString, nsString &newsgroupName, nsMsgKey key, nsISupports * aConsumer, nsIUrlListener *aUrlListener, nsIURI **_retval)
{
#ifdef DEBUG_NEWS
  printf("nsNntpService::RunNewsUrl(%s,%s,%u,...)\n", (const char *)nsAutoCString(urlString), (const char *)nsAutoCString(newsgroupName), key);
#endif
  
  nsCOMPtr <nsINntpUrl> nntpUrl;
  nsresult rv = NS_OK;

  rv = nsComponentManager::CreateInstance(kCNntpUrlCID, nsnull, nsINntpUrl::GetIID(), getter_AddRefs(nntpUrl));
  if (NS_FAILED(rv) || !nntpUrl) return rv;
  
  nsCOMPtr <nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(nntpUrl);
  // don't worry this cast is really okay...there'a bug in XPIDL compiler that is preventing
  // a "cont char *" in paramemter for uri SetSpec...
  mailnewsurl->SetSpec(nsCAutoString(urlString));
  mailnewsurl->SetPort(NEWS_PORT);

  if (newsgroupName != "") {
    nsCOMPtr <nsINNTPNewsgroup> newsgroup;
    rv = nsComponentManager::CreateInstance(kCNNTPNewsgroupCID, nsnull, nsINNTPNewsgroup::GetIID(), getter_AddRefs(newsgroup));
    if (NS_FAILED(rv) || !newsgroup) return rv;                       
       
    rv = newsgroup->Initialize(newsgroupName.GetBuffer(), nsnull /* set */, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    
    rv = nntpUrl->SetNewsgroup(newsgroup);
    if (NS_FAILED(rv)) return rv;
    
    // if we are running a news url to display a message, these
    // will be used later, to mark the message as read after we finish loading
    rv = nntpUrl->SetMessageKey(key);
    if (NS_FAILED(rv)) return rv;
        
    rv = nntpUrl->SetNewsgroupName((char *)(newsgroupName.GetBuffer()));
    if (NS_FAILED(rv)) return rv;
  }
  
  if (aUrlListener) // register listener if there is one...
    mailnewsurl->RegisterListener(aUrlListener);

  // almost there...now create a nntp protocol instance to run the url in...
  nsNNTPProtocol *nntpProtocol = nsnull;

  nntpProtocol = new nsNNTPProtocol();
  if (!nntpProtocol) return NS_ERROR_OUT_OF_MEMORY;
  
  rv = nntpProtocol->Initialize(mailnewsurl);
  if (NS_FAILED(rv)) return rv;
  
  rv = nntpProtocol->LoadUrl(mailnewsurl, aConsumer);
  if (NS_FAILED(rv)) return rv;
  
  if (_retval)
	  nntpUrl->QueryInterface(nsIURI::GetIID(), (void **) _retval);
	
  return rv;
}

NS_IMETHODIMP nsNntpService::GetNewNews(nsINntpIncomingServer *nntpServer, const char *uri, nsIUrlListener * aUrlListener, nsIURI **_retval)
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

NS_IMETHODIMP nsNntpService::CancelMessages(const char *hostname, const char *newsgroupname, nsISupportsArray *messages, nsISupports * aConsumer, nsIUrlListener * aUrlListener, nsIURI ** aURL)
{
  nsresult rv = NS_OK;
  PRUint32 count = 0;

  if (!hostname) return NS_ERROR_NULL_POINTER;
  if (PL_strlen(hostname) == 0) return NS_ERROR_FAILURE;

  NS_WITH_SERVICE(nsIPrompt, dialog, kCNetSupportDialogCID, &rv);
  if (NS_FAILED(rv)) return rv;
    
  if (!messages) {
    nsAutoString alertText("No articles are selected.");
    if (dialog)
      rv = dialog->Alert(alertText.GetUnicode());
    
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
      rv = dialog->Alert(alertText.GetUnicode());
    return NS_ERROR_FAILURE;
  }
  
  nsMsgKey key;
  nsString messageId("", eOneByte);
  
  nsCOMPtr<nsISupports> msgSupports = getter_AddRefs(messages->ElementAt(0));
  nsCOMPtr<nsIMessage> message(do_QueryInterface(msgSupports));
  if (message) {
    rv = message->GetMessageKey(&key);
    if (NS_FAILED(rv)) return rv;
    rv = message->GetMessageId(&messageId);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    return NS_ERROR_FAILURE;
  }
  
  nsString urlStr("", eOneByte);
  urlStr += kNewsRootURI;
  urlStr += "/";
  urlStr += hostname;
  urlStr += "/";
  urlStr += messageId;
  urlStr += "?cancel";

#ifdef DEBUG_NEWS
  printf("attempt to cancel the message (key,ID,cancel url): (%d,%s,%s)\n", key, messageId.GetBuffer(),urlStr.GetBuffer());
#endif  

  nsString newsgroupNameStr(newsgroupname,eOneByte);
  rv = RunNewsUrl(urlStr, newsgroupNameStr, key, aConsumer, aUrlListener, aURL);

  return rv; 
}

NS_IMETHODIMP nsNntpService::GetScheme(char * *aScheme)
{
	nsresult rv = NS_OK;
	if (aScheme)
		*aScheme = PL_strdup("news");
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 
}

NS_IMETHODIMP nsNntpService::GetDefaultPort(PRInt32 *aDefaultPort)
{
	nsresult rv = NS_OK;
	if (aDefaultPort)
		*aDefaultPort = NEWS_PORT;
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 	
}

NS_IMETHODIMP nsNntpService::MakeAbsolute(const char *aRelativeSpec, nsIURI *aBaseURI, char **_retval)
{
	// no such thing as relative urls for smtp.....
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}

NS_IMETHODIMP nsNntpService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
	// i just haven't implemented this yet...I will be though....
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}

NS_IMETHODIMP nsNntpService::NewChannel(const char *verb, nsIURI *aURI, nsIEventSinkGetter *eventSinkGetter, nsIEventQueue *eventQueue, nsIChannel **_retval)
{
	// mscott - right now, I don't like the idea of returning channels to the caller. They just want us
	// to run the url, they don't want a channel back...I'm going to be addressing this issue with
	// the necko team in more detail later on.
	NS_ASSERTION(0, "unimplemented");
	return NS_OK;
}
