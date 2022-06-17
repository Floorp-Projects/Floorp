/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceTiming.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/PerformanceTimingBinding.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIHttpChannel.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "nsITimedChannel.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PerformanceTiming, mPerformance)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(PerformanceTiming, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(PerformanceTiming, Release)

/* static */
PerformanceTimingData* PerformanceTimingData::Create(
    nsITimedChannel* aTimedChannel, nsIHttpChannel* aChannel,
    DOMHighResTimeStamp aZeroTime, nsAString& aInitiatorType,
    nsAString& aEntryName) {
  MOZ_ASSERT(NS_IsMainThread());

  // Check if resource timing is prefed off.
  if (!StaticPrefs::dom_enable_resource_timing()) {
    return nullptr;
  }

  if (!aChannel || !aTimedChannel) {
    return nullptr;
  }

  bool reportTiming = true;
  aTimedChannel->GetReportResourceTiming(&reportTiming);

  if (!reportTiming) {
    return nullptr;
  }

  aTimedChannel->GetInitiatorType(aInitiatorType);

  // If the initiator type had no valid value, then set it to the default
  // ("other") value.
  if (aInitiatorType.IsEmpty()) {
    aInitiatorType = u"other"_ns;
  }

  // According to the spec, "The name attribute must return the resolved URL
  // of the requested resource. This attribute must not change even if the
  // fetch redirected to a different URL."
  nsCOMPtr<nsIURI> originalURI;
  aChannel->GetOriginalURI(getter_AddRefs(originalURI));

  nsAutoCString name;
  originalURI->GetSpec(name);
  CopyUTF8toUTF16(name, aEntryName);

  // The nsITimedChannel argument will be used to gather all the timings.
  // The nsIHttpChannel argument will be used to check if any cross-origin
  // redirects occurred.
  // The last argument is the "zero time" (offset). Since we don't want
  // any offset for the resource timing, this will be set to "0" - the
  // resource timing returns a relative timing (no offset).
  return new PerformanceTimingData(aTimedChannel, aChannel, 0);
}

PerformanceTiming::PerformanceTiming(Performance* aPerformance,
                                     nsITimedChannel* aChannel,
                                     nsIHttpChannel* aHttpChannel,
                                     DOMHighResTimeStamp aZeroTime)
    : mPerformance(aPerformance) {
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");

  mTimingData.reset(new PerformanceTimingData(
      aChannel, aHttpChannel,
      nsRFPService::ReduceTimePrecisionAsMSecs(
          aZeroTime, aPerformance->GetRandomTimelineSeed(),
          aPerformance->IsSystemPrincipal(),
          aPerformance->CrossOriginIsolated())));

  // Non-null aHttpChannel implies that this PerformanceTiming object is being
  // used for subresources, which is irrelevant to this probe.
  if (!aHttpChannel && StaticPrefs::dom_enable_performance() &&
      IsTopLevelContentDocument()) {
    Telemetry::Accumulate(Telemetry::TIME_TO_RESPONSE_START_MS,
                          mTimingData->ResponseStartHighRes(aPerformance) -
                              mTimingData->ZeroTime());
  }
}

