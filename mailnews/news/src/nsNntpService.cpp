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
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Scott MacGregor <mscott@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"    // precompiled header...
#include "nntpCore.h"
#include "nsMsgNewsCID.h"
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
#include "nsMsgBaseCID.h"
#include "nsMsgNewsCID.h"
#include "nsIMessage.h"
#include "nsINetSupportDialogService.h"
#include "nsIPref.h"
#include "nsCRT.h"  // for nsCRT::strtok
#include "nsNntpService.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsIMsgAccountManager.h"
#include "nsIMessengerMigrator.h"
#include "nsINntpIncomingServer.h"
#include "nsICmdLineHandler.h"
#include "nsICategoryManager.h"
#include "nsIDocShell.h"
#include "nsIMessengerWindowService.h"
#include "nsIMsgSearchSession.h"

#undef GetPort  // XXX Windows!
#undef SetPort  // XXX Windows!

#define PREF_NETWORK_HOSTS_NNTP_SERVER	"network.hosts.nntp_server"
#define PREF_MAIL_ROOT_NNTP 	"mail.root.nntp"

static NS_DEFINE_CID(kCNntpUrlCID, NS_NNTPURL_CID);
static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
static NS_DEFINE_CID(kCNNTPNewsgroupCID, NS_NNTPNEWSGROUP_CID);
static NS_DEFINE_CID(kCNNTPNewsgroupPostCID, NS_NNTPNEWSGROUPPOST_CID);
static NS_DEFINE_CID(kCNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);   
static NS_DEFINE_CID(kCPrefServiceCID, NS_PREF_CID); 
static NS_DEFINE_CID(kFileLocatorCID,       NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kMessengerMigratorCID, NS_MESSENGERMIGRATOR_CID);
static NS_DEFINE_IID(kIFileLocatorIID,      NS_IFILELOCATOR_IID);
                    
nsNntpService::nsNntpService()
{
    NS_INIT_REFCNT();
    mPrintingOperation = PR_FALSE;
}

nsNntpService::~nsNntpService()
{
	// do nothing
}

NS_IMPL_THREADSAFE_ADDREF(nsNntpService);
NS_IMPL_THREADSAFE_RELEASE(nsNntpService);

NS_IMPL_QUERY_INTERFACE6(nsNntpService,
                         nsINntpService,
                         nsIMsgMessageService,
                         nsIProtocolHandler,
                         nsIMsgProtocolInfo,
                         nsICmdLineHandler,
						 nsIContentHandler)

////////////////////////////////////////////////////////////////////////////////////////
// nsIMsgMessageService support
////////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsNntpService::SaveMessageToDisk(const char *aMessageURI, 
                                 nsIFileSpec *aFile, 
                                 PRBool aAddDummyEnvelope, 
                                 nsIUrlListener *aUrlListener, 
                                 nsIURI **aURL,
                                 PRBool canonicalLineEnding,
								 nsIMsgWindow *aMsgWindow)
{
    nsresult rv = NS_OK;
    if (!aMessageURI) 
        return NS_ERROR_NULL_POINTER;

#ifdef DEBUG_NEWS
    printf("nsNntpService::SaveMessageToDisk(%s,...)\n",aMessageURI);
#endif

    nsCAutoString uri(aMessageURI);
    nsCAutoString newsgroupName;
    nsMsgKey key = nsMsgKey_None;
    
    if (PL_strncmp(aMessageURI, kNewsMessageRootURI, kNewsMessageRootURILen) == 0)
	    rv = ConvertNewsMessageURI2NewsURI(aMessageURI, uri, newsgroupName, &key);
    else
        return NS_ERROR_UNEXPECTED;

    // now create a url with this uri spec
    nsCOMPtr<nsIURI> myuri;

    rv = ConstructNntpUrl(uri, newsgroupName, key, aUrlListener, aMsgWindow, getter_AddRefs(myuri));
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsINntpUrl> nntpUrl = do_QueryInterface(myuri);
        nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(myuri);
        nntpUrl->SetNewsAction(nsINntpUrl::ActionSaveMessageToDisk);
        if (msgUrl)
        {
		    msgUrl->SetMessageFile(aFile);
            msgUrl->SetAddDummyEnvelope(aAddDummyEnvelope);
            msgUrl->SetCanonicalLineEnding(canonicalLineEnding);
        }   
    
        rv = RunNewsUrl(myuri, nsnull, nsnull);
    }

    if (aURL)
    {
	    *aURL = myuri;
	    NS_IF_ADDREF(*aURL);
    }

  return rv;
}

