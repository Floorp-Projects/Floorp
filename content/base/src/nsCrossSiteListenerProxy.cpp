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
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsXMLHttpRequest.h"
#include "nsAsyncRedirectVerifyHelper.h"

static PRBool gDisableCORS = PR_FALSE;
static PRBool gDisableCORSPrivateData = PR_FALSE;

class nsChannelCanceller
{
public:
  nsChannelCanceller(nsIChannel* aChannel)
    : mChannel(aChannel)
  {
  }
  ~nsChannelCanceller()
  {
    if (mChannel) {
      mChannel->Cancel(NS_ERROR_DOM_BAD_URI);
    }
  }

  void DontCancel()
  {
    mChannel = nsnull;
  }

private:
  nsIChannel* mChannel;
};

NS_IMPL_ISUPPORTS5(nsCrossSiteListenerProxy, nsIStreamListener,
                   nsIRequestObserver, nsIChannelEventSink,
                   nsIInterfaceRequestor, nsIAsyncVerifyRedirectCallback)

/* static */
void
nsCrossSiteListenerProxy::Startup()
{
  nsContentUtils::AddBoolPrefVarCache("content.cors.disable", &gDisableCORS);
  nsContentUtils::AddBoolPrefVarCache("content.cors.no_private_data", &gDisableCORSPrivateData);
}

nsCrossSiteListenerProxy::nsCrossSiteListenerProxy(nsIStreamListener* aOuter,
                                                   nsIPrincipal* aRequestingPrincipal,
                                                   nsIChannel* aChannel,
                                                   PRBool aWithCredentials,
                                                   nsresult* aResult)
  : mOuterListener(aOuter),
    mRequestingPrincipal(aRequestingPrincipal),
    mWithCredentials(aWithCredentials && !gDisableCORSPrivateData),
    mRequestApproved(PR_FALSE),
    mHasBeenCrossSite(PR_FALSE),
    mIsPreflight(PR_FALSE)
{
  aChannel->GetNotificationCallbacks(getter_AddRefs(mOuterNotificationCallbacks));
  aChannel->SetNotificationCallbacks(this);

  *aResult = UpdateChannel(aChannel);
  if (NS_FAILED(*aResult)) {
    mOuterListener = nsnull;
    mRequestingPrincipal = nsnull;
    mOuterNotificationCallbacks = nsnull;
  }
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
    mWithCredentials(aWithCredentials && !gDisableCORSPrivateData),
    mRequestApproved(PR_FALSE),
    mHasBeenCrossSite(PR_FALSE),
    mIsPreflight(PR_TRUE),
    mPreflightMethod(aPreflightMethod),
    mPreflightHeaders(aPreflightHeaders)
{
  for (PRUint32 i = 0; i < mPreflightHeaders.Length(); ++i) {
    ToLowerCase(mPreflightHeaders[i]);
  }
  mPreflightHeaders.Sort();

  aChannel->GetNotificationCallbacks(getter_AddRefs(mOuterNotificationCallbacks));
  aChannel->SetNotificationCallbacks(this);

  *aResult = UpdateChannel(aChannel);
  if (NS_FAILED(*aResult)) {
    mOuterListener = nsnull;
    mRequestingPrincipal = nsnull;
    mOuterNotificationCallbacks = nsnull;
  }
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::OnStartRequest(nsIRequest* aRequest,
                                         nsISupports* aContext)
{
  mRequestApproved = NS_SUCCEEDED(CheckRequestApproved(aRequest));
  if (!mRequestApproved) {
    if (nsXMLHttpRequest::sAccessControlCache) {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
      if (channel) {
      nsCOMPtr<nsIURI> uri;
        NS_GetFinalChannelURI(channel, getter_AddRefs(uri));
        if (uri) {
          nsXMLHttpRequest::sAccessControlCache->
            RemoveEntries(uri, mRequestingPrincipal);
        }
      }
    }

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
nsCrossSiteListenerProxy::CheckRequestApproved(nsIRequest* aRequest)
{
  // Check if this was actually a cross domain request
  if (!mHasBeenCrossSite) {
    return NS_OK;
  }

  if (gDisableCORS) {
    return NS_ERROR_DOM_BAD_URI;
  }

  // Check if the request failed
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(status, status);

  // Test that things worked on a HTTP level
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aRequest);
  NS_ENSURE_TRUE(http, NS_ERROR_DOM_BAD_URI);

  // Check the Access-Control-Allow-Origin header
  nsCAutoString allowedOriginHeader;
  rv = http->GetResponseHeader(
    NS_LITERAL_CSTRING("Access-Control-Allow-Origin"), allowedOriginHeader);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mWithCredentials || !allowedOriginHeader.EqualsLiteral("*")) {
    nsCAutoString origin;
    rv = nsContentUtils::GetASCIIOrigin(mRequestingPrincipal, origin);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!allowedOriginHeader.Equals(origin) ||
        origin.EqualsLiteral("null")) {
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
    PRBool succeeded;
    rv = http->GetRequestSucceeded(&succeeded);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!succeeded) {
      return NS_ERROR_DOM_BAD_URI;
    }

    nsCAutoString headerVal;
    // The "Access-Control-Allow-Methods" header contains a comma separated
    // list of method names.
    http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Allow-Methods"),
                            headerVal);
    PRBool foundMethod = mPreflightMethod.EqualsLiteral("GET") ||
                         mPreflightMethod.EqualsLiteral("HEAD") ||
                         mPreflightMethod.EqualsLiteral("POST");
    nsCCharSeparatedTokenizer methodTokens(headerVal, ',');
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
    headerVal.Truncate();
    http->GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Allow-Headers"),
                            headerVal);
    nsTArray<nsCString> headers;
    nsCCharSeparatedTokenizer headerTokens(headerVal, ',');
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
nsCrossSiteListenerProxy::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                                 nsIChannel *aNewChannel,
                                                 PRUint32 aFlags,
                                                 nsIAsyncVerifyRedirectCallback *cb)
{
  nsresult rv;
  if (!NS_IsInternalSameURIRedirect(aOldChannel, aNewChannel, aFlags)) {
    rv = CheckRequestApproved(aOldChannel);
    if (NS_FAILED(rv)) {
      if (nsXMLHttpRequest::sAccessControlCache) {
        nsCOMPtr<nsIURI> oldURI;
        NS_GetFinalChannelURI(aOldChannel, getter_AddRefs(oldURI));
        if (oldURI) {
          nsXMLHttpRequest::sAccessControlCache->
            RemoveEntries(oldURI, mRequestingPrincipal);
        }
      }
      aOldChannel->Cancel(NS_ERROR_DOM_BAD_URI);
      return NS_ERROR_DOM_BAD_URI;
    }
  }

  // Prepare to receive callback
  mRedirectCallback = cb;
  mOldRedirectChannel = aOldChannel;
  mNewRedirectChannel = aNewChannel;

  nsCOMPtr<nsIChannelEventSink> outer =
    do_GetInterface(mOuterNotificationCallbacks);
  if (outer) {
    rv = outer->AsyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags, this);
    if (NS_FAILED(rv)) {
        aOldChannel->Cancel(rv); // is this necessary...?
        mRedirectCallback = nsnull;
        mOldRedirectChannel = nsnull;
        mNewRedirectChannel = nsnull;
    }
    return rv;  
  }

  (void) OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