// Copy the timing info from the channel so we don't need to keep the channel
// alive just to get the timestamps.
PerformanceTimingData::PerformanceTimingData(nsITimedChannel* aChannel,
                                             nsIHttpChannel* aHttpChannel,
                                             DOMHighResTimeStamp aZeroTime)
    : mZeroTime(0.0),
      mFetchStart(0.0),
      mEncodedBodySize(0),
      mTransferSize(0),
      mDecodedBodySize(0),
      mRedirectCount(0),
      mAllRedirectsSameOrigin(true),
      mAllRedirectsPassTAO(true),
      mSecureConnection(false),
      mTimingAllowed(true),
      mInitialized(false) {
  mInitialized = !!aChannel;
  mZeroTime = aZeroTime;

  if (!StaticPrefs::dom_enable_performance() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    mZeroTime = 0;
  }

  nsCOMPtr<nsIURI> uri;
  if (aHttpChannel) {
    aHttpChannel->GetURI(getter_AddRefs(uri));
  } else {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
    if (httpChannel) {
      httpChannel->GetURI(getter_AddRefs(uri));
    }
  }

  if (uri) {
    mSecureConnection = uri->SchemeIs("https");
  }

  if (aChannel) {
    aChannel->GetAsyncOpen(&mAsyncOpen);
    aChannel->GetAllRedirectsSameOrigin(&mAllRedirectsSameOrigin);
    aChannel->GetAllRedirectsPassTimingAllowCheck(&mAllRedirectsPassTAO);
    aChannel->GetRedirectCount(&mRedirectCount);
    aChannel->GetRedirectStart(&mRedirectStart);
    aChannel->GetRedirectEnd(&mRedirectEnd);
    aChannel->GetDomainLookupStart(&mDomainLookupStart);
    aChannel->GetDomainLookupEnd(&mDomainLookupEnd);
    aChannel->GetConnectStart(&mConnectStart);
    aChannel->GetSecureConnectionStart(&mSecureConnectionStart);
    aChannel->GetConnectEnd(&mConnectEnd);
    aChannel->GetRequestStart(&mRequestStart);
    aChannel->GetResponseStart(&mResponseStart);
    aChannel->GetCacheReadStart(&mCacheReadStart);
    aChannel->GetResponseEnd(&mResponseEnd);
    aChannel->GetCacheReadEnd(&mCacheReadEnd);

    aChannel->GetDispatchFetchEventStart(&mWorkerStart);
    aChannel->GetHandleFetchEventStart(&mWorkerRequestStart);
    // TODO: Track when FetchEvent.respondWith() promise resolves as
    //       ServiceWorker interception responseStart?
    aChannel->GetHandleFetchEventEnd(&mWorkerResponseEnd);

    // The performance timing api essentially requires that the event timestamps
    // have a strict relation with each other. The truth, however, is the
    // browser engages in a number of speculative activities that sometimes mean
    // connections and lookups begin at different times. Workaround that here by
    // clamping these values to what we expect FetchStart to be.  This means the
    // later of AsyncOpen or WorkerStart times.
    if (!mAsyncOpen.IsNull()) {
      // We want to clamp to the expected FetchStart value.  This is later of
      // the AsyncOpen and WorkerStart values.
      const TimeStamp* clampTime = &mAsyncOpen;
      if (!mWorkerStart.IsNull() && mWorkerStart > mAsyncOpen) {
        clampTime = &mWorkerStart;
      }

      if (!mDomainLookupStart.IsNull() && mDomainLookupStart < *clampTime) {
        mDomainLookupStart = *clampTime;
      }

      if (!mDomainLookupEnd.IsNull() && mDomainLookupEnd < *clampTime) {
        mDomainLookupEnd = *clampTime;
      }

      if (!mConnectStart.IsNull() && mConnectStart < *clampTime) {
        mConnectStart = *clampTime;
      }

      if (mSecureConnection && !mSecureConnectionStart.IsNull() &&
          mSecureConnectionStart < *clampTime) {
        mSecureConnectionStart = *clampTime;
      }

      if (!mConnectEnd.IsNull() && mConnectEnd < *clampTime) {
        mConnectEnd = *clampTime;
      }
    }
  }

  // The aHttpChannel argument is null if this PerformanceTiming object is
  // being used for navigation timing (which is only relevant for documents).
  // It has a non-null value if this PerformanceTiming object is being used
  // for resource timing, which can include document loads, both toplevel and
  // in subframes, and resources linked from a document.
  if (aHttpChannel) {
    SetPropertiesFromHttpChannel(aHttpChannel, aChannel);
  }
}