NS_IMETHODIMP 
nsNntpService::DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, 
                                       nsIMsgWindow *aMsgWindow, nsIUrlListener * aUrlListener, const PRUnichar * aCharsetOverride, nsIURI ** aURL)
{
  nsresult rv = NS_OK;
  
  if (!aMessageURI) {
    return NS_ERROR_NULL_POINTER;
  }

#ifdef DEBUG_NEWS
  printf("nsNntpService::DisplayMessage(%s,...)\n",aMessageURI);
#endif

  nsCAutoString uri(aMessageURI);
  nsCAutoString newsgroupName;
  nsMsgKey key = nsMsgKey_None;
 
  if (PL_strncmp(aMessageURI, kNewsMessageRootURI, kNewsMessageRootURILen) == 0)
	rv = ConvertNewsMessageURI2NewsURI(aMessageURI, uri, newsgroupName, &key);
  else
    return NS_ERROR_UNEXPECTED;

  // now create a url with this uri spec
  nsCOMPtr<nsIURI> myuri;

  // rhp: If we are displaying this message for the purposes of printing, append
  // the magic operand.
  if (mPrintingOperation)
    uri.Append("?header=print");

  rv = ConstructNntpUrl(uri, newsgroupName, key, aUrlListener, aMsgWindow, getter_AddRefs(myuri));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsINntpUrl> nntpUrl (do_QueryInterface(myuri));
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(nntpUrl));
    msgUrl->SetMsgWindow(aMsgWindow);
    nntpUrl->SetNewsAction(nsINntpUrl::ActionDisplayArticle);
    nsCOMPtr<nsIMsgI18NUrl> i18nurl (do_QueryInterface(msgUrl));
    i18nurl->SetCharsetOverRide(aCharsetOverride);

    // now is where our behavior differs....if the consumer is the docshell then we want to 
    // run the url in the webshell in order to display it. If it isn't a docshell then just
    // run the news url like we would any other news url. 
	  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aDisplayConsumer, &rv));
    if (NS_SUCCEEDED(rv) && docShell)
	    rv = docShell->LoadURI(myuri, nsnull);
    else
      rv = RunNewsUrl(myuri, aMsgWindow, aDisplayConsumer);
  }

  if (aURL) {
	  *aURL = myuri;
	  NS_IF_ADDREF(*aURL);
  }

  return rv;
}


NS_IMETHODIMP nsNntpService::OpenAttachment(const char *aContentType, 
                                            const char *aFileName,
                                            const char *aUrl, 
                                            const char *aMessageUri, 
                                            nsISupports *aDisplayConsumer, 
                                            nsIMsgWindow *aMsgWindow, 
                                            nsIUrlListener *aUrlListener)
{
  // news doesn't know anything about opening attachments....
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsNntpService::GetUrlForUri(const char *aMessageURI, nsIURI **aURL, nsIMsgWindow *aMsgWindow) 
{
  nsresult rv = NS_OK;
  nsCAutoString uri(aMessageURI);
  nsCAutoString newsgroupName;
  nsMsgKey key = nsMsgKey_None;
    
  if (PL_strncmp(aMessageURI, kNewsMessageRootURI, kNewsMessageRootURILen) == 0)
  {
	  rv = ConvertNewsMessageURI2NewsURI(aMessageURI, uri, newsgroupName, &key);
    if (NS_SUCCEEDED(rv))
      rv = ConstructNntpUrl(uri, newsgroupName, key, nsnull, aMsgWindow, aURL);
  }
  else 
    rv = NS_ERROR_UNEXPECTED;

  return rv;

}

nsresult nsNntpService::ConvertNewsMessageURI2NewsURI(const char *messageURI, nsCString &newsURI, nsCString &newsgroupName, nsMsgKey *key)
{
  nsCAutoString hostname;
  nsCAutoString messageUriWithoutKey;
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

  rv = nsNewsURI2Path(kNewsMessageRootURI, messageUriWithoutKey, pathResult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIMsgDatabase> newsDBFactory;
  nsCOMPtr<nsIMsgDatabase> newsDB;
  
  rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, NS_GET_IID(nsIMsgDatabase), getter_AddRefs(newsDBFactory));
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

  nsXPIDLCString messageId;
  rv = msgHdr->GetMessageId(getter_Copies(messageId));

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
  newsURI += (const char*)messageId;

#ifdef DEBUG_NEWS
  printf("newsURI = %s\n", (const char *)nsCAutoString(newsURI));
#endif

  return NS_OK;
}


NS_IMETHODIMP
nsNntpService::CopyMessage(const char * aSrcMailboxURI, nsIStreamListener * aMailboxCopyHandler, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIMsgWindow *aMsgWindow, nsIURI **aURL)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsISupports> streamSupport;
    if (!aSrcMailboxURI || !aMailboxCopyHandler) return rv;
    streamSupport = do_QueryInterface(aMailboxCopyHandler, &rv);
    if (NS_SUCCEEDED(rv))
        rv = DisplayMessage(aSrcMailboxURI, streamSupport, aMsgWindow, aUrlListener, nsnull, aURL);
	return rv;
}

NS_IMETHODIMP
nsNntpService::CopyMessages(nsMsgKeyArray *keys, nsIMsgFolder *srcFolder, nsIStreamListener * aMailboxCopyHandler, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIMsgWindow *aMsgWindow, nsIURI **aURL)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

typedef struct _findNewsServerEntry {
  const char *newsgroup;
  nsINntpIncomingServer *server;
} findNewsServerEntry;


PRBool 
nsNntpService::findNewsServerWithGroup(nsISupports *aElement, void *data)
{
	nsresult rv;

	nsCOMPtr<nsINntpIncomingServer> newsserver = do_QueryInterface(aElement, &rv);
	if (NS_FAILED(rv) || ! newsserver) return PR_TRUE;

	findNewsServerEntry *entry = (findNewsServerEntry*) data;

	PRBool containsGroup = PR_FALSE;

	rv = newsserver->ContainsNewsgroup((const char *)(entry->newsgroup), &containsGroup);
	if (NS_FAILED(rv)) return PR_TRUE;

	if (containsGroup) {	
		entry->server = newsserver;
		return PR_FALSE;            // stop on first find
	}
	else {
		return PR_TRUE;
	}
}

void 
nsNntpService::FindServerWithNewsgroup(nsCString &host, nsCString &groupName)
{
	nsresult rv;

 	NS_WITH_SERVICE(nsIMsgAccountManager, accountManager, kMsgAccountManagerCID, &rv);
	if (NS_FAILED(rv)) return;
	nsCOMPtr<nsISupportsArray> servers;
	
	rv = accountManager->GetAllServers(getter_AddRefs(servers));
	if (NS_FAILED(rv)) return;

	findNewsServerEntry serverInfo;
	serverInfo.server = nsnull;
  	serverInfo.newsgroup = (const char *)groupName;

	servers->EnumerateForwards(findNewsServerWithGroup, (void *)&serverInfo);
	if (serverInfo.server) {
		nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(serverInfo.server);
		nsXPIDLCString thisHostname;
  		rv = server->GetHostName(getter_Copies(thisHostname));
		if (NS_FAILED(rv)) return;

		host = (const char *)thisHostname;
	}
}

