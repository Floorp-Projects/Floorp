/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Scott MacGregor <mscott@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"    // precompiled header...
#include "nntpCore.h"
#include "nsMsgNewsCID.h"
#include "nsINntpUrl.h"
#include "nsNNTPProtocol.h"
#include "nsNNTPNewsgroupPost.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsNewsUtils.h"
#include "nsNewsDatabase.h"
#include "nsMsgDBCID.h"
#include "nsMsgBaseCID.h"
#include "nsIPref.h"
#include "nsCRT.h"  // for nsCRT::strtok
#include "nsNntpService.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsIDirectoryService.h"
#include "nsIMsgAccountManager.h"
#include "nsIMessengerMigrator.h"
#include "nsINntpIncomingServer.h"
#include "nsICmdLineHandler.h"
#include "nsICategoryManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIMessengerWindowService.h"
#include "nsIMsgSearchSession.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIWebNavigation.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsIPrompt.h"
#include "nsIRDFService.h"
#include "nsNewsDownloader.h"
#include "prprf.h"
#include "nsICacheService.h"
#include "nsNetCID.h"

#undef GetPort  // XXX Windows!
#undef SetPort  // XXX Windows!

#define PREF_NETWORK_HOSTS_NNTP_SERVER	"network.hosts.nntp_server"
#define PREF_MAIL_ROOT_NNTP 	"mail.root.nntp"

static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
static NS_DEFINE_CID(kCPrefServiceCID, NS_PREF_CID); 
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kMessengerMigratorCID, NS_MESSENGERMIGRATOR_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);
                    
nsNntpService::nsNntpService()
{
    NS_INIT_REFCNT();
    mPrintingOperation = PR_FALSE;
	mOpenAttachmentOperation = PR_FALSE;
    mCopyingOperation = PR_FALSE;
}

nsNntpService::~nsNntpService()
{
	// do nothing
}

NS_IMPL_THREADSAFE_ADDREF(nsNntpService);
NS_IMPL_THREADSAFE_RELEASE(nsNntpService);

NS_IMPL_QUERY_INTERFACE7(nsNntpService,
                         nsINntpService,
                         nsIMsgMessageService,
                         nsIProtocolHandler,
                         nsIMsgProtocolInfo,
                         nsICmdLineHandler,
                         nsIMsgMessageFetchPartService,
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
    NS_ENSURE_ARG_POINTER(aMessageURI);
 
    // double check it is a news_message:/ uri   
    if (PL_strncmp(aMessageURI, kNewsMessageRootURI, kNewsMessageRootURILen)) {
        rv = NS_ERROR_UNEXPECTED;
        NS_ENSURE_SUCCESS(rv,rv);
    }

    nsCOMPtr <nsIMsgFolder> folder;
    nsMsgKey key = nsMsgKey_None;
    rv = DecomposeNewsMessageURI(aMessageURI, getter_AddRefs(folder), &key);
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsXPIDLCString messageIdURL;
    rv = CreateMessageIDURL(folder, key, getter_Copies(messageIdURL));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIURI> url;
    rv = ConstructNntpUrl(messageIdURL.get(), aUrlListener, aMsgWindow, aMessageURI, nsINntpUrl::ActionSaveMessageToDisk, getter_AddRefs(url));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(url);
    if (msgUrl) {
        msgUrl->SetMessageFile(aFile);
        msgUrl->SetAddDummyEnvelope(aAddDummyEnvelope);
        msgUrl->SetCanonicalLineEnding(canonicalLineEnding);
    }   
    
    rv = RunNewsUrl(url, nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv,rv);

    if (aURL)
    {
	    *aURL = url;
	    NS_IF_ADDREF(*aURL);
    }

  return rv;
}