PerformanceTimingData::PerformanceTimingData(
    const IPCPerformanceTimingData& aIPCData)
    : mNextHopProtocol(aIPCData.nextHopProtocol()),
      mAsyncOpen(aIPCData.asyncOpen()),
      mRedirectStart(aIPCData.redirectStart()),
      mRedirectEnd(aIPCData.redirectEnd()),
      mDomainLookupStart(aIPCData.domainLookupStart()),
      mDomainLookupEnd(aIPCData.domainLookupEnd()),
      mConnectStart(aIPCData.connectStart()),
      mSecureConnectionStart(aIPCData.secureConnectionStart()),
      mConnectEnd(aIPCData.connectEnd()),
      mRequestStart(aIPCData.requestStart()),
      mResponseStart(aIPCData.responseStart()),
      mCacheReadStart(aIPCData.cacheReadStart()),
      mResponseEnd(aIPCData.responseEnd()),
      mCacheReadEnd(aIPCData.cacheReadEnd()),
      mWorkerStart(aIPCData.workerStart()),
      mWorkerRequestStart(aIPCData.workerRequestStart()),
      mWorkerResponseEnd(aIPCData.workerResponseEnd()),
      mZeroTime(aIPCData.zeroTime()),
      mFetchStart(aIPCData.fetchStart()),
      mEncodedBodySize(aIPCData.encodedBodySize()),
      mTransferSize(aIPCData.transferSize()),
      mDecodedBodySize(aIPCData.decodedBodySize()),
      mRedirectCount(aIPCData.redirectCount()),
      mAllRedirectsSameOrigin(aIPCData.allRedirectsSameOrigin()),
      mAllRedirectsPassTAO(aIPCData.allRedirectsPassTAO()),
      mSecureConnection(aIPCData.secureConnection()),
      mTimingAllowed(aIPCData.timingAllowed()),
      mInitialized(aIPCData.initialized()) {
  for (const auto& serverTimingData : aIPCData.serverTiming()) {
    RefPtr<nsServerTiming> timing = new nsServerTiming();
    timing->SetName(serverTimingData.name());
    timing->SetDuration(serverTimingData.duration());
    timing->SetDescription(serverTimingData.description());
    mServerTiming.AppendElement(timing);
  }
}

IPCPerformanceTimingData PerformanceTimingData::ToIPC() {
  nsTArray<IPCServerTiming> ipcServerTiming;
  for (auto& serverTimingData : mServerTiming) {
    nsAutoCString name;
    Unused << serverTimingData->GetName(name);
    double duration = 0;
    Unused << serverTimingData->GetDuration(&duration);
    nsAutoCString description;
    Unused << serverTimingData->GetDescription(description);
    ipcServerTiming.AppendElement(IPCServerTiming(name, duration, description));
  }
  return IPCPerformanceTimingData(
      ipcServerTiming, mNextHopProtocol, mAsyncOpen, mRedirectStart,
      mRedirectEnd, mDomainLookupStart, mDomainLookupEnd, mConnectStart,
      mSecureConnectionStart, mConnectEnd, mRequestStart, mResponseStart,
      mCacheReadStart, mResponseEnd, mCacheReadEnd, mWorkerStart,
      mWorkerRequestStart, mWorkerResponseEnd, mZeroTime, mFetchStart,
      mEncodedBodySize, mTransferSize, mDecodedBodySize, mRedirectCount,
      mAllRedirectsSameOrigin, mAllRedirectsPassTAO, mSecureConnection,
      mTimingAllowed, mInitialized);
}

void PerformanceTimingData::SetPropertiesFromHttpChannel(
    nsIHttpChannel* aHttpChannel, nsITimedChannel* aChannel) {
  MOZ_ASSERT(aHttpChannel);

  nsAutoCString protocol;
  Unused << aHttpChannel->GetProtocolVersion(protocol);
  CopyUTF8toUTF16(protocol, mNextHopProtocol);

  Unused << aHttpChannel->GetEncodedBodySize(&mEncodedBodySize);
  Unused << aHttpChannel->GetTransferSize(&mTransferSize);
  Unused << aHttpChannel->GetDecodedBodySize(&mDecodedBodySize);
  if (mDecodedBodySize == 0) {
    mDecodedBodySize = mEncodedBodySize;
  }

  mTimingAllowed = CheckAllowedOrigin(aHttpChannel, aChannel);
  aChannel->GetAllRedirectsPassTimingAllowCheck(&mAllRedirectsPassTAO);

  aChannel->GetNativeServerTiming(mServerTiming);
}

PerformanceTiming::~PerformanceTiming() = default;

DOMHighResTimeStamp PerformanceTimingData::FetchStartHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!mFetchStart) {
    if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
      return mZeroTime;
    }
    MOZ_ASSERT(!mAsyncOpen.IsNull(),
               "The fetch start time stamp should always be "
               "valid if the performance timing is enabled");
    if (!mAsyncOpen.IsNull()) {
      if (!mWorkerRequestStart.IsNull() && mWorkerRequestStart > mAsyncOpen) {
        mFetchStart = TimeStampToDOMHighRes(aPerformance, mWorkerRequestStart);
      } else {
        mFetchStart = TimeStampToDOMHighRes(aPerformance, mAsyncOpen);
      }
    }
  }
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      mFetchStart, aPerformance->GetRandomTimelineSeed(),
      aPerformance->IsSystemPrincipal(), aPerformance->CrossOriginIsolated());
}