nsresult nsNntpService::FindHostFromGroup(nsCString &host, nsCString &groupName)
{
  nsresult rv = NS_OK;
  // host always comes in as ""
  NS_ASSERTION(host.IsEmpty(), "host is not empty");
  if (!host.IsEmpty()) return NS_ERROR_FAILURE;
 
  FindServerWithNewsgroup(host, groupName);

  if (host.IsEmpty()) {
    NS_WITH_SERVICE(nsIPref, prefs, kCPrefServiceCID, &rv);
    if (NS_FAILED(rv) || (!prefs)) {
      return rv;
    } 
    
    nsXPIDLCString default_nntp_server;
    // if we get here, we know prefs is not null
    rv = prefs->CopyCharPref(PREF_NETWORK_HOSTS_NNTP_SERVER, getter_Copies(default_nntp_server));
    if (NS_SUCCEEDED(rv) && ((const char *)default_nntp_server) && (PL_strlen((const char *)default_nntp_server))) {
        host = (const char *)default_nntp_server;
    }
    else {
      // if all else fails, use "news" as the default_nntp_server
      host = "news";
    }
  }
  
  if (host.IsEmpty())
    return NS_ERROR_FAILURE;
  else 
    return NS_OK;
}

nsresult 
nsNntpService::SetUpNntpUrlForPosting(nsINntpUrl *nntpUrl, const char *newsgroupsNames, const char *newshost, char **newsUrlSpec)
{
  nsresult rv = NS_OK;
  nsCAutoString host;

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
  char *list = nsCRT::strdup(newsgroupsNames);
  char *token = nsnull;
  char *rest = list;
  nsCAutoString str;
  PRUint32 numGroups = 0;   // the number of newsgroup we are attempt to post to
  nsCAutoString currentGroup;

  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) {
    str = token;
    str.StripWhitespace();

    if (!str.IsEmpty()) {
#ifdef DEBUG_NEWS
      printf("value = %s\n", str.GetBuffer());
#endif
      nsCAutoString theRest;
      nsCAutoString currentHost;
      
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
        CRTFREEIF(list);
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
        theRest.Right(currentGroup, slashpos);
#ifdef DEBUG_NEWS
        printf("currentHost == %s\n", currentHost.GetBuffer());
#endif
      }
      else if (newshost && nsCRT::strlen(newshost) > 0)
      {
        currentHost.Assign(newshost);
      }
      else {
        // theRest is "group"
        rv = FindHostFromGroup(currentHost, str);
        currentGroup = str;
        if (NS_FAILED(rv)) {
        CRTFREEIF(list);
		return rv;
	}
      }

      numGroups++;
      if (host.IsEmpty()) {
        host = currentHost;

        //we have our host, we're done.
        //break;
      }
      else {
        if (!host.Equals(currentHost)) {
          printf("todo, implement an alert:  no cross posting to multiple hosts!\n"); 
          CRTFREEIF(list);
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
  CRTFREEIF(list);
  
  if (host.IsEmpty())
    return NS_ERROR_FAILURE;


  // if the user tried to post to one newsgroup, set that information in the 
  // nntp url.  this can save them an authentication, if they've already logged in
  // and we have that information in the single signon database
  if ((numGroups == 1) && ((const char *)currentGroup)) {
    rv = nntpUrl->SetNewsgroupName((const char *)currentGroup);
    if (NS_FAILED(rv)) return rv;
  }

  *newsUrlSpec = PR_smprintf("%s/%s",kNewsRootURI,(const char *)host);
  if (!*newsUrlSpec) return NS_ERROR_FAILURE;

  return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////////////
// nsINntpService support
////////////////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsNntpService::ConvertNewsgroupsString(const char *newsgroupsNames, char **_retval)
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
  char *list = nsCRT::strdup(newsgroupsNames);
  char *token = nsnull;
  char *rest = list;
  nsCAutoString host;
  nsCAutoString str;
  nsCAutoString retvalStr;
    
  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) {
    str = token;
    str.StripWhitespace();

    if (!str.IsEmpty()) {
#ifdef DEBUG_NEWS
      printf("value = %s\n", str.GetBuffer());
#endif
      nsCAutoString currentHost;
      nsCAutoString theRest;

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
        CRTFREEIF(list);
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
        nsCAutoString currentGroup;
        
        // theRest is "host/group"
        theRest.Left(currentHost, slashpos);
        
#ifdef DEBUG_NEWS
        printf("currentHost == %s\n", currentHost.GetBuffer());
#endif
        // from "host/group", put "group" into currentGroup;
        theRest.Right(currentGroup, theRest.Length() - currentHost.Length() - 1);

        NS_ASSERTION(!currentGroup.IsEmpty(), "currentGroup is empty");
        if (currentGroup.IsEmpty()) {
          CRTFREEIF(list);
          return NS_ERROR_FAILURE;
        }
        
        // build up the retvalStr;
        if (!retvalStr.IsEmpty()) {
          retvalStr += ",";
        }
        retvalStr += currentGroup;
      }
      else {
        // theRest is "group"
        rv = FindHostFromGroup(currentHost, str);
        if (NS_FAILED(rv)) {
            CRTFREEIF(list);
            return rv;
        }

        // build up the retvalStr;
        if (!retvalStr.IsEmpty()) {
          retvalStr += ",";
        }
        retvalStr += str;
      }

      if (currentHost.IsEmpty()) {
#ifdef DEBUG_NEWS
        printf("empty current host!\n");
#endif
        CRTFREEIF(list);
        return NS_ERROR_FAILURE;
      }
      
      if (host.IsEmpty()) {
#ifdef DEBUG_NEWS
        printf("got a host, set it\n");
#endif
        host = currentHost;
      }
      else {
        if (!host.Equals(currentHost)) {
#ifdef DEBUG_NEWS
          printf("no cross posting to multiple hosts!\n");
#endif
          CRTFREEIF(list);
          return NS_ERROR_NNTP_NO_CROSS_POSTING;
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
  CRTFREEIF(list);
  
  *_retval = nsCRT::strdup(retvalStr.GetBuffer());
  if (!*_retval) return NS_ERROR_OUT_OF_MEMORY;
  
#ifdef DEBUG_NEWS
  printf("Newsgroups header = %s\n", *_retval);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsNntpService::PostMessage(nsIFileSpec *fileToPost, const char *newsgroupsNames, const char *newshost, nsIUrlListener * aUrlListener, nsIMsgWindow *aMsgWindow, nsIURI **_retval)
{
#ifdef DEBUG_NEWS
  printf("nsNntpService::PostMessage(??,%s,??,??)\n",newsgroupsNames);
#endif
  if (!aMsgWindow) return NS_ERROR_NULL_POINTER;
  if (!newsgroupsNames) return NS_ERROR_NULL_POINTER;
  if (PL_strlen(newsgroupsNames) == 0) return NS_ERROR_FAILURE;
    
  NS_LOCK_INSTANCE();
  
  nsCOMPtr <nsINntpUrl> nntpUrl;
  nsresult rv = NS_OK;
  
  rv = nsComponentManager::CreateInstance(kCNntpUrlCID, nsnull, NS_GET_IID(nsINntpUrl), getter_AddRefs(nntpUrl));
  if (NS_FAILED(rv) || !nntpUrl) return rv;

  nntpUrl->SetNewsAction(nsINntpUrl::ActionPostArticle);

  nsXPIDLCString newsUrlSpec;
  rv = SetUpNntpUrlForPosting(nntpUrl, newsgroupsNames, newshost, getter_Copies(newsUrlSpec));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(nntpUrl);
  if (!mailnewsurl) return NS_ERROR_FAILURE;

  mailnewsurl->SetSpec((const char *)newsUrlSpec);
  
  if (aUrlListener) // register listener if there is one...
    mailnewsurl->RegisterListener(aUrlListener);
  
  // almost there...now create a nntp protocol instance to run the url in...
  nsNNTPProtocol *nntpProtocol = nsnull;

  // ### try to access cache - but can't find server yet...
  nntpProtocol = new nsNNTPProtocol(mailnewsurl, aMsgWindow);
  if (!nntpProtocol) return NS_ERROR_OUT_OF_MEMORY;;
  
  rv = nntpProtocol->Initialize(mailnewsurl, aMsgWindow);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr <nsINNTPNewsgroupPost> post;
  rv = nsComponentManager::CreateInstance(kCNNTPNewsgroupPostCID, nsnull, NS_GET_IID(nsINNTPNewsgroupPost), getter_AddRefs(post));
  if (NS_FAILED(rv) || !post) return rv;

  rv = post->SetPostMessageFile(fileToPost);
  if (NS_FAILED(rv)) return rv;
  
  rv = nntpUrl->SetMessageToPost(post);
  if (NS_FAILED(rv)) return rv;
            
  rv = nntpProtocol->LoadNewsUrl(mailnewsurl, /* aConsumer */ nsnull);
		
  if (_retval)
	  nntpUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) _retval);
    
  NS_UNLOCK_INSTANCE();

  return rv;
}

nsresult nsNntpService::ConstructNntpUrl(const char * urlString, const char * newsgroupName, nsMsgKey key, nsIUrlListener *aUrlListener,  
                                         nsIMsgWindow * aMsgWindow, nsIURI ** aUrl)
{
  nsCOMPtr <nsINntpUrl> nntpUrl;
  nsresult rv = NS_OK;

  rv = nsComponentManager::CreateInstance(kCNntpUrlCID, nsnull, NS_GET_IID(nsINntpUrl), getter_AddRefs(nntpUrl));
  if (NS_FAILED(rv) || !nntpUrl) return rv;
  
  nsCOMPtr <nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(nntpUrl);
  mailnewsurl->SetMsgWindow(aMsgWindow);
  nsCOMPtr <nsIMsgMessageUrl> msgUrl = do_QueryInterface(nntpUrl);
  msgUrl->SetUri(urlString);
  mailnewsurl->SetSpec(urlString);

  if (newsgroupName != "") {
    nsCOMPtr <nsINNTPNewsgroup> newsgroup;
    rv = nsComponentManager::CreateInstance(kCNNTPNewsgroupCID, nsnull, NS_GET_IID(nsINNTPNewsgroup), getter_AddRefs(newsgroup));
    if (NS_FAILED(rv) || !newsgroup) return rv;                       
       
    rv = newsgroup->Initialize(newsgroupName, nsnull /* set */, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    
    rv = nntpUrl->SetNewsgroup(newsgroup);
    if (NS_FAILED(rv)) return rv;
    
    // if we are running a news url to display a message, these
    // will be used later, to mark the message as read after we finish loading
    rv = nntpUrl->SetMessageKey(key);
    if (NS_FAILED(rv)) return rv;
        
    rv = nntpUrl->SetNewsgroupName((char *) newsgroupName);
    if (NS_FAILED(rv)) return rv;
  }
  
  if (aUrlListener) // register listener if there is one...
    mailnewsurl->RegisterListener(aUrlListener);

  (*aUrl) = mailnewsurl;
  NS_IF_ADDREF(*aUrl);
  return rv;

}

nsresult
nsNntpService::CreateNewsAccount(const char *username, const char *hostname, PRBool isSecure, PRInt32 port, nsIMsgIncomingServer **server)
{
	nsresult rv;
	// username can be null.
	if (!hostname || !server) return NS_ERROR_NULL_POINTER;
	
	nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_PROGID, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!accountManager) return NS_ERROR_FAILURE;


	nsCOMPtr <nsIMsgAccount> account;
	rv = accountManager->CreateAccount(getter_AddRefs(account));
	if (NS_FAILED(rv)) return rv;

	rv = accountManager->CreateIncomingServer(username, hostname, "nntp", server);
	if (NS_FAILED(rv)) return rv;

	rv = (*server)->SetIsSecure(isSecure);
	if (NS_FAILED(rv)) return rv;
	
	rv = (*server)->SetPort(port);
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr <nsIMsgIdentity> identity;
	rv = accountManager->CreateIdentity(getter_AddRefs(identity));
	if (NS_FAILED(rv)) return rv;
	if (!identity) return NS_ERROR_FAILURE;

	// the identity isn't filled in, so it is not valid.
	rv = (*server)->SetValid(PR_FALSE);
	if (NS_FAILED(rv)) return rv;

	// hook them together
	rv = account->SetIncomingServer(*server);
	if (NS_FAILED(rv)) return rv;
	rv = account->AddIdentity(identity);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

nsresult
nsNntpService::GetProtocolForUri(nsIURI *aUri, nsIMsgWindow *aMsgWindow, nsINNTPProtocol **aProtocol)
{
  nsXPIDLCString hostName;
  nsXPIDLCString userName;
  nsXPIDLCString scheme;
  PRInt32 port = 0;
  nsresult rv;
  
#ifdef DEBUG_sspitzer
  nsXPIDLCString spec;
  rv = aUri->GetSpec(getter_Copies(spec));
  printf("GetProtocolForUri(%s,...)\n",(const char *)spec);
#endif

  rv = aUri->GetHost(getter_Copies(hostName));
  rv = aUri->GetPreHost(getter_Copies(userName));
  rv = aUri->GetScheme(getter_Copies(scheme));
  rv = aUri->GetPort(&port);

  nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_PROGID, &rv);
  if (NS_FAILED(rv)) return rv;
  if (!accountManager) return NS_ERROR_FAILURE;

  // find the incoming server, it if exists.
  // migrate if necessary, before searching for it.
  // if it doesn't exist, create it.
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsCOMPtr<nsINntpIncomingServer> nntpServer;

#ifdef DEBUG_sspitzer
  printf("for bug, #36661, see if there are any accounts, if not, try migrating.  should this be pushed into FindServer()?\n");
#endif
  nsCOMPtr <nsISupportsArray> accounts;
  rv = accountManager->GetAccounts(getter_AddRefs(accounts));
  if (NS_FAILED(rv)) return rv;

  PRUint32 accountCount;
  rv = accounts->Count(&accountCount);
  if (NS_FAILED(rv)) return rv;

  if (accountCount == 0) {
	nsCOMPtr <nsIMessengerMigrator> messengerMigrator = do_GetService(kMessengerMigratorCID, &rv);
    if (NS_FAILED(rv)) return rv;
	if (!messengerMigrator) return NS_ERROR_FAILURE;

	// migration can fail;
 	messengerMigrator->UpgradePrefs(); 
  }

  // xxx todo what if we have two servers on the same host, but different ports?
  // or no port, but isSecure (snews:// vs news://) is different?
  rv = accountManager->FindServer((const char *)userName,
                                (const char *)hostName,
                                "nntp",
                                getter_AddRefs(server));

  if (NS_FAILED(rv) || !server) {
	  PRBool isSecure = PR_FALSE;
	  if (nsCRT::strcasecmp("snews",(const char *)scheme) == 0) {
		  isSecure = PR_TRUE;
	  }
	  rv = CreateNewsAccount((const char *)userName,(const char *)hostName,isSecure,port,getter_AddRefs(server));
  }
   
  if (NS_FAILED(rv)) return rv;
  if (!server) return NS_ERROR_FAILURE;
  
  nntpServer = do_QueryInterface(server, &rv);

  if (!nntpServer || NS_FAILED(rv))
    return rv;

  rv = nntpServer->GetNntpConnection(aUri, aMsgWindow, aProtocol);
  if (NS_FAILED(rv) || !*aProtocol) 
    return NS_ERROR_OUT_OF_MEMORY;
  return rv;
}
nsresult 
nsNntpService::RunNewsUrl(nsIURI * aUri, nsIMsgWindow *aMsgWindow, nsISupports * aConsumer)
{
  nsresult rv;

  // almost there...now create a nntp protocol instance to run the url in...
  nsCOMPtr <nsINNTPProtocol> nntpProtocol;
  rv = GetProtocolForUri(aUri, aMsgWindow, getter_AddRefs(nntpProtocol));

  if (NS_SUCCEEDED(rv))
    rv = nntpProtocol->Initialize(aUri, aMsgWindow);
  if (NS_FAILED(rv)) return rv;
  
  rv = nntpProtocol->LoadNewsUrl(aUri, aConsumer);
  return rv;
}

NS_IMETHODIMP nsNntpService::GetNewNews(nsINntpIncomingServer *nntpServer, const char *uri, nsIUrlListener * aUrlListener, nsIMsgWindow *aMsgWindow, nsIURI **_retval)
{
  if (!uri) return NS_ERROR_NULL_POINTER;

#ifdef DEBUG_NEWS
  printf("nsNntpService::GetNewNews(%s)\n", uri);
#endif
  
  NS_LOCK_INSTANCE();
  nsresult rv = NS_OK;
  nsXPIDLCString nntpHostName;
  
  nsCOMPtr<nsIMsgIncomingServer> server;
  server = do_QueryInterface(nntpServer);
  
  // convert normal host to nntp host.
  // XXX - this doesn't handle QI failing very well
  if (server) {
    // load up required server information
    server->GetHostName(getter_Copies(nntpHostName));
  }
#ifdef DEBUG_NEWS
  else {
	  printf("server == nsnull\n");
  }
#endif
  
#ifdef DEBUG_NEWS
  if (nntpHostName) {
    printf("get news from news://%s\n", (const char *) nntpHostName);
  }
  else {
    printf("nntpHostName is null\n");
  }
#endif

  nsCAutoString uriStr(uri);
  nsCAutoString newsgroupName;
  
  NS_ASSERTION((uriStr.Find(kNewsRootURI) == 0), "uriStr didn't start with news:/");
  if (uriStr.Find(kNewsRootURI) == 0) {
    // uriStr looks like this:  
    // "news://news.mcom.com/mcom.linux"
    // or this:
    // "news://sspitzer@news.mcom.com/mcom.linux"

    PRInt32 atPos = uriStr.FindChar('@');
    if (atPos == -1) {
	    uriStr.Right(newsgroupName, uriStr.Length() - kNewsRootURILen /* for news:/ */ - 1 /* for the slash */ - PL_strlen(nntpHostName) /* for the hostname */ -1 /* for the next slash */);
    }
    else {
	    uriStr.Right(newsgroupName, uriStr.Length() - atPos /* for "news://<username>" */ -1 /* for the @ */ - PL_strlen(nntpHostName) /* for the hostname */ -1 /* for the next slash */);
    }
    
	nsCOMPtr<nsIURI> aUrl;
	rv = ConstructNntpUrl(uriStr, newsgroupName, nsMsgKey_None, aUrlListener,  aMsgWindow, getter_AddRefs(aUrl));
	if (NS_FAILED(rv)) return rv;
	nsCOMPtr<nsINntpUrl> nntpUrl = do_QueryInterface(aUrl);
	if (nntpUrl)
		nntpUrl->SetNewsAction(nsINntpUrl::ActionGetNewNews);
	nsCOMPtr<nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(aUrl);
	if (mailNewsUrl)
	{
		mailNewsUrl->SetUpdatingFolder(PR_TRUE);
		mailNewsUrl->SetMsgWindow(aMsgWindow);
	}

    rv = RunNewsUrl(aUrl, aMsgWindow, nsnull);  
	
	if (_retval)
	{
		*_retval = aUrl;
		NS_IF_ADDREF(*_retval);
	}
  }
  else 
    rv = NS_ERROR_FAILURE;
  
      
  NS_UNLOCK_INSTANCE();
  return rv;
}

NS_IMETHODIMP nsNntpService::CancelMessages(const char *hostname, const char *newsgroupname, nsISupportsArray *messages, nsISupports * aConsumer, nsIUrlListener * aUrlListener, nsIMsgWindow *aMsgWindow, nsIURI ** aURL)
{
  nsresult rv = NS_OK;
  PRUint32 count = 0;

  NS_ENSURE_ARG(aMsgWindow);

  if (!hostname) return NS_ERROR_NULL_POINTER;
  if (PL_strlen(hostname) == 0) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrompt> dialog;
  aMsgWindow->GetPromptDialog(getter_AddRefs(dialog));
  NS_ENSURE_TRUE(dialog, NS_ERROR_FAILURE);
    
  if (!messages) {
    nsAutoString alertText; alertText.AssignWithConversion("No articles are selected.");
    if (dialog)
      rv = dialog->Alert(nsnull, alertText.GetUnicode());
    
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
    nsAutoString alertText; alertText.AssignWithConversion("You can only cancel one article at a time.");
    if (dialog)
      rv = dialog->Alert(nsnull, alertText.GetUnicode());
    return NS_ERROR_FAILURE;
  }
  
  nsMsgKey key;
  nsXPIDLCString messageId;
  
  nsCOMPtr<nsISupports> msgSupports = getter_AddRefs(messages->ElementAt(0));
  nsCOMPtr<nsIMessage> message(do_QueryInterface(msgSupports));
  if (message) {
    rv = message->GetMessageKey(&key);
    if (NS_FAILED(rv)) return rv;
    rv = message->GetMessageId(getter_Copies(messageId));
    if (NS_FAILED(rv)) return rv;
  }
  else {
    return NS_ERROR_FAILURE;
  }
  
  nsCAutoString urlStr;
  urlStr += kNewsRootURI;
  urlStr += "/";
  urlStr += hostname;
  urlStr += "/";
  urlStr += (const char*)messageId;
  urlStr += "?cancel";

#ifdef DEBUG_NEWS
  printf("attempt to cancel the message (key,ID,cancel url): (%d,%s,%s)\n", key, (const char *)messageId, (const char *)urlStr);
#endif /* DEBUG_NEWS */ 

  nsCAutoString newsgroupNameStr(newsgroupname);
  nsCOMPtr<nsIURI> url;
  rv = ConstructNntpUrl((const char *)urlStr, newsgroupNameStr, key, aUrlListener,  aMsgWindow, getter_AddRefs(url));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsINntpUrl> nntpUrl = do_QueryInterface(url);
  if (nntpUrl)
	nntpUrl->SetNewsAction(nsINntpUrl::ActionCancelArticle);
  rv = RunNewsUrl(url, aMsgWindow, aConsumer);  

  if (aURL)
  {
	*aURL = url;
	NS_IF_ADDREF(*aURL);
  }
  return rv; 
}

NS_IMETHODIMP nsNntpService::GetScheme(char * *aScheme)
{
	nsresult rv = NS_OK;
	if (aScheme)
		*aScheme = nsCRT::strdup("news");
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 
}

NS_IMETHODIMP nsNntpService::GetDefaultPort(PRInt32 *aDefaultPort)
{
    NS_ENSURE_ARG_POINTER(aDefaultPort);
    *aDefaultPort = NEWS_PORT;
	return NS_OK;
}

NS_IMETHODIMP
nsNntpService::GetDefaultServerPort(PRInt32 *aDefaultPort)
{
    return GetDefaultPort(aDefaultPort);
}

NS_IMETHODIMP nsNntpService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
	nsresult rv = NS_OK;

	nsCOMPtr<nsINntpUrl> nntpUrl;
	rv = nsComponentManager::CreateInstance(kCNntpUrlCID, nsnull, NS_GET_IID(nsINntpUrl), getter_AddRefs(nntpUrl));
	if (NS_FAILED(rv)) return rv;
	nntpUrl->SetNewsAction(nsINntpUrl::ActionDisplayArticle);

	nntpUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) _retval);

	(*_retval)->SetSpec(aSpec);
	return rv;
}

