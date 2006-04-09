/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <scott@scott-macgregor.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMsgContentPolicy.h"
#include "nsIServiceManager.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsIMsgHeaderParser.h"
#include "nsIAbMDBDirectory.h"

#include "nsIMsgMailNewsUrl.h"
#include "nsIMsgWindow.h"
#include "nsIMimeMiscStatus.h"
#include "nsIMsgMessageService.h"
#include "nsIMsgIncomingServer.h"
#include "nsIRssIncomingServer.h"
#include "nsIMsgHdr.h"
#include "nsMsgUtils.h"

// needed by the content load policy manager
#include "nsIExternalProtocolService.h"
#include "nsCExternalHandlerService.h"

// needed for the cookie policy manager
#include "nsNetUtil.h"
#include "nsICookie2.h"
#include "nsICookieManager2.h"

// needed for mailnews content load policy manager
#include "nsIDocShell.h"
#include "nsContentPolicyUtils.h"

static const char kBlockRemoteImages[] = "mailnews.message_display.disable_remote_image";
static const char kRemoteImagesUseWhiteList[] = "mailnews.message_display.disable_remote_images.useWhitelist";
static const char kRemoteImagesWhiteListURI[] = "mailnews.message_display.disable_remote_images.whiteListAbURI";
static const char kAllowPlugins[] = "mailnews.message_display.allow.plugins";
static const char kTrustedDomains[] =  "mail.trusteddomains";

// Per message headder flags to keep track of whether the user is allowing remote
// content for a particular message. 
// if you change or add more values to these constants, be sure to modify
// the corresponding definitions in mailWindowOverlay.js
#define kNoRemoteContentPolicy 0
#define kBlockRemoteContent 1
#define kAllowRemoteContent 2

NS_IMPL_ISUPPORTS3(nsMsgContentPolicy, 
                   nsIContentPolicy,
                   nsIObserver,
                   nsISupportsWeakReference)

nsMsgContentPolicy::nsMsgContentPolicy()
{
  mAllowPlugins = PR_FALSE;
  mUseRemoteImageWhiteList = PR_TRUE;
  mBlockRemoteImages = PR_TRUE;
}

nsMsgContentPolicy::~nsMsgContentPolicy()
{
  // hey, we are going away...clean up after ourself....unregister our observer
  nsresult rv;
  nsCOMPtr<nsIPrefBranch2> prefInternal = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    prefInternal->RemoveObserver(kBlockRemoteImages, this);
    prefInternal->RemoveObserver(kRemoteImagesUseWhiteList, this);
    prefInternal->RemoveObserver(kRemoteImagesWhiteListURI, this);
    prefInternal->RemoveObserver(kAllowPlugins, this);
  }
}

nsresult nsMsgContentPolicy::Init()
{
  nsresult rv;

  // register ourself as an observer on the mail preference to block remote images
  nsCOMPtr<nsIPrefBranch2> prefInternal = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  prefInternal->AddObserver(kBlockRemoteImages, this, PR_TRUE);
  prefInternal->AddObserver(kRemoteImagesUseWhiteList, this, PR_TRUE);
  prefInternal->AddObserver(kRemoteImagesWhiteListURI, this, PR_TRUE);
  prefInternal->AddObserver(kAllowPlugins, this, PR_TRUE);

  prefInternal->GetBoolPref(kAllowPlugins, &mAllowPlugins);
  prefInternal->GetBoolPref(kRemoteImagesUseWhiteList, &mUseRemoteImageWhiteList);
  prefInternal->GetCharPref(kRemoteImagesWhiteListURI, getter_Copies(mRemoteImageWhiteListURI));
  prefInternal->GetCharPref(kTrustedDomains, getter_Copies(mTrustedMailDomains));
  return prefInternal->GetBoolPref(kBlockRemoteImages, &mBlockRemoteImages);
}

