/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/FetchDriver.h"

#include "nsIDocument.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIJARChannel.h"
#include "nsIScriptSecurityManager.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIUploadChannel2.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPipe.h"

#include "nsContentPolicyUtils.h"
#include "nsCORSListenerProxy.h"
#include "nsDataHandler.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

#include "mozilla/dom/File.h"
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/unused.h"

#include "Fetch.h"
#include "InternalRequest.h"
#include "InternalResponse.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(FetchDriver,
                  nsIStreamListener, nsIChannelEventSink, nsIInterfaceRequestor,
                  nsIAsyncVerifyRedirectCallback, nsIThreadRetargetableStreamListener)

FetchDriver::FetchDriver(InternalRequest* aRequest, nsIPrincipal* aPrincipal,
                         nsILoadGroup* aLoadGroup)
  : mPrincipal(aPrincipal)
  , mLoadGroup(aLoadGroup)
  , mRequest(aRequest)
  , mFetchRecursionCount(0)
  , mCORSFlagEverSet(false)
  , mResponseAvailableCalled(false)
{
}

FetchDriver::~FetchDriver()
{
  // We assert this since even on failures, we should call
  // FailWithNetworkError().
  MOZ_ASSERT(mResponseAvailableCalled);
}

nsresult
FetchDriver::Fetch(FetchDriverObserver* aObserver)
{
  workers::AssertIsOnMainThread();
  mObserver = aObserver;

  Telemetry::Accumulate(Telemetry::SERVICE_WORKER_REQUEST_PASSTHROUGH,
                        mRequest->WasCreatedByFetchEvent());

  return Fetch(false /* CORS flag */);
}

nsresult
FetchDriver::Fetch(bool aCORSFlag)
{
  // We do not currently implement parts of the spec that lead to recursion.
  MOZ_ASSERT(mFetchRecursionCount == 0);
  mFetchRecursionCount++;

  // FIXME(nsm): Deal with HSTS.

  if (!mRequest->IsSynchronous() && mFetchRecursionCount <= 1) {
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableMethodWithArg<bool>(this, &FetchDriver::ContinueFetch, aCORSFlag);
    nsresult rv = NS_DispatchToCurrentThread(r);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailWithNetworkError();
    }
    return rv;
  }

  MOZ_CRASH("Synchronous fetch not supported");
}

FetchDriver::MainFetchOp
FetchDriver::SetTaintingAndGetNextOp(bool aCORSFlag)
{
  workers::AssertIsOnMainThread();

  nsAutoCString url;
  mRequest->GetURL(url);
  nsCOMPtr<nsIURI> requestURI;
  nsresult rv = NS_NewURI(getter_AddRefs(requestURI), url,
                          nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return MainFetchOp(NETWORK_ERROR);
  }

  // CSP/mixed content checks.
  int16_t shouldLoad;
  rv = NS_CheckContentLoadPolicy(mRequest->ContentPolicyType(),
                                 requestURI,
                                 mPrincipal,
                                 mDocument,
                                 // FIXME(nsm): Should MIME be extracted from
                                 // Content-Type header?
                                 EmptyCString(), /* mime guess */
                                 nullptr, /* extra */
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  if (NS_WARN_IF(NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad))) {
    // Disallowed by content policy.
    return MainFetchOp(NETWORK_ERROR);
  }

  // Begin Step 8 of the Main Fetch algorithm
  // https://fetch.spec.whatwg.org/#fetching

  nsAutoCString scheme;
  rv = requestURI->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return MainFetchOp(NETWORK_ERROR);
  }

  // request's current url's origin is request's origin and the CORS flag is unset
  // request's current url's scheme is "data" and request's same-origin data-URL flag is set
  // request's current url's scheme is "about"
  rv = mPrincipal->CheckMayLoad(requestURI, false /* report */,
                                false /* allowIfInheritsPrincipal */);
  if ((!aCORSFlag && NS_SUCCEEDED(rv)) ||
      (scheme.EqualsLiteral("data") && mRequest->SameOriginDataURL()) ||
      scheme.EqualsLiteral("about")) {
    return MainFetchOp(BASIC_FETCH);
  }

  // request's mode is "same-origin"
  if (mRequest->Mode() == RequestMode::Same_origin) {
    return MainFetchOp(NETWORK_ERROR);
  }

  // request's mode is "no-cors"
  if (mRequest->Mode() == RequestMode::No_cors) {
    mRequest->SetResponseTainting(InternalRequest::RESPONSETAINT_OPAQUE);
    return MainFetchOp(BASIC_FETCH);
  }

  // request's current url's scheme is not one of "http" and "https"
  if (!scheme.EqualsLiteral("http") && !scheme.EqualsLiteral("https")) {
    return MainFetchOp(NETWORK_ERROR);
  }

  // request's mode is "cors-with-forced-preflight"
  // request's unsafe-request flag is set and either request's method is not
  // a simple method or a header in request's header list is not a simple header
  if (mRequest->Mode() == RequestMode::Cors_with_forced_preflight ||
      (mRequest->UnsafeRequest() && (!mRequest->HasSimpleMethod() ||
                                     !mRequest->Headers()->HasOnlySimpleHeaders()))) {
    mRequest->SetResponseTainting(InternalRequest::RESPONSETAINT_CORS);
    mRequest->SetRedirectMode(RequestRedirect::Error);

    // Note, the following text from Main Fetch step 8 is handled in
    // nsCORSListenerProxy when CheckRequestApproved() fails:
    //
    //  The result of performing an HTTP fetch using request with the CORS
    //  flag and CORS-preflight flag set. If the result is a network error,
    //  clear cache entries using request.

    return MainFetchOp(HTTP_FETCH, true /* cors */, true /* preflight */);
  }

  // Otherwise
  mRequest->SetResponseTainting(InternalRequest::RESPONSETAINT_CORS);
  return MainFetchOp(HTTP_FETCH, true /* cors */, false /* preflight */);
}

