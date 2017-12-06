/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/FetchDriver.h"

#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIDocument.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsPriority.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIUploadChannel2.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPipe.h"

#include "nsContentPolicyUtils.h"
#include "nsDataHandler.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsHttpChannel.h"

#include "mozilla/dom/File.h"
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/Unused.h"

#include "Fetch.h"
#include "FetchUtil.h"
#include "InternalRequest.h"
#include "InternalResponse.h"

namespace mozilla {
namespace dom {

namespace {

bool
ShouldCheckSRI(const InternalRequest* const aRequest,
               const InternalResponse* const aResponse)
{
  MOZ_DIAGNOSTIC_ASSERT(aRequest);
  MOZ_DIAGNOSTIC_ASSERT(aResponse);

  return !aRequest->GetIntegrity().IsEmpty() &&
         aResponse->Type() != ResponseType::Error;
}

} // anonymous namespace

//-----------------------------------------------------------------------------
// AlternativeDataStreamListener
//-----------------------------------------------------------------------------
class AlternativeDataStreamListener final : public nsIStreamListener,
                                            public nsIThreadRetargetableStreamListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  // The status of AlternativeDataStreamListener
  // LOADING: is the initial status, loading the alternative data
  // COMPLETED: Alternative data loading is completed
  // CANCELED: Alternative data loading is canceled, this would make
  //           AlternativeDataStreamListener ignore all channel callbacks
  // FALLBACK: fallback the channel callbacks to FetchDriver
  // Depends on different situaions, the status transition could be followings
  // 1. LOADING->COMPLETED
  //    This is the normal status transition for alternative data loading
  //
  // 2. LOADING->CANCELED
  //    LOADING->COMPLETED->CANCELED
  //    Alternative data loading could be canceled when cacheId from alternative
  //    data channel does not match with from main data channel(The cacheID
  //    checking is in FetchDriver::OnStartRequest).
  //    Notice the alternative data loading could finish before the cacheID
  //    checking, so the statust transition could be LOADING->COMPLETED->CANCELED
  //
  // 3. LOADING->FALLBACK
  //    For the case that alternative data loading could not be initialized, i.e.
  //    alternative data does not exist or no preferred alternative data type is
  //    requested. Once the status becomes FALLBACK, AlternativeDataStreamListener
  //    transits the channel callback request to FetchDriver, and the status
  //    should not go back to LOADING, COMPLETED, or CANCELED anymore.
  enum eStatus {
    LOADING = 0,
    COMPLETED,
    CANCELED,
    FALLBACK
  };

  AlternativeDataStreamListener(FetchDriver* aFetchDriver,
                                nsIChannel* aChannel,
                                const nsACString& aAlternativeDataType);
  eStatus Status();
  void Cancel();
  uint64_t GetAlternativeDataCacheEntryId();
  const nsACString& GetAlternativeDataType() const;
  already_AddRefed<nsICacheInfoChannel> GetCacheInfoChannel();
  already_AddRefed<nsIInputStream> GetAlternativeInputStream();

private:
  ~AlternativeDataStreamListener() = default;