nsresult
nsNntpService::CreateMessageIDURL(nsIMsgFolder *folder, nsMsgKey key, char **url)
{
    NS_ENSURE_ARG_POINTER(folder);
    NS_ENSURE_ARG_POINTER(url);
    if (key == nsMsgKey_None) return NS_ERROR_INVALID_ARG;

    nsCOMPtr <nsIMsgDBHdr> hdr;
    nsresult rv = folder->GetMessageHeader(key, getter_AddRefs(hdr));
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsXPIDLCString messageID;
    rv = hdr->GetMessageId(getter_Copies(messageID));
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr <nsIMsgFolder> rootFolder;
    rv = folder->GetRootFolder(getter_AddRefs(rootFolder));
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsXPIDLCString rootFolderURI;
    rv = rootFolder->GetURI(getter_Copies(rootFolderURI));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCAutoString uri;
    uri = rootFolderURI.get();
    uri += '/';
    uri += messageID.get();
    *url = nsCRT::strdup(uri.get());
    if (!*url) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpService::DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, 
                                       nsIMsgWindow *aMsgWindow, nsIUrlListener * aUrlListener, const PRUnichar * aCharsetOverride, nsIURI ** aURL)
{
  nsresult rv = NS_OK;
  NS_ENSURE_ARG_POINTER(aMessageURI);

  nsCOMPtr <nsIMsgFolder> folder;
  nsMsgKey key = nsMsgKey_None;
  rv = DecomposeNewsMessageURI(aMessageURI, getter_AddRefs(folder), &key);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCAutoString uri;
  //
  // if we are copying, we need the uri to be a news://host/group#key uri
  // so that we can get back to the nsIMsgDBHdr.
  //
  // if we are displaying (or printing), we want the news://host/message-id url
  // we keep the original uri around, for cancelling and so we can get to the
  // articles by doing GROUP and then ARTICLE <n>.
  //
  // using news://host/message-id has an extra benefit.
  // we'll use that to look up in the cache, so if 
  // you are reading a message that you've already read, you
  // (from a cross post) it would be in your cache.
  // 
  // if we used news://host/group#key, we would not have that behaviour
  //
  // XXX fix it so copy operations check the memory cache
  if (mCopyingOperation) {
    uri = aMessageURI;
  }
  else {
    nsXPIDLCString messageIdURL;
    rv = CreateMessageIDURL(folder, key, getter_Copies(messageIdURL));
    NS_ENSURE_SUCCESS(rv,rv);
    uri = messageIdURL.get();
  }

  // rhp: If we are displaying this message for the purposes of printing, append
  // the magic operand.
  if (mPrintingOperation)
    uri.Append("?header=print");

  nsNewsAction action = nsINntpUrl::ActionFetchArticle;
  if (mOpenAttachmentOperation)
    action = nsINntpUrl::ActionFetchPart;

  nsCOMPtr<nsIURI> url;
  rv = ConstructNntpUrl(uri.get(), aUrlListener, aMsgWindow, aMessageURI, action, getter_AddRefs(url));
  NS_ENSURE_SUCCESS(rv,rv);

  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr <nsIMsgMailNewsUrl> msgUrl = do_QueryInterface(url,&rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIMsgI18NUrl> i18nurl = do_QueryInterface(msgUrl,&rv);
    NS_ENSURE_SUCCESS(rv,rv);

    i18nurl->SetCharsetOverRide(aCharsetOverride);

    PRBool shouldStoreMsgOffline = PR_FALSE;
    PRBool hasMsgOffline = PR_FALSE;

    if (folder)
    {
      nsCOMPtr <nsIMsgNewsFolder> newsFolder = do_QueryInterface(folder);
      if (newsFolder)
      {
        folder->ShouldStoreMsgOffline(key, &shouldStoreMsgOffline);
        folder->HasMsgOffline(key, &hasMsgOffline);
        msgUrl->SetMsgIsInLocalCache(hasMsgOffline);
        if (WeAreOffline())
        {
          if (!hasMsgOffline)
          {
            nsCOMPtr<nsIMsgIncomingServer> server;

            rv = folder->GetServer(getter_AddRefs(server));
            if (server)
              return server->DisplayOfflineMsg(aMsgWindow);
          }
        }
        newsFolder->SetSaveArticleOffline(shouldStoreMsgOffline);
      }
    }

    // now is where our behavior differs....if the consumer is the docshell then we want to 
    // run the url in the webshell in order to display it. If it isn't a docshell then just
    // run the news url like we would any other news url. 
	  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aDisplayConsumer, &rv));
	  if (NS_SUCCEEDED(rv) && docShell) 
    {
		nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
		// DIRTY LITTLE HACK --> if we are opening an attachment we want the docshell to
        // treat this load as if it were a user click event. Then the dispatching stuff will be much
        // happier.
      if (mOpenAttachmentOperation) 
      {
			docShell->CreateLoadInfo(getter_AddRefs(loadInfo));
			loadInfo->SetLoadType(nsIDocShellLoadInfo::loadLink);
		}
	    
	    rv = docShell->LoadURI(url, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE);
	  }
	  else 
    {
      nsCOMPtr<nsIStreamListener> aStreamListener = do_QueryInterface(aDisplayConsumer, &rv);
      if (NS_SUCCEEDED(rv) && aStreamListener)
      {
        nsCOMPtr<nsIChannel> aChannel;
        nsCOMPtr<nsILoadGroup> aLoadGroup;
        nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(url, &rv);
        if (NS_SUCCEEDED(rv) && mailnewsUrl)
        {
          if (aMsgWindow)
            mailnewsUrl->SetMsgWindow(aMsgWindow);
          mailnewsUrl->GetLoadGroup(getter_AddRefs(aLoadGroup));
        }
        rv = NewChannel(url, getter_AddRefs(aChannel));
        if (NS_FAILED(rv)) return rv;
        
        rv = aChannel->SetLoadGroup(aLoadGroup);
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsISupports> aCtxt = do_QueryInterface(url);
        //  now try to open the channel passing in our display consumer as the listener 
        rv = aChannel->AsyncOpen(aStreamListener, aCtxt);
      }
      else
		rv = RunNewsUrl(url, aMsgWindow, aDisplayConsumer);
	  }
  }

  if (aURL) {
	  *aURL = url;
	  NS_IF_ADDREF(*aURL);
  }
  return rv;
}

NS_IMETHODIMP 
nsNntpService::FetchMessage(nsIMsgFolder *folder, nsMsgKey key, nsIMsgWindow *aMsgWindow, nsISupports * aConsumer, nsIUrlListener * aUrlListener, nsIURI ** aURL)
{
  nsresult rv = NS_OK;
  NS_ENSURE_ARG_POINTER(folder);

  nsCOMPtr<nsIMsgNewsFolder> msgNewsFolder = do_QueryInterface(folder, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIMsgDBHdr> hdr;
  rv = folder->GetMessageHeader(key, getter_AddRefs(hdr));
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString originalMessageUri;
  rv = folder->GetUriForMsg(hdr, getter_Copies(originalMessageUri));
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString messageIdURL;
  rv = CreateMessageIDURL(folder, key, getter_Copies(messageIdURL));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIURI> url;
  rv = ConstructNntpUrl((const char *)messageIdURL, aUrlListener, aMsgWindow, originalMessageUri.get(), nsINntpUrl::ActionFetchArticle, getter_AddRefs(url));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = RunNewsUrl(url, aMsgWindow, aConsumer);
  NS_ENSURE_SUCCESS(rv,rv);

  if (aURL) 
  {
	  *aURL = url;
	  NS_IF_ADDREF(*aURL);
  }

  return rv;
}

NS_IMETHODIMP nsNntpService::FetchMimePart(nsIURI *aURI, const char *aMessageURI, nsISupports *aDisplayConsumer, nsIMsgWindow *aMsgWindow, nsIUrlListener *aUrlListener, nsIURI **aURL)
{
  nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(aURI));
  msgUrl->SetMsgWindow(aMsgWindow);

  // set up the url listener
	if (aUrlListener)
		msgUrl->RegisterListener(aUrlListener);
 
  return RunNewsUrl(msgUrl, aMsgWindow, aDisplayConsumer);
}