nsresult
FetchDriver::ContinueFetch(bool aCORSFlag)
{
  workers::AssertIsOnMainThread();

  MainFetchOp nextOp = SetTaintingAndGetNextOp(aCORSFlag);

  if (nextOp.mType == NETWORK_ERROR) {
    return FailWithNetworkError();
  }

  if (nextOp.mType == BASIC_FETCH) {
    return BasicFetch();
  }

  if (nextOp.mType == HTTP_FETCH) {
    return HttpFetch(nextOp.mCORSFlag, nextOp.mCORSPreflightFlag);
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected main fetch operation!");
  return FailWithNetworkError();
 }

nsresult
FetchDriver::BasicFetch()
{
  nsAutoCString url;
  mRequest->GetURL(url);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri),
                 url,
                 nullptr,
                 nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  nsAutoCString scheme;
  rv = uri->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  if (scheme.LowerCaseEqualsLiteral("about")) {
    if (url.EqualsLiteral("about:blank")) {
      nsRefPtr<InternalResponse> response =
        new InternalResponse(200, NS_LITERAL_CSTRING("OK"));
      ErrorResult result;
      response->Headers()->Append(NS_LITERAL_CSTRING("content-type"),
                                  NS_LITERAL_CSTRING("text/html;charset=utf-8"),
                                  result);
      MOZ_ASSERT(!result.Failed());
      nsCOMPtr<nsIInputStream> body;
      rv = NS_NewCStringInputStream(getter_AddRefs(body), EmptyCString());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        FailWithNetworkError();
        return rv;
      }

      response->SetBody(body);
      BeginResponse(response);
      return SucceedWithResponse();
    }
    return FailWithNetworkError();
  }

  if (scheme.LowerCaseEqualsLiteral("blob")) {
    nsRefPtr<BlobImpl> blobImpl;
    rv = NS_GetBlobForBlobURI(uri, getter_AddRefs(blobImpl));
    BlobImpl* blob = static_cast<BlobImpl*>(blobImpl.get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailWithNetworkError();
      return rv;
    }

    nsRefPtr<InternalResponse> response = new InternalResponse(200, NS_LITERAL_CSTRING("OK"));
    ErrorResult result;
    uint64_t size = blob->GetSize(result);
    if (NS_WARN_IF(result.Failed())) {
      FailWithNetworkError();
      return result.StealNSResult();
    }

    nsAutoString sizeStr;
    sizeStr.AppendInt(size);
    response->Headers()->Append(NS_LITERAL_CSTRING("Content-Length"), NS_ConvertUTF16toUTF8(sizeStr), result);
    if (NS_WARN_IF(result.Failed())) {
      FailWithNetworkError();
      return result.StealNSResult();
    }

    nsAutoString type;
    blob->GetType(type);
    response->Headers()->Append(NS_LITERAL_CSTRING("Content-Type"), NS_ConvertUTF16toUTF8(type), result);
    if (NS_WARN_IF(result.Failed())) {
      FailWithNetworkError();
      return result.StealNSResult();
    }

    nsCOMPtr<nsIInputStream> stream;
    blob->GetInternalStream(getter_AddRefs(stream), result);
    if (NS_WARN_IF(result.Failed())) {
      FailWithNetworkError();
      return result.StealNSResult();
    }

    response->SetBody(stream);
    BeginResponse(response);
    return SucceedWithResponse();
  }

  if (scheme.LowerCaseEqualsLiteral("data")) {
    nsAutoCString method;
    mRequest->GetMethod(method);
    if (method.LowerCaseEqualsASCII("get")) {
      nsresult rv;
      nsCOMPtr<nsIProtocolHandler> dataHandler =
        do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "data", &rv);

      if (NS_WARN_IF(NS_FAILED(rv))) {
        return FailWithNetworkError();
      }

      nsCOMPtr<nsIChannel> channel;
      rv = dataHandler->NewChannel(uri, getter_AddRefs(channel));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return FailWithNetworkError();
      }

      nsCOMPtr<nsIInputStream> stream;
      rv = channel->Open(getter_AddRefs(stream));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return FailWithNetworkError();
      }

      // nsDataChannel will parse the data URI when it is Open()ed and set the
      // correct content type and charset.
      nsAutoCString contentType;
      if (NS_SUCCEEDED(channel->GetContentType(contentType))) {
        nsAutoCString charset;
        if (NS_SUCCEEDED(channel->GetContentCharset(charset)) && !charset.IsEmpty()) {
          contentType.AppendLiteral(";charset=");
          contentType.Append(charset);
        }
      } else {
        NS_WARNING("Could not get content type from data channel");
      }

      nsRefPtr<InternalResponse> response = new InternalResponse(200, NS_LITERAL_CSTRING("OK"));
      ErrorResult result;
      response->Headers()->Append(NS_LITERAL_CSTRING("Content-Type"), contentType, result);
      if (NS_WARN_IF(result.Failed())) {
        FailWithNetworkError();
        return result.StealNSResult();
      }

      response->SetBody(stream);
      BeginResponse(response);
      return SucceedWithResponse();
    }

    return FailWithNetworkError();
  }

  if (scheme.LowerCaseEqualsLiteral("http") ||
      scheme.LowerCaseEqualsLiteral("https") ||
      scheme.LowerCaseEqualsLiteral("app")) {
    return HttpFetch();
  }

  return FailWithNetworkError();
}