  // This creates a strong reference cycle with FetchDriver and its
  // mAltDataListener. We need to clear at least one reference of them once the
  // data loading finishes.
  RefPtr<FetchDriver> mFetchDriver;
  nsCString mAlternativeDataType;
  nsCOMPtr<nsIInputStream> mPipeAlternativeInputStream;
  nsCOMPtr<nsIOutputStream> mPipeAlternativeOutputStream;
  uint64_t mAlternativeDataCacheEntryId;
  nsCOMPtr<nsICacheInfoChannel> mCacheInfoChannel;
  nsCOMPtr<nsIChannel> mChannel;
  Atomic<eStatus> mStatus;
};

NS_IMPL_ISUPPORTS(AlternativeDataStreamListener,
                  nsIStreamListener,
                  nsIThreadRetargetableStreamListener)

AlternativeDataStreamListener::AlternativeDataStreamListener(FetchDriver* aFetchDriver,
                                                             nsIChannel* aChannel,
                                                             const nsACString& aAlternativeDataType)
  : mFetchDriver(aFetchDriver)
  , mAlternativeDataType(aAlternativeDataType)
  , mAlternativeDataCacheEntryId(0)
  , mChannel(aChannel)
  , mStatus(AlternativeDataStreamListener::LOADING)
{
  MOZ_DIAGNOSTIC_ASSERT(mFetchDriver);
  MOZ_DIAGNOSTIC_ASSERT(mChannel);
}

AlternativeDataStreamListener::eStatus
AlternativeDataStreamListener::Status()
{
  return mStatus;
}

void
AlternativeDataStreamListener::Cancel()
{
  mAlternativeDataCacheEntryId = 0;
  mCacheInfoChannel = nullptr;
  mPipeAlternativeOutputStream = nullptr;
  mPipeAlternativeInputStream = nullptr;
  if (mChannel && mStatus != AlternativeDataStreamListener::FALLBACK) {
    // if mStatus is fallback, we need to keep channel to forward request back to
    // FetchDriver
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nullptr;
  }
  mStatus = AlternativeDataStreamListener::CANCELED;
}

uint64_t
AlternativeDataStreamListener::GetAlternativeDataCacheEntryId()
{
  return mAlternativeDataCacheEntryId;
}

const nsACString&
AlternativeDataStreamListener::GetAlternativeDataType() const
{
  return mAlternativeDataType;
}

already_AddRefed<nsIInputStream>
AlternativeDataStreamListener::GetAlternativeInputStream()
{
  nsCOMPtr<nsIInputStream> inputStream = mPipeAlternativeInputStream;
  return inputStream.forget();
}

already_AddRefed<nsICacheInfoChannel>
AlternativeDataStreamListener::GetCacheInfoChannel()
{
  nsCOMPtr<nsICacheInfoChannel> channel = mCacheInfoChannel;
  return channel.forget();
}

NS_IMETHODIMP
AlternativeDataStreamListener::OnStartRequest(nsIRequest* aRequest,
                                              nsISupports* aContext)
{
  workers::AssertIsOnMainThread();
  MOZ_ASSERT(!mAlternativeDataType.IsEmpty());
  // Checking the alternative data type is the same between we asked and the
  // saved in the channel.
  nsAutoCString alternativeDataType;
  nsCOMPtr<nsICacheInfoChannel> cic = do_QueryInterface(aRequest);
  mStatus = AlternativeDataStreamListener::LOADING;
  if (cic &&
      NS_SUCCEEDED(cic->GetAlternativeDataType(alternativeDataType)) &&
      mAlternativeDataType.Equals(alternativeDataType) &&
      NS_SUCCEEDED(cic->GetCacheEntryId(&mAlternativeDataCacheEntryId))) {

    MOZ_DIAGNOSTIC_ASSERT(!mPipeAlternativeInputStream);
    MOZ_DIAGNOSTIC_ASSERT(!mPipeAlternativeOutputStream);
    nsresult rv = NS_NewPipe(
      getter_AddRefs(mPipeAlternativeInputStream),
      getter_AddRefs(mPipeAlternativeOutputStream),
      0 /* default segment size */,
      UINT32_MAX /* infinite pipe */,
      true /* non-blocking input, otherwise you deadlock */,
      false /* blocking output, since the pipe is 'in'finite */);

    if (NS_FAILED(rv)) {
      mFetchDriver->FailWithNetworkError(rv);
      return rv;
    }

    MOZ_DIAGNOSTIC_ASSERT(!mCacheInfoChannel);
    mCacheInfoChannel = cic;

    // call FetchDriver::HttpFetch to load main body
    MOZ_ASSERT(mFetchDriver);
    return mFetchDriver->HttpFetch();

  } else {
    // Needn't load alternative data, since alternative data does not exist.
    // Set status to FALLBACK to reuse the opened channel to load main body, then
    // call FetchDriver::OnStartRequest to continue the work.
    // Unfortunately can't change the stream listener to mFetchDriver, need to
    // keep AlternativeDataStreamListener alive to redirect OnDataAvailable and
    // OnStopRequest to mFetchDriver.
    MOZ_ASSERT(alternativeDataType.IsEmpty());
    mStatus = AlternativeDataStreamListener::FALLBACK;
    mAlternativeDataCacheEntryId = 0;
    MOZ_ASSERT(mFetchDriver);
    return mFetchDriver->OnStartRequest(aRequest, aContext);
  }
  return NS_OK;
}

NS_IMETHODIMP
AlternativeDataStreamListener::OnDataAvailable(nsIRequest* aRequest,
                                               nsISupports* aContext,
                                               nsIInputStream* aInputStream,
                                               uint64_t aOffset,
                                               uint32_t aCount)
{
  if (mStatus == AlternativeDataStreamListener::LOADING) {
    MOZ_ASSERT(mPipeAlternativeOutputStream);
    uint32_t read;
    return aInputStream->ReadSegments(NS_CopySegmentToStream,
                                      mPipeAlternativeOutputStream,
                                      aCount, &read);
  }
  if (mStatus == AlternativeDataStreamListener::FALLBACK) {
    MOZ_ASSERT(mFetchDriver);
    return mFetchDriver->OnDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount);
  }
  return NS_OK;
}

NS_IMETHODIMP
AlternativeDataStreamListener::OnStopRequest(nsIRequest* aRequest,
                                             nsISupports* aContext,
                                             nsresult aStatusCode)
{
  workers::AssertIsOnMainThread();

  // Alternative data loading is going to finish, breaking the reference cycle
  // here by taking the ownership to a loacl variable.
  RefPtr<FetchDriver> fetchDriver = mFetchDriver.forget();

  if (mStatus == AlternativeDataStreamListener::CANCELED) {
    // do nothing
    return NS_OK;
  }

  if (mStatus == AlternativeDataStreamListener::FALLBACK) {
    MOZ_ASSERT(fetchDriver);
    return fetchDriver->OnStopRequest(aRequest, aContext, aStatusCode);
  }

  MOZ_DIAGNOSTIC_ASSERT(mStatus == AlternativeDataStreamListener::LOADING);

  MOZ_ASSERT(!mAlternativeDataType.IsEmpty() &&
             mPipeAlternativeOutputStream &&
             mPipeAlternativeInputStream);

  mPipeAlternativeOutputStream->Close();
  mPipeAlternativeOutputStream = nullptr;

  // Cleanup the states for alternative data if needed.
  if (NS_FAILED(aStatusCode)) {
    mAlternativeDataCacheEntryId = 0;
    mCacheInfoChannel = nullptr;
    mPipeAlternativeInputStream = nullptr;
  }
  mStatus = AlternativeDataStreamListener::COMPLETED;
  // alternative data loading finish, call FetchDriver::FinishOnStopRequest to
  // continue the final step for the case FetchDriver::OnStopRequest is called
  // earlier than AlternativeDataStreamListener::OnStopRequest
  MOZ_ASSERT(fetchDriver);
  return fetchDriver->FinishOnStopRequest(this);
}

NS_IMETHODIMP
AlternativeDataStreamListener::CheckListenerChain()
{
  return NS_OK;
}
//-----------------------------------------------------------------------------
// FetchDriver
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(FetchDriver,
                  nsIStreamListener, nsIChannelEventSink, nsIInterfaceRequestor,
                  nsIThreadRetargetableStreamListener)