DOMTimeMilliSec PerformanceTiming::FetchStart() {
  return static_cast<int64_t>(mTimingData->FetchStartHighRes(mPerformance));
}

bool PerformanceTimingData::CheckAllowedOrigin(nsIHttpChannel* aResourceChannel,
                                               nsITimedChannel* aChannel) {
  if (!IsInitialized()) {
    return false;
  }

  // Check that the current document passes the ckeck.
  nsCOMPtr<nsILoadInfo> loadInfo = aResourceChannel->LoadInfo();

  // TYPE_DOCUMENT loads have no loadingPrincipal.
  if (loadInfo->GetExternalContentPolicyType() ==
      ExtContentPolicy::TYPE_DOCUMENT) {
    return true;
  }

  nsCOMPtr<nsIPrincipal> principal = loadInfo->GetLoadingPrincipal();

  // Check if the resource is either same origin as the page that started
  // the load, or if the response contains the proper Timing-Allow-Origin
  // header with the domain of the page that started the load.
  return aChannel->TimingAllowCheck(principal);
}

uint8_t PerformanceTimingData::GetRedirectCount() const {
  if (!StaticPrefs::dom_enable_performance() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return 0;
  }
  if (!mAllRedirectsSameOrigin) {
    return 0;
  }
  return mRedirectCount;
}

bool PerformanceTimingData::ShouldReportCrossOriginRedirect(
    bool aEnsureSameOriginAndIgnoreTAO) const {
  if (!StaticPrefs::dom_enable_performance() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return false;
  }

  if (!mTimingAllowed || mRedirectCount == 0) {
    return false;
  }

  // If the redirect count is 0, or if one of the cross-origin
  // redirects doesn't have the proper Timing-Allow-Origin header,
  // then RedirectStart and RedirectEnd will be set to zero
  return aEnsureSameOriginAndIgnoreTAO ? mAllRedirectsSameOrigin
                                       : mAllRedirectsPassTAO;
}

DOMHighResTimeStamp PerformanceTimingData::AsyncOpenHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized() ||
      mAsyncOpen.IsNull()) {
    return mZeroTime;
  }
  DOMHighResTimeStamp rawValue =
      TimeStampToDOMHighRes(aPerformance, mAsyncOpen);
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawValue, aPerformance->GetRandomTimelineSeed(),
      aPerformance->IsSystemPrincipal(), aPerformance->CrossOriginIsolated());
}

DOMHighResTimeStamp PerformanceTimingData::WorkerStartHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized() ||
      mWorkerStart.IsNull()) {
    return mZeroTime;
  }
  DOMHighResTimeStamp rawValue =
      TimeStampToDOMHighRes(aPerformance, mWorkerStart);
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawValue, aPerformance->GetRandomTimelineSeed(),
      aPerformance->IsSystemPrincipal(), aPerformance->CrossOriginIsolated());
}

/**
 * RedirectStartHighRes() is used by both the navigation timing and the
 * resource timing. Since, navigation timing and resource timing check and
 * interpret cross-domain redirects in a different manner,
 * RedirectStartHighRes() will make no checks for cross-domain redirect.
 * It's up to the consumers of this method (PerformanceTiming::RedirectStart()
 * and PerformanceResourceTiming::RedirectStart() to make such verifications.
 *
 * @return a valid timing if the Performance Timing is enabled
 */
DOMHighResTimeStamp PerformanceTimingData::RedirectStartHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
    return mZeroTime;
  }
  return TimeStampToReducedDOMHighResOrFetchStart(aPerformance, mRedirectStart);
}

DOMTimeMilliSec PerformanceTiming::RedirectStart() {
  if (!mTimingData->IsInitialized()) {
    return 0;
  }
  // We have to check if all the redirect URIs had the same origin (since there
  // is no check in RedirectStartHighRes())
  if (mTimingData->AllRedirectsSameOrigin() &&
      mTimingData->RedirectCountReal()) {
    return static_cast<int64_t>(
        mTimingData->RedirectStartHighRes(mPerformance));
  }
  return 0;
}

