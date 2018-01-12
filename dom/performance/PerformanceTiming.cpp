/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceTiming.h"
#include "mozilla/dom/PerformanceTimingBinding.h"
#include "mozilla/Telemetry.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocument.h"
#include "nsITimedChannel.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PerformanceTiming, mPerformance)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(PerformanceTiming, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(PerformanceTiming, Release)

PerformanceTiming::PerformanceTiming(Performance* aPerformance,
                                     nsITimedChannel* aChannel,
                                     nsIHttpChannel* aHttpChannel,
                                     DOMHighResTimeStamp aZeroTime)
  : mPerformance(aPerformance),
    mFetchStart(0.0),
    mZeroTime(nsRFPService::ReduceTimePrecisionAsMSecs(aZeroTime)),
    mRedirectCount(0),
    mTimingAllowed(true),
    mAllRedirectsSameOrigin(true),
    mInitialized(!!aChannel),
    mReportCrossOriginRedirect(true)
{
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");

  if (!nsContentUtils::IsPerformanceTimingEnabled() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    mZeroTime = 0;
  }

  // The aHttpChannel argument is null if this PerformanceTiming object is
  // being used for navigation timing (which is only relevant for documents).
  // It has a non-null value if this PerformanceTiming object is being used
  // for resource timing, which can include document loads, both toplevel and
  // in subframes, and resources linked from a document.
  if (aHttpChannel) {
    mTimingAllowed = CheckAllowedOrigin(aHttpChannel, aChannel);
    bool redirectsPassCheck = false;
    aChannel->GetAllRedirectsPassTimingAllowCheck(&redirectsPassCheck);
    mReportCrossOriginRedirect = mTimingAllowed && redirectsPassCheck;
  }

  mSecureConnection = false;
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
    nsresult rv = uri->SchemeIs("https", &mSecureConnection);
    if (NS_FAILED(rv)) {
      mSecureConnection = false;
    }
  }
  InitializeTimingInfo(aChannel);

  // Non-null aHttpChannel implies that this PerformanceTiming object is being
  // used for subresources, which is irrelevant to this probe.
  if (!aHttpChannel &&
      nsContentUtils::IsPerformanceTimingEnabled() &&
      IsTopLevelContentDocument()) {
    Telemetry::Accumulate(Telemetry::TIME_TO_RESPONSE_START_MS,
                          ResponseStartHighRes() - mZeroTime);
  }
}

// Copy the timing info from the channel so we don't need to keep the channel
// alive just to get the timestamps.
void
PerformanceTiming::InitializeTimingInfo(nsITimedChannel* aChannel)
{
  if (aChannel) {
    aChannel->GetAsyncOpen(&mAsyncOpen);
    aChannel->GetAllRedirectsSameOrigin(&mAllRedirectsSameOrigin);
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
    // have a strict relation with each other. The truth, however, is the browser
    // engages in a number of speculative activities that sometimes mean connections
    // and lookups begin at different times. Workaround that here by clamping
    // these values to what we expect FetchStart to be.  This means the later of
    // AsyncOpen or WorkerStart times.
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
}

PerformanceTiming::~PerformanceTiming()
{
}

DOMHighResTimeStamp
PerformanceTiming::FetchStartHighRes()
{
  if (!mFetchStart) {
    if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
        nsContentUtils::ShouldResistFingerprinting()) {
      return mZeroTime;
    }
    MOZ_ASSERT(!mAsyncOpen.IsNull(), "The fetch start time stamp should always be "
        "valid if the performance timing is enabled");
    if (!mAsyncOpen.IsNull()) {
      if (!mWorkerRequestStart.IsNull() && mWorkerRequestStart > mAsyncOpen) {
        mFetchStart = TimeStampToDOMHighRes(mWorkerRequestStart);
      } else {
        mFetchStart = TimeStampToDOMHighRes(mAsyncOpen);
      }
    }
  }
  return nsRFPService::ReduceTimePrecisionAsMSecs(mFetchStart);
}

DOMTimeMilliSec
PerformanceTiming::FetchStart()
{
  return static_cast<int64_t>(FetchStartHighRes());
}