FetchDriver::FetchDriver(InternalRequest* aRequest, nsIPrincipal* aPrincipal,
                         nsILoadGroup* aLoadGroup, nsIEventTarget* aMainThreadEventTarget,
                         bool aIsTrackingFetch)
  : mPrincipal(aPrincipal)
  , mLoadGroup(aLoadGroup)
  , mRequest(aRequest)
  , mMainThreadEventTarget(aMainThreadEventTarget)
  , mNeedToObserveOnDataAvailable(false)
  , mIsTrackingFetch(aIsTrackingFetch)
#ifdef DEBUG
  , mResponseAvailableCalled(false)
  , mFetchCalled(false)
#endif
{
  AssertIsOnMainThread();

  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aMainThreadEventTarget);
}

FetchDriver::~FetchDriver()
{
  AssertIsOnMainThread();

  // We assert this since even on failures, we should call
  // FailWithNetworkError().
  MOZ_ASSERT(mResponseAvailableCalled);
}

nsresult
FetchDriver::Fetch(AbortSignal* aSignal, FetchDriverObserver* aObserver)
{
  workers::AssertIsOnMainThread();
#ifdef DEBUG
  MOZ_ASSERT(!mFetchCalled);
  mFetchCalled = true;
#endif

  mObserver = aObserver;

  Telemetry::Accumulate(Telemetry::SERVICE_WORKER_REQUEST_PASSTHROUGH,
                        mRequest->WasCreatedByFetchEvent());

  // FIXME(nsm): Deal with HSTS.

  MOZ_RELEASE_ASSERT(!mRequest->IsSynchronous(),
                     "Synchronous fetch not supported");


  UniquePtr<mozilla::ipc::PrincipalInfo> principalInfo(new mozilla::ipc::PrincipalInfo());
  nsresult rv = PrincipalToPrincipalInfo(mPrincipal, principalInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mRequest->SetPrincipalInfo(Move(principalInfo));

  // If the signal is aborted, it's time to inform the observer and terminate
  // the operation.
  if (aSignal) {
    if (aSignal->Aborted()) {
      Abort();
      return NS_OK;
    }

    Follow(aSignal);
  }

  rv = HttpFetch(mRequest->GetPreferredAlternativeDataType());
  if (NS_FAILED(rv)) {
    FailWithNetworkError(rv);
  }

  // Any failure is handled by FailWithNetworkError notifying the aObserver.
  return NS_OK;
}

// This function implements the "HTTP Fetch" algorithm from the Fetch spec.
// Functionality is often split between here, the CORS listener proxy and the
// Necko HTTP implementation.
nsresult
FetchDriver::HttpFetch(const nsACString& aPreferredAlternativeDataType)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Step 1. "Let response be null."
  mResponse = nullptr;
  mOnStopRequestCalled = false;
  nsresult rv;

  nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString url;
  mRequest->GetURL(url);
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), url, nullptr, nullptr, ios);
  NS_ENSURE_SUCCESS(rv, rv);

  // Unsafe requests aren't allowed with when using no-core mode.
  if (mRequest->Mode() == RequestMode::No_cors &&
      mRequest->UnsafeRequest() &&
      (!mRequest->HasSimpleMethod() ||
       !mRequest->Headers()->HasOnlySimpleHeaders())) {
    MOZ_ASSERT(false, "The API should have caught this");
    return NS_ERROR_DOM_BAD_URI;
  }

  // non-GET requests aren't allowed for blob.
  if (IsBlobURI(uri)) {
    nsAutoCString method;
    mRequest->GetMethod(method);
    if (!method.EqualsLiteral("GET")) {
      return NS_ERROR_DOM_NETWORK_ERR;
    }
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

  // Step 3.3 "Let credentials flag be set if one of
  //  - request's credentials mode is "include"
  //  - request's credentials mode is "same-origin" and either the CORS flag
  //    is unset or response tainting is "opaque"
  // is true, and unset otherwise."

  // Set skip serviceworker flag.
  // While the spec also gates on the client being a ServiceWorker, we can't
  // infer that here. Instead we rely on callers to set the flag correctly.
  const nsLoadFlags bypassFlag = mRequest->SkipServiceWorker() ?
                                 nsIChannel::LOAD_BYPASS_SERVICE_WORKER : 0;

  nsSecurityFlags secFlags = nsILoadInfo::SEC_ABOUT_BLANK_INHERITS;
  if (mRequest->Mode() == RequestMode::Cors) {
    secFlags |= nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;
  } else if (mRequest->Mode() == RequestMode::Same_origin ||
             mRequest->Mode() == RequestMode::Navigate) {
    secFlags |= nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS;
  } else if (mRequest->Mode() == RequestMode::No_cors) {
    secFlags |= nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS;
  } else {
    MOZ_ASSERT_UNREACHABLE("Unexpected request mode!");
    return NS_ERROR_UNEXPECTED;
  }

  if (mRequest->GetRedirectMode() != RequestRedirect::Follow) {
    secFlags |= nsILoadInfo::SEC_DONT_FOLLOW_REDIRECTS;
  }

  // This is handles the use credentials flag in "HTTP
  // network or cache fetch" in the spec and decides whether to transmit
  // cookies and other identifying information.
  if (mRequest->GetCredentialsMode() == RequestCredentials::Include) {
    secFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
  } else if (mRequest->GetCredentialsMode() == RequestCredentials::Omit) {
    secFlags |= nsILoadInfo::SEC_COOKIES_OMIT;
  } else if (mRequest->GetCredentialsMode() == RequestCredentials::Same_origin) {
    secFlags |= nsILoadInfo::SEC_COOKIES_SAME_ORIGIN;
  } else {
    MOZ_ASSERT_UNREACHABLE("Unexpected credentials mode!");
    return NS_ERROR_UNEXPECTED;
  }

  // From here on we create a channel and set its properties with the
  // information from the InternalRequest. This is an implementation detail.
  MOZ_ASSERT(mLoadGroup);
  nsCOMPtr<nsIChannel> chan;

  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL |
    bypassFlag | nsIChannel::LOAD_CLASSIFY_URI;
  if (mDocument) {
    MOZ_ASSERT(mDocument->NodePrincipal() == mPrincipal);
    rv = NS_NewChannel(getter_AddRefs(chan),
                       uri,
                       mDocument,
                       secFlags,
                       mRequest->ContentPolicyType(),
                       mLoadGroup,
                       nullptr, /* aCallbacks */
                       loadFlags,
                       ios);
  } else {
    rv = NS_NewChannel(getter_AddRefs(chan),
                       uri,
                       mPrincipal,
                       secFlags,
                       mRequest->ContentPolicyType(),
                       mLoadGroup,
                       nullptr, /* aCallbacks */
                       loadFlags,
                       ios);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Insert ourselves into the notification callbacks chain so we can set
  // headers on redirects.
#ifdef DEBUG
  {
    nsCOMPtr<nsIInterfaceRequestor> notificationCallbacks;
    chan->GetNotificationCallbacks(getter_AddRefs(notificationCallbacks));
    MOZ_ASSERT(!notificationCallbacks);
  }
#endif
  chan->SetNotificationCallbacks(this);

  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(chan));
  // Mark channel as urgent-start if the Fetch is triggered by user input
  // events.
  if (cos && EventStateManager::IsHandlingUserInput()) {
    cos->AddClassFlags(nsIClassOfService::UrgentStart);
  }

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
    NS_ENSURE_SUCCESS(rv, rv);

    // Set the same headers.
    SetRequestHeaders(httpChan);

    net::ReferrerPolicy net_referrerPolicy = mRequest->GetEnvironmentReferrerPolicy();
    // Step 6 of
    // https://fetch.spec.whatwg.org/#main-fetch
    // If request's referrer policy is the empty string and request's client is
    // non-null, then set request's referrer policy to request's client's
    // associated referrer policy.
    // Basically, "client" is not in our implementation, we use
    // EnvironmentReferrerPolicy of the worker or document context
    if (mRequest->ReferrerPolicy_() == ReferrerPolicy::_empty) {
      mRequest->SetReferrerPolicy(net_referrerPolicy);
    }
    // Step 7 of
    // https://fetch.spec.whatwg.org/#main-fetch
    // If request’s referrer policy is the empty string,
    // then set request’s referrer policy to "no-referrer-when-downgrade".
    if (mRequest->ReferrerPolicy_() == ReferrerPolicy::_empty) {
      net::ReferrerPolicy referrerPolicy =
        static_cast<net::ReferrerPolicy>(NS_GetDefaultReferrerPolicy());
      mRequest->SetReferrerPolicy(referrerPolicy);
    }

    rv = FetchUtil::SetRequestReferrer(mPrincipal,
                                       mDocument,
                                       httpChan,
                                       mRequest);
    NS_ENSURE_SUCCESS(rv, rv);

    // Bug 1120722 - Authorization will be handled later.
    // Auth may require prompting, we don't support it yet.
    // The next patch in this same bug prevents this from aborting the request.
    // Credentials checks for CORS are handled by nsCORSListenerProxy,

    nsCOMPtr<nsIHttpChannelInternal> internalChan = do_QueryInterface(httpChan);

    // Conversion between enumerations is safe due to static asserts in
    // dom/workers/ServiceWorkerManager.cpp
    rv = internalChan->SetCorsMode(static_cast<uint32_t>(mRequest->Mode()));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = internalChan->SetRedirectMode(static_cast<uint32_t>(mRequest->GetRedirectMode()));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    mRequest->MaybeSkipCacheIfPerformingRevalidation();
    rv = internalChan->SetFetchCacheMode(static_cast<uint32_t>(mRequest->GetCacheMode()));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = internalChan->SetIntegrityMetadata(mRequest->GetIntegrity());
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  // Step 5. Proxy authentication will be handled by Necko.

  // Continue setting up 'HTTPRequest'. Content-Type and body data.
  nsCOMPtr<nsIUploadChannel2> uploadChan = do_QueryInterface(chan);
  if (uploadChan) {
    nsAutoCString contentType;
    ErrorResult result;
    mRequest->Headers()->GetFirst(NS_LITERAL_CSTRING("content-type"), contentType, result);
    // We don't actually expect "result" to have failed here: that only happens
    // for invalid header names.  But if for some reason it did, just propagate
    // it out.
    if (result.Failed()) {
      return result.StealNSResult();
    }

    // Now contentType is the header that was set in mRequest->Headers(), or a
    // void string if no header was set.
#ifdef DEBUG
    bool hasContentTypeHeader =
      mRequest->Headers()->Has(NS_LITERAL_CSTRING("content-type"), result);
    MOZ_ASSERT(!result.Failed());
    MOZ_ASSERT_IF(!hasContentTypeHeader, contentType.IsVoid());
#endif // DEBUG

    int64_t bodyLength;
    nsCOMPtr<nsIInputStream> bodyStream;
    mRequest->GetBody(getter_AddRefs(bodyStream), &bodyLength);
    if (bodyStream) {
      nsAutoCString method;
      mRequest->GetMethod(method);
      rv = uploadChan->ExplicitSetUploadStream(bodyStream, contentType,
                                               bodyLength, method,
                                               false /* aStreamHasHeaders */);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // If preflight is required, start a "CORS preflight fetch"
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch-0. All the
  // implementation is handled by the http channel calling into
  // nsCORSListenerProxy. We just inform it which unsafe headers are included
  // in the request.
  if (mRequest->Mode() == RequestMode::Cors) {
    AutoTArray<nsCString, 5> unsafeHeaders;
    mRequest->Headers()->GetUnsafeHeaders(unsafeHeaders);
    nsCOMPtr<nsILoadInfo> loadInfo = chan->GetLoadInfo();
    if (loadInfo) {
      loadInfo->SetCorsPreflightInfo(unsafeHeaders, false);
    }
  }

  if (mIsTrackingFetch && nsContentUtils::IsTailingEnabled()) {
    cos->AddClassFlags(nsIClassOfService::Throttleable |
                       nsIClassOfService::Tail);
  }

  if (mIsTrackingFetch && nsContentUtils::IsLowerNetworkPriority()) {
    nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(chan);
    if (p) {
      p->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
    }
  }

  // if the preferred alternative data type in InternalRequest is not empty, set
  // the data type on the created channel and also create a AlternativeDataStreamListener
  // to be the stream listener of the channel.
  if (!aPreferredAlternativeDataType.IsEmpty()) {
    nsCOMPtr<nsICacheInfoChannel> cic = do_QueryInterface(chan);
    if (cic) {
      cic->PreferAlternativeDataType(aPreferredAlternativeDataType);
      MOZ_ASSERT(!mAltDataListener);
      mAltDataListener =
        new AlternativeDataStreamListener(this, chan, aPreferredAlternativeDataType);
      rv = chan->AsyncOpen2(mAltDataListener);
    } else {
      rv = chan->AsyncOpen2(this);
    }
  } else {
    rv = chan->AsyncOpen2(this);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Step 4 onwards of "HTTP Fetch" is handled internally by Necko.

  mChannel = chan;
  return NS_OK;
}
already_AddRefed<InternalResponse>
FetchDriver::BeginAndGetFilteredResponse(InternalResponse* aResponse,
                                         bool aFoundOpaqueRedirect)
{
  MOZ_ASSERT(aResponse);
  AutoTArray<nsCString, 4> reqURLList;
  mRequest->GetURLListWithoutFragment(reqURLList);
  MOZ_ASSERT(!reqURLList.IsEmpty());
  aResponse->SetURLList(reqURLList);
  RefPtr<InternalResponse> filteredResponse;
  if (aFoundOpaqueRedirect) {
    filteredResponse = aResponse->OpaqueRedirectResponse();
  } else {
    switch (mRequest->GetResponseTainting()) {
      case LoadTainting::Basic:
        filteredResponse = aResponse->BasicResponse();
        break;
      case LoadTainting::CORS:
        filteredResponse = aResponse->CORSResponse();
        break;
      case LoadTainting::Opaque: {
        filteredResponse = aResponse->OpaqueResponse();
        nsresult rv = filteredResponse->GeneratePaddingInfo();
        if (NS_WARN_IF(NS_FAILED(rv))) { return nullptr; }
        break;
      }
      default:
        MOZ_CRASH("Unexpected case");
    }
  }

  MOZ_ASSERT(filteredResponse);
  MOZ_ASSERT(mObserver);
  if (!ShouldCheckSRI(mRequest, filteredResponse)) {
    mObserver->OnResponseAvailable(filteredResponse);
  #ifdef DEBUG
    mResponseAvailableCalled = true;
  #endif
  }

  return filteredResponse.forget();
}

void
FetchDriver::FailWithNetworkError(nsresult rv)
{
  workers::AssertIsOnMainThread();
  RefPtr<InternalResponse> error = InternalResponse::NetworkError(rv);
  if (mObserver) {
    mObserver->OnResponseAvailable(error);
#ifdef DEBUG
    mResponseAvailableCalled = true;
#endif
    mObserver->OnResponseEnd(FetchDriverObserver::eByNetworking);
    mObserver = nullptr;
  }

  mChannel = nullptr;
}

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
  if (NS_FAILED(rv)) {
    FailWithNetworkError(rv);
    return rv;
  }

  // We should only get to the following code once.
  MOZ_ASSERT(!mPipeOutputStream);
  MOZ_ASSERT(mObserver);

  mNeedToObserveOnDataAvailable = mObserver->NeedOnDataAvailable();

  RefPtr<InternalResponse> response;
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);

  // On a successful redirect we perform the following substeps of HTTP Fetch,
  // step 5, "redirect status", step 11.

  bool foundOpaqueRedirect = false;

  int64_t contentLength = InternalResponse::UNKNOWN_BODY_SIZE;
  rv = channel->GetContentLength(&contentLength);
  MOZ_ASSERT_IF(NS_FAILED(rv), contentLength == InternalResponse::UNKNOWN_BODY_SIZE);

  if (httpChannel) {
    uint32_t responseStatus;
    rv = httpChannel->GetResponseStatus(&responseStatus);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (mozilla::net::nsHttpChannel::IsRedirectStatus(responseStatus)) {
      if (mRequest->GetRedirectMode() == RequestRedirect::Error) {
        FailWithNetworkError(NS_BINDING_ABORTED);
        return NS_BINDING_FAILED;
      }
      if (mRequest->GetRedirectMode() == RequestRedirect::Manual) {
        foundOpaqueRedirect = true;
      }
    }

    nsAutoCString statusText;
    rv = httpChannel->GetResponseStatusText(statusText);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    response = new InternalResponse(responseStatus, statusText);

    response->Headers()->FillResponseHeaders(httpChannel);

    // If Content-Encoding or Transfer-Encoding headers are set, then the actual
    // Content-Length (which refer to the decoded data) is obscured behind the encodings.
    ErrorResult result;
    if (response->Headers()->Has(NS_LITERAL_CSTRING("content-encoding"), result) ||
        response->Headers()->Has(NS_LITERAL_CSTRING("transfer-encoding"), result)) {
      NS_WARNING("Cannot know response Content-Length due to presence of Content-Encoding "
                 "or Transfer-Encoding headers.");
      contentLength = InternalResponse::UNKNOWN_BODY_SIZE;
    }
    MOZ_ASSERT(!result.Failed());
  } else {
    response = new InternalResponse(200, NS_LITERAL_CSTRING("OK"));

    ErrorResult result;
    nsAutoCString contentType;
    rv = channel->GetContentType(contentType);
    if (NS_SUCCEEDED(rv) && !contentType.IsEmpty()) {
      nsAutoCString contentCharset;
      channel->GetContentCharset(contentCharset);
      if (NS_SUCCEEDED(rv) && !contentCharset.IsEmpty()) {
        contentType += NS_LITERAL_CSTRING(";charset=") + contentCharset;
      }

      response->Headers()->Append(NS_LITERAL_CSTRING("Content-Type"),
                                  contentType,
                                  result);
      MOZ_ASSERT(!result.Failed());
    }

    if (contentLength > 0) {
      nsAutoCString contentLenStr;
      contentLenStr.AppendInt(contentLength);
      response->Headers()->Append(NS_LITERAL_CSTRING("Content-Length"),
                                  contentLenStr,
                                  result);
      MOZ_ASSERT(!result.Failed());
    }
  }

  nsCOMPtr<nsICacheInfoChannel> cic = do_QueryInterface(aRequest);
  if (cic && mAltDataListener) {
    // Skip the case that mAltDataListener->Status() equals to FALLBACK, that means
    // the opened channel for alternative data loading is reused for loading the
    // main data.
    if (mAltDataListener->Status() != AlternativeDataStreamListener::FALLBACK) {
      // Verify the cache ID is the same with from alternative data cache.
      // If the cache ID is different, droping the alternative data loading,
      // otherwise setup the response's alternative body and cacheInfoChannel.
      uint64_t cacheEntryId = 0;
      if (NS_SUCCEEDED(cic->GetCacheEntryId(&cacheEntryId)) &&
          cacheEntryId != mAltDataListener->GetAlternativeDataCacheEntryId()) {
        mAltDataListener->Cancel();
      } else {
        // AlternativeDataStreamListener::OnStartRequest had already been called,
        // the alternative data input stream and cacheInfo channel must be created.
        nsCOMPtr<nsICacheInfoChannel> cacheInfo = mAltDataListener->GetCacheInfoChannel();
        nsCOMPtr<nsIInputStream> altInputStream = mAltDataListener->GetAlternativeInputStream();
        MOZ_ASSERT(altInputStream && cacheInfo);
        response->SetAlternativeBody(altInputStream);
        nsMainThreadPtrHandle<nsICacheInfoChannel> handle(
          new nsMainThreadPtrHolder<nsICacheInfoChannel>("nsICacheInfoChannel",
                                                         cacheInfo,
                                                         false));
        response->SetCacheInfoChannel(handle);
      }
    } else if (!mAltDataListener->GetAlternativeDataType().IsEmpty()) {
      // If the status is FALLBACK and the mAltDataListener::mAlternativeDataType
      // is not empty, that means the data need to be saved into cache, setup the
      // response's nsICacheInfoChannel for caching the data after loading.
      nsMainThreadPtrHandle<nsICacheInfoChannel> handle(
        new nsMainThreadPtrHolder<nsICacheInfoChannel>("nsICacheInfoChannel",
                                                       cic,
                                                       false));
      response->SetCacheInfoChannel(handle);
    }
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
    FailWithNetworkError(rv);
    // Cancel request.
    return rv;
  }
  response->SetBody(pipeInputStream, contentLength);

  response->InitChannelInfo(channel);

  nsCOMPtr<nsIURI> channelURI;
  rv = channel->GetURI(getter_AddRefs(channelURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError(rv);
    // Cancel request.
    return rv;
  }

  nsCOMPtr<nsILoadInfo> loadInfo;
  rv = channel->GetLoadInfo(getter_AddRefs(loadInfo));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError(rv);
    return rv;
  }

  // Propagate any tainting from the channel back to our response here.  This
  // step is not reflected in the spec because the spec is written such that
  // FetchEvent.respondWith() just passes the already-tainted Response back to
  // the outer fetch().  In gecko, however, we serialize the Response through
  // the channel and must regenerate the tainting from the channel in the
  // interception case.
  mRequest->MaybeIncreaseResponseTainting(loadInfo->GetTainting());

  // Resolves fetch() promise which may trigger code running in a worker.  Make
  // sure the Response is fully initialized before calling this.
  mResponse = BeginAndGetFilteredResponse(response, foundOpaqueRedirect);
  if (NS_WARN_IF(!mResponse)) {
    // Fail to generate a paddingInfo for opaque response.
    MOZ_DIAGNOSTIC_ASSERT(mResponse->Type() == ResponseType::Opaque);
    FailWithNetworkError(NS_ERROR_UNEXPECTED);
    return rv;
  }

  // From "Main Fetch" step 19: SRI-part1.
  if (ShouldCheckSRI(mRequest, mResponse) && mSRIMetadata.IsEmpty()) {
    nsIConsoleReportCollector* reporter = nullptr;
    if (mObserver) {
      reporter = mObserver->GetReporter();
    }

    nsAutoCString sourceUri;
    if (mDocument && mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    } else if (!mWorkerScript.IsEmpty()) {
      sourceUri.Assign(mWorkerScript);
    }
    SRICheck::IntegrityMetadata(mRequest->GetIntegrity(), sourceUri,
                                reporter, &mSRIMetadata);
    mSRIDataVerifier = new SRICheckDataVerifier(mSRIMetadata, sourceUri,
                                                reporter);

    // Do not retarget off main thread when using SRI API.
    return NS_OK;
  }

  nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError(rv);
    // Cancel request.
    return rv;
  }

  // Try to retarget off main thread.
  if (nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(aRequest)) {
    Unused << NS_WARN_IF(NS_FAILED(rr->RetargetDeliveryTo(sts)));
  }
  return NS_OK;
}