NS_IMETHODIMP nsNntpService::OpenAttachment(const char *aContentType, 
                                            const char *aFileName,
                                            const char *aUrl, 
                                            const char *aMessageUri, 
                                            nsISupports *aDisplayConsumer, 
                                            nsIMsgWindow *aMsgWindow, 
                                            nsIUrlListener *aUrlListener)
{

  nsCOMPtr<nsIURI> url;
  nsresult rv = NS_OK;
  nsCAutoString newsUrl;
  newsUrl = aUrl;
  newsUrl += "&type=";
  newsUrl += aContentType;
  newsUrl += "&filename=";
  newsUrl += aFileName;

  NewURI(newsUrl, nsnull, getter_AddRefs(url));

  if (NS_SUCCEEDED(rv) && url)
  {
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(url));
    msgUrl->SetMsgWindow(aMsgWindow);
    msgUrl->SetFileName(aFileName);

    // set up the url listener
	  if (aUrlListener)
	  	msgUrl->RegisterListener(aUrlListener);

	  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aDisplayConsumer, &rv));
	  if (NS_SUCCEEDED(rv) && docShell) 
    {
		  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
			docShell->CreateLoadInfo(getter_AddRefs(loadInfo));
			loadInfo->SetLoadType(nsIDocShellLoadInfo::loadLink);
	    return docShell->LoadURI(url, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE);
    }
    else
      return RunNewsUrl(url, aMsgWindow, aDisplayConsumer);
	}
  return NS_OK;
}

NS_IMETHODIMP nsNntpService::GetUrlForUri(const char *aMessageURI, nsIURI **aURL, nsIMsgWindow *aMsgWindow) 
{
  nsresult rv = NS_OK;
   
  NS_ENSURE_ARG_POINTER(aMessageURI);

  // double check that it is a news_message:/ uri
  if (PL_strncmp(aMessageURI, kNewsMessageRootURI, kNewsMessageRootURILen)) {
    rv = NS_ERROR_UNEXPECTED;
    NS_ENSURE_SUCCESS(rv,rv);
  }

  nsCOMPtr <nsIMsgFolder> folder;
  nsMsgKey key = nsMsgKey_None;
  rv = DecomposeNewsMessageURI(aMessageURI, getter_AddRefs(folder), &key);
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString messageIdURL;
  rv = CreateMessageIDURL(folder, key, getter_Copies(messageIdURL));
  NS_ENSURE_SUCCESS(rv,rv);

  // this is only called by view message source
  rv = ConstructNntpUrl(messageIdURL.get(), nsnull, aMsgWindow, aMessageURI, nsINntpUrl::ActionFetchArticle, aURL);
  NS_ENSURE_SUCCESS(rv,rv);
  if (folder && *aURL)
  {
    nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(*aURL);
    if (mailnewsUrl)
    {
      PRBool useLocalCache = PR_FALSE;
      folder->HasMsgOffline(key, &useLocalCache);  
      mailnewsUrl->SetMsgIsInLocalCache(useLocalCache);
    }
  }
  return rv;

}