/**
 * RedirectEndHighRes() is used by both the navigation timing and the resource
 * timing. Since, navigation timing and resource timing check and interpret
 * cross-domain redirects in a different manner, RedirectEndHighRes() will make
 * no checks for cross-domain redirect. It's up to the consumers of this method
 * (PerformanceTiming::RedirectEnd() and
 * PerformanceResourceTiming::RedirectEnd() to make such verifications.
 *
 * @return a valid timing if the Performance Timing is enabled
 */
DOMHighResTimeStamp PerformanceTimingData::RedirectEndHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
    return mZeroTime;
  }
  return TimeStampToReducedDOMHighResOrFetchStart(aPerformance, mRedirectEnd);
}

DOMTimeMilliSec PerformanceTiming::RedirectEnd() {
  if (!mTimingData->IsInitialized()) {
    return 0;
  }
  // We have to check if all the redirect URIs had the same origin (since there
  // is no check in RedirectEndHighRes())
  if (mTimingData->AllRedirectsSameOrigin() &&
      mTimingData->RedirectCountReal()) {
    return static_cast<int64_t>(mTimingData->RedirectEndHighRes(mPerformance));
  }
  return 0;
}

DOMHighResTimeStamp PerformanceTimingData::DomainLookupStartHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
    return mZeroTime;
  }
  // Bug 1637985 - DomainLookup information may be useful for fingerprinting.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return FetchStartHighRes(aPerformance);
  }
  return TimeStampToReducedDOMHighResOrFetchStart(aPerformance,
                                                  mDomainLookupStart);
}

DOMTimeMilliSec PerformanceTiming::DomainLookupStart() {
  return static_cast<int64_t>(
      mTimingData->DomainLookupStartHighRes(mPerformance));
}

DOMHighResTimeStamp PerformanceTimingData::DomainLookupEndHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
    return mZeroTime;
  }
  // Bug 1637985 - DomainLookup information may be useful for fingerprinting.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return FetchStartHighRes(aPerformance);
  }
  // Bug 1155008 - nsHttpTransaction is racy. Return DomainLookupStart when null
  if (mDomainLookupEnd.IsNull()) {
    return DomainLookupStartHighRes(aPerformance);
  }
  DOMHighResTimeStamp rawValue =
      TimeStampToDOMHighRes(aPerformance, mDomainLookupEnd);
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawValue, aPerformance->GetRandomTimelineSeed(),
      aPerformance->IsSystemPrincipal(), aPerformance->CrossOriginIsolated());
}

DOMTimeMilliSec PerformanceTiming::DomainLookupEnd() {
  return static_cast<int64_t>(
      mTimingData->DomainLookupEndHighRes(mPerformance));
}

DOMHighResTimeStamp PerformanceTimingData::ConnectStartHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
    return mZeroTime;
  }
  if (mConnectStart.IsNull()) {
    return DomainLookupEndHighRes(aPerformance);
  }
  DOMHighResTimeStamp rawValue =
      TimeStampToDOMHighRes(aPerformance, mConnectStart);
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawValue, aPerformance->GetRandomTimelineSeed(),
      aPerformance->IsSystemPrincipal(), aPerformance->CrossOriginIsolated());
}

DOMTimeMilliSec PerformanceTiming::ConnectStart() {
  return static_cast<int64_t>(mTimingData->ConnectStartHighRes(mPerformance));
}

DOMHighResTimeStamp PerformanceTimingData::SecureConnectionStartHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
    return mZeroTime;
  }
  if (!mSecureConnection) {
    return 0;  // We use 0 here, because mZeroTime is sometimes set to the
               // navigation start time.
  }
  if (mSecureConnectionStart.IsNull()) {
    return ConnectStartHighRes(aPerformance);
  }
  DOMHighResTimeStamp rawValue =
      TimeStampToDOMHighRes(aPerformance, mSecureConnectionStart);
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawValue, aPerformance->GetRandomTimelineSeed(),
      aPerformance->IsSystemPrincipal(), aPerformance->CrossOriginIsolated());
}