bool
PerformanceTiming::CheckAllowedOrigin(nsIHttpChannel* aResourceChannel,
                                      nsITimedChannel* aChannel)
{
  if (!IsInitialized()) {
    return false;
  }

  // Check that the current document passes the ckeck.
  nsCOMPtr<nsILoadInfo> loadInfo;
  aResourceChannel->GetLoadInfo(getter_AddRefs(loadInfo));
  if (!loadInfo) {
    return false;
  }

  // TYPE_DOCUMENT loads have no loadingPrincipal.  And that's OK, because we
  // never actually need to have a performance timing entry for TYPE_DOCUMENT
  // loads.
  if (loadInfo->GetExternalContentPolicyType() == nsIContentPolicy::TYPE_DOCUMENT) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal = loadInfo->LoadingPrincipal();

  // Check if the resource is either same origin as the page that started
  // the load, or if the response contains the proper Timing-Allow-Origin
  // header with the domain of the page that started the load.
  return aChannel->TimingAllowCheck(principal);
}

bool
PerformanceTiming::TimingAllowed() const
{
  return mTimingAllowed;
}

uint8_t
PerformanceTiming::GetRedirectCount() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return 0;
  }
  if (!mAllRedirectsSameOrigin) {
    return 0;
  }
  return mRedirectCount;
}

bool
PerformanceTiming::ShouldReportCrossOriginRedirect() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return false;
  }

  // If the redirect count is 0, or if one of the cross-origin
  // redirects doesn't have the proper Timing-Allow-Origin header,
  // then RedirectStart and RedirectEnd will be set to zero
  return (mRedirectCount != 0) && mReportCrossOriginRedirect;
}

DOMHighResTimeStamp
PerformanceTiming::AsyncOpenHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting() || mAsyncOpen.IsNull()) {
    return mZeroTime;
  }
  return nsRFPService::ReduceTimePrecisionAsMSecs(TimeStampToDOMHighRes(mAsyncOpen));
}

DOMHighResTimeStamp
PerformanceTiming::WorkerStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting() || mWorkerStart.IsNull()) {
    return mZeroTime;
  }
  return nsRFPService::ReduceTimePrecisionAsMSecs(TimeStampToDOMHighRes(mWorkerStart));
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
DOMHighResTimeStamp
PerformanceTiming::RedirectStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return mZeroTime;
  }
  return TimeStampToReducedDOMHighResOrFetchStart(mRedirectStart);
}

DOMTimeMilliSec
PerformanceTiming::RedirectStart()
{
  if (!IsInitialized()) {
    return 0;
  }
  // We have to check if all the redirect URIs had the same origin (since there
  // is no check in RedirectStartHighRes())
  if (mAllRedirectsSameOrigin && mRedirectCount) {
    return static_cast<int64_t>(RedirectStartHighRes());
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
DOMHighResTimeStamp
PerformanceTiming::RedirectEndHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return mZeroTime;
  }
  return TimeStampToReducedDOMHighResOrFetchStart(mRedirectEnd);
}

DOMTimeMilliSec
PerformanceTiming::RedirectEnd()
{
  if (!IsInitialized()) {
    return 0;
  }
  // We have to check if all the redirect URIs had the same origin (since there
  // is no check in RedirectEndHighRes())
  if (mAllRedirectsSameOrigin && mRedirectCount) {
    return static_cast<int64_t>(RedirectEndHighRes());
  }
  return 0;
}

DOMHighResTimeStamp
PerformanceTiming::DomainLookupStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return mZeroTime;
  }
  return TimeStampToReducedDOMHighResOrFetchStart(mDomainLookupStart);
}

DOMTimeMilliSec
PerformanceTiming::DomainLookupStart()
{
  return static_cast<int64_t>(DomainLookupStartHighRes());
}

DOMHighResTimeStamp
PerformanceTiming::DomainLookupEndHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return mZeroTime;
  }
  // Bug 1155008 - nsHttpTransaction is racy. Return DomainLookupStart when null
  return mDomainLookupEnd.IsNull() ? DomainLookupStartHighRes()
                                   : nsRFPService::ReduceTimePrecisionAsMSecs(
                                       TimeStampToDOMHighRes(mDomainLookupEnd));
}