// This function implements the "HTTP Fetch" algorithm from the Fetch spec.
// Functionality is often split between here, the CORS listener proxy and the
// Necko HTTP implementation.
nsresult
FetchDriver::HttpFetch(bool aCORSFlag, bool aCORSPreflightFlag, bool aAuthenticationFlag)
{
  // Step 1. "Let response be null."
  mResponse = nullptr;
  nsresult rv;

  // We need to track the CORS flag through redirects.  Since there is no way
  // for us to go from CORS mode to non-CORS mode, we just need to remember
  // if it has ever been set.
  mCORSFlagEverSet = mCORSFlagEverSet || aCORSFlag;

  nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  nsAutoCString url;
  mRequest->GetURL(url);
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri),
                          url,
                          nullptr,
                          nullptr,
                          ios);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  // Step 2 deals with letting ServiceWorkers intercept requests. This is
  // handled by Necko after the channel is opened.
  // FIXME(nsm): Bug 1119026: The channel's skip service worker flag should be
  // set based on the Request's flag.

  // Step 3.1 "If the CORS preflight flag is set and one of these conditions is
  // true..." is handled by the CORS proxy.
  //
  // Step 3.2 "Set request's skip service worker flag." This isn't required
  // since Necko will fall back to the network if the ServiceWorker does not
  // respond with a valid Response.
  //
  // NS_StartCORSPreflight() will automatically kick off the original request
  // if it succeeds, so we need to have everything setup for the original
  // request too.

  // Step 3.3 "Let credentials flag be set if either request's credentials mode
  // is include, or request's credentials mode is same-origin and the CORS flag
  // is unset, and unset otherwise."
  bool useCredentials = false;
  if (mRequest->GetCredentialsMode() == RequestCredentials::Include ||
      (mRequest->GetCredentialsMode() == RequestCredentials::Same_origin && !aCORSFlag)) {
    useCredentials = true;
  }

  // This is effectivetly the opposite of the use credentials flag in "HTTP
  // network or cache fetch" in the spec and decides whether to transmit
  // cookies and other identifying information. LOAD_ANONYMOUS also prevents
  // new cookies sent by the server from being stored.
  const nsLoadFlags credentialsFlag = useCredentials ? 0 : nsIRequest::LOAD_ANONYMOUS;

  // From here on we create a channel and set its properties with the
  // information from the InternalRequest. This is an implementation detail.
  MOZ_ASSERT(mLoadGroup);
  nsCOMPtr<nsIChannel> chan;
  rv = NS_NewChannel(getter_AddRefs(chan),
                     uri,
                     mPrincipal,
                     nsILoadInfo::SEC_NORMAL,
                     mRequest->ContentPolicyType(),
                     mLoadGroup,
                     nullptr, /* aCallbacks */
                     nsIRequest::LOAD_NORMAL | credentialsFlag,
                     ios);
  mLoadGroup = nullptr;
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  // Insert ourselves into the notification callbacks chain so we can handle
  // cross-origin redirects.
  chan->GetNotificationCallbacks(getter_AddRefs(mNotificationCallbacks));
  chan->SetNotificationCallbacks(this);

  // FIXME(nsm): Bug 1120715.
  // Step 3.4 "If request's cache mode is default and request's header list
  // contains a header named `If-Modified-Since`, `If-None-Match`,
  // `If-Unmodified-Since`, `If-Match`, or `If-Range`, set request's cache mode
  // to no-store."

  // Step 3.5 begins "HTTP network or cache fetch".
  // HTTP network or cache fetch
  // ---------------------------
  // Step 1 "Let HTTPRequest..." The channel is the HTTPRequest.
  nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(chan);
  if (httpChan) {
    // Copy the method.
    nsAutoCString method;
    mRequest->GetMethod(method);
    rv = httpChan->SetRequestMethod(method);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailWithNetworkError();
      return rv;
    }

    // Set the same headers.
    nsAutoTArray<InternalHeaders::Entry, 5> headers;
    mRequest->Headers()->GetEntries(headers);
    for (uint32_t i = 0; i < headers.Length(); ++i) {
      if (headers[i].mValue.IsEmpty()) {
        httpChan->SetEmptyRequestHeader(headers[i].mName);
      } else {
        httpChan->SetRequestHeader(headers[i].mName, headers[i].mValue, false /* merge */);
      }
    }

    // Step 2. Set the referrer.
    nsAutoString referrer;
    mRequest->GetReferrer(referrer);
    if (referrer.EqualsLiteral(kFETCH_CLIENT_REFERRER_STR)) {
      rv = nsContentUtils::SetFetchReferrerURIWithPolicy(mPrincipal,
                                                         mDocument,
                                                         httpChan);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return FailWithNetworkError();
      }
    } else if (referrer.IsEmpty()) {
      rv = httpChan->SetReferrerWithPolicy(nullptr, net::RP_No_Referrer);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return FailWithNetworkError();
      }
    } else {
      // From "Determine request's Referrer" step 3
      // "If request's referrer is a URL, let referrerSource be request's
      // referrer."
      //
      // XXXnsm - We never actually hit this from a fetch() call since both
      // fetch and Request() create a new internal request whose referrer is
      // always set to about:client. Should we just crash here instead until
      // someone tries to use FetchDriver for non-fetch() APIs?
      nsCOMPtr<nsIURI> referrerURI;
      rv = NS_NewURI(getter_AddRefs(referrerURI), referrer, nullptr, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return FailWithNetworkError();
      }

      rv =
        httpChan->SetReferrerWithPolicy(referrerURI,
                                        mDocument ? mDocument->GetReferrerPolicy() :
                                                    net::RP_Default);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return FailWithNetworkError();
      }
    }

    // Step 3 "If HTTPRequest's force Origin header flag is set..."
    if (mRequest->ForceOriginHeader()) {
      nsAutoString origin;
      rv = nsContentUtils::GetUTFOrigin(mPrincipal, origin);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return FailWithNetworkError();
      }
      httpChan->SetRequestHeader(NS_LITERAL_CSTRING("origin"),
                                 NS_ConvertUTF16toUTF8(origin),
                                 false /* merge */);
    }
    // Bug 1120722 - Authorization will be handled later.
    // Auth may require prompting, we don't support it yet.
    // The next patch in this same bug prevents this from aborting the request.
    // Credentials checks for CORS are handled by nsCORSListenerProxy,

    nsCOMPtr<nsIHttpChannelInternal> internalChan = do_QueryInterface(httpChan);

    // Conversion between enumerations is safe due to static asserts in
    // dom/workers/ServiceWorkerManager.cpp
    internalChan->SetCorsMode(static_cast<uint32_t>(mRequest->Mode()));
    internalChan->SetRedirectMode(static_cast<uint32_t>(mRequest->GetRedirectMode()));
  }

  // Step 5. Proxy authentication will be handled by Necko.
  // FIXME(nsm): Bug 1120715.
  // Step 7-10. "If request's cache mode is neither no-store nor reload..."

  // Continue setting up 'HTTPRequest'. Content-Type and body data.
  nsCOMPtr<nsIUploadChannel2> uploadChan = do_QueryInterface(chan);
  if (uploadChan) {
    nsAutoCString contentType;
    ErrorResult result;
    mRequest->Headers()->Get(NS_LITERAL_CSTRING("content-type"), contentType, result);
    // This is an error because the Request constructor explicitly extracts and
    // sets a content-type per spec.
    if (result.Failed()) {
      return FailWithNetworkError();
    }

    nsCOMPtr<nsIInputStream> bodyStream;
    mRequest->GetBody(getter_AddRefs(bodyStream));
    if (bodyStream) {
      nsAutoCString method;
      mRequest->GetMethod(method);
      rv = uploadChan->ExplicitSetUploadStream(bodyStream, contentType, -1, method, false /* aStreamHasHeaders */);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return FailWithNetworkError();
      }
    }
  }

  // Set skip serviceworker flag.
  // While the spec also gates on the client being a ServiceWorker, we can't
  // infer that here. Instead we rely on callers to set the flag correctly.
  if (mRequest->SkipServiceWorker()) {
    if (httpChan) {
      nsCOMPtr<nsIHttpChannelInternal> internalChan = do_QueryInterface(httpChan);
      internalChan->ForceNoIntercept();
    } else {
      nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(chan);
      // If it is not an http channel, it has to be a jar one.
      MOZ_ASSERT(jarChannel);
      jarChannel->ForceNoIntercept();
    }
  }

  nsCOMPtr<nsIStreamListener> listener = this;

  // Only use nsCORSListenerProxy if we are in CORS mode.  Otherwise it
  // will overwrite the CorsMode flag unconditionally to "cors" or
  // "cors-with-forced-preflight".
  if (mRequest->Mode() == RequestMode::Cors) {
    // Set up a CORS proxy that will handle the various requirements of the CORS
    // protocol. It handles the preflight cache and CORS response headers.
    // If the request is allowed, it will start our original request
    // and our observer will be notified. On failure, our observer is notified
    // directly.
    nsRefPtr<nsCORSListenerProxy> corsListener =
      new nsCORSListenerProxy(this, mPrincipal, useCredentials);
    rv = corsListener->Init(chan, DataURIHandling::Allow);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return FailWithNetworkError();
    }
    listener = corsListener.forget();
  }

  // If preflight is required, start a "CORS preflight fetch"
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch-0. All the
  // implementation is handled by NS_StartCORSPreflight, we just set up the
  // unsafeHeaders so they can be verified against the response's
  // "Access-Control-Allow-Headers" header.
  if (aCORSPreflightFlag) {
    MOZ_ASSERT(mRequest->Mode() != RequestMode::No_cors,
               "FetchDriver::ContinueFetch() should ensure that the request is not no-cors");
    MOZ_ASSERT(httpChan, "CORS preflight can only be used with HTTP channels");
    nsAutoTArray<nsCString, 5> unsafeHeaders;
    mRequest->Headers()->GetUnsafeHeaders(unsafeHeaders);

    nsCOMPtr<nsIHttpChannelInternal> internalChan = do_QueryInterface(httpChan);
    rv = internalChan->SetCorsPreflightParameters(unsafeHeaders, useCredentials, mPrincipal);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return FailWithNetworkError();
    }
  }

  rv = chan->AsyncOpen(listener, nullptr);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return FailWithNetworkError();
  }

  // Step 4 onwards of "HTTP Fetch" is handled internally by Necko.
  return NS_OK;
}