nsresult nsMsgContentPolicy::IsSenderInWhiteList(nsIMsgDBHdr * aMsgHdr, PRBool * aWhiteListed)
{
  *aWhiteListed = PR_FALSE;
  NS_ENSURE_ARG_POINTER(aMsgHdr); 
  nsresult rv = NS_OK;

  if (mBlockRemoteImages && mUseRemoteImageWhiteList && !mRemoteImageWhiteListURI.IsEmpty())
  {
    nsXPIDLCString author;
    rv = aMsgHdr->GetAuthor(getter_Copies(author));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr <nsIRDFResource> resource;
    rv = rdfService->GetResource(mRemoteImageWhiteListURI, getter_AddRefs(resource));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr <nsIAbMDBDirectory> addressBook = do_QueryInterface(resource, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgHeaderParser> headerParser = do_GetService("@mozilla.org/messenger/headerparser;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLCString emailAddress; 
    rv = headerParser->ExtractHeaderAddressMailboxes(nsnull, author, getter_Copies(emailAddress));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = addressBook->HasCardForEmailAddress(emailAddress, aWhiteListed);
  }
  
  return rv;
}

nsresult nsMsgContentPolicy::IsTrustedDomain(nsIURI * aContentLocation, PRBool * aTrustedDomain)
{
  *aTrustedDomain = PR_FALSE;
  NS_ENSURE_ARG_POINTER(aContentLocation); 
  nsresult rv = NS_OK;

  // get the host name of the server hosting the remote image
  nsCAutoString host;
  aContentLocation->GetHost(host);

  if (!mTrustedMailDomains.IsEmpty()) 
  {
    const char *domain, *domainEnd, *end;
    PRUint32 hostLen, domainLen;

    domain = mTrustedMailDomains.BeginReading();
    domainEnd = mTrustedMailDomains.EndReading(); 
    nsACString::const_iterator hostStart;

    host.BeginReading(hostStart);
    hostLen = host.Length();

    do {
      // skip any whitespace
      while (*domain == ' ' || *domain == '\t')
        ++domain;
      
      // find end of this domain in the string
      end = strchr(domain, ',');
      if (!end)
        end = domainEnd;

      // to see if the hostname is in the domain, check if the domain
      // matches the end of the hostname.
      domainLen = end - domain;
      if (domainLen && hostLen >= domainLen) {
        const char *hostTail = hostStart.get() + hostLen - domainLen;
        if (PL_strncasecmp(domain, hostTail, domainLen) == 0) 
        {
          // now, make sure either that the hostname is a direct match or
          // that the hostname begins with a dot.
          if (hostLen == domainLen || *hostTail == '.' || *(hostTail - 1) == '.')
          {
            *aTrustedDomain = PR_TRUE;
            break;
          }
        }
      }
      
      domain = end + 1;
    } while (*end);
  }

  return rv;
}


NS_IMETHODIMP
nsMsgContentPolicy::ShouldLoad(PRUint32          aContentType,
                               nsIURI           *aContentLocation,
                               nsIURI           *aRequestingLocation,
                               nsISupports      *aRequestingContext,
                               const nsACString &aMimeGuess,
                               nsISupports      *aExtra,
                               PRInt16          *aDecision)
{
  nsresult rv = NS_OK;
  *aDecision = nsIContentPolicy::ACCEPT;

  NS_ENSURE_ARG_POINTER(aContentLocation);

  // NOTE: Not using NS_ENSURE_ARG_POINTER because this is a legitimate case
  // that can happen, and it shouldn't print a warning message.
  if (!aRequestingLocation)
    return NS_ERROR_INVALID_POINTER;

#ifndef MOZ_THUNDERBIRD
  // Go find out if we are dealing with mailnews. Anything else
  // isn't our concern and we accept content.
  nsIDocShell *shell = NS_CP_GetDocShellFromContext(aRequestingContext);
  nsCOMPtr<nsIDocShellTreeItem> docshellTreeItem(do_QueryInterface(shell));
  if (!docshellTreeItem)
    return NS_OK;

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  // we want the app docshell, so don't use GetSameTypeRootTreeItem
  rv = docshellTreeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  if (NS_FAILED(rv))
    return NS_OK;

  nsCOMPtr<nsIDocShell> docshell(do_QueryInterface(rootItem));
  if (!docshell)
    return NS_OK;

  PRUint32 appType;
  rv = docshell->GetAppType(&appType);
  // We only want to deal with mailnews
  if (NS_FAILED(rv) || appType != nsIDocShell::APP_TYPE_MAIL)
    return NS_OK;
#endif

  if (aContentType == nsIContentPolicy::TYPE_OBJECT)
  {
    // only allow the plugin to load if the allow plugins pref has been set
    if (!mAllowPlugins)
      *aDecision = nsIContentPolicy::REJECT_TYPE;
    return NS_OK;
  }

  // if aRequestingLocation is chrome, about or resource, allow aContentLocation to load
  PRBool isChrome = PR_FALSE;
  PRBool isRes = PR_FALSE;
  PRBool isAbout = PR_FALSE;

  rv = aRequestingLocation->SchemeIs("chrome", &isChrome);
  rv |= aRequestingLocation->SchemeIs("resource", &isRes);
  rv |= aRequestingLocation->SchemeIs("about", &isAbout);

  if (NS_SUCCEEDED(rv) && (isChrome || isRes || isAbout))
    return rv;

  // Now default to reject so when NS_ENSURE_SUCCESS errors content is rejected
  *aDecision = nsIContentPolicy::REJECT_REQUEST;

  // if aContentLocation is a protocol we handle (imap, pop3, mailbox, etc)
  // or is a chrome url, then allow the load
  nsCAutoString contentScheme;
  PRBool isExposedProtocol = PR_FALSE;
  rv = aContentLocation->GetScheme(contentScheme);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_THUNDERBIRD
  nsCOMPtr<nsIExternalProtocolService> extProtService = do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
  rv = extProtService->IsExposedProtocol(contentScheme.get(), &isExposedProtocol);
  NS_ENSURE_SUCCESS(rv, rv);
#else
  if (contentScheme.LowerCaseEqualsLiteral("mailto") ||
      contentScheme.LowerCaseEqualsLiteral("news") ||
      contentScheme.LowerCaseEqualsLiteral("snews") ||
      contentScheme.LowerCaseEqualsLiteral("nntp") ||
      contentScheme.LowerCaseEqualsLiteral("imap") ||
      contentScheme.LowerCaseEqualsLiteral("addbook") ||
      contentScheme.LowerCaseEqualsLiteral("pop") ||
      contentScheme.LowerCaseEqualsLiteral("mailbox") ||
      contentScheme.LowerCaseEqualsLiteral("about"))
    isExposedProtocol = PR_TRUE;
#endif
    
  rv = aContentLocation->SchemeIs("chrome", &isChrome);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isExposedProtocol || isChrome)
  {
    *aDecision = nsIContentPolicy::ACCEPT;
    return rv;
  }

  // for unexposed protocols, we never try to load any of them with the exception of http and https.
  // this means we never even try to load urls that we don't handle ourselves like ftp and gopher.
  PRBool isHttp = PR_FALSE;
  PRBool isHttps = PR_FALSE;

  rv = aContentLocation->SchemeIs("http", &isHttp);
  rv |= aContentLocation->SchemeIs("https", &isHttps);

  if (!NS_SUCCEEDED(rv) || (!isHttp && !isHttps))
    return rv;

  // Look into http and https more closely to determine if the load should be allowed

  // If we do not block remote content then return with an accept content here
  if (!mBlockRemoteImages)
  {
    *aDecision = nsIContentPolicy::ACCEPT;
    return NS_OK;
  }

  // now do some more detective work to better refine our decision...
  // (1) examine the msg hdr value for the remote content policy on this particular message to
  //     see if this particular message has special rights to bypass the remote content check
  // (2) special case RSS urls, always allow them to load remote images since the user explicitly
  //     subscribed to the feed.
  // (3) Check the personal address book and use it as a white list for senders
  //     who are allowed to send us remote images

  // get the msg hdr for the message URI we are actually loading
  NS_ENSURE_TRUE(aRequestingLocation, NS_OK);
  nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(aRequestingLocation, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString resourceURI;
  msgUrl->GetUri(getter_Copies(resourceURI));

  // get the msg service for this URI
  nsCOMPtr<nsIMsgMessageService> msgService;
  rv = GetMessageServiceFromURI(resourceURI.get(), getter_AddRefs(msgService));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  rv = msgService->MessageURIToMsgHdr(resourceURI, getter_AddRefs(msgHdr));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aRequestingLocation, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Case #1, check the db hdr for the remote content policy on this particular message
  PRUint32 remoteContentPolicy = kNoRemoteContentPolicy;
  msgHdr->GetUint32Property("remoteContentPolicy", &remoteContentPolicy);

  // Case #2, check if the message is in an RSS folder
  PRBool isRSS = PR_FALSE;
  IsRSSArticle(aRequestingLocation, &isRSS);

  // Case #3, author is in our white list..
  PRBool authorInWhiteList = PR_FALSE;
  IsSenderInWhiteList(msgHdr, &authorInWhiteList);

  // Case #4, the domain for the remote image is in our white list
  PRBool trustedDomain = PR_FALSE;
  IsTrustedDomain(aContentLocation, &trustedDomain);

  // Case #1 and #2: special case RSS. Allow urls that are RSS feeds to show remote image (Bug #250246)
  // Honor the message specific remote content policy
  if (isRSS || remoteContentPolicy == kAllowRemoteContent || authorInWhiteList || trustedDomain)
  {
    *aDecision = nsIContentPolicy::ACCEPT;
    return rv;
  }

  if (!remoteContentPolicy) // kNoRemoteContentPolicy means we have never set a value on the message
    msgHdr->SetUint32Property("remoteContentPolicy", kBlockRemoteContent);

  // now we need to call out the msg sink informing it that this message has remote content
  nsCOMPtr<nsIMsgWindow> msgWindow;
  rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow)); 
  NS_ENSURE_TRUE(msgWindow, NS_OK); // it's not an error for the msg window to be null

  nsCOMPtr<nsIMsgHeaderSink> msgHdrSink;
  rv = msgWindow->GetMsgHeaderSink(getter_AddRefs(msgHdrSink));
  NS_ENSURE_TRUE(msgHdrSink, rv);
  msgHdrSink->OnMsgHasRemoteContent(msgHdr); // notify the UI to show the remote content hdr bar so the user can overide

  return rv;
}

