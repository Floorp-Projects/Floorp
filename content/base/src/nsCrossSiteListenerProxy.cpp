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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc> (Original Author)
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

#include "nsCrossSiteListenerProxy.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsICharsetAlias.h"
#include "nsMimeTypes.h"
#include "nsIStreamConverterService.h"
#include "nsStringStream.h"
#include "nsParserUtils.h"
#include "nsGkAtoms.h"
#include "nsWhitespaceTokenizer.h"
#include "nsIChannelEventSink.h"
#include "nsCommaSeparatedTokenizer.h"
#include "prclist.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "prtime.h"

// =========================================================================
// Support classes

// Class implementing the preflight request cache
#define ACCESS_CONTROL_CACHE_SIZE 100

class nsACPreflightCache
{
public:
  static void Shutdown()
  {
    PR_INIT_CLIST(&mList);
    delete mTable;
    mTable = nsnull;
  }

  struct TokenTime
  {
    nsCString token;
    PRTime expirationTime;
  };

  class CacheEntry : public PRCList
  {
  public:
    CacheEntry(nsCString& aKey)
      : mKey(aKey)
    {
      MOZ_COUNT_CTOR(nsACPreflightCache::CacheEntry);
    }
    
    ~CacheEntry()
    {
      MOZ_COUNT_DTOR(nsACPreflightCache::CacheEntry);
    }

    void PurgeExpired(PRTime now);
    PRBool CheckRequest(const nsCString& aMethod,
                        const nsTArray<nsCString>& aCustomHeaders);

    nsCString mKey;
    nsTArray<TokenTime> mMethods;
    nsTArray<TokenTime> mHeaders;
  };


  static CacheEntry* GetEntry(nsIURI* aURI, nsIPrincipal* aPrincipal,
                              PRBool aWithCredentials, PRBool aCreate);

private:
  PR_STATIC_CALLBACK(PLDHashOperator)
    RemoveExpiredEntries(const nsACString& aKey, nsAutoPtr<CacheEntry>& aValue,
                         void* aUserData);

  static PRBool GetCacheKey(nsIURI* aURI, nsIPrincipal* aPrincipal,
                            PRBool aWithCredentials, nsACString& _retval);

  static nsClassHashtable<nsCStringHashKey, CacheEntry>* mTable;
  static PRCList mList;
};


// Class used as streamlistener and notification callback when
// doing the initial GET request for an access-control check