NS_IMETHODIMP nsNntpService::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
	nsresult rv = NS_OK;
  nsCOMPtr <nsINNTPProtocol> nntpProtocol;
  rv = GetProtocolForUri(aURI, nsnull, getter_AddRefs(nntpProtocol));
  if (NS_SUCCEEDED(rv))
	  rv = nntpProtocol->Initialize(aURI, nsnull);
  if (NS_FAILED(rv)) return rv;

  return nntpProtocol->QueryInterface(NS_GET_IID(nsIChannel), (void **) _retval);
}

NS_IMETHODIMP
nsNntpService::SetDefaultLocalPath(nsIFileSpec *aPath)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kCPrefServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = prefs->SetFilePref(PREF_MAIL_ROOT_NNTP, aPath, PR_FALSE /* set default */);
    return rv;
}

NS_IMETHODIMP
nsNntpService::GetDefaultLocalPath(nsIFileSpec ** aResult)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kCPrefServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = prefs->GetFilePref(PREF_MAIL_ROOT_NNTP, aResult);
    if (NS_SUCCEEDED(rv)) return rv;

    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = locator->GetFileLocation(nsSpecialFileSpec::App_NewsDirectory50, aResult);
    if (NS_FAILED(rv)) return rv; 

    rv = SetDefaultLocalPath(*aResult);
    return rv;
}
    