NS_IMETHODIMP
nsMsgContentPolicy::ShouldProcess(PRUint32          aContentType,
                                  nsIURI           *aContentLocation,
                                  nsIURI           *aRequestingLocation,
                                  nsISupports      *aRequestingContext,
                                  const nsACString &aMimeGuess,
                                  nsISupports      *aExtra,
                                  PRInt16          *aDecision)
{
  *aDecision = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

NS_IMETHODIMP nsMsgContentPolicy::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  if (!nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) 
  {
    NS_LossyConvertUTF16toASCII pref(aData);

    nsresult rv;

    nsCOMPtr<nsIPrefBranch2> prefBranchInt = do_QueryInterface(aSubject, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (pref.Equals(kBlockRemoteImages))
      prefBranchInt->GetBoolPref(kBlockRemoteImages, &mBlockRemoteImages);
    else if (pref.Equals(kRemoteImagesUseWhiteList))
      prefBranchInt->GetBoolPref(kRemoteImagesUseWhiteList, &mUseRemoteImageWhiteList);
    else if (pref.Equals(kRemoteImagesWhiteListURI))
      prefBranchInt->GetCharPref(kRemoteImagesWhiteListURI, getter_Copies(mRemoteImageWhiteListURI));
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsMsgCookiePolicy, nsICookiePermission)


NS_IMETHODIMP nsMsgCookiePolicy::SetAccess(nsIURI         *aURI,
                                           nsCookieAccess  aAccess)
{
  // we don't support cookie access lists for mail
  return NS_OK;
}

NS_IMETHODIMP nsMsgCookiePolicy::CanAccess(nsIURI         *aURI,
                                            nsIURI         *aFirstURI,
                                            nsIChannel     *aChannel,
                                            nsCookieAccess *aResult)
{
  // by default we deny all cookies in mail
  *aResult = ACCESS_DENY;
  NS_ENSURE_ARG_POINTER(aChannel);
  
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem;
  NS_QueryNotificationCallbacks(aChannel, docShellTreeItem);

  NS_ENSURE_TRUE(docShellTreeItem, NS_OK);
  PRInt32 itemType;
  docShellTreeItem->GetItemType(&itemType);

  // allow chome docshells to set cookies
  if (itemType == nsIDocShellTreeItem::typeChrome)
    *aResult = ACCESS_DEFAULT;
  else // allow RSS articles in content to access cookies
  {
  NS_ENSURE_TRUE(aFirstURI, NS_OK);  
  PRBool isRSS = PR_FALSE;
  IsRSSArticle(aFirstURI, &isRSS);
  if (isRSS)
    *aResult = ACCESS_DEFAULT;
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgCookiePolicy::CanSetCookie(nsIURI     *aURI,
                                              nsIChannel *aChannel,
                                              nsICookie2 *aCookie,
                                              PRBool     *aIsSession,
                                              PRInt64    *aExpiry,
                                              PRBool     *aResult)
{
  *aResult = PR_TRUE;
  return NS_OK;
}
