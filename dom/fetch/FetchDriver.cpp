/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/FetchDriver.h"

#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/dom/Document.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIFileChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsISupportsPriority.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIUploadChannel2.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPipe.h"

#include "nsContentPolicyUtils.h"
#include "nsDataHandler.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsHttpChannel.h"

#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/Unused.h"

#include "Fetch.h"
#include "FetchUtil.h"
#include "InternalRequest.h"
#include "InternalResponse.h"

namespace mozilla {
namespace dom {

namespace {

void GetBlobURISpecFromChannel(nsIRequest* aRequest, nsCString& aBlobURISpec) {
  MOZ_ASSERT(aRequest);

  aBlobURISpec.SetIsVoid(true);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (!channel) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = channel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return;
  }

  if (!dom::IsBlobURI(uri)) {
    return;
  }

  uri->GetSpec(aBlobURISpec);
}

bool ShouldCheckSRI(const InternalRequest* const aRequest,
                    const InternalResponse* const aResponse) {
  MOZ_DIAGNOSTIC_ASSERT(aRequest);
  MOZ_DIAGNOSTIC_ASSERT(aResponse);

  return !aRequest->GetIntegrity().IsEmpty() &&
         aResponse->Type() != ResponseType::Error;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
// AlternativeDataStreamListener
//-----------------------------------------------------------------------------
class AlternativeDataStreamListener final
    : public nsIStreamListener,
      public nsIThreadRetargetableStreamListener {
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
  //    checking, so the statust transition could be
  //    LOADING->COMPLETED->CANCELED
  //
  // 3. LOADING->FALLBACK
  //    For the case that alternative data loading could not be initialized,
  //    i.e. alternative data does not exist or no preferred alternative data
  //    type is requested. Once the status becomes FALLBACK,
  //    AlternativeDataStreamListener transits the channel callback request to
  //    FetchDriver, and the status should not go back to LOADING, COMPLETED, or
  //    CANCELED anymore.
  enum eStatus { LOADING = 0, COMPLETED, CANCELED, FALLBACK };

  AlternativeDataStreamListener(FetchDriver* aFetchDriver, nsIChannel* aChannel,
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

NS_IMPL_ISUPPORTS(AlternativeDataStreamListener, nsIStreamListener,
                  nsIThreadRetargetableStreamListener)

AlternativeDataStreamListener::AlternativeDataStreamListener(
    FetchDriver* aFetchDriver, nsIChannel* aChannel,
    const nsACString& aAlternativeDataType)
    : mFetchDriver(aFetchDriver),
      mAlternativeDataType(aAlternativeDataType),
      mAlternativeDataCacheEntryId(0),
      mChannel(aChannel),
      mStatus(AlternativeDataStreamListener::LOADING) {
  MOZ_DIAGNOSTIC_ASSERT(mFetchDriver);
  MOZ_DIAGNOSTIC_ASSERT(mChannel);
}

AlternativeDataStreamListener::eStatus AlternativeDataStreamListener::Status() {
  return mStatus;
}

void AlternativeDataStreamListener::Cancel() {
  mAlternativeDataCacheEntryId = 0;
  mCacheInfoChannel = nullptr;
  mPipeAlternativeOutputStream = nullptr;
  mPipeAlternativeInputStream = nullptr;
  if (mChannel && mStatus != AlternativeDataStreamListener::FALLBACK) {
    // if mStatus is fallback, we need to keep channel to forward request back
    // to FetchDriver
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nullptr;
  }
  mStatus = AlternativeDataStreamListener::CANCELED;
}

uint64_t AlternativeDataStreamListener::GetAlternativeDataCacheEntryId() {
  return mAlternativeDataCacheEntryId;
}

const nsACString& AlternativeDataStreamListener::GetAlternativeDataType()
    const {
  return mAlternativeDataType;
}

already_AddRefed<nsIInputStream>
AlternativeDataStreamListener::GetAlternativeInputStream() {
  nsCOMPtr<nsIInputStream> inputStream = mPipeAlternativeInputStream;
  return inputStream.forget();
}

already_AddRefed<nsICacheInfoChannel>
AlternativeDataStreamListener::GetCacheInfoChannel() {
  nsCOMPtr<nsICacheInfoChannel> channel = mCacheInfoChannel;
  return channel.forget();
}

NS_IMETHODIMP
AlternativeDataStreamListener::OnStartRequest(nsIRequest* aRequest) {
  AssertIsOnMainThread();
  MOZ_ASSERT(!mAlternativeDataType.IsEmpty());
  // Checking the alternative data type is the same between we asked and the
  // saved in the channel.
  nsAutoCString alternativeDataType;
  nsCOMPtr<nsICacheInfoChannel> cic = do_QueryInterface(aRequest);
  mStatus = AlternativeDataStreamListener::LOADING;
  if (cic && NS_SUCCEEDED(cic->GetAlternativeDataType(alternativeDataType)) &&
      mAlternativeDataType.Equals(alternativeDataType) &&
      NS_SUCCEEDED(cic->GetCacheEntryId(&mAlternativeDataCacheEntryId))) {
    MOZ_DIAGNOSTIC_ASSERT(!mPipeAlternativeInputStream);
    MOZ_DIAGNOSTIC_ASSERT(!mPipeAlternativeOutputStream);
    nsresult rv =
        NS_NewPipe(getter_AddRefs(mPipeAlternativeInputStream),
                   getter_AddRefs(mPipeAlternativeOutputStream),
                   0 /* default segment size */, UINT32_MAX /* infinite pipe */,
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
    // Set status to FALLBACK to reuse the opened channel to load main body,
    // then call FetchDriver::OnStartRequest to continue the work. Unfortunately
    // can't change the stream listener to mFetchDriver, need to keep
    // AlternativeDataStreamListener alive to redirect OnDataAvailable and
    // OnStopRequest to mFetchDriver.
    MOZ_ASSERT(alternativeDataType.IsEmpty());
    mStatus = AlternativeDataStreamListener::FALLBACK;
    mAlternativeDataCacheEntryId = 0;
    MOZ_ASSERT(mFetchDriver);
    return mFetchDriver->OnStartRequest(aRequest);
  }
  return NS_OK;
}

NS_IMETHODIMP
AlternativeDataStreamListener::OnDataAvailable(nsIRequest* aRequest,
                                               nsIInputStream* aInputStream,
                                               uint64_t aOffset,
                                               uint32_t aCount) {
  if (mStatus == AlternativeDataStreamListener::LOADING) {
    MOZ_ASSERT(mPipeAlternativeOutputStream);
    uint32_t read = 0;
    return aInputStream->ReadSegments(
        NS_CopySegmentToStream, mPipeAlternativeOutputStream, aCount, &read);
  }
  if (mStatus == AlternativeDataStreamListener::FALLBACK) {
    MOZ_ASSERT(mFetchDriver);
    return mFetchDriver->OnDataAvailable(aRequest, aInputStream, aOffset,
                                         aCount);
  }
  return NS_OK;
}

NS_IMETHODIMP
AlternativeDataStreamListener::OnStopRequest(nsIRequest* aRequest,
                                             nsresult aStatusCode) {
  AssertIsOnMainThread();

  // Alternative data loading is going to finish, breaking the reference cycle
  // here by taking the ownership to a loacl variable.
  RefPtr<FetchDriver> fetchDriver = std::move(mFetchDriver);

  if (mStatus == AlternativeDataStreamListener::CANCELED) {
    // do nothing
    return NS_OK;
  }

  if (mStatus == AlternativeDataStreamListener::FALLBACK) {
    MOZ_ASSERT(fetchDriver);
    return fetchDriver->OnStopRequest(aRequest, aStatusCode);
  }

  MOZ_DIAGNOSTIC_ASSERT(mStatus == AlternativeDataStreamListener::LOADING);

  MOZ_ASSERT(!mAlternativeDataType.IsEmpty() && mPipeAlternativeOutputStream &&
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
  fetchDriver->FinishOnStopRequest(this);
  return NS_OK;
}

NS_IMETHODIMP
AlternativeDataStreamListener::CheckListenerChain() { return NS_OK; }
//-----------------------------------------------------------------------------
// FetchDriver
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(FetchDriver, nsIStreamListener, nsIChannelEventSink,
                  nsIInterfaceRequestor, nsIThreadRetargetableStreamListener)

FetchDriver::FetchDriver(InternalRequest* aRequest, nsIPrincipal* aPrincipal,
                         nsILoadGroup* aLoadGroup,
                         nsIEventTarget* aMainThreadEventTarget,
                         nsICookieJarSettings* aCookieJarSettings,
                         PerformanceStorage* aPerformanceStorage,
                         bool aIsTrackingFetch)
    : mPrincipal(aPrincipal),
      mLoadGroup(aLoadGroup),
      mRequest(aRequest),
      mMainThreadEventTarget(aMainThreadEventTarget),
      mCookieJarSettings(aCookieJarSettings),
      mPerformanceStorage(aPerformanceStorage),
      mNeedToObserveOnDataAvailable(false),
      mIsTrackingFetch(aIsTrackingFetch),
      mOnStopRequestCalled(false)
#ifdef DEBUG
      ,
      mResponseAvailableCalled(false),
      mFetchCalled(false)
#endif
{
  AssertIsOnMainThread();

  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aMainThreadEventTarget);
}

FetchDriver::~FetchDriver() {
  AssertIsOnMainThread();

  // We assert this since even on failures, we should call
  // FailWithNetworkError().
  MOZ_ASSERT(mResponseAvailableCalled);
}

nsresult FetchDriver::Fetch(AbortSignalImpl* aSignalImpl,
                            FetchDriverObserver* aObserver) {
  AssertIsOnMainThread();
#ifdef DEBUG
  MOZ_ASSERT(!mFetchCalled);
  mFetchCalled = true;
#endif

  mObserver = aObserver;

  // FIXME(nsm): Deal with HSTS.

  MOZ_RELEASE_ASSERT(!mRequest->IsSynchronous(),
                     "Synchronous fetch not supported");

  UniquePtr<mozilla::ipc::PrincipalInfo> principalInfo(
      new mozilla::ipc::PrincipalInfo());
  nsresult rv = PrincipalToPrincipalInfo(mPrincipal, principalInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mRequest->SetPrincipalInfo(std::move(principalInfo));

  // If the signal is aborted, it's time to inform the observer and terminate
  // the operation.
  if (aSignalImpl) {
    if (aSignalImpl->Aborted()) {
      Abort();
      return NS_OK;
    }

    Follow(aSignalImpl);
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
nsresult FetchDriver::HttpFetch(
    const nsACString& aPreferredAlternativeDataType) {
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
  rv = NS_NewURI(getter_AddRefs(uri), url);
  NS_ENSURE_SUCCESS(rv, rv);

  // Unsafe requests aren't allowed with when using no-core mode.
  if (mRequest->Mode() == RequestMode::No_cors && mRequest->UnsafeRequest() &&
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
  const nsLoadFlags bypassFlag = mRequest->SkipServiceWorker()
                                     ? nsIChannel::LOAD_BYPASS_SERVICE_WORKER
                                     : 0;

  nsSecurityFlags secFlags = 0;
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
  } else if (mRequest->GetCredentialsMode() ==
             RequestCredentials::Same_origin) {
    secFlags |= nsILoadInfo::SEC_COOKIES_SAME_ORIGIN;
  } else {
    MOZ_ASSERT_UNREACHABLE("Unexpected credentials mode!");
    return NS_ERROR_UNEXPECTED;
  }

  // From here on we create a channel and set its properties with the
  // information from the InternalRequest. This is an implementation detail.
  MOZ_ASSERT(mLoadGroup);
  nsCOMPtr<nsIChannel> chan;

  nsLoadFlags loadFlags = nsIRequest::LOAD_BACKGROUND | bypassFlag;
  if (mDocument) {
    MOZ_ASSERT(mDocument->NodePrincipal() == mPrincipal);
    MOZ_ASSERT(mDocument->CookieJarSettings() == mCookieJarSettings);
    rv = NS_NewChannel(getter_AddRefs(chan), uri, mDocument, secFlags,
                       mRequest->ContentPolicyType(),
                       nullptr,             /* aPerformanceStorage */
                       mLoadGroup, nullptr, /* aCallbacks */
                       loadFlags, ios);
  } else if (mClientInfo.isSome()) {
    rv = NS_NewChannel(getter_AddRefs(chan), uri, mPrincipal, mClientInfo.ref(),
                       mController, secFlags, mRequest->ContentPolicyType(),
                       mCookieJarSettings, mPerformanceStorage, mLoadGroup,
                       nullptr, /* aCallbacks */
                       loadFlags, ios);
  } else {
    rv =
        NS_NewChannel(getter_AddRefs(chan), uri, mPrincipal, secFlags,
                      mRequest->ContentPolicyType(), mCookieJarSettings,
                      mPerformanceStorage, mLoadGroup, nullptr, /* aCallbacks */
                      loadFlags, ios);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (mCSPEventListener) {
    nsCOMPtr<nsILoadInfo> loadInfo = chan->LoadInfo();
    rv = loadInfo->SetCspEventListener(mCSPEventListener);
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
  if (cos && UserActivation::IsHandlingUserInput()) {
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
    SetRequestHeaders(httpChan, false);

    // Step 5 of https://fetch.spec.whatwg.org/#main-fetch
    // If request's referrer policy is the empty string and request's client is
    // non-null, then set request's referrer policy to request's client's
    // associated referrer policy.
    // Basically, "client" is not in our implementation, we use
    // EnvironmentReferrerPolicy of the worker or document context
    ReferrerPolicy referrerPolicy = mRequest->GetEnvironmentReferrerPolicy();
    if (mRequest->ReferrerPolicy_() == ReferrerPolicy::_empty) {
      mRequest->SetReferrerPolicy(referrerPolicy);
    }
    // Step 6 of https://fetch.spec.whatwg.org/#main-fetch
    // If request’s referrer policy is the empty string,
    // then set request’s referrer policy to the user-set default policy.
    if (mRequest->ReferrerPolicy_() == ReferrerPolicy::_empty) {
      nsCOMPtr<nsILoadInfo> loadInfo = httpChan->LoadInfo();
      bool isPrivate = loadInfo->GetOriginAttributes().mPrivateBrowsingId > 0;
      referrerPolicy =
          ReferrerInfo::GetDefaultReferrerPolicy(httpChan, uri, isPrivate);
      mRequest->SetReferrerPolicy(referrerPolicy);
    }

    rv = FetchUtil::SetRequestReferrer(mPrincipal, mDocument, httpChan,
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
    rv = internalChan->SetRedirectMode(
        static_cast<uint32_t>(mRequest->GetRedirectMode()));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    mRequest->MaybeSkipCacheIfPerformingRevalidation();
    rv = internalChan->SetFetchCacheMode(
        static_cast<uint32_t>(mRequest->GetCacheMode()));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = internalChan->SetIntegrityMetadata(mRequest->GetIntegrity());
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // Set the initiator type
    nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(httpChan));
    if (timedChannel) {
      timedChannel->SetInitiatorType(NS_LITERAL_STRING("fetch"));
    }
  }

  // Step 5. Proxy authentication will be handled by Necko.

  // Continue setting up 'HTTPRequest'. Content-Type and body data.
  nsCOMPtr<nsIUploadChannel2> uploadChan = do_QueryInterface(chan);
  if (uploadChan) {
    nsAutoCString contentType;
    ErrorResult result;
    mRequest->Headers()->GetFirst(NS_LITERAL_CSTRING("content-type"),
                                  contentType, result);
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
#endif  // DEBUG

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
    nsCOMPtr<nsILoadInfo> loadInfo = chan->LoadInfo();
    loadInfo->SetCorsPreflightInfo(unsafeHeaders, false);
  }

  if (mIsTrackingFetch && StaticPrefs::network_http_tailing_enabled() && cos) {
    cos->AddClassFlags(nsIClassOfService::Throttleable |
                       nsIClassOfService::Tail);
  }

  if (mIsTrackingFetch &&
      StaticPrefs::privacy_trackingprotection_lower_network_priority()) {
    nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(chan);
    if (p) {
      p->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
    }
  }

  NotifyNetworkMonitorAlternateStack(chan, std::move(mOriginStack));

  // if the preferred alternative data type in InternalRequest is not empty, set
  // the data type on the created channel and also create a
  // AlternativeDataStreamListener to be the stream listener of the channel.
  if (!aPreferredAlternativeDataType.IsEmpty()) {
    nsCOMPtr<nsICacheInfoChannel> cic = do_QueryInterface(chan);
    if (cic) {
      cic->PreferAlternativeDataType(aPreferredAlternativeDataType,
                                     EmptyCString(), true);
      MOZ_ASSERT(!mAltDataListener);
      mAltDataListener = new AlternativeDataStreamListener(
          this, chan, aPreferredAlternativeDataType);
      rv = chan->AsyncOpen(mAltDataListener);
    } else {
      rv = chan->AsyncOpen(this);
    }
  } else {
    // Integrity check cannot be done on alt-data yet.
    if (mRequest->GetIntegrity().IsEmpty()) {
      nsCOMPtr<nsICacheInfoChannel> cic = do_QueryInterface(chan);
      if (cic) {
        cic->PreferAlternativeDataType(
            NS_LITERAL_CSTRING(WASM_ALT_DATA_TYPE_V1),
            NS_LITERAL_CSTRING(WASM_CONTENT_TYPE), false);
      }
    }

    rv = chan->AsyncOpen(this);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  // Step 4 onwards of "HTTP Fetch" is handled internally by Necko.

  mChannel = chan;
  return NS_OK;
}
already_AddRefed<InternalResponse> FetchDriver::BeginAndGetFilteredResponse(
    InternalResponse* aResponse, bool aFoundOpaqueRedirect) {
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
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return nullptr;
        }
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

void FetchDriver::FailWithNetworkError(nsresult rv) {
  AssertIsOnMainThread();
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
FetchDriver::OnStartRequest(nsIRequest* aRequest) {
  AssertIsOnMainThread();

  // Note, this can be called multiple times if we are doing an opaqueredirect.
  // In that case we will get a simulated OnStartRequest() and then the real
  // channel will call in with an errored OnStartRequest().

  if (!mChannel) {
    MOZ_ASSERT(!mObserver);
    return NS_BINDING_ABORTED;
  }

  nsresult rv;
  aRequest->GetStatus(&rv);
  if (NS_FAILED(rv)) {
    FailWithNetworkError(rv);
    return rv;
  }

  // We should only get to the following code once.
  MOZ_ASSERT(!mPipeOutputStream);

  if (!mObserver) {
    MOZ_ASSERT(false, "We should have mObserver here.");
    FailWithNetworkError(NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
  }

  mNeedToObserveOnDataAvailable = mObserver->NeedOnDataAvailable();

  RefPtr<InternalResponse> response;
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);

  // On a successful redirect we perform the following substeps of HTTP Fetch,
  // step 5, "redirect status", step 11.

  bool foundOpaqueRedirect = false;

  nsAutoCString contentType;
  channel->GetContentType(contentType);

  int64_t contentLength = InternalResponse::UNKNOWN_BODY_SIZE;
  rv = channel->GetContentLength(&contentLength);
  MOZ_ASSERT_IF(NS_FAILED(rv),
                contentLength == InternalResponse::UNKNOWN_BODY_SIZE);

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

    response = new InternalResponse(responseStatus, statusText,
                                    mRequest->GetCredentialsMode());

    UniquePtr<mozilla::ipc::PrincipalInfo> principalInfo(
        new mozilla::ipc::PrincipalInfo());
    nsresult rv = PrincipalToPrincipalInfo(mPrincipal, principalInfo.get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    response->SetPrincipalInfo(std::move(principalInfo));

    response->Headers()->FillResponseHeaders(httpChannel);

    // If Content-Encoding or Transfer-Encoding headers are set, then the actual
    // Content-Length (which refer to the decoded data) is obscured behind the
    // encodings.
    ErrorResult result;
    if (response->Headers()->Has(NS_LITERAL_CSTRING("content-encoding"),
                                 result) ||
        response->Headers()->Has(NS_LITERAL_CSTRING("transfer-encoding"),
                                 result)) {
      // We cannot trust the content-length when content-encoding or
      // transfer-encoding are set.  There are many servers which just
      // get this wrong.
      contentLength = InternalResponse::UNKNOWN_BODY_SIZE;
    }
    MOZ_ASSERT(!result.Failed());
  } else {
    response = new InternalResponse(200, NS_LITERAL_CSTRING("OK"),
                                    mRequest->GetCredentialsMode());

    if (!contentType.IsEmpty()) {
      nsAutoCString contentCharset;
      channel->GetContentCharset(contentCharset);
      if (NS_SUCCEEDED(rv) && !contentCharset.IsEmpty()) {
        contentType += NS_LITERAL_CSTRING(";charset=") + contentCharset;
      }

      IgnoredErrorResult result;
      response->Headers()->Append(NS_LITERAL_CSTRING("Content-Type"),
                                  contentType, result);
      MOZ_ASSERT(!result.Failed());
    }

    if (contentLength > 0) {
      nsAutoCString contentLenStr;
      contentLenStr.AppendInt(contentLength);

      IgnoredErrorResult result;
      response->Headers()->Append(NS_LITERAL_CSTRING("Content-Length"),
                                  contentLenStr, result);
      MOZ_ASSERT(!result.Failed());
    }
  }

  nsCOMPtr<nsICacheInfoChannel> cic = do_QueryInterface(aRequest);
  if (cic) {
    if (mAltDataListener) {
      // Skip the case that mAltDataListener->Status() equals to FALLBACK, that
      // means the opened channel for alternative data loading is reused for
      // loading the main data.
      if (mAltDataListener->Status() !=
          AlternativeDataStreamListener::FALLBACK) {
        // Verify the cache ID is the same with from alternative data cache.
        // If the cache ID is different, droping the alternative data loading,
        // otherwise setup the response's alternative body and cacheInfoChannel.
        uint64_t cacheEntryId = 0;
        if (NS_SUCCEEDED(cic->GetCacheEntryId(&cacheEntryId)) &&
            cacheEntryId !=
                mAltDataListener->GetAlternativeDataCacheEntryId()) {
          mAltDataListener->Cancel();
        } else {
          // AlternativeDataStreamListener::OnStartRequest had already been
          // called, the alternative data input stream and cacheInfo channel
          // must be created.
          nsCOMPtr<nsICacheInfoChannel> cacheInfo =
              mAltDataListener->GetCacheInfoChannel();
          nsCOMPtr<nsIInputStream> altInputStream =
              mAltDataListener->GetAlternativeInputStream();
          MOZ_ASSERT(altInputStream && cacheInfo);
          response->SetAlternativeBody(altInputStream);
          nsMainThreadPtrHandle<nsICacheInfoChannel> handle(
              new nsMainThreadPtrHolder<nsICacheInfoChannel>(
                  "nsICacheInfoChannel", cacheInfo, false));
          response->SetCacheInfoChannel(handle);
        }
      } else if (!mAltDataListener->GetAlternativeDataType().IsEmpty()) {
        // If the status is FALLBACK and the
        // mAltDataListener::mAlternativeDataType is not empty, that means the
        // data need to be saved into cache, setup the response's
        // nsICacheInfoChannel for caching the data after loading.
        nsMainThreadPtrHandle<nsICacheInfoChannel> handle(
            new nsMainThreadPtrHolder<nsICacheInfoChannel>(
                "nsICacheInfoChannel", cic, false));
        response->SetCacheInfoChannel(handle);
      }
    } else if (!cic->PreferredAlternativeDataTypes().IsEmpty()) {
      MOZ_ASSERT(cic->PreferredAlternativeDataTypes().Length() == 1);
      MOZ_ASSERT(cic->PreferredAlternativeDataTypes()[0].type().EqualsLiteral(
          WASM_ALT_DATA_TYPE_V1));
      MOZ_ASSERT(
          cic->PreferredAlternativeDataTypes()[0].contentType().EqualsLiteral(
              WASM_CONTENT_TYPE));

      if (contentType.EqualsLiteral(WASM_CONTENT_TYPE)) {
        // We want to attach the CacheInfoChannel to the response object such
        // that we can track its origin when the Response object is manipulated
        // by JavaScript code. This is important for WebAssembly, which uses
        // fetch to query its sources in JavaScript and transfer the Response
        // object to other function responsible for storing the alternate data
        // using the CacheInfoChannel.
        nsMainThreadPtrHandle<nsICacheInfoChannel> handle(
            new nsMainThreadPtrHolder<nsICacheInfoChannel>(
                "nsICacheInfoChannel", cic, false));
        response->SetCacheInfoChannel(handle);
      }
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
                  false /* blocking output, since the pipe is 'in'finite */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError(rv);
    // Cancel request.
    return rv;
  }
  response->SetBody(pipeInputStream, contentLength);

  // If the request is a file channel, then remember the local path to
  // that file so we can later create File blobs rather than plain ones.
  nsCOMPtr<nsIFileChannel> fc = do_QueryInterface(aRequest);
  if (fc) {
    nsCOMPtr<nsIFile> file;
    rv = fc->GetFile(getter_AddRefs(file));
    if (!NS_WARN_IF(NS_FAILED(rv))) {
      nsAutoString path;
      file->GetPath(path);
      response->SetBodyLocalPath(path);
    }
  } else {
    // If the request is a blob URI, then remember that URI so that we
    // can later just use that blob instance instead of cloning it.
    nsCString blobURISpec;
    GetBlobURISpecFromChannel(aRequest, blobURISpec);
    if (!blobURISpec.IsVoid()) {
      response->SetBodyBlobURISpec(blobURISpec);
    }
  }

  response->InitChannelInfo(channel);

  nsCOMPtr<nsIURI> channelURI;
  rv = channel->GetURI(getter_AddRefs(channelURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError(rv);
    // Cancel request.
    return rv;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
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
    MOZ_DIAGNOSTIC_ASSERT(mRequest->GetResponseTainting() ==
                              LoadTainting::Opaque &&
                          !foundOpaqueRedirect);
    FailWithNetworkError(NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
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
    SRICheck::IntegrityMetadata(mRequest->GetIntegrity(), sourceUri, reporter,
                                &mSRIMetadata);
    mSRIDataVerifier =
        MakeUnique<SRICheckDataVerifier>(mSRIMetadata, sourceUri, reporter);

    // Do not retarget off main thread when using SRI API.
    return NS_OK;
  }

  nsCOMPtr<nsIEventTarget> sts =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
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
class DataAvailableRunnable final : public Runnable {
  RefPtr<FetchDriverObserver> mObserver;

 public:
  explicit DataAvailableRunnable(FetchDriverObserver* aObserver)
      : Runnable("dom::DataAvailableRunnable"), mObserver(aObserver) {
    MOZ_ASSERT(aObserver);
  }

  NS_IMETHOD
  Run() override {
    mObserver->OnDataAvailable();
    mObserver = nullptr;
    return NS_OK;
  }
};

struct SRIVerifierAndOutputHolder {
  SRIVerifierAndOutputHolder(SRICheckDataVerifier* aVerifier,
                             nsIOutputStream* aOutputStream)
      : mVerifier(aVerifier), mOutputStream(aOutputStream) {}

  SRICheckDataVerifier* mVerifier;
  nsIOutputStream* mOutputStream;

 private:
  SRIVerifierAndOutputHolder() = delete;
};

// Just like NS_CopySegmentToStream, but also sends the data into an
// SRICheckDataVerifier.
nsresult CopySegmentToStreamAndSRI(nsIInputStream* aInStr, void* aClosure,
                                   const char* aBuffer, uint32_t aOffset,
                                   uint32_t aCount, uint32_t* aCountWritten) {
  auto holder = static_cast<SRIVerifierAndOutputHolder*>(aClosure);
  MOZ_DIAGNOSTIC_ASSERT(holder && holder->mVerifier && holder->mOutputStream,
                        "Bogus holder");
  nsresult rv = holder->mVerifier->Update(
      aCount, reinterpret_cast<const uint8_t*>(aBuffer));
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

}  // anonymous namespace

NS_IMETHODIMP
FetchDriver::OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aInputStream,
                             uint64_t aOffset, uint32_t aCount) {
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
        nsresult rv = mMainThreadEventTarget->Dispatch(runnable.forget(),
                                                       NS_DISPATCH_NORMAL);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }

  // Needs to be initialized to 0 because in some cases nsStringInputStream may
  // not write to aRead.
  uint32_t aRead = 0;
  MOZ_ASSERT(mResponse);
  MOZ_ASSERT(mPipeOutputStream);

  // From "Main Fetch" step 19: SRI-part2.
  // Note: Avoid checking the hidden opaque body.
  nsresult rv;
  if (mResponse->Type() != ResponseType::Opaque &&
      ShouldCheckSRI(mRequest, mResponse)) {
    MOZ_ASSERT(mSRIDataVerifier);

    SRIVerifierAndOutputHolder holder(mSRIDataVerifier.get(),
                                      mPipeOutputStream);
    rv = aInputStream->ReadSegments(CopySegmentToStreamAndSRI, &holder, aCount,
                                    &aRead);
  } else {
    rv = aInputStream->ReadSegments(NS_CopySegmentToStream, mPipeOutputStream,
                                    aCount, &aRead);
  }

  // If no data was read, it's possible the output stream is closed but the
  // ReadSegments call followed its contract of returning NS_OK despite write
  // errors.  Unfortunately, nsIOutputStream has an ill-conceived contract when
  // taken together with ReadSegments' contract, because the pipe will just
  // NS_OK if we try and invoke its Write* functions ourselves with a 0 count.
  // So we must just assume the pipe is broken.
  if (aRead == 0 && aCount != 0) {
    return NS_BASE_STREAM_CLOSED;
  }
  return rv;
}

NS_IMETHODIMP
FetchDriver::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  AssertIsOnMainThread();

  MOZ_DIAGNOSTIC_ASSERT(!mOnStopRequestCalled);
  mOnStopRequestCalled = true;

  // main data loading is going to finish, breaking the reference cycle.
  RefPtr<AlternativeDataStreamListener> altDataListener =
      std::move(mAltDataListener);

  // We need to check mObserver, which is nulled by FailWithNetworkError(),
  // because in the case of "error" redirect mode, aStatusCode may be NS_OK but
  // mResponse will definitely be null so we must not take the else branch.
  if (NS_FAILED(aStatusCode) || !mObserver) {
    nsCOMPtr<nsIAsyncOutputStream> outputStream =
        do_QueryInterface(mPipeOutputStream);
    if (outputStream) {
      outputStream->CloseWithStatus(NS_FAILED(aStatusCode) ? aStatusCode
                                                           : NS_BINDING_FAILED);
    }
    if (altDataListener) {
      altDataListener->Cancel();
    }

    // We proceed as usual here, since we've already created a successful
    // response from OnStartRequest.
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
      nsresult rv =
          mSRIDataVerifier->Verify(mSRIMetadata, channel, sourceUri, reporter);
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

  FinishOnStopRequest(altDataListener);
  return NS_OK;
}

void FetchDriver::FinishOnStopRequest(
    AlternativeDataStreamListener* aAltDataListener) {
  AssertIsOnMainThread();
  // OnStopRequest is not called from channel, that means the main data loading
  // does not finish yet. Reaching here since alternative data loading finishes.
  if (!mOnStopRequestCalled) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(!mAltDataListener);
  // Wait for alternative data loading finish if we needed it.
  if (aAltDataListener &&
      aAltDataListener->Status() == AlternativeDataStreamListener::LOADING) {
    // For LOADING case, channel holds the reference of altDataListener, no need
    // to restore it to mAltDataListener.
    return;
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
}

NS_IMETHODIMP
FetchDriver::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                    nsIChannel* aNewChannel, uint32_t aFlags,
                                    nsIAsyncVerifyRedirectCallback* aCallback) {
  nsCOMPtr<nsIHttpChannel> oldHttpChannel = do_QueryInterface(aOldChannel);
  nsCOMPtr<nsIHttpChannel> newHttpChannel = do_QueryInterface(aNewChannel);
  if (newHttpChannel) {
    uint32_t responseCode = 0;
    nsAutoCString method;
    mRequest->GetMethod(method);
    bool rewriteToGET = false;
    if (oldHttpChannel &&
        NS_SUCCEEDED(oldHttpChannel->GetResponseStatus(&responseCode))) {
      // Fetch 4.4.11
      rewriteToGET =
          (responseCode <= 302 && method.LowerCaseEqualsASCII("post")) ||
          (responseCode == 303 && !method.LowerCaseEqualsASCII("get") &&
           !method.LowerCaseEqualsASCII("head"));
    }

    SetRequestHeaders(newHttpChannel, rewriteToGET);
  }

  // "HTTP-redirect fetch": step 14 "Append locationURL to request's URL list."
  // However, ignore internal redirects here.  We don't want to flip
  // Response.redirected to true if an internal redirect occurs.  These
  // should be transparent to script.
  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(aNewChannel->GetURI(getter_AddRefs(uri)));

  nsCOMPtr<nsIURI> uriClone;
  nsresult rv = NS_GetURIWithoutRef(uri, getter_AddRefs(uriClone));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsCString spec;
  rv = uriClone->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsCString fragment;
  rv = uri->GetRef(fragment);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!(aFlags & nsIChannelEventSink::REDIRECT_INTERNAL)) {
    mRequest->AddURL(spec, fragment);
  } else {
    // Overwrite the URL only when the request is redirected by a service
    // worker.
    mRequest->SetURLForInternalRedirect(aFlags, spec, fragment);
  }

  // In redirect, httpChannel already took referrer-policy into account, so
  // updates request’s associated referrer policy from channel.
  if (newHttpChannel) {
    nsAutoString computedReferrerSpec;
    nsCOMPtr<nsIReferrerInfo> referrerInfo = newHttpChannel->GetReferrerInfo();
    if (referrerInfo) {
      mRequest->SetReferrerPolicy(referrerInfo->ReferrerPolicy());
      Unused << referrerInfo->GetComputedReferrerSpec(computedReferrerSpec);
    }

    // Step 8 https://fetch.spec.whatwg.org/#main-fetch
    // If request’s referrer is not "no-referrer" (empty), set request’s
    // referrer to the result of invoking determine request’s referrer.
    mRequest->SetReferrer(computedReferrerSpec);
  }

  aCallback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
FetchDriver::CheckListenerChain() { return NS_OK; }

NS_IMETHODIMP
FetchDriver::GetInterface(const nsIID& aIID, void** aResult) {
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

void FetchDriver::SetDocument(Document* aDocument) {
  // Cannot set document after Fetch() has been called.
  MOZ_ASSERT(!mFetchCalled);
  mDocument = aDocument;
}

void FetchDriver::SetCSPEventListener(nsICSPEventListener* aCSPEventListener) {
  MOZ_ASSERT(!mFetchCalled);
  mCSPEventListener = aCSPEventListener;
}

void FetchDriver::SetClientInfo(const ClientInfo& aClientInfo) {
  MOZ_ASSERT(!mFetchCalled);
  mClientInfo.emplace(aClientInfo);
}

void FetchDriver::SetController(
    const Maybe<ServiceWorkerDescriptor>& aController) {
  MOZ_ASSERT(!mFetchCalled);
  mController = aController;
}

void FetchDriver::SetRequestHeaders(nsIHttpChannel* aChannel,
                                    bool aStripRequestBodyHeader) const {
  MOZ_ASSERT(aChannel);

  // nsIHttpChannel has a set of pre-configured headers (Accept,
  // Accept-Languages, ...) and we don't want to merge the Request's headers
  // with them. This array is used to know if the current header has been aleady
  // set, if yes, we ask necko to merge it with the previous one, otherwise, we
  // don't want the merge.
  nsTArray<nsCString> headersSet;

  AutoTArray<InternalHeaders::Entry, 5> headers;
  mRequest->Headers()->GetEntries(headers);
  for (uint32_t i = 0; i < headers.Length(); ++i) {
    if (aStripRequestBodyHeader &&
        (headers[i].mName.LowerCaseEqualsASCII("content-type") ||
         headers[i].mName.LowerCaseEqualsASCII("content-encoding") ||
         headers[i].mName.LowerCaseEqualsASCII("content-language") ||
         headers[i].mName.LowerCaseEqualsASCII("content-location"))) {
      continue;
    }

    bool alreadySet = headersSet.Contains(headers[i].mName);
    if (!alreadySet) {
      headersSet.AppendElement(headers[i].mName);
    }

    if (headers[i].mValue.IsEmpty()) {
      DebugOnly<nsresult> rv =
          aChannel->SetEmptyRequestHeader(headers[i].mName);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    } else {
      DebugOnly<nsresult> rv = aChannel->SetRequestHeader(
          headers[i].mName, headers[i].mValue, alreadySet /* merge */);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  nsAutoCString method;
  mRequest->GetMethod(method);
  if (!method.EqualsLiteral("GET") && !method.EqualsLiteral("HEAD")) {
    nsAutoString origin;
    if (NS_SUCCEEDED(nsContentUtils::GetUTFOrigin(mPrincipal, origin))) {
      DebugOnly<nsresult> rv = aChannel->SetRequestHeader(
          nsDependentCString(net::nsHttp::Origin),
          NS_ConvertUTF16toUTF8(origin), false /* merge */);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
}

void FetchDriver::Abort() {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

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

}  // namespace dom
}  // namespace mozilla