NS_IMETHODIMP
nsNntpService::DecomposeNewsURI(const char *uri, nsIMsgFolder **folder, nsMsgKey *aMsgKey)
{
  nsresult rv;

  if (nsCRT::strncmp(uri, kNewsMessageRootURI, kNewsMessageRootURILen) == 0) {
    rv = DecomposeNewsMessageURI(uri, folder, aMsgKey);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else {
    rv = GetFolderFromUri(uri, folder);
    NS_ENSURE_SUCCESS(rv,rv);
    *aMsgKey = nsMsgKey_None;
  }
  return rv;
}

nsresult
nsNntpService::DecomposeNewsMessageURI(const char * aMessageURI, nsIMsgFolder ** aFolder, nsMsgKey *aMsgKey)
{
    NS_ENSURE_ARG_POINTER(aMessageURI);
    NS_ENSURE_ARG_POINTER(aFolder);
    NS_ENSURE_ARG_POINTER(aMsgKey);

    nsresult rv = NS_OK;
    nsCAutoString folderURI;

    rv = nsParseNewsMessageURI(aMessageURI, folderURI, aMsgKey);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = GetFolderFromUri(folderURI, aFolder);
    NS_ENSURE_SUCCESS(rv,rv);

    return NS_OK;
}

nsresult
nsNntpService::GetFolderFromUri(const char *uri, nsIMsgFolder **folder)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(uri);
    NS_ENSURE_ARG_POINTER(folder);

    nsCOMPtr <nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1",&rv);
    NS_ENSURE_SUCCESS(rv,rv);
    
    // the user might have typed in or clicked on a nntp:// url
    // to support this, we turn it into a news:// url
    nsCOMPtr<nsIRDFResource> res;
    if ((nsCRT::strlen(uri) > kNntpRootURILen) && nsCRT::strncmp(uri, kNntpRootURI, kNntpRootURILen) == 0) {
      nsCAutoString uriStr(kNewsRootURI);
      uriStr.Append(uri+kNntpRootURILen);
      rv = rdf->GetResource(uriStr.get(), getter_AddRefs(res));
    }
    else {
      rv = rdf->GetResource(uri, getter_AddRefs(res));
    }
    NS_ENSURE_SUCCESS(rv,rv);

    rv = res->QueryInterface(NS_GET_IID(nsIMsgFolder), (void **)folder);
    NS_ENSURE_SUCCESS(rv,rv);
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
    if (NS_SUCCEEDED(rv)) {
        mCopyingOperation = PR_TRUE;
        rv = DisplayMessage(aSrcMailboxURI, streamSupport, aMsgWindow, aUrlListener, nsnull, aURL);
        mCopyingOperation = PR_FALSE;
    }
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

nsresult
nsNntpService::FindServerWithNewsgroup(nsCString &host, nsCString &groupName)
{
	nsresult rv;

    nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
	nsCOMPtr<nsISupportsArray> servers;
	
	rv = accountManager->GetAllServers(getter_AddRefs(servers));
    NS_ENSURE_SUCCESS(rv,rv);

	findNewsServerEntry serverInfo;
	serverInfo.server = nsnull;
  	serverInfo.newsgroup = (const char *)groupName;

#ifdef DEBUG_seth
    printf("XXX this only looks at the list of subscribed newsgroups.  fix to use the hostinfo.dat information\n");
#endif

	servers->EnumerateForwards(findNewsServerWithGroup, (void *)&serverInfo);
	if (serverInfo.server) {
		nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(serverInfo.server);
		nsXPIDLCString thisHostname;
      rv = server->GetRealHostName(getter_Copies(thisHostname));
        NS_ENSURE_SUCCESS(rv,rv);

		host = (const char *)thisHostname;
	}
    
    return NS_OK;
}

nsresult nsNntpService::FindHostFromGroup(nsCString &host, nsCString &groupName)
{
  nsresult rv = NS_OK;
  // host always comes in as ""
  NS_ASSERTION(host.IsEmpty(), "host is not empty");
  if (!host.IsEmpty()) return NS_ERROR_FAILURE;
 
  rv = FindServerWithNewsgroup(host, groupName);
  NS_ENSURE_SUCCESS(rv,rv);

  // host can be empty
  return NS_OK;
}

nsresult 
nsNntpService::SetUpNntpUrlForPosting(nsINntpUrl *nntpUrl, const char *newsgroupsNames, const char *newspostingUrl, char **newsUrlSpec)
{
  nsresult rv = NS_OK;
  NS_ENSURE_ARG_POINTER(nntpUrl);
  NS_ENSURE_ARG_POINTER(newsgroupsNames);
  if (*newsgroupsNames == '\0') return NS_ERROR_FAILURE;

  // XXX TODO rewrite this
  // instead of using the hostname, we need to keep track of the current server id
  // if newspostingUrl is non-null, we'll use that to determine the initial currentServer
  // before we do that, I need to make sure the newspostingUrl we pass in is correct.
  // until then, it is going to be safer to ignore it and try to determine the posting host
  // from the newsgroups.

  nsCAutoString host;

  // newsgroupsNames can be a comma seperated list of these:
  // news://host/group
  // news://group
  // host/group
  // group

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
      nsCAutoString theRest;
      nsCAutoString currentHost;
      
      // does str start with "news:/"?
      if (str.Find(kNewsRootURI) == 0) {
        // we have news://group or news://host/group
        // set theRest to what's after news://
        str.Right(theRest, str.Length() - kNewsRootURILen /* for news:/ */ - 1 /* for the slash */);
      }
      else if (str.Find(":/") != -1) {
        // we have x:/y where x != news. this is bad, return failure
        CRTFREEIF(list);
        return NS_ERROR_FAILURE;
      }
      else {
        theRest = str;
      }
      // theRest is "group" or "host/group"
      PRInt32 slashpos = theRest.FindChar('/');
      if (slashpos > 0 ) {
        // theRest is "host/group"
        theRest.Left(currentHost, slashpos);
        theRest.Right(currentGroup, theRest.Length() - slashpos);
      }
      else {
        // str is "group"
        rv = FindHostFromGroup(currentHost, str);
        currentGroup = str;
        if (NS_FAILED(rv)) {
          CRTFREEIF(list);
		  return rv;
	    }
      }

      numGroups++;
      if (!currentHost.IsEmpty()) {
        if (host.IsEmpty()) {
          host = currentHost;
        }
        else {
          if (!host.Equals(currentHost)) {
            // yikes, we are trying to cross post
            CRTFREEIF(list);
            return NS_ERROR_NNTP_NO_CROSS_POSTING;
          }
        }
      }
      
      str = "";
      currentHost = "";
    }
    token = nsCRT::strtok(rest, ",", &rest);
  }    
  CRTFREEIF(list);
  
  // if we don't have a news host, find the first news server and use it
  if (host.IsEmpty()) {
    nsCOMPtr<nsIMsgIncomingServer> server;
    nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = accountManager->FindServer("","","nntp", getter_AddRefs(server));
    if (NS_SUCCEEDED(rv) && server) {
        nsXPIDLCString newsHostName;
        rv = server->GetRealHostName(getter_Copies(newsHostName));
        if (NS_SUCCEEDED(rv)) {
            host = (const char *)newsHostName;
        }
    }
  }

  // if we *still* don't have a hostname, use "news"
  if (host.IsEmpty()) {
    host = "news";
  }

  *newsUrlSpec = PR_smprintf("%s/%s",kNewsRootURI,(const char *)host);
  if (!*newsUrlSpec) return NS_ERROR_FAILURE;

  return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////////////