NS_IMETHODIMP
nsNntpService::GetServerIID(nsIID* *aServerIID)
{
    *aServerIID = new nsIID(NS_GET_IID(nsINntpIncomingServer));
    return NS_OK;
}

NS_IMETHODIMP
nsNntpService::GetRequiresUsername(PRBool *aRequiresUsername)
{
        NS_ENSURE_ARG_POINTER(aRequiresUsername);
        *aRequiresUsername = PR_FALSE;
        return NS_OK;
}

NS_IMETHODIMP
nsNntpService::GetPreflightPrettyNameWithEmailAddress(PRBool *aPreflightPrettyNameWithEmailAddress)
{
        NS_ENSURE_ARG_POINTER(aPreflightPrettyNameWithEmailAddress);
        *aPreflightPrettyNameWithEmailAddress = PR_FALSE;
        return NS_OK;
}

NS_IMETHODIMP
nsNntpService::GetCanDelete(PRBool *aCanDelete)
{
        NS_ENSURE_ARG_POINTER(aCanDelete);
        *aCanDelete = PR_TRUE;
        return NS_OK;
}

NS_IMETHODIMP
nsNntpService::GetCanDuplicate(PRBool *aCanDuplicate)
{
        NS_ENSURE_ARG_POINTER(aCanDuplicate);
        *aCanDuplicate = PR_TRUE;
        return NS_OK;
}        