nsresult
FetchDriver::ContinueHttpFetchAfterNetworkFetch()
{
  workers::AssertIsOnMainThread();
  MOZ_ASSERT(mResponse);
  MOZ_ASSERT(!mResponse->IsError());

  return SucceedWithResponse();
}

already_AddRefed<InternalResponse>
FetchDriver::BeginAndGetFilteredResponse(InternalResponse* aResponse, nsIURI* aFinalURI)
{
  MOZ_ASSERT(aResponse);
  nsAutoCString reqURL;
  if (aFinalURI) {
    aFinalURI->GetSpec(reqURL);
  } else {
    mRequest->GetURL(reqURL);
  }
  DebugOnly<nsresult> rv = aResponse->StripFragmentAndSetUrl(reqURL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // FIXME(nsm): Handle mixed content check, step 7 of fetch.

  nsRefPtr<InternalResponse> filteredResponse;
  switch (mRequest->GetResponseTainting()) {
    case InternalRequest::RESPONSETAINT_BASIC:
      filteredResponse = aResponse->BasicResponse();
      break;
    case InternalRequest::RESPONSETAINT_CORS:
      filteredResponse = aResponse->CORSResponse();
      break;
    case InternalRequest::RESPONSETAINT_OPAQUE:
      filteredResponse = aResponse->OpaqueResponse();
      break;
    case InternalRequest::RESPONSETAINT_OPAQUEREDIRECT:
      filteredResponse = aResponse->OpaqueRedirectResponse();
      break;
    default:
      MOZ_CRASH("Unexpected case");
  }

  MOZ_ASSERT(filteredResponse);
  MOZ_ASSERT(mObserver);
  mObserver->OnResponseAvailable(filteredResponse);
  mResponseAvailableCalled = true;
  return filteredResponse.forget();
}

void
FetchDriver::BeginResponse(InternalResponse* aResponse)
{
  nsRefPtr<InternalResponse> r = BeginAndGetFilteredResponse(aResponse, nullptr);
  // Release the ref.
}

nsresult
FetchDriver::SucceedWithResponse()
{
  workers::AssertIsOnMainThread();
  if (mObserver) {
    mObserver->OnResponseEnd();
    mObserver = nullptr;
  }
  return NS_OK;
}

nsresult
FetchDriver::FailWithNetworkError()
{
  workers::AssertIsOnMainThread();
  nsRefPtr<InternalResponse> error = InternalResponse::NetworkError();
  if (mObserver) {
    mObserver->OnResponseAvailable(error);
    mResponseAvailableCalled = true;
    mObserver->OnResponseEnd();
    mObserver = nullptr;
  }
  return NS_OK;
}

namespace {
class FillResponseHeaders final : public nsIHttpHeaderVisitor {
  InternalResponse* mResponse;

  ~FillResponseHeaders()
  { }
public:
  NS_DECL_ISUPPORTS

  explicit FillResponseHeaders(InternalResponse* aResponse)
    : mResponse(aResponse)
  {
  }

  NS_IMETHOD
  VisitHeader(const nsACString & aHeader, const nsACString & aValue) override
  {
    ErrorResult result;
    mResponse->Headers()->Append(aHeader, aValue, result);
    if (result.Failed()) {
      NS_WARNING(nsPrintfCString("Fetch ignoring illegal header - '%s': '%s'",
                                 PromiseFlatCString(aHeader).get(),
                                 PromiseFlatCString(aValue).get()).get());
      result.SuppressException();
    }
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(FillResponseHeaders, nsIHttpHeaderVisitor)
} // namespace

NS_IMETHODIMP
FetchDriver::OnStartRequest(nsIRequest* aRequest,
                            nsISupports* aContext)
{
  workers::AssertIsOnMainThread();

  // Note, this can be called multiple times if we are doing an opaqueredirect.
  // In that case we will get a simulated OnStartRequest() and then the real
  // channel will call in with an errored OnStartRequest().

  nsresult rv;
  aRequest->GetStatus(&rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  // We should only get to the following code once.
  MOZ_ASSERT(!mPipeOutputStream);
  MOZ_ASSERT(mObserver);

  nsRefPtr<InternalResponse> response;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
  if (httpChannel) {
    uint32_t responseStatus;
    httpChannel->GetResponseStatus(&responseStatus);

    nsAutoCString statusText;
    httpChannel->GetResponseStatusText(statusText);

    response = new InternalResponse(responseStatus, statusText);

    nsRefPtr<FillResponseHeaders> visitor = new FillResponseHeaders(response);
    rv = httpChannel->VisitResponseHeaders(visitor);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      NS_WARNING("Failed to visit all headers.");
    }
  } else {
    nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(aRequest);
    // If it is not an http channel, it has to be a jar one.
    MOZ_ASSERT(jarChannel);

    // We simulate the http protocol for jar/app requests
    uint32_t responseStatus = 200;
    nsAutoCString statusText;
    response = new InternalResponse(responseStatus, NS_LITERAL_CSTRING("OK"));
    ErrorResult result;
    nsAutoCString contentType;
    jarChannel->GetContentType(contentType);
    response->Headers()->Append(NS_LITERAL_CSTRING("content-type"),
                                contentType,
                                result);
    MOZ_ASSERT(!result.Failed());
  }

  // We open a pipe so that we can immediately set the pipe's read end as the
  // response's body. Setting the segment size to UINT32_MAX means that the
  // pipe has infinite space. The nsIChannel will continue to buffer data in
  // xpcom events even if we block on a fixed size pipe.  It might be possible
  // to suspend the channel and then resume when there is space available, but
  // for now use an infinite pipe to avoid blocking.
  nsCOMPtr<nsIInputStream> pipeInputStream;
  rv = NS_NewPipe(getter_AddRefs(pipeInputStream),
                  getter_AddRefs(mPipeOutputStream),
                  0, /* default segment size */
                  UINT32_MAX /* infinite pipe */,
                  true /* non-blocking input, otherwise you deadlock */,
                  false /* blocking output, since the pipe is 'in'finite */ );
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    // Cancel request.
    return rv;
  }
  response->SetBody(pipeInputStream);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  response->InitChannelInfo(channel);

  nsCOMPtr<nsIURI> channelURI;
  rv = channel->GetURI(getter_AddRefs(channelURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    // Cancel request.
    return rv;
  }

  // Resolves fetch() promise which may trigger code running in a worker.  Make
  // sure the Response is fully initialized before calling this.
  mResponse = BeginAndGetFilteredResponse(response, channelURI);

  nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    // Cancel request.
    return rv;
  }

  // Try to retarget off main thread.
  if (nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(aRequest)) {
    NS_WARN_IF(NS_FAILED(rr->RetargetDeliveryTo(sts)));
  }
  return NS_OK;
}

NS_IMETHODIMP
FetchDriver::OnDataAvailable(nsIRequest* aRequest,
                             nsISupports* aContext,
                             nsIInputStream* aInputStream,
                             uint64_t aOffset,
                             uint32_t aCount)
{
  // NB: This can be called on any thread!  But we're guaranteed that it is
  // called between OnStartRequest and OnStopRequest, so we don't need to worry
  // about races.

  uint32_t aRead;
  MOZ_ASSERT(mResponse);
  MOZ_ASSERT(mPipeOutputStream);

  nsresult rv = aInputStream->ReadSegments(NS_CopySegmentToStream,
                                           mPipeOutputStream,
                                           aCount, &aRead);
  return rv;
}

NS_IMETHODIMP
FetchDriver::OnStopRequest(nsIRequest* aRequest,
                           nsISupports* aContext,
                           nsresult aStatusCode)
{
  workers::AssertIsOnMainThread();
  if (NS_FAILED(aStatusCode)) {
    nsCOMPtr<nsIAsyncOutputStream> outputStream = do_QueryInterface(mPipeOutputStream);
    if (outputStream) {
      outputStream->CloseWithStatus(NS_BINDING_FAILED);
    }
    // We proceed as usual here, since we've already created a successful response
    // from OnStartRequest.
    SucceedWithResponse();
    return aStatusCode;
  }

  if (mPipeOutputStream) {
    mPipeOutputStream->Close();
  }

  ContinueHttpFetchAfterNetworkFetch();
  return NS_OK;
}

// This is called when the channel is redirected.
NS_IMETHODIMP
FetchDriver::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                    nsIChannel* aNewChannel,
                                    uint32_t aFlags,
                                    nsIAsyncVerifyRedirectCallback *aCallback)
{
  NS_PRECONDITION(aNewChannel, "Redirect without a channel?");

  nsresult rv;

  // HTTP Fetch step 5, "redirect status", step 1
  if (NS_WARN_IF(mRequest->GetRedirectMode() == RequestRedirect::Error)) {
    aOldChannel->Cancel(NS_BINDING_FAILED);
    return NS_BINDING_FAILED;
  }

  // HTTP Fetch step 5, "redirect status", steps 2 through 6 are automatically
  // handled by necko before calling AsyncOnChannelRedirect() with the new
  // nsIChannel.

  // HTTP Fetch step 5, "redirect status", steps 7 and 8 enforcing a redirect
  // count are done by Necko.  The pref used is "network.http.redirection-limit"
  // which is set to 20 by default.

  // HTTP Fetch Step 9, "redirect status". We only unset this for spec
  // compatibility. Any actions we take on mRequest here do not affect what the
  //channel does.
  mRequest->UnsetSameOriginDataURL();

  // HTTP Fetch step 5, "redirect status", step 10 requires us to halt the
  // redirect, but successfully return an opaqueredirect Response to the
  // initiating Fetch.
  if (mRequest->GetRedirectMode() == RequestRedirect::Manual) {
    // Ideally we would simply not cancel the old channel and allow it to
    // be processed as normal.  Unfortunately this is quite fragile and
    // other redirect handlers can easily break it for certain use cases.
    //
    // For example, nsCORSListenerProxy cancels vetoed redirect channels.
    // The HTTP cache will also error on vetoed redirects when the
    // redirect has been previously cached.
    //
    // Therefore simulate the completion of the channel to produce the
    // opaqueredirect Response and then cancel the original channel.  This
    // will result in OnStartRequest() getting called twice, but the second
    // time will be with an error response (from the Cancel) which will
    // be ignored.
    mRequest->SetResponseTainting(InternalRequest::RESPONSETAINT_OPAQUEREDIRECT);
    unused << OnStartRequest(aOldChannel, nullptr);
    unused << OnStopRequest(aOldChannel, nullptr, NS_OK);

    aOldChannel->Cancel(NS_BINDING_FAILED);

    return NS_BINDING_FAILED;
  }

  // The following steps are from HTTP Fetch step 5, "redirect status", step 11
  // which requires the RequestRedirect to be "follow".
  MOZ_ASSERT(mRequest->GetRedirectMode() == RequestRedirect::Follow);

  // HTTP Fetch step 5, "redirect status", steps 11.1 and 11.2 block redirecting
  // to a URL with credentials in CORS mode.  This is implemented in
  // nsCORSListenerProxy.

  mRedirectCallback = aCallback;
  mOldRedirectChannel = aOldChannel;
  mNewRedirectChannel = aNewChannel;

  nsCOMPtr<nsIChannelEventSink> outer =
    do_GetInterface(mNotificationCallbacks);
  if (outer) {
    // The callee is supposed to call OnRedirectVerifyCallback() on success,
    // and nobody has to call it on failure, so we can just return after this
    // block.
    rv = outer->AsyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags, this);
    if (NS_FAILED(rv)) {
      aOldChannel->Cancel(rv);
      mRedirectCallback = nullptr;
      mOldRedirectChannel = nullptr;
      mNewRedirectChannel = nullptr;
    }
    return rv;
  }

  (void) OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
FetchDriver::CheckListenerChain()
{
  return NS_OK;
}

// Returns NS_OK if no preflight is required, error otherwise.
nsresult
FetchDriver::DoesNotRequirePreflight(nsIChannel* aChannel)
{
  // If this is a same-origin request or the channel's URI inherits
  // its principal, it's allowed.
  if (nsContentUtils::CheckMayLoad(mPrincipal, aChannel, true)) {
    return NS_OK;
  }

  // Check if we need to do a preflight request.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  NS_ENSURE_TRUE(httpChannel, NS_ERROR_DOM_BAD_URI);

  nsAutoCString method;
  httpChannel->GetRequestMethod(method);
  if (mRequest->Mode() == RequestMode::Cors_with_forced_preflight ||
      !mRequest->Headers()->HasOnlySimpleHeaders() ||
      (!method.LowerCaseEqualsLiteral("get") &&
       !method.LowerCaseEqualsLiteral("post") &&
       !method.LowerCaseEqualsLiteral("head"))) {
    return NS_ERROR_DOM_BAD_URI;
  }

  return NS_OK;
}

NS_IMETHODIMP
FetchDriver::GetInterface(const nsIID& aIID, void **aResult)
{
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    *aResult = static_cast<nsIChannelEventSink*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  nsresult rv;

  if (mNotificationCallbacks) {
    rv = mNotificationCallbacks->GetInterface(aIID, aResult);
    if (NS_SUCCEEDED(rv)) {
      NS_ASSERTION(*aResult, "Lying nsIInterfaceRequestor implementation!");
      return rv;
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIStreamListener))) {
    *aResult = static_cast<nsIStreamListener*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(NS_GET_IID(nsIRequestObserver))) {
    *aResult = static_cast<nsIRequestObserver*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
FetchDriver::OnRedirectVerifyCallback(nsresult aResult)
{
  // On a successful redirect we perform the following substeps of HTTP Fetch,
  // step 5, "redirect status", step 11.
  if (NS_SUCCEEDED(aResult)) {
    // Step 11.5 "Append locationURL to request's url list." so that when we set the
    // Response's URL from the Request's URL in Main Fetch, step 15, we get the
    // final value. Note, we still use a single URL value instead of a list.
    nsCOMPtr<nsIURI> newURI;
    nsresult rv = NS_GetFinalChannelURI(mNewRedirectChannel, getter_AddRefs(newURI));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResult = rv;
    } else {
      // We need to update our request's URL.
      nsAutoCString newUrl;
      newURI->GetSpec(newUrl);
      mRequest->SetURL(newUrl);
    }
  }

  if (NS_FAILED(aResult)) {
    mOldRedirectChannel->Cancel(aResult);
  }

  // Implement Main Fetch step 8 again on redirect.
  MainFetchOp nextOp = SetTaintingAndGetNextOp(mCORSFlagEverSet);

  if (nextOp.mType == NETWORK_ERROR) {
    // Cancel the channel if Main Fetch blocks the redirect from continuing.
    aResult = NS_ERROR_DOM_BAD_URI;
    mOldRedirectChannel->Cancel(aResult);
  } else {
    // Otherwise, we rely on necko and the CORS proxy to do the right thing
    // as the redirect is followed.  In general this means basic or http
    // fetch.  If we've ever been CORS, we need to stay CORS.
    MOZ_ASSERT(nextOp.mType == BASIC_FETCH || nextOp.mType == HTTP_FETCH);
    MOZ_ASSERT_IF(mCORSFlagEverSet, nextOp.mType == HTTP_FETCH);
    MOZ_ASSERT_IF(mCORSFlagEverSet, nextOp.mCORSFlag);
  }

  mOldRedirectChannel = nullptr;
  mNewRedirectChannel = nullptr;
  mRedirectCallback->OnRedirectVerifyCallback(aResult);
  mRedirectCallback = nullptr;
  return NS_OK;
}

void
FetchDriver::SetDocument(nsIDocument* aDocument)
{
  // Cannot set document after Fetch() has been called.
  MOZ_ASSERT(mFetchRecursionCount == 0);
  mDocument = aDocument;
}

} // namespace dom
} // namespace mozilla