// nsINntpService support
////////////////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsNntpService::GenerateNewsHeaderValsForPosting(const char *newsgroupsList, char **newsgroupsHeaderVal, char **newshostHeaderVal)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(newsgroupsList);
  NS_ENSURE_ARG_POINTER(newsgroupsHeaderVal);
  NS_ENSURE_ARG_POINTER(newshostHeaderVal);
  NS_ENSURE_ARG_POINTER(*newsgroupsList);

  // newsgroupsList can be a comma seperated list of these:
  // news://host/group
  // news://group
  // host/group
  // group
  //
  // we are not going to allow the user to cross post to multiple hosts.
  // if we detect that, we stop and return error.

  // nsCRT::strtok is going destroy what we pass to it, so we need to make a copy of newsgroupsNames.
  char *list = nsCRT::strdup(newsgroupsList);
  char *token = nsnull;
  char *rest = list;
  nsCAutoString host;
  nsCAutoString str;
  nsCAutoString newsgroups;
    
  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) {
    str = token;
    str.StripWhitespace();

    if (!str.IsEmpty()) {
      nsCAutoString currentHost;
      nsCAutoString theRest;

      // does str start with "news:/"?
      if (str.Find(kNewsRootURI) == 0) {
        // we have news://group or news://host/group
        // set theRest to what's after news://
        str.Right(theRest, str.Length() - kNewsRootURILen /* for news:/ */ - 1 /* for the slash */);
      }
      else if (str.Find(":/") != -1) {
        // we have x:/y where x != news. this is bad, return failure
        CRTFREEIF(list);
        return NS_ERROR_FAILURE;
      }
      else {
        theRest = str;
      }

      // theRest is "group" or "host/group"
      PRInt32 slashpos = theRest.FindChar('/');
      if (slashpos > 0 ) {
        nsCAutoString currentGroup;
        
        // theRest is "host/group"
        theRest.Left(currentHost, slashpos);

        // from "host/group", put "group" into currentGroup;
        theRest.Right(currentGroup, theRest.Length() - currentHost.Length() - 1);

        NS_ASSERTION(!currentGroup.IsEmpty(), "currentGroup is empty");
        if (currentGroup.IsEmpty()) {
          CRTFREEIF(list);
          return NS_ERROR_FAILURE;
        }
        
        // build up the newsgroups
        if (!newsgroups.IsEmpty()) {
          newsgroups += ",";
        }
        newsgroups += currentGroup;
      }
      else {
        // str is "group"
        rv = FindHostFromGroup(currentHost, str);
        if (NS_FAILED(rv)) {
            CRTFREEIF(list);
            return rv;
        }

        // build up the newsgroups
        if (!newsgroups.IsEmpty()) {
          newsgroups += ",";
        }
        newsgroups += str;
      }

      if (!currentHost.IsEmpty()) {
        if (host.IsEmpty()) {
          host = currentHost;
        }
        else {
          if (!host.Equals(currentHost)) {
            CRTFREEIF(list);
            return NS_ERROR_NNTP_NO_CROSS_POSTING;
          }
        }
      }

      str = "";
      currentHost = "";
    }
    token = nsCRT::strtok(rest, ",", &rest);
  }
  CRTFREEIF(list);
  
  *newshostHeaderVal = ToNewCString(host);
  if (!*newshostHeaderVal) return NS_ERROR_OUT_OF_MEMORY;

  *newsgroupsHeaderVal = ToNewCString(newsgroups);
  if (!*newsgroupsHeaderVal) return NS_ERROR_OUT_OF_MEMORY;
  
  return NS_OK;
}

NS_IMETHODIMP
nsNntpService::PostMessage(nsIFileSpec *fileToPost, const char *newsgroupsNames, const char *newspostingUrl, nsIUrlListener * aUrlListener, nsIMsgWindow *aMsgWindow, nsIURI **_retval)
{
  // aMsgWindow might be null
  NS_ENSURE_ARG_POINTER(newsgroupsNames);
 
  if (*newsgroupsNames == '\0') return NS_ERROR_INVALID_ARG;
    
  NS_LOCK_INSTANCE();
  
  nsresult rv = NS_OK;
  
  nsCOMPtr <nsINntpUrl> nntpUrl = do_CreateInstance(NS_NNTPURL_CONTRACTID,&rv);
  NS_ENSURE_SUCCESS(rv,rv);
  if (!nntpUrl) return NS_ERROR_FAILURE;

  rv = nntpUrl->SetNewsAction(nsINntpUrl::ActionPostArticle);
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString newsUrlSpec;
  rv = SetUpNntpUrlForPosting(nntpUrl, newsgroupsNames, newspostingUrl, getter_Copies(newsUrlSpec));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(nntpUrl, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  if (!mailnewsurl) return NS_ERROR_FAILURE;

  mailnewsurl->SetSpec((const char *)newsUrlSpec);
  
  if (aUrlListener) // register listener if there is one...
    mailnewsurl->RegisterListener(aUrlListener);
  
  nsCOMPtr <nsINNTPNewsgroupPost> post = do_CreateInstance(NS_NNTPNEWSGROUPPOST_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  if (!post) return NS_ERROR_FAILURE;

  rv = post->SetPostMessageFile(fileToPost);
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = nntpUrl->SetMessageToPost(post);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIURI> url = do_QueryInterface(nntpUrl);
  rv = RunNewsUrl(url, aMsgWindow, nsnull /* consumer */);
  NS_ENSURE_SUCCESS(rv,rv);
		
  if (_retval)
	  nntpUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) _retval);
    
  NS_UNLOCK_INSTANCE();

  return rv;
}