class nsACPreflightListener : public nsIStreamListener,
                              public nsIInterfaceRequestor,
                              public nsIChannelEventSink
{
public:
  nsACPreflightListener(nsIChannel* aOuterChannel,
                        nsIStreamListener* aOuterListener,
                        nsISupports* aOuterContext,
                        nsIPrincipal* aReferrerPrincipal,
                        PRBool aWithCredentials)
   : mOuterChannel(aOuterChannel), mOuterListener(aOuterListener),
     mOuterContext(aOuterContext), mReferrerPrincipal(aReferrerPrincipal),
     mWithCredentials(aWithCredentials)
  { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

private:
  void AddResultToCache(nsIRequest* aRequest);

  nsCOMPtr<nsIChannel> mOuterChannel;
  nsCOMPtr<nsIStreamListener> mOuterListener;
  nsCOMPtr<nsISupports> mOuterContext;
  nsCOMPtr<nsIPrincipal> mReferrerPrincipal;
  PRBool mWithCredentials;
};

// =========================================================================
// nsCrossSiteListenerProxy

NS_IMPL_ISUPPORTS4(nsCrossSiteListenerProxy, nsIStreamListener,
                   nsIRequestObserver, nsIChannelEventSink,
                   nsIInterfaceRequestor)

nsCrossSiteListenerProxy::nsCrossSiteListenerProxy(nsIStreamListener* aOuter,
                                                   nsIPrincipal* aRequestingPrincipal,
                                                   nsIChannel* aChannel,
                                                   PRBool aWithCredentials,
                                                   nsresult* aResult)
  : mOuterListener(aOuter),
    mRequestingPrincipal(aRequestingPrincipal),
    mWithCredentials(aWithCredentials),
    mRequestApproved(PR_FALSE),
    mHasBeenCrossSite(PR_FALSE),
    mIsPreflight(PR_FALSE)
{
  aChannel->GetNotificationCallbacks(getter_AddRefs(mOuterNotificationCallbacks));
  aChannel->SetNotificationCallbacks(this);

  *aResult = UpdateChannel(aChannel);
}


nsCrossSiteListenerProxy::nsCrossSiteListenerProxy(nsIStreamListener* aOuter,
                                                   nsIPrincipal* aRequestingPrincipal,
                                                   nsIChannel* aChannel,
                                                   PRBool aWithCredentials,
                                                   const nsCString& aPreflightMethod,
                                                   const nsTArray<nsCString>& aPreflightHeaders,
                                                   nsresult* aResult)
  : mOuterListener(aOuter),
    mRequestingPrincipal(aRequestingPrincipal),
    mWithCredentials(aWithCredentials),
    mRequestApproved(PR_FALSE),
    mHasBeenCrossSite(PR_FALSE),
    mIsPreflight(PR_TRUE),
    mPreflightMethod(aPreflightMethod),
    mPreflightHeaders(aPreflightHeaders)
{
  aChannel->GetNotificationCallbacks(getter_AddRefs(mOuterNotificationCallbacks));
  aChannel->SetNotificationCallbacks(this);

  *aResult = UpdateChannel(aChannel);
}

nsresult
nsCrossSiteListenerProxy::CheckPreflight(nsIHttpChannel* aRequestChannel,
                                         nsIStreamListener* aRequestListener,
                                         nsISupports* aRequestContext,
                                         nsIPrincipal* aRequestingPrincipal,
                                         PRBool aForcePreflight,
                                         nsTArray<nsCString>& aUnsafeHeaders,
                                         PRBool aWithCredentials,
                                         PRBool* aPreflighted,
                                         nsIChannel** aPreflightChannel,
                                         nsIStreamListener** aPreflightListener)
{
  *aPreflighted = PR_FALSE;

  nsCAutoString method;
  aRequestChannel->GetRequestMethod(method);

  if (!aUnsafeHeaders.IsEmpty() || aForcePreflight) {
    *aPreflighted = PR_TRUE;
  }
  else if (method.LowerCaseEqualsLiteral("post")) {
    nsCAutoString contentTypeHeader;
    aRequestChannel->GetRequestHeader(NS_LITERAL_CSTRING("Content-Type"),
                                      contentTypeHeader);

    nsCAutoString contentType, charset;
    NS_ParseContentType(contentTypeHeader, contentType, charset);
    if (!contentType.LowerCaseEqualsLiteral("text/plain")) {
      *aPreflighted = PR_TRUE;
    }
  }
  else if (!method.LowerCaseEqualsLiteral("get")) {
    *aPreflighted = PR_TRUE;
  }

  if (!*aPreflighted) {
    return NS_OK;
  }

  // If so, set up the preflight

  // Check to see if this initial OPTIONS request has already been cached
  // in our special Access Control Cache.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aRequestChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsACPreflightCache::CacheEntry* entry =
    nsACPreflightCache::GetEntry(uri, aRequestingPrincipal,
                                 aWithCredentials, PR_FALSE);

  if (entry && entry->CheckRequest(method, aUnsafeHeaders)) {
    return NS_OK;
  }

  // Either it wasn't cached or the cached result has expired. Build a
  // channel for the OPTIONS request.
  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = aRequestChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  NS_ENSURE_SUCCESS(rv, rv);

  nsLoadFlags loadFlags;
  rv = aRequestChannel->GetLoadFlags(&loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> preflightChannel;
  rv = NS_NewChannel(getter_AddRefs(preflightChannel), uri, nsnull,
                     loadGroup, nsnull, loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> preflightHttp = do_QueryInterface(preflightChannel);
  NS_ASSERTION(preflightHttp, "Failed to QI to nsIHttpChannel!");

  rv = preflightHttp->SetRequestMethod(NS_LITERAL_CSTRING("OPTIONS"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Add channel headers
  rv = preflightHttp->
    SetRequestHeader(NS_LITERAL_CSTRING("Access-Control-Request-Method"),
                     method, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aUnsafeHeaders.IsEmpty()) {
    nsCAutoString headers;
    for (PRUint32 i = 0; i < aUnsafeHeaders.Length(); ++i) {
      if (i != 0) {
        headers += ',';
      }
      headers += aUnsafeHeaders[i];
    }
    rv = preflightHttp->
      SetRequestHeader(NS_LITERAL_CSTRING("Access-Control-Request-Headers"),
                       headers, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set up listener
  nsCOMPtr<nsIStreamListener> preflightListener =
    new nsACPreflightListener(preflightChannel, aRequestListener,
                              aRequestContext, aRequestingPrincipal,
                              aWithCredentials);
  NS_ENSURE_TRUE(preflightListener, NS_ERROR_OUT_OF_MEMORY);

  preflightListener =
    new nsCrossSiteListenerProxy(preflightListener, aRequestingPrincipal,
                                 preflightChannel, aWithCredentials, method,
                                 aUnsafeHeaders, &rv);
  NS_ENSURE_TRUE(preflightListener, NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_SUCCESS(rv, rv);

  preflightChannel.swap(*aPreflightChannel);
  preflightListener.swap(*aPreflightListener);

  return NS_OK;
}

void
nsCrossSiteListenerProxy::ShutdownPreflightCache()
{
  nsACPreflightCache::Shutdown();
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::OnStartRequest(nsIRequest* aRequest,
                                         nsISupports* aContext)
{
  mRequestApproved = NS_SUCCEEDED(CheckRequestApproved(aRequest));
  if (!mRequestApproved) {
    aRequest->Cancel(NS_ERROR_DOM_BAD_URI);
    mOuterListener->OnStartRequest(aRequest, aContext);

    return NS_ERROR_DOM_BAD_URI;
  }

  return mOuterListener->OnStartRequest(aRequest, aContext);
}

PRBool
IsValidHTTPToken(const nsCSubstring& aToken)
{
  if (aToken.IsEmpty()) {
    return PR_FALSE;
  }

  nsCSubstring::const_char_iterator iter, end;

  aToken.BeginReading(iter);
  aToken.EndReading(end);

  while (iter != end) {
    if (*iter <= 32 ||
        *iter >= 127 ||
        *iter == '(' ||
        *iter == ')' ||
        *iter == '<' ||
        *iter == '>' ||
        *iter == '@' ||
        *iter == ',' ||
        *iter == ';' ||
        *iter == ':' ||
        *iter == '\\' ||
        *iter == '\"' ||
        *iter == '/' ||
        *iter == '[' ||
        *iter == ']' ||
        *iter == '?' ||
        *iter == '=' ||
        *iter == '{' ||
        *iter == '}') {
      return PR_FALSE;
    }
    ++iter;
  }

  return PR_TRUE;
}

nsresult
GetOrigin(nsIPrincipal* aPrincipal, nsCString& aOrigin)
{
  aOrigin.AssignLiteral("null");

  nsCString host;
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = uri->GetAsciiHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!host.IsEmpty()) {
    nsCString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    aOrigin = scheme + NS_LITERAL_CSTRING("://") + host;

    // If needed, append the port
    PRInt32 port;
    uri->GetPort(&port);
    if (port != -1) {
      PRInt32 defaultPort = NS_GetDefaultPort(scheme.get());
      if (port != defaultPort) {
        aOrigin.Append(":");
        aOrigin.AppendInt(port);
      }
    }
  }
  else {
    aOrigin.AssignLiteral("null");
  }

  return NS_OK;
}

nsresult
nsCrossSiteListenerProxy::CheckRequestApproved(nsIRequest* aRequest)
{
  // Check if this was actually a cross domain request
  if (!mHasBeenCrossSite) {
    return NS_OK;
  }

  // Check if the request failed
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(status, status);

  // Test that things worked on a HTTP level
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aRequest);
  NS_ENSURE_TRUE(http, NS_ERROR_DOM_BAD_URI);

  PRBool succeeded;
  rv = http->GetRequestSucceeded(&succeeded);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(succeeded, NS_ERROR_DOM_BAD_URI);

  // Check the Access-Control-Allow-Origin header
  nsCAutoString allowedOriginHeader;
  rv = http->GetResponseHeader(
    NS_LITERAL_CSTRING("Access-Control-Allow-Origin"), allowedOriginHeader);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mWithCredentials || !allowedOriginHeader.EqualsLiteral("*")) {
    nsCAutoString origin;
    rv = GetOrigin(mRequestingPrincipal, origin);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!allowedOriginHeader.Equals(origin)) {
      return NS_ERROR_DOM_BAD_URI;
    }
  }

  // Check Access-Control-Allow-Credentials header
  if (mWithCredentials) {
    nsCAutoString allowCredentialsHeader;
    rv = http->GetResponseHeader(
      NS_LITERAL_CSTRING("Access-Control-Allow-Credentials"), allowCredentialsHeader);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!allowCredentialsHeader.EqualsLiteral("true")) {
      return NS_ERROR_DOM_BAD_URI;
    }
  }

  if (mIsPreflight) {
    nsCAutoString headerVal;
    // The "Access-Control-Allow-Methods" header contains a comma separated
    // list of method names.
    http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Allow-Methods"),
                            headerVal);
    PRBool foundMethod = mPreflightMethod.EqualsLiteral("GET") ||
      mPreflightMethod.EqualsLiteral("POST");
    nsCCommaSeparatedTokenizer methodTokens(headerVal);
    while(methodTokens.hasMoreTokens()) {
      const nsDependentCSubstring& method = methodTokens.nextToken();
      if (method.IsEmpty()) {
        continue;
      }
      if (!IsValidHTTPToken(method)) {
        return NS_ERROR_DOM_BAD_URI;
      }
      foundMethod |= mPreflightMethod.Equals(method);
    }
    NS_ENSURE_TRUE(foundMethod, NS_ERROR_DOM_BAD_URI);

    // The "Access-Control-Allow-Headers" header contains a comma separated
    // list of header names.
    http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Allow-Headers"),
                            headerVal);
    nsTArray<nsCString> headers;
    nsCCommaSeparatedTokenizer headerTokens(headerVal);
    while(headerTokens.hasMoreTokens()) {
      const nsDependentCSubstring& header = headerTokens.nextToken();
      if (header.IsEmpty()) {
        continue;
      }
      if (!IsValidHTTPToken(header)) {
        return NS_ERROR_DOM_BAD_URI;
      }
      headers.AppendElement(header);
    }
    for (PRUint32 i = 0; i < mPreflightHeaders.Length(); ++i) {
      if (!headers.Contains(mPreflightHeaders[i],
                            nsCaseInsensitiveCStringArrayComparator())) {
        return NS_ERROR_DOM_BAD_URI;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::OnStopRequest(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsresult aStatusCode)
{
  return mOuterListener->OnStopRequest(aRequest, aContext, aStatusCode);
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::OnDataAvailable(nsIRequest* aRequest,
                                          nsISupports* aContext, 
                                          nsIInputStream* aInputStream,
                                          PRUint32 aOffset,
                                          PRUint32 aCount)
{
  if (!mRequestApproved) {
    return NS_ERROR_DOM_BAD_URI;
  }
  return mOuterListener->OnDataAvailable(aRequest, aContext, aInputStream,
                                         aOffset, aCount);
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::GetInterface(const nsIID & aIID, void **aResult)
{
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    *aResult = static_cast<nsIChannelEventSink*>(this);
    NS_ADDREF_THIS();

    return NS_OK;
  }

  return mOuterNotificationCallbacks ?
    mOuterNotificationCallbacks->GetInterface(aIID, aResult) :
    NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::OnChannelRedirect(nsIChannel *aOldChannel,
                                            nsIChannel *aNewChannel,
                                            PRUint32    aFlags)
{
  nsresult rv = CheckRequestApproved(aOldChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannelEventSink> outer =
    do_GetInterface(mOuterNotificationCallbacks);
  if (outer) {
    rv = outer->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return UpdateChannel(aNewChannel);
}

nsresult
nsCrossSiteListenerProxy::UpdateChannel(nsIChannel* aChannel)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check that the uri is ok to load
  rv = nsContentUtils::GetSecurityManager()->
    CheckLoadURIWithPrincipal(mRequestingPrincipal, uri,
                              nsIScriptSecurityManager::STANDARD);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mHasBeenCrossSite &&
      NS_SUCCEEDED(mRequestingPrincipal->CheckMayLoad(uri, PR_FALSE))) {
    return NS_OK;
  }

  // It's a cross site load
  mHasBeenCrossSite = PR_TRUE;

  nsCString userpass;
  uri->GetUserPass(userpass);
  NS_ENSURE_TRUE(userpass.IsEmpty(), NS_ERROR_DOM_BAD_URI);

  // Add the Origin header
  nsCAutoString origin;
  rv = GetOrigin(mRequestingPrincipal, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aChannel);
  NS_ENSURE_TRUE(http, NS_ERROR_FAILURE);

  rv = http->SetRequestHeader(NS_LITERAL_CSTRING("Origin"), origin, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make cookie-less if needed
  if (mIsPreflight || !mWithCredentials) {
    nsLoadFlags flags;
    rv = http->GetLoadFlags(&flags);
    NS_ENSURE_SUCCESS(rv, rv);

    flags |= nsIRequest::LOAD_ANONYMOUS;
    rv = http->SetLoadFlags(flags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// =========================================================================
// nsACPreflightListener

NS_IMPL_ISUPPORTS4(nsACPreflightListener, nsIStreamListener, nsIRequestObserver,
                   nsIInterfaceRequestor, nsIChannelEventSink)

void
nsACPreflightListener::AddResultToCache(nsIRequest *aRequest)
{
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aRequest);
  NS_ASSERTION(http, "Request was not http");

  // The "Access-Control-Max-Age" header should return an age in seconds.
  nsCAutoString headerVal;
  http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Max-Age"),
                          headerVal);
  if (headerVal.IsEmpty()) {
    return;
  }

  // Sanitize the string. We only allow 'delta-seconds' as specified by
  // http://dev.w3.org/2006/waf/access-control (digits 0-9 with no leading or
  // trailing non-whitespace characters).
  PRUint32 age = 0;
  nsCSubstring::const_char_iterator iter, end;
  headerVal.BeginReading(iter);
  headerVal.EndReading(end);
  while (iter != end) {
    if (*iter < '0' || *iter > '9') {
      return;
    }
    age = age * 10 + (*iter - '0');
    // Cap at 24 hours. This also avoids overflow
    age = PR_MIN(age, 86400);
    ++iter;
  }

  if (!age) {
    return;
  }


  // String seems fine, go ahead and cache.
  // Note that we have already checked that these headers follow the correct
  // syntax.

  nsCOMPtr<nsIURI> uri;
  http->GetURI(getter_AddRefs(uri));

  // PR_Now gives microseconds
  PRTime expirationTime = PR_Now() + (PRUint64)age * PR_USEC_PER_SEC;

  nsACPreflightCache::CacheEntry* entry =
    nsACPreflightCache::GetEntry(uri, mReferrerPrincipal, mWithCredentials,
                                 PR_TRUE);
  if (!entry) {
    return;
  }

  // The "Access-Control-Allow-Methods" header contains a comma separated
  // list of method names.
  http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Allow-Methods"),
                          headerVal);

  nsCCommaSeparatedTokenizer methods(headerVal);
  while(methods.hasMoreTokens()) {
    const nsDependentCSubstring& method = methods.nextToken();
    if (method.IsEmpty()) {
      continue;
    }
    PRUint32 i;
    for (i = 0; i < entry->mMethods.Length(); ++i) {
      if (entry->mMethods[i].token.Equals(method)) {
        entry->mMethods[i].expirationTime = expirationTime;
        break;
      }
    }
    if (i == entry->mMethods.Length()) {
      nsACPreflightCache::TokenTime* newMethod =
        entry->mMethods.AppendElement();
      if (!newMethod) {
        return;
      }

      newMethod->token = method;
      newMethod->expirationTime = expirationTime;
    }
  }

  // The "Access-Control-Allow-Headers" header contains a comma separated
  // list of method names.
  http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Allow-Headers"),
                          headerVal);

  nsCCommaSeparatedTokenizer headers(headerVal);
  while(headers.hasMoreTokens()) {
    const nsDependentCSubstring& header = headers.nextToken();
    if (header.IsEmpty()) {
      continue;
    }
    PRUint32 i;
    for (i = 0; i < entry->mHeaders.Length(); ++i) {
      if (entry->mHeaders[i].token.Equals(header)) {
        entry->mHeaders[i].expirationTime = expirationTime;
        break;
      }
    }
    if (i == entry->mHeaders.Length()) {
      nsACPreflightCache::TokenTime* newHeader =
        entry->mHeaders.AppendElement();
      if (!newHeader) {
        return;
      }

      newHeader->token = header;
      newHeader->expirationTime = expirationTime;
    }
  }
}

NS_IMETHODIMP
nsACPreflightListener::OnStartRequest(nsIRequest *aRequest,
                                      nsISupports *aContext)
{
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);

  if (NS_SUCCEEDED(rv)) {
    rv = status;
  }

  if (NS_SUCCEEDED(rv)) {
    // Everything worked, try to cache and then fire off the actual request.
    AddResultToCache(aRequest);

    rv = mOuterChannel->AsyncOpen(mOuterListener, mOuterContext);
  }

  if (NS_FAILED(rv)) {
    mOuterChannel->Cancel(rv);
    mOuterListener->OnStartRequest(mOuterChannel, mOuterContext);
    mOuterListener->OnStopRequest(mOuterChannel, mOuterContext, rv);
    
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsACPreflightListener::OnStopRequest(nsIRequest *aRequest,
                                     nsISupports *aContext,
                                     nsresult aStatus)
{
  return NS_OK;
}

/** nsIStreamListener methods **/

NS_IMETHODIMP
nsACPreflightListener::OnDataAvailable(nsIRequest *aRequest,
                                       nsISupports *ctxt,
                                       nsIInputStream *inStr,
                                       PRUint32 sourceOffset,
                                       PRUint32 count)
{
  return NS_OK;
}

NS_IMETHODIMP
nsACPreflightListener::OnChannelRedirect(nsIChannel *aOldChannel,
                                         nsIChannel *aNewChannel,
                                         PRUint32 aFlags)
{
  // No redirects allowed for now.
  return NS_ERROR_DOM_BAD_URI;
}

NS_IMETHODIMP
nsACPreflightListener::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

// =========================================================================
// nsACPreflightCache

nsClassHashtable<nsCStringHashKey, nsACPreflightCache::CacheEntry>*
  nsACPreflightCache::mTable;
PRCList nsACPreflightCache::mList;

void
nsACPreflightCache::CacheEntry::PurgeExpired(PRTime now)
{
  PRUint32 i;
  for (i = 0; i < mMethods.Length(); ++i) {
    if (now >= mMethods[i].expirationTime) {
      mMethods.RemoveElementAt(i--);
    }
  }
  for (i = 0; i < mHeaders.Length(); ++i) {
    if (now >= mHeaders[i].expirationTime) {
      mHeaders.RemoveElementAt(i--);
    }
  }
}

PRBool
nsACPreflightCache::CacheEntry::CheckRequest(const nsCString& aMethod,
                                             const nsTArray<nsCString>& aHeaders)
{
  PurgeExpired(PR_Now());

  if (!aMethod.EqualsLiteral("GET") && !aMethod.EqualsLiteral("POST")) {
    PRUint32 i;
    for (i = 0; i < mMethods.Length(); ++i) {
      if (aMethod.Equals(mMethods[i].token))
        break;
    }
    if (i == mMethods.Length()) {
      return PR_FALSE;
    }
  }

  for (PRUint32 i = 0; i < aHeaders.Length(); ++i) {
    PRUint32 j;
    for (j = 0; j < mHeaders.Length(); ++j) {
      if (aHeaders[i].Equals(mHeaders[j].token,
                             nsCaseInsensitiveCStringComparator())) {
        break;
      }
    }
    if (j == mHeaders.Length()) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

nsACPreflightCache::CacheEntry*
nsACPreflightCache::GetEntry(nsIURI* aURI, nsIPrincipal* aPrincipal,
                             PRBool aWithCredentials, PRBool aCreate)
{
  if (!mTable) {
    if (!aCreate) {
      return nsnull;
    }

    PR_INIT_CLIST(&mList);

    mTable = new nsClassHashtable<nsCStringHashKey, CacheEntry>;
    NS_ENSURE_TRUE(mTable, nsnull);

    if (!mTable->Init()) {
      delete mTable;
      mTable = nsnull;
      return nsnull;
    }
  }

  nsCString key;
  if (!GetCacheKey(aURI, aPrincipal, aWithCredentials, key)) {
    NS_WARNING("Invalid cache key!");
    return nsnull;
  }

  CacheEntry* entry;

  if (mTable->Get(key, &entry)) {
    // Entry already existed so just return it. Also update the LRU list.

    // Move to the head of the list.
    PR_REMOVE_LINK(entry);
    PR_INSERT_LINK(entry, &mList);

    return entry;
  }

  if (!aCreate) {
    return nsnull;
  }

  // This is a new entry, allocate and insert into the table now so that any
  // failures don't cause items to be removed from a full cache.
  entry = new CacheEntry(key);
  if (!entry) {
    NS_WARNING("Failed to allocate new cache entry!");
    return nsnull;
  }

  if (!mTable->Put(key, entry)) {
    // Failed, clean up the new entry.
    delete entry;

    NS_WARNING("Failed to add entry to the access control cache!");
    return nsnull;
  }

  PR_INSERT_LINK(entry, &mList);

  NS_ASSERTION(mTable->Count() <= ACCESS_CONTROL_CACHE_SIZE + 1,
               "Something is borked, too many entries in the cache!");

  // Now enforce the max count.
  if (mTable->Count() > ACCESS_CONTROL_CACHE_SIZE) {
    // Try to kick out all the expired entries.
    PRTime now = PR_Now();
    mTable->Enumerate(RemoveExpiredEntries, &now);

    // If that didn't remove anything then kick out the least recently used
    // entry.
    if (mTable->Count() > ACCESS_CONTROL_CACHE_SIZE) {
      CacheEntry* lruEntry = static_cast<CacheEntry*>(PR_LIST_TAIL(&mList));
      PR_REMOVE_LINK(lruEntry);

      // This will delete 'lruEntry'.
      mTable->Remove(lruEntry->mKey);

      NS_ASSERTION(mTable->Count() >= ACCESS_CONTROL_CACHE_SIZE,
                   "Somehow tried to remove an entry that was never added!");
    }
  }
  
  return entry;
}

/* static */ PR_CALLBACK PLDHashOperator
nsACPreflightCache::RemoveExpiredEntries(const nsACString& aKey,
                                         nsAutoPtr<CacheEntry>& aValue,
                                         void* aUserData)
{
  PRTime* now = static_cast<PRTime*>(aUserData);
  
  aValue->PurgeExpired(*now);
  
  if (aValue->mHeaders.IsEmpty() &&
      aValue->mHeaders.IsEmpty()) {
    // Expired, remove from the list as well as the hash table.
    PR_REMOVE_LINK(aValue);
    return PL_DHASH_REMOVE;
  }
  
  return PL_DHASH_NEXT;
}

/* static */ PRBool
nsACPreflightCache::GetCacheKey(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                PRBool aWithCredentials, nsACString& _retval)
{
  NS_ASSERTION(aURI, "Null uri!");
  NS_ASSERTION(aPrincipal, "Null principal!");
  
  NS_NAMED_LITERAL_CSTRING(space, " ");

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  
  nsCAutoString scheme, host, port;
  if (uri) {
    uri->GetScheme(scheme);
    uri->GetHost(host);
    port.AppendInt(NS_GetRealPort(uri));
  }

  nsCAutoString cred;
  if (aWithCredentials) {
    _retval.AssignLiteral("cred");
  }
  else {
    _retval.AssignLiteral("nocred");
  }

  nsCAutoString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  _retval.Assign(cred + space + scheme + space + host + space + port + space +
                 spec);

  return PR_TRUE;
}