NS_IMETHODIMP
nsNntpService::GetDefaultCopiesAndFoldersPrefsToServer(PRBool *aDefaultCopiesAndFoldersPrefsToServer)
{
	NS_ENSURE_ARG_POINTER(aDefaultCopiesAndFoldersPrefsToServer);
	// when a news account is created, the copies and folder prefs for the associated identity
	// don't point to folders on the server. 
	// this makes sense, since there is no "Drafts" folder on a news server.
	// they'll point to the ones on "Local Folders"
	*aDefaultCopiesAndFoldersPrefsToServer = PR_FALSE;
    return NS_OK;
}

//
// rhp: Right now, this is the same as simple DisplayMessage, but it will change
// to support print rendering.
//
NS_IMETHODIMP nsNntpService::DisplayMessageForPrinting(const char* aMessageURI, nsISupports * aDisplayConsumer, 
                                                  nsIMsgWindow *aMsgWindow, nsIUrlListener * aUrlListener, nsIURI ** aURL)
{
  mPrintingOperation = PR_TRUE;
  nsresult rv = DisplayMessage(aMessageURI, aDisplayConsumer, aMsgWindow, aUrlListener, nsnull, aURL);
  mPrintingOperation = PR_FALSE;
  return rv;
}

NS_IMETHODIMP nsNntpService::Search(nsIMsgSearchSession *aSearchSession, nsIMsgWindow *aMsgWindow, nsIMsgFolder *aMsgFolder, const char *aSearchUri)
{
  NS_ENSURE_ARG(aMsgFolder);
  nsCOMPtr <nsIMsgIncomingServer> server;
  nsresult rv = aMsgFolder->GetServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server)
  {
  	nsCOMPtr<nsIURI> uri;
    nsXPIDLCString serverUri;
    nsXPIDLString newsgroupName;
    rv = server->GetServerURI(getter_Copies(serverUri));
    if (NS_FAILED(rv)) return rv;
    aMsgFolder->GetName(getter_Copies(newsgroupName));
    if (NS_FAILED(rv)) return rv;

    nsCString searchUrl(NS_STATIC_CAST(const char *, serverUri));
    nsCString asciiNewsgroupName;
    asciiNewsgroupName.AssignWithConversion(newsgroupName);
    searchUrl.Append(aSearchUri);
    nsCOMPtr <nsIUrlListener> urlListener = do_QueryInterface(aSearchSession);

    rv = ConstructNntpUrl(searchUrl.GetBuffer(), asciiNewsgroupName.GetBuffer(), nsMsgKey_None, urlListener,  aMsgWindow, getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(uri));
    if (msgurl)
      msgurl->SetSearchSession(aSearchSession);
    // run the url to update the counts
      rv = RunNewsUrl(uri, nsnull, nsnull);  
    if (NS_FAILED(rv)) return rv;

  }
  return rv;
}