nsresult 
nsNntpService::ConstructNntpUrl(const char *urlString, nsIUrlListener *aUrlListener, nsIMsgWindow *aMsgWindow, const char *originalMessageUri, PRInt32 action, nsIURI ** aUrl)
{
  nsresult rv = NS_OK;

  nsCOMPtr <nsINntpUrl> nntpUrl = do_CreateInstance(NS_NNTPURL_CONTRACTID,&rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr <nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(nntpUrl);
  mailnewsurl->SetMsgWindow(aMsgWindow);
  nsCOMPtr <nsIMsgMessageUrl> msgUrl = do_QueryInterface(nntpUrl);
  msgUrl->SetUri(urlString);
  mailnewsurl->SetSpec(urlString);
  nntpUrl->SetNewsAction(action);
  
  if (originalMessageUri) {
    // we'll use this later in nsNNTPProtocol::ParseURL()
    rv = msgUrl->SetOriginalSpec(originalMessageUri);
    NS_ENSURE_SUCCESS(rv,rv);
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
	
	nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

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

    // by default, news accounts should be composing in plain text
    rv = identity->SetComposeHtml(PR_FALSE);
    NS_ENSURE_SUCCESS(rv,rv);

	// the identity isn't filled in, so it is not valid.
	rv = (*server)->SetValid(PR_FALSE);
	if (NS_FAILED(rv)) return rv;

	// hook them together
	rv = account->SetIncomingServer(*server);
	if (NS_FAILED(rv)) return rv;
	rv = account->AddIdentity(identity);
	if (NS_FAILED(rv)) return rv;

	// Now save the new acct info to pref file.
	rv = accountManager->SaveAccountInfo();
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

nsresult
nsNntpService::GetProtocolForUri(nsIURI *aUri, nsIMsgWindow *aMsgWindow, nsINNTPProtocol **aProtocol)
{
  nsXPIDLCString hostName;
  nsXPIDLCString userName;
  nsXPIDLCString scheme;
  nsXPIDLCString path;
  PRInt32 port = 0;
  nsresult rv;
  
  rv = aUri->GetHost(getter_Copies(hostName));
  rv = aUri->GetPreHost(getter_Copies(userName));
  rv = aUri->GetScheme(getter_Copies(scheme));
  rv = aUri->GetPort(&port);
  rv = aUri->GetPath(getter_Copies(path));

  nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

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

  // news:group becomes news://group, so we have three types of urls:
  // news://group       (autosubscribing without a host)
  // news://host/group  (autosubscribing with a host)
  // news://host        (updating the unread message counts on a server)
  //
  // first, check if hostName is really a server or a group
  // by looking for a server with hostName
  //
  // xxx todo what if we have two servers on the same host, but different ports?
  // or no port, but isSecure (snews:// vs news://) is different?
  rv = accountManager->FindServer((const char *)userName,
                                (const char *)hostName,
                                "nntp",
                                getter_AddRefs(server));

  // if we didn't find the server, and path was "/", this is a news://group url
  if (!server && !(nsCRT::strcmp("/",(const char *)path))) {
    // the uri was news://group and we want to turn that into news://host/group
    // step 1, set the path to be the hostName;
    rv = aUri->SetPath((const char *)hostName);
    NS_ENSURE_SUCCESS(rv,rv);

    // until we support default news servers, use the first nntp server we find
    rv = accountManager->FindServer("","","nntp", getter_AddRefs(server));
    if (NS_FAILED(rv) || !server) {
        // step 2, set the uri's hostName and the local variable hostName
        // to be "news"
        rv = aUri->SetHost("news");
        NS_ENSURE_SUCCESS(rv,rv);

        rv = aUri->GetHost(getter_Copies(hostName));
        NS_ENSURE_SUCCESS(rv,rv);
    }
    else {
        // step 2, set the uri's hostName and the local variable hostName
        // to be the host name of the server we found
        rv = server->GetHostName(getter_Copies(hostName));
        NS_ENSURE_SUCCESS(rv,rv);
    
        rv = aUri->SetHost((const char *)hostName);
        NS_ENSURE_SUCCESS(rv,rv);
    }
  }

  if (NS_FAILED(rv) || !server) {
	  PRBool isSecure = PR_FALSE;
	  if (nsCRT::strcasecmp("snews",(const char *)scheme) == 0) {
		  isSecure = PR_TRUE;
          if ((port == 0) || (port == -1)) {
              port = SECURE_NEWS_PORT;
          }
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

PRBool nsNntpService::WeAreOffline()
{
	nsresult rv = NS_OK;
  PRBool offline = PR_FALSE;

  nsCOMPtr<nsIIOService> netService(do_GetService(kIOServiceCID, &rv));
  if (NS_SUCCEEDED(rv) && netService)
  {
    netService->GetOffline(&offline);
  }
  return offline;
}

nsresult 
nsNntpService::RunNewsUrl(nsIURI * aUri, nsIMsgWindow *aMsgWindow, nsISupports * aConsumer)
{
  nsresult rv;

  if (WeAreOffline())
    return NS_MSG_ERROR_OFFLINE;

  // almost there...now create a nntp protocol instance to run the url in...
  nsCOMPtr <nsINNTPProtocol> nntpProtocol;
  rv = GetProtocolForUri(aUri, aMsgWindow, getter_AddRefs(nntpProtocol));

  if (NS_SUCCEEDED(rv))
    rv = nntpProtocol->Initialize(aUri, aMsgWindow);
  if (NS_FAILED(rv)) return rv;
  
  rv = nntpProtocol->LoadNewsUrl(aUri, aConsumer);
  return rv;
}

NS_IMETHODIMP nsNntpService::GetNewNews(nsINntpIncomingServer *nntpServer, const char *uri, PRBool aGetOld, nsIUrlListener * aUrlListener, nsIMsgWindow *aMsgWindow, nsIURI **_retval)
{
  if (!uri) return NS_ERROR_NULL_POINTER;

  NS_LOCK_INSTANCE();
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIMsgIncomingServer> server;
  server = do_QueryInterface(nntpServer);
 
  /* double check that it is a "news:/" url */
  if (nsCRT::strncmp(uri, kNewsRootURI, kNewsRootURILen) == 0) {
    nsCOMPtr<nsIURI> aUrl;
    rv = ConstructNntpUrl(uri, aUrlListener, aMsgWindow, nsnull, nsINntpUrl::ActionGetNewNews, getter_AddRefs(aUrl));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsINntpUrl> nntpUrl = do_QueryInterface(aUrl);
    if (nntpUrl) {
      rv = nntpUrl->SetGetOldMessages(aGetOld);
      if (NS_FAILED(rv)) return rv;
    }
    
    nsCOMPtr<nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(aUrl);
    if (mailNewsUrl) {
      mailNewsUrl->SetUpdatingFolder(PR_TRUE);
    }

    rv = RunNewsUrl(aUrl, aMsgWindow, nsnull);  
	
    if (_retval) {
      *_retval = aUrl;
      NS_IF_ADDREF(*_retval);
    }
  }
  else {
    NS_ASSERTION(0,"not a news:/ url");
    rv = NS_ERROR_FAILURE;
  }
  
      
  NS_UNLOCK_INSTANCE();
  return rv;
}

NS_IMETHODIMP 
nsNntpService::CancelMessage(const char *cancelURL, const char *messageURI, nsISupports * aConsumer, nsIUrlListener * aUrlListener, nsIMsgWindow *aMsgWindow, nsIURI ** aURL)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(cancelURL);
  NS_ENSURE_ARG_POINTER(messageURI);

  nsCOMPtr<nsIURI> url;
  // the url should be "news://host/message-id?cancel"
  rv = ConstructNntpUrl(cancelURL, aUrlListener,  aMsgWindow, messageURI, nsINntpUrl::ActionCancelArticle, getter_AddRefs(url));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = RunNewsUrl(url, aMsgWindow, aConsumer);  
  NS_ENSURE_SUCCESS(rv,rv);

  if (aURL) {
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

NS_IMETHODIMP nsNntpService::GetDefaultDoBiff(PRBool *aDoBiff)
{
    NS_ENSURE_ARG_POINTER(aDoBiff);
    // by default, don't do biff for NNTP servers
    *aDoBiff = PR_FALSE;    
    return NS_OK;
}

NS_IMETHODIMP nsNntpService::GetDefaultPort(PRInt32 *aDefaultPort)
{
    NS_ENSURE_ARG_POINTER(aDefaultPort);
    *aDefaultPort = NEWS_PORT;
	return NS_OK;
}

NS_IMETHODIMP nsNntpService::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    *_retval = PR_TRUE; // allow news on any port
    return NS_OK;
}

NS_IMETHODIMP
nsNntpService::GetDefaultServerPort(PRBool isSecure, PRInt32 *aDefaultPort)
{
    nsresult rv = NS_OK;

    // Return Secure NNTP Port if secure option chosen i.e., if isSecure is TRUE
    if (isSecure)
        *aDefaultPort = SECURE_NEWS_PORT;
    else
        rv = GetDefaultPort(aDefaultPort);
 
    return rv;
}

NS_IMETHODIMP nsNntpService::GetProtocolFlags(PRUint32 *aUritype)
{
    NS_ENSURE_ARG_POINTER(aUritype);
    *aUritype = URI_NORELATIVE;
    return NS_OK;
}

NS_IMETHODIMP nsNntpService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
	nsresult rv = NS_OK;

    nsCOMPtr <nsINntpUrl> nntpUrl = do_CreateInstance(NS_NNTPURL_CONTRACTID,&rv);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!nntpUrl) return NS_ERROR_FAILURE;

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
    nsCOMPtr<nsIPref> prefs(do_GetService(kCPrefServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = prefs->SetFilePref(PREF_MAIL_ROOT_NNTP, aPath, PR_FALSE /* set default */);
    return rv;
}

NS_IMETHODIMP
nsNntpService::GetDefaultLocalPath(nsIFileSpec ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    
    nsresult rv;
    nsCOMPtr<nsIPref> prefs(do_GetService(kCPrefServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;
    
    PRBool havePref = PR_FALSE;
    nsCOMPtr<nsIFile> localFile;
    nsCOMPtr<nsILocalFile> prefLocal;
    rv = prefs->GetFileXPref(PREF_MAIL_ROOT_NNTP, getter_AddRefs(prefLocal));
    if (NS_SUCCEEDED(rv)) {
        localFile = prefLocal;
        havePref = PR_TRUE;
    }
    if (!localFile) {
        rv = NS_GetSpecialDirectory(NS_APP_NEWS_50_DIR, getter_AddRefs(localFile));
        if (NS_FAILED(rv)) return rv;
        havePref = PR_FALSE;
    }
        
    PRBool exists;
    rv = localFile->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists) {
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
        if (NS_FAILED(rv)) return rv;
    }
    
    // Make the resulting nsIFileSpec
    // TODO: Convert arg to nsILocalFile and avoid this
    nsXPIDLCString pathBuf;
    rv = localFile->GetPath(getter_Copies(pathBuf));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIFileSpec> outSpec;
    rv = NS_NewFileSpec(getter_AddRefs(outSpec));
    if (NS_FAILED(rv)) return rv;
    outSpec->SetNativePath(pathBuf);
    
    if (!havePref || !exists)
        rv = SetDefaultLocalPath(outSpec);
        
    *aResult = outSpec;
    NS_IF_ADDREF(*aResult);
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
nsNntpService::GetCanLoginAtStartUp(PRBool *aCanLoginAtStartUp)
{
        NS_ENSURE_ARG_POINTER(aCanLoginAtStartUp);
        *aCanLoginAtStartUp = PR_FALSE;
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
nsNntpService::GetCanGetMessages(PRBool *aCanGetMessages)
{
    NS_ENSURE_ARG_POINTER(aCanGetMessages);
    *aCanGetMessages = PR_FALSE;
    return NS_OK;
}  

NS_IMETHODIMP
nsNntpService::GetShowComposeMsgLink(PRBool *showComposeMsgLink)
{
    NS_ENSURE_ARG_POINTER(showComposeMsgLink);
    *showComposeMsgLink = PR_FALSE;
    return NS_OK;
}  

NS_IMETHODIMP
nsNntpService::GetNeedToBuildSpecialFolderURIs(PRBool *needToBuildSpecialFolderURIs)
{
    NS_ENSURE_ARG_POINTER(needToBuildSpecialFolderURIs);
    *needToBuildSpecialFolderURIs = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsNntpService::GetSpecialFoldersDeletionAllowed(PRBool *specialFoldersDeletionAllowed)
{
    NS_ENSURE_ARG_POINTER(specialFoldersDeletionAllowed);
    *specialFoldersDeletionAllowed = PR_FALSE;
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
  NS_ENSURE_ARG(aSearchUri);
    
  nsresult rv;

  nsXPIDLCString folderUri;
  rv = aMsgFolder->GetURI(getter_Copies(folderUri));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCAutoString searchUrl((const char *)folderUri);
  searchUrl += aSearchUri;
  nsCOMPtr <nsIUrlListener> urlListener = do_QueryInterface(aSearchSession);

  nsCOMPtr<nsIURI> url;
  rv = ConstructNntpUrl(searchUrl.get(), urlListener, aMsgWindow, nsnull, nsINntpUrl::ActionSearch, getter_AddRefs(url));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(url));
  if (msgurl)
    msgurl->SetSearchSession(aSearchSession);

  // run the url to update the counts
  rv = RunNewsUrl(url, nsnull, nsnull);  
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}


NS_IMETHODIMP
nsNntpService::UpdateCounts(nsINntpIncomingServer *aNntpServer, nsIMsgWindow *aMsgWindow)
{
	nsresult rv;
    NS_ENSURE_ARG_POINTER(aNntpServer);

	nsCOMPtr<nsIURI> url;
	nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aNntpServer, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!server) return NS_ERROR_FAILURE;

	nsXPIDLCString serverUri;
	rv = server->GetServerURI(getter_Copies(serverUri));
	if (NS_FAILED(rv)) return rv;

    rv = ConstructNntpUrl((const char *)serverUri, nsnull, aMsgWindow, nsnull, nsINntpUrl::ActionUpdateCounts, getter_AddRefs(url));
	if (NS_FAILED(rv)) return rv;

	// run the url to update the counts
    rv = RunNewsUrl(url, aMsgWindow, nsnull);

    // being offline is not an error.
    if (NS_SUCCEEDED(rv) || (rv == NS_MSG_ERROR_OFFLINE)) {
      return NS_OK;
    }
    return rv;
}

NS_IMETHODIMP 
nsNntpService::GetListOfGroupsOnServer(nsINntpIncomingServer *aNntpServer, nsIMsgWindow *aMsgWindow)
{
	nsresult rv;

    NS_ENSURE_ARG_POINTER(aNntpServer);

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

	nsCOMPtr<nsIURI> url;
    rv = ConstructNntpUrl((const char *)uriStr, listener, aMsgWindow, nsnull, nsINntpUrl::ActionListGroups, getter_AddRefs(url));
	if (NS_FAILED(rv)) return rv;

	// now run the url to add the rest of the groups
    rv = RunNewsUrl(url, aMsgWindow, nsnull);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

CMDLINEHANDLER3_IMPL(nsNntpService,"-news","general.startup.news","Start with news.",NS_NEWSSTARTUPHANDLER_CONTRACTID,"News Cmd Line Handler", PR_FALSE,"", PR_TRUE)

NS_IMETHODIMP nsNntpService::GetChromeUrlForTask(char **aChromeUrlForTask) 
{ 
    if (!aChromeUrlForTask) return NS_ERROR_FAILURE; 
	nsresult rv;
	nsCOMPtr<nsIPref> prefService(do_GetService(kCPrefServiceCID, &rv));
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
nsNntpService::HandleContent(const char * aContentType, const char * aCommand, nsISupports * aWindowContext, nsIRequest *request)
{
  nsresult rv = NS_OK;
  if (!request) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
  if (!aChannel) return NS_ERROR_NULL_POINTER;

  if (nsCRT::strcasecmp(aContentType, "x-application-newsgroup") == 0) {
      nsCOMPtr<nsIURI> uri;
      rv = aChannel->GetURI(getter_AddRefs(uri));
	  if (NS_FAILED(rv)) return rv;

      if (uri) { 	
		nsCOMPtr <nsIMessengerWindowService> messengerWindowService = do_GetService(NS_MESSENGERWINDOWSERVICE_CONTRACTID,&rv);
		if (NS_FAILED(rv)) return rv;

		rv = messengerWindowService->OpenMessengerWindowWithUri(uri);
		if (NS_FAILED(rv)) return rv;
	  }
  }

  return rv;
}

NS_IMETHODIMP
nsNntpService::MessageURIToMsgHdr(const char *uri, nsIMsgDBHdr **_retval)
{
  NS_ENSURE_ARG_POINTER(uri);
  NS_ENSURE_ARG_POINTER(_retval);
  nsresult rv = NS_OK;

  nsCOMPtr <nsIMsgFolder> folder;
  nsMsgKey msgKey;

  rv = DecomposeNewsMessageURI(uri, getter_AddRefs(folder), &msgKey);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = folder->GetMessageHeader(msgKey, _retval);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

NS_IMETHODIMP
nsNntpService::DownloadNewsgroupsForOffline(nsIMsgWindow *aMsgWindow, nsIUrlListener *aListener)
{
  nsresult rv = NS_OK;
  nsMsgDownloadAllNewsgroups *newsgroupDownloader = new nsMsgDownloadAllNewsgroups(aMsgWindow, aListener);
  if (newsgroupDownloader)
    rv = newsgroupDownloader->ProcessNextGroup();
  else
    rv = NS_ERROR_OUT_OF_MEMORY;
  return rv;
}

NS_IMETHODIMP nsNntpService::GetCacheSession(nsICacheSession **result)
{
  nsresult rv = NS_OK;
  if (!mCacheSession)
  {
    nsCOMPtr<nsICacheService> serv = do_GetService(kCacheServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = serv->CreateSession("NNTP-memory-only", nsICache::STORE_IN_MEMORY, nsICache::STREAM_BASED, getter_AddRefs(mCacheSession));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mCacheSession->SetDoomEntriesIfExpired(PR_FALSE);
  }

  *result = mCacheSession;
  NS_IF_ADDREF(*result);
  return rv;
}