DOMTimeMilliSec PerformanceTiming::SecureConnectionStart() {
  return static_cast<int64_t>(
      mTimingData->SecureConnectionStartHighRes(mPerformance));
}

DOMHighResTimeStamp PerformanceTimingData::ConnectEndHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
    return mZeroTime;
  }
  // Bug 1155008 - nsHttpTransaction is racy. Return ConnectStart when null
  if (mConnectEnd.IsNull()) {
    return ConnectStartHighRes(aPerformance);
  }
  DOMHighResTimeStamp rawValue =
      TimeStampToDOMHighRes(aPerformance, mConnectEnd);
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawValue, aPerformance->GetRandomTimelineSeed(),
      aPerformance->IsSystemPrincipal(), aPerformance->CrossOriginIsolated());
}

DOMTimeMilliSec PerformanceTiming::ConnectEnd() {
  return static_cast<int64_t>(mTimingData->ConnectEndHighRes(mPerformance));
}

DOMHighResTimeStamp PerformanceTimingData::RequestStartHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
    return mZeroTime;
  }

  if (mRequestStart.IsNull()) {
    mRequestStart = mWorkerRequestStart;
  }

  return TimeStampToReducedDOMHighResOrFetchStart(aPerformance, mRequestStart);
}

DOMTimeMilliSec PerformanceTiming::RequestStart() {
  return static_cast<int64_t>(mTimingData->RequestStartHighRes(mPerformance));
}

DOMHighResTimeStamp PerformanceTimingData::ResponseStartHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
    return mZeroTime;
  }
  if (mResponseStart.IsNull() ||
      (!mCacheReadStart.IsNull() && mCacheReadStart < mResponseStart)) {
    mResponseStart = mCacheReadStart;
  }

  if (mResponseStart.IsNull() ||
      (!mRequestStart.IsNull() && mResponseStart < mRequestStart)) {
    mResponseStart = mRequestStart;
  }
  return TimeStampToReducedDOMHighResOrFetchStart(aPerformance, mResponseStart);
}

DOMTimeMilliSec PerformanceTiming::ResponseStart() {
  return static_cast<int64_t>(mTimingData->ResponseStartHighRes(mPerformance));
}

DOMHighResTimeStamp PerformanceTimingData::ResponseEndHighRes(
    Performance* aPerformance) {
  MOZ_ASSERT(aPerformance);

  if (!StaticPrefs::dom_enable_performance() || !IsInitialized()) {
    return mZeroTime;
  }
  if (mResponseEnd.IsNull() ||
      (!mCacheReadEnd.IsNull() && mCacheReadEnd < mResponseEnd)) {
    mResponseEnd = mCacheReadEnd;
  }
  if (mResponseEnd.IsNull()) {
    mResponseEnd = mWorkerResponseEnd;
  }
  // Bug 1155008 - nsHttpTransaction is racy. Return ResponseStart when null
  if (mResponseEnd.IsNull()) {
    return ResponseStartHighRes(aPerformance);
  }
  DOMHighResTimeStamp rawValue =
      TimeStampToDOMHighRes(aPerformance, mResponseEnd);
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawValue, aPerformance->GetRandomTimelineSeed(),
      aPerformance->IsSystemPrincipal(), aPerformance->CrossOriginIsolated());
}

DOMTimeMilliSec PerformanceTiming::ResponseEnd() {
  return static_cast<int64_t>(mTimingData->ResponseEndHighRes(mPerformance));
}

JSObject* PerformanceTiming::WrapObject(JSContext* cx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return PerformanceTiming_Binding::Wrap(cx, this, aGivenProto);
}

bool PerformanceTiming::IsTopLevelContentDocument() const {
  nsCOMPtr<Document> document = mPerformance->GetDocumentIfCurrent();
  if (!document) {
    return false;
  }

  if (BrowsingContext* bc = document->GetBrowsingContext()) {
    return bc->IsTopContent();
  }
  return false;
}

nsTArray<nsCOMPtr<nsIServerTiming>> PerformanceTimingData::GetServerTiming() {
  if (!StaticPrefs::dom_enable_performance() || !IsInitialized() ||
      !TimingAllowed()) {
    return nsTArray<nsCOMPtr<nsIServerTiming>>();
  }

  return mServerTiming.Clone();
}

}  // namespace mozilla::dom