DOMTimeMilliSec
PerformanceTiming::DomainLookupEnd()
{
  return static_cast<int64_t>(DomainLookupEndHighRes());
}

DOMHighResTimeStamp
PerformanceTiming::ConnectStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return mZeroTime;
  }
  return mConnectStart.IsNull() ? DomainLookupEndHighRes()
                                : nsRFPService::ReduceTimePrecisionAsMSecs(
                                    TimeStampToDOMHighRes(mConnectStart));
}

DOMTimeMilliSec
PerformanceTiming::ConnectStart()
{
  return static_cast<int64_t>(ConnectStartHighRes());
}

DOMHighResTimeStamp
PerformanceTiming::SecureConnectionStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return mZeroTime;
  }
  return !mSecureConnection
    ? 0 // We use 0 here, because mZeroTime is sometimes set to the navigation
        // start time.
    : (mSecureConnectionStart.IsNull() ? mZeroTime
                                       : nsRFPService::ReduceTimePrecisionAsMSecs(
                                           TimeStampToDOMHighRes(mSecureConnectionStart)));
}

DOMTimeMilliSec
PerformanceTiming::SecureConnectionStart()
{
  return static_cast<int64_t>(SecureConnectionStartHighRes());
}

DOMHighResTimeStamp
PerformanceTiming::ConnectEndHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return mZeroTime;
  }
  // Bug 1155008 - nsHttpTransaction is racy. Return ConnectStart when null
  return mConnectEnd.IsNull() ? ConnectStartHighRes()
                              : nsRFPService::ReduceTimePrecisionAsMSecs(
                                  TimeStampToDOMHighRes(mConnectEnd));
}

DOMTimeMilliSec
PerformanceTiming::ConnectEnd()
{
  return static_cast<int64_t>(ConnectEndHighRes());
}

DOMHighResTimeStamp
PerformanceTiming::RequestStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return mZeroTime;
  }

  if (mRequestStart.IsNull()) {
    mRequestStart = mWorkerRequestStart;
  }

  return TimeStampToReducedDOMHighResOrFetchStart(mRequestStart);
}

DOMTimeMilliSec
PerformanceTiming::RequestStart()
{
  return static_cast<int64_t>(RequestStartHighRes());
}

DOMHighResTimeStamp
PerformanceTiming::ResponseStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
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
  return TimeStampToReducedDOMHighResOrFetchStart(mResponseStart);
}

DOMTimeMilliSec
PerformanceTiming::ResponseStart()
{
  return static_cast<int64_t>(ResponseStartHighRes());
}

DOMHighResTimeStamp
PerformanceTiming::ResponseEndHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized() ||
      nsContentUtils::ShouldResistFingerprinting()) {
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
  return mResponseEnd.IsNull() ? ResponseStartHighRes()
                               : nsRFPService::ReduceTimePrecisionAsMSecs(
                                   TimeStampToDOMHighRes(mResponseEnd));
}

DOMTimeMilliSec
PerformanceTiming::ResponseEnd()
{
  return static_cast<int64_t>(ResponseEndHighRes());
}

bool
PerformanceTiming::IsInitialized() const
{
  return mInitialized;
}

JSObject*
PerformanceTiming::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceTimingBinding::Wrap(cx, this, aGivenProto);
}

bool
PerformanceTiming::IsTopLevelContentDocument() const
{
  nsCOMPtr<nsIDocument> document = mPerformance->GetDocumentIfCurrent();
  if (!document) {
    return false;
  }
  nsCOMPtr<nsIDocShell> docShell = document->GetDocShell();
  if (!docShell) {
    return false;
  }
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  Unused << docShell->GetSameTypeRootTreeItem(getter_AddRefs(rootItem));
  if (rootItem.get() != static_cast<nsIDocShellTreeItem*>(docShell.get())) {
    return false;
  }
  return rootItem->ItemType() == nsIDocShellTreeItem::typeContent;
}

} // dom namespace
} // mozilla namespace