nsCrossSiteListenerProxy::OnRedirectVerifyCallback(nsresult result)
{
  NS_ASSERTION(mRedirectCallback, "mRedirectCallback not set in callback");
  NS_ASSERTION(mOldRedirectChannel, "mOldRedirectChannel not set in callback");
  NS_ASSERTION(mNewRedirectChannel, "mNewRedirectChannel not set in callback");

  if (NS_SUCCEEDED(result)) {
      nsresult rv = UpdateChannel(mNewRedirectChannel);
      if (NS_FAILED(rv)) {
          NS_WARNING("nsCrossSiteListenerProxy::OnRedirectVerifyCallback: "
                     "UpdateChannel() returned failure");
      }
      result = rv;
  }

  if (NS_FAILED(result)) {
    mOldRedirectChannel->Cancel(result);
  }

  mOldRedirectChannel = nsnull;
  mNewRedirectChannel = nsnull;
  mRedirectCallback->OnRedirectVerifyCallback(result);
  mRedirectCallback   = nsnull;
  return NS_OK;
}

nsresult
nsCrossSiteListenerProxy::UpdateChannel(nsIChannel* aChannel)
{
  nsCOMPtr<nsIURI> uri, originalURI;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aChannel->GetOriginalURI(getter_AddRefs(originalURI));
  NS_ENSURE_SUCCESS(rv, rv);  

  // Check that the uri is ok to load
  rv = nsContentUtils::GetSecurityManager()->
    CheckLoadURIWithPrincipal(mRequestingPrincipal, uri,
                              nsIScriptSecurityManager::STANDARD);
  NS_ENSURE_SUCCESS(rv, rv);

  if (originalURI != uri) {
    rv = nsContentUtils::GetSecurityManager()->
      CheckLoadURIWithPrincipal(mRequestingPrincipal, originalURI,
                                nsIScriptSecurityManager::STANDARD);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mHasBeenCrossSite &&
      NS_SUCCEEDED(mRequestingPrincipal->CheckMayLoad(uri, PR_FALSE)) &&
      (originalURI == uri ||
       NS_SUCCEEDED(mRequestingPrincipal->CheckMayLoad(originalURI,
                                                       PR_FALSE)))) {
    return NS_OK;
  }

  // It's a cross site load
  mHasBeenCrossSite = PR_TRUE;

  nsCString userpass;
  uri->GetUserPass(userpass);
  NS_ENSURE_TRUE(userpass.IsEmpty(), NS_ERROR_DOM_BAD_URI);

  // Add the Origin header
  nsCAutoString origin;
  rv = nsContentUtils::GetASCIIOrigin(mRequestingPrincipal, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aChannel);
  NS_ENSURE_TRUE(http, NS_ERROR_FAILURE);

  rv = http->SetRequestHeader(NS_LITERAL_CSTRING("Origin"), origin, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add preflight headers if this is a preflight request
  if (mIsPreflight) {
    rv = http->
      SetRequestHeader(NS_LITERAL_CSTRING("Access-Control-Request-Method"),
                       mPreflightMethod, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mPreflightHeaders.IsEmpty()) {
      nsCAutoString headers;
      for (PRUint32 i = 0; i < mPreflightHeaders.Length(); ++i) {
        if (i != 0) {
          headers += ',';
        }
        headers += mPreflightHeaders[i];
      }
      rv = http->
        SetRequestHeader(NS_LITERAL_CSTRING("Access-Control-Request-Headers"),
                         headers, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

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