NS_IMETHODIMP
nsNntpService::UpdateCounts(nsINntpIncomingServer *aNntpServer, nsIMsgWindow *aMsgWindow)
{
	nsresult rv;
#ifdef DEBUG_NEWS
	printf("in UpdateCountsForNewsgroup()\n");
#endif
	if (!aNntpServer) return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIURI> uri;
	nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aNntpServer, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!server) return NS_ERROR_FAILURE;

	nsXPIDLCString serverUri;
	rv = server->GetServerURI(getter_Copies(serverUri));
	if (NS_FAILED(rv)) return rv;

	rv = ConstructNntpUrl((const char *)serverUri, "", nsMsgKey_None, nsnull, aMsgWindow, getter_AddRefs(uri));
	if (NS_FAILED(rv)) return rv;

	// run the url to update the counts
    rv = RunNewsUrl(uri, aMsgWindow, nsnull);  
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP 
nsNntpService::BuildSubscribeDatasource(nsINntpIncomingServer *aNntpServer, nsIMsgWindow *aMsgWindow)
{
	nsresult rv;
#ifdef DEBUG_NEWS
	printf("in BuildSubscribeDatasource()\n");
#endif
	if (!aNntpServer) return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIURI> uri;
	nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aNntpServer, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!server) return NS_ERROR_FAILURE;

	nsXPIDLCString serverUri;
	rv = server->GetServerURI(getter_Copies(serverUri));

	nsCAutoString uriStr;
	uriStr += (const char *)serverUri;
	uriStr += "/*";
		
	nsCOMPtr <nsIUrlListener> listener = do_QueryInterface(aNntpServer, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!listener) return NS_ERROR_FAILURE;
	rv = ConstructNntpUrl((const char *)uriStr, "", nsMsgKey_None, listener, aMsgWindow, getter_AddRefs(uri));
	if (NS_FAILED(rv)) return rv;

	// now run the url to add the rest of the groups
        rv = RunNewsUrl(uri, aMsgWindow, nsnull);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