namespace {

// Runnable to call the observer OnDataAvailable on the main-thread.
class DataAvailableRunnable final : public Runnable
{
  RefPtr<FetchDriverObserver> mObserver;

public:
  explicit DataAvailableRunnable(FetchDriverObserver* aObserver)
    : Runnable("dom::DataAvailableRunnable")
    , mObserver(aObserver)
  {
     MOZ_ASSERT(aObserver);
  }

  NS_IMETHOD
  Run() override
  {
    mObserver->OnDataAvailable();
    mObserver = nullptr;
    return NS_OK;
  }
};

struct SRIVerifierAndOutputHolder {
  SRIVerifierAndOutputHolder(SRICheckDataVerifier* aVerifier,
                             nsIOutputStream* aOutputStream)
    : mVerifier(aVerifier)
    , mOutputStream(aOutputStream)
  {}

  SRICheckDataVerifier* mVerifier;
  nsIOutputStream* mOutputStream;

private:
  SRIVerifierAndOutputHolder() = delete;
};

// Just like NS_CopySegmentToStream, but also sends the data into an
// SRICheckDataVerifier.
nsresult
CopySegmentToStreamAndSRI(nsIInputStream* aInStr,
                          void* aClosure,
                          const char* aBuffer,
                          uint32_t aOffset,
                          uint32_t aCount,
                          uint32_t* aCountWritten)
{
  auto holder = static_cast<SRIVerifierAndOutputHolder*>(aClosure);
  MOZ_DIAGNOSTIC_ASSERT(holder && holder->mVerifier && holder->mOutputStream,
                        "Bogus holder");
  nsresult rv =
    holder->mVerifier->Update(aCount,
                              reinterpret_cast<const uint8_t*>(aBuffer));
  NS_ENSURE_SUCCESS(rv, rv);

  // The rest is just like NS_CopySegmentToStream.
  *aCountWritten = 0;
  while (aCount) {
    uint32_t n = 0;
    rv = holder->mOutputStream->Write(aBuffer, aCount, &n);
    if (NS_FAILED(rv)) {
      return rv;
    }
    aBuffer += n;
    aCount -= n;
    *aCountWritten += n;
  }
  return NS_OK;
}

} // anonymous namespace

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

  if (mNeedToObserveOnDataAvailable) {
    mNeedToObserveOnDataAvailable = false;
    if (mObserver) {
      if (NS_IsMainThread()) {
        mObserver->OnDataAvailable();
      } else {
        RefPtr<Runnable> runnable = new DataAvailableRunnable(mObserver);
        nsresult rv =
          mMainThreadEventTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }

  uint32_t aRead;
  MOZ_ASSERT(mResponse);
  MOZ_ASSERT(mPipeOutputStream);

  // From "Main Fetch" step 19: SRI-part2.
  // Note: Avoid checking the hidden opaque body.
  if (mResponse->Type() != ResponseType::Opaque &&
      ShouldCheckSRI(mRequest, mResponse)) {
    MOZ_ASSERT(mSRIDataVerifier);

    SRIVerifierAndOutputHolder holder(mSRIDataVerifier, mPipeOutputStream);
    nsresult rv = aInputStream->ReadSegments(CopySegmentToStreamAndSRI,
                                             &holder, aCount, &aRead);
    return rv;
  }

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

  MOZ_DIAGNOSTIC_ASSERT(!mOnStopRequestCalled);
  mOnStopRequestCalled = true;

  // main data loading is going to finish, breaking the reference cycle.
  RefPtr<AlternativeDataStreamListener> altDataListener = mAltDataListener.forget();

  // We need to check mObserver, which is nulled by FailWithNetworkError(),
  // because in the case of "error" redirect mode, aStatusCode may be NS_OK but
  // mResponse will definitely be null so we must not take the else branch.
  if (NS_FAILED(aStatusCode) || !mObserver) {
    nsCOMPtr<nsIAsyncOutputStream> outputStream = do_QueryInterface(mPipeOutputStream);
    if (outputStream) {
      outputStream->CloseWithStatus(NS_BINDING_FAILED);
    }
    if (altDataListener) {
      altDataListener->Cancel();
    }

    // We proceed as usual here, since we've already created a successful response
    // from OnStartRequest.
  } else {
    MOZ_ASSERT(mResponse);
    MOZ_ASSERT(!mResponse->IsError());

    // From "Main Fetch" step 19: SRI-part3.
    if (ShouldCheckSRI(mRequest, mResponse)) {
      MOZ_ASSERT(mSRIDataVerifier);

      nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

      nsIConsoleReportCollector* reporter = nullptr;
      if (mObserver) {
        reporter = mObserver->GetReporter();
      }

      nsAutoCString sourceUri;
      if (mDocument && mDocument->GetDocumentURI()) {
        mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
      } else if (!mWorkerScript.IsEmpty()) {
        sourceUri.Assign(mWorkerScript);
      }
      nsresult rv = mSRIDataVerifier->Verify(mSRIMetadata, channel, sourceUri,
                                             reporter);
      if (NS_FAILED(rv)) {
        if (altDataListener) {
          altDataListener->Cancel();
        }
        FailWithNetworkError(rv);
        // Cancel request.
        return rv;
      }
    }

    if (mPipeOutputStream) {
      mPipeOutputStream->Close();
    }
  }

  return FinishOnStopRequest(altDataListener);
}

nsresult
FetchDriver::FinishOnStopRequest(AlternativeDataStreamListener* aAltDataListener)
{
  workers::AssertIsOnMainThread();
  // OnStopRequest is not called from channel, that means the main data loading
  // does not finish yet. Reaching here since alternative data loading finishes.
  if (!mOnStopRequestCalled) {
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(!mAltDataListener);
  // Wait for alternative data loading finish if we needed it.
  if (aAltDataListener &&
      aAltDataListener->Status() == AlternativeDataStreamListener::LOADING) {
    // For LOADING case, channel holds the reference of altDataListener, no need
    // to restore it to mAltDataListener.
    return NS_OK;
  }

  if (mObserver) {
    // From "Main Fetch" step 19.1, 19.2: Process response.
    if (ShouldCheckSRI(mRequest, mResponse)) {
      MOZ_ASSERT(mResponse);
      mObserver->OnResponseAvailable(mResponse);
      #ifdef DEBUG
        mResponseAvailableCalled = true;
      #endif
    }

    mObserver->OnResponseEnd(FetchDriverObserver::eByNetworking);
    mObserver = nullptr;
  }

  mChannel = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
FetchDriver::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                    nsIChannel* aNewChannel,
                                    uint32_t aFlags,
                                    nsIAsyncVerifyRedirectCallback *aCallback)
{
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aNewChannel);
  if (httpChannel) {
    SetRequestHeaders(httpChannel);
  }

  nsCOMPtr<nsIHttpChannel> oldHttpChannel = do_QueryInterface(aOldChannel);
  nsAutoCString tRPHeaderCValue;
  if (oldHttpChannel) {
    Unused << oldHttpChannel->GetResponseHeader(NS_LITERAL_CSTRING("referrer-policy"),
                                                tRPHeaderCValue);
  }

  // "HTTP-redirect fetch": step 14 "Append locationURL to request's URL list."
  // However, ignore internal redirects here.  We don't want to flip
  // Response.redirected to true if an internal redirect occurs.  These
  // should be transparent to script.
  if (!(aFlags & nsIChannelEventSink::REDIRECT_INTERNAL)) {
    nsCOMPtr<nsIURI> uri;
    MOZ_ALWAYS_SUCCEEDS(aNewChannel->GetURI(getter_AddRefs(uri)));

    nsCOMPtr<nsIURI> uriClone;
    nsresult rv = uri->CloneIgnoringRef(getter_AddRefs(uriClone));
    if(NS_WARN_IF(NS_FAILED(rv))){
      return rv;
    }
    nsCString spec;
    rv = uriClone->GetSpec(spec);
    if(NS_WARN_IF(NS_FAILED(rv))){
      return rv;
    }
    nsCString fragment;
    rv = uri->GetRef(fragment);
    if(NS_WARN_IF(NS_FAILED(rv))){
      return rv;
    }

    mRequest->AddURL(spec, fragment);
  }

  NS_ConvertUTF8toUTF16 tRPHeaderValue(tRPHeaderCValue);
  // updates request’s associated referrer policy according to the
  // Referrer-Policy header (if any).
  if (!tRPHeaderValue.IsEmpty()) {
    net::ReferrerPolicy net_referrerPolicy =
      nsContentUtils::GetReferrerPolicyFromHeader(tRPHeaderValue);
    if (net_referrerPolicy != net::RP_Unset) {
      mRequest->SetReferrerPolicy(net_referrerPolicy);
      // Should update channel's referrer policy
      if (httpChannel) {
        nsresult rv = FetchUtil::SetRequestReferrer(mPrincipal,
                                                    mDocument,
                                                    httpChannel,
                                                    mRequest);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  aCallback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
FetchDriver::CheckListenerChain()
{
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
  if (aIID.Equals(NS_GET_IID(nsIStreamListener))) {
    *aResult = static_cast<nsIStreamListener*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIRequestObserver))) {
    *aResult = static_cast<nsIRequestObserver*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return QueryInterface(aIID, aResult);
}

void
FetchDriver::SetDocument(nsIDocument* aDocument)
{
  // Cannot set document after Fetch() has been called.
  MOZ_ASSERT(!mFetchCalled);
  mDocument = aDocument;
}

void
FetchDriver::SetRequestHeaders(nsIHttpChannel* aChannel) const
{
  MOZ_ASSERT(aChannel);

  AutoTArray<InternalHeaders::Entry, 5> headers;
  mRequest->Headers()->GetEntries(headers);
  bool hasAccept = false;
  for (uint32_t i = 0; i < headers.Length(); ++i) {
    if (!hasAccept && headers[i].mName.EqualsLiteral("accept")) {
      hasAccept = true;
    }
    if (headers[i].mValue.IsEmpty()) {
      DebugOnly<nsresult> rv = aChannel->SetEmptyRequestHeader(headers[i].mName);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    } else {
      DebugOnly<nsresult> rv =
        aChannel->SetRequestHeader(headers[i].mName, headers[i].mValue,
                                   false /* merge */);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  if (!hasAccept) {
    DebugOnly<nsresult> rv =
      aChannel->SetRequestHeader(NS_LITERAL_CSTRING("accept"),
                                 NS_LITERAL_CSTRING("*/*"),
                                 false /* merge */);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if (mRequest->ForceOriginHeader()) {
    nsAutoString origin;
    if (NS_SUCCEEDED(nsContentUtils::GetUTFOrigin(mPrincipal, origin))) {
      DebugOnly<nsresult> rv =
        aChannel->SetRequestHeader(NS_LITERAL_CSTRING("origin"),
                                   NS_ConvertUTF16toUTF8(origin),
                                   false /* merge */);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
}

void
FetchDriver::Abort()
{
  if (mObserver) {
  #ifdef DEBUG
    mResponseAvailableCalled = true;
  #endif
    mObserver->OnResponseEnd(FetchDriverObserver::eAborted);
    mObserver = nullptr;
  }

  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nullptr;
  }
}

} // namespace dom
} // namespace mozilla