CMDLINEHANDLER3_IMPL(nsNntpService,"-news","general.startup.news","Start with news.",NS_NEWSSTARTUPHANDLER_PROGID,"News Cmd Line Handler", PR_FALSE,"", PR_TRUE)

NS_IMETHODIMP nsNntpService::GetChromeUrlForTask(char **aChromeUrlForTask) 
{ 
    if (!aChromeUrlForTask) return NS_ERROR_FAILURE; 
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefService, kCPrefServiceCID, &rv);
	if (NS_SUCCEEDED(rv))
	{
		PRInt32 layout;
		rv = prefService->GetIntPref("mail.pane_config", &layout);		
		if(NS_SUCCEEDED(rv))
		{
			if(layout == 0)
				*aChromeUrlForTask = PL_strdup("chrome://messenger/content/messenger.xul");
			else
				*aChromeUrlForTask = PL_strdup("chrome://messenger/content/mail3PaneWindowVertLayout.xul");

			return NS_OK;

		}	
	}
	*aChromeUrlForTask = PL_strdup("chrome://messenger/content/messenger.xul"); 
    return NS_OK; 
}



NS_IMETHODIMP 
nsNntpService::HandleContent(const char * aContentType, const char * aCommand, const char * aWindowTarget, nsISupports * aWindowContext, nsIChannel * aChannel)
{
  nsresult rv = NS_OK;
  if (!aChannel) return NS_ERROR_NULL_POINTER;

  if (nsCRT::strcasecmp(aContentType, "x-application-newsgroup") == 0) {
      nsCOMPtr<nsIURI> uri;
      rv = aChannel->GetURI(getter_AddRefs(uri));
	  if (NS_FAILED(rv)) return rv;

      if (uri) { 	
		nsCOMPtr <nsIMessengerWindowService> messengerWindowService = do_GetService(NS_MESSENGERWINDOWSERVICE_PROGID,&rv);
		if (NS_FAILED(rv)) return rv;

		rv = messengerWindowService->OpenMessengerWindowWithUri(uri);
		if (NS_FAILED(rv)) return rv;
	  }
  }

  return rv;
}
