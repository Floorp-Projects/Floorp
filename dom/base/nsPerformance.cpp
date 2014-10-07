/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPerformance.h"
#include "nsCOMPtr.h"
#include "nsIHttpChannel.h"
#include "nsITimedChannel.h"
#include "nsDOMNavigationTiming.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"
#include "PerformanceEntry.h"
#include "PerformanceResourceTiming.h"
#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceTimingBinding.h"
#include "mozilla/dom/PerformanceNavigationBinding.h"
#include "mozilla/TimeStamp.h"
#include "nsThreadUtils.h"
#include "nsILoadInfo.h"

using namespace mozilla;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsPerformanceTiming, mPerformance)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsPerformanceTiming, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsPerformanceTiming, Release)

nsPerformanceTiming::nsPerformanceTiming(nsPerformance* aPerformance,
                                         nsITimedChannel* aChannel,
                                         nsIHttpChannel* aHttpChannel,
                                         DOMHighResTimeStamp aZeroTime)
  : mPerformance(aPerformance),
    mChannel(aChannel),
    mFetchStart(0.0),
    mZeroTime(aZeroTime),
    mTimingAllowed(true),
    mReportCrossOriginRedirect(true)
{
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");

  if (!nsContentUtils::IsPerformanceTimingEnabled()) {
    mZeroTime = 0;
  }

  // The aHttpChannel argument is null if this nsPerformanceTiming object
  // is being used for the navigation timing (document) and has a non-null
  // value for the resource timing (any resources within the page).
  if (aHttpChannel) {
    mTimingAllowed = CheckAllowedOrigin(aHttpChannel);
    bool redirectsPassCheck = false;
    mChannel->GetAllRedirectsPassTimingAllowCheck(&redirectsPassCheck);
    mReportCrossOriginRedirect = mTimingAllowed && redirectsPassCheck;
  }
}

nsPerformanceTiming::~nsPerformanceTiming()
{
}

DOMHighResTimeStamp
nsPerformanceTiming::FetchStartHighRes()
{
  if (!mFetchStart) {
    if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
      return mZeroTime;
    }
    TimeStamp stamp;
    mChannel->GetAsyncOpen(&stamp);
    MOZ_ASSERT(!stamp.IsNull(), "The fetch start time stamp should always be "
        "valid if the performance timing is enabled");
    mFetchStart = (!stamp.IsNull())
        ? TimeStampToDOMHighRes(stamp)
        : 0.0;
  }
  return mFetchStart;
}

DOMTimeMilliSec
nsPerformanceTiming::FetchStart()
{
  return static_cast<int64_t>(FetchStartHighRes());
}

bool
nsPerformanceTiming::CheckAllowedOrigin(nsIHttpChannel* aResourceChannel)
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
  nsCOMPtr<nsIPrincipal> principal = loadInfo->LoadingPrincipal();

  // Check if the resource is either same origin as the page that started
  // the load, or if the response contains the proper Timing-Allow-Origin
  // header with the domain of the page that started the load.
  return mChannel->TimingAllowCheck(principal);
}

bool
nsPerformanceTiming::TimingAllowed() const
{
  return mTimingAllowed;
}

uint16_t
nsPerformanceTiming::GetRedirectCount() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return 0;
  }
  bool sameOrigin;
  mChannel->GetAllRedirectsSameOrigin(&sameOrigin);
  if (!sameOrigin) {
    return 0;
  }
  uint16_t redirectCount;
  mChannel->GetRedirectCount(&redirectCount);
  return redirectCount;
}

bool
nsPerformanceTiming::ShouldReportCrossOriginRedirect() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return false;
  }

  // If the redirect count is 0, or if one of the cross-origin
  // redirects doesn't have the proper Timing-Allow-Origin header,
  // then RedirectStart and RedirectEnd will be set to zero
  uint16_t redirectCount;
  mChannel->GetRedirectCount(&redirectCount);
  return (redirectCount != 0) && mReportCrossOriginRedirect;
}

/**
 * RedirectStartHighRes() is used by both the navigation timing and the
 * resource timing. Since, navigation timing and resource timing check and
 * interpret cross-domain redirects in a different manner,
 * RedirectStartHighRes() will make no checks for cross-domain redirect.
 * It's up to the consumers of this method (nsPerformanceTiming::RedirectStart()
 * and PerformanceResourceTiming::RedirectStart() to make such verifications.
 *
 * @return a valid timing if the Performance Timing is enabled
 */
DOMHighResTimeStamp
nsPerformanceTiming::RedirectStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return mZeroTime;
  }
  mozilla::TimeStamp stamp;
  mChannel->GetRedirectStart(&stamp);
  return TimeStampToDOMHighResOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::RedirectStart()
{
  if (!IsInitialized()) {
    return mZeroTime;
  }
  // We have to check if all the redirect URIs had the same origin (since there
  // is no check in RedirectStartHighRes())
  bool sameOrigin;
  mChannel->GetAllRedirectsSameOrigin(&sameOrigin);
  if (sameOrigin) {
    return static_cast<int64_t>(RedirectStartHighRes());
  }
  return 0;
}

/**
 * RedirectEndHighRes() is used by both the navigation timing and the resource
 * timing. Since, navigation timing and resource timing check and interpret
 * cross-domain redirects in a different manner, RedirectEndHighRes() will make
 * no checks for cross-domain redirect. It's up to the consumers of this method
 * (nsPerformanceTiming::RedirectEnd() and
 * PerformanceResourceTiming::RedirectEnd() to make such verifications.
 *
 * @return a valid timing if the Performance Timing is enabled
 */
DOMHighResTimeStamp
nsPerformanceTiming::RedirectEndHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return mZeroTime;
  }
  mozilla::TimeStamp stamp;
  mChannel->GetRedirectEnd(&stamp);
  return TimeStampToDOMHighResOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::RedirectEnd()
{
  if (!IsInitialized()) {
    return mZeroTime;
  }
  // We have to check if all the redirect URIs had the same origin (since there
  // is no check in RedirectEndHighRes())
  bool sameOrigin;
  mChannel->GetAllRedirectsSameOrigin(&sameOrigin);
  if (sameOrigin) {
    return static_cast<int64_t>(RedirectEndHighRes());
  }
  return 0;
}

DOMHighResTimeStamp
nsPerformanceTiming::DomainLookupStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return mZeroTime;
  }
  mozilla::TimeStamp stamp;
  mChannel->GetDomainLookupStart(&stamp);
  return TimeStampToDOMHighResOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::DomainLookupStart()
{
  return static_cast<int64_t>(DomainLookupStartHighRes());
}

DOMHighResTimeStamp
nsPerformanceTiming::DomainLookupEndHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return mZeroTime;
  }
  mozilla::TimeStamp stamp;
  mChannel->GetDomainLookupEnd(&stamp);
  return TimeStampToDOMHighResOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::DomainLookupEnd()
{
  return static_cast<int64_t>(DomainLookupEndHighRes());
}

DOMHighResTimeStamp
nsPerformanceTiming::ConnectStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return mZeroTime;
  }
  mozilla::TimeStamp stamp;
  mChannel->GetConnectStart(&stamp);
  return TimeStampToDOMHighResOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::ConnectStart()
{
  return static_cast<int64_t>(ConnectStartHighRes());
}

DOMHighResTimeStamp
nsPerformanceTiming::ConnectEndHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return mZeroTime;
  }
  mozilla::TimeStamp stamp;
  mChannel->GetConnectEnd(&stamp);
  return TimeStampToDOMHighResOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::ConnectEnd()
{
  return static_cast<int64_t>(ConnectEndHighRes());
}

DOMHighResTimeStamp
nsPerformanceTiming::RequestStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return mZeroTime;
  }
  mozilla::TimeStamp stamp;
  mChannel->GetRequestStart(&stamp);
  return TimeStampToDOMHighResOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::RequestStart()
{
  return static_cast<int64_t>(RequestStartHighRes());
}

DOMHighResTimeStamp
nsPerformanceTiming::ResponseStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return mZeroTime;
  }
  mozilla::TimeStamp stamp;
  mChannel->GetResponseStart(&stamp);
  mozilla::TimeStamp cacheStamp;
  mChannel->GetCacheReadStart(&cacheStamp);
  if (stamp.IsNull() || (!cacheStamp.IsNull() && cacheStamp < stamp)) {
    stamp = cacheStamp;
  }
  return TimeStampToDOMHighResOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::ResponseStart()
{
  return static_cast<int64_t>(ResponseStartHighRes());
}

DOMHighResTimeStamp
nsPerformanceTiming::ResponseEndHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return mZeroTime;
  }
  mozilla::TimeStamp stamp;
  mChannel->GetResponseEnd(&stamp);
  mozilla::TimeStamp cacheStamp;
  mChannel->GetCacheReadEnd(&cacheStamp);
  if (stamp.IsNull() || (!cacheStamp.IsNull() && cacheStamp < stamp)) {
    stamp = cacheStamp;
  }
  return TimeStampToDOMHighResOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::ResponseEnd()
{
  return static_cast<int64_t>(ResponseEndHighRes());
}

bool
nsPerformanceTiming::IsInitialized() const
{
  return !!mChannel;
}

JSObject*
nsPerformanceTiming::WrapObject(JSContext *cx)
{
  return dom::PerformanceTimingBinding::Wrap(cx, this);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsPerformanceNavigation, mPerformance)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsPerformanceNavigation, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsPerformanceNavigation, Release)

nsPerformanceNavigation::nsPerformanceNavigation(nsPerformance* aPerformance)
  : mPerformance(aPerformance)
{
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");
}

nsPerformanceNavigation::~nsPerformanceNavigation()
{
}

JSObject*
nsPerformanceNavigation::WrapObject(JSContext *cx)
{
  return dom::PerformanceNavigationBinding::Wrap(cx, this);
}


NS_IMPL_CYCLE_COLLECTION_INHERITED(nsPerformance, DOMEventTargetHelper,
                                   mWindow, mTiming,
                                   mNavigation, mEntries,
                                   mParentPerformance)
NS_IMPL_ADDREF_INHERITED(nsPerformance, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsPerformance, DOMEventTargetHelper)

nsPerformance::nsPerformance(nsPIDOMWindow* aWindow,
                             nsDOMNavigationTiming* aDOMTiming,
                             nsITimedChannel* aChannel,
                             nsPerformance* aParentPerformance)
  : DOMEventTargetHelper(aWindow),
    mWindow(aWindow),
    mDOMTiming(aDOMTiming),
    mChannel(aChannel),
    mParentPerformance(aParentPerformance),
    mPrimaryBufferSize(kDefaultBufferSize)
{
  MOZ_ASSERT(aWindow, "Parent window object should be provided");
}

nsPerformance::~nsPerformance()
{
}

// QueryInterface implementation for nsPerformance
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPerformance)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)


nsPerformanceTiming*
nsPerformance::Timing()
{
  if (!mTiming) {
    // For navigation timing, the third argument (an nsIHtttpChannel) is null
    // since the cross-domain redirect were already checked.
    // The last argument (zero time) for performance.timing is the navigation
    // start value.
    mTiming = new nsPerformanceTiming(this, mChannel, nullptr,
        mDOMTiming->GetNavigationStart());
  }
  return mTiming;
}

void
nsPerformance::DispatchBufferFullEvent()
{
  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  if (NS_SUCCEEDED(rv)) {
    // it bubbles, and it isn't cancelable
    rv = event->InitEvent(NS_LITERAL_STRING("resourcetimingbufferfull"), true, false);
    if (NS_SUCCEEDED(rv)) {
      event->SetTrusted(true);
      DispatchDOMEvent(nullptr, event, nullptr, nullptr);
    }
  }
}

nsPerformanceNavigation*
nsPerformance::Navigation()
{
  if (!mNavigation) {
    mNavigation = new nsPerformanceNavigation(this);
  }
  return mNavigation;
}

DOMHighResTimeStamp
nsPerformance::Now()
{
  return GetDOMTiming()->TimeStampToDOMHighRes(mozilla::TimeStamp::Now());
}

JSObject*
nsPerformance::WrapObject(JSContext *cx)
{
  return dom::PerformanceBinding::Wrap(cx, this);
}

void
nsPerformance::GetEntries(nsTArray<nsRefPtr<PerformanceEntry> >& retval)
{
  MOZ_ASSERT(NS_IsMainThread());

  retval = mEntries;
}

void
nsPerformance::GetEntriesByType(const nsAString& entryType,
                                nsTArray<nsRefPtr<PerformanceEntry> >& retval)
{
  MOZ_ASSERT(NS_IsMainThread());

  retval.Clear();
  uint32_t count = mEntries.Length();
  for (uint32_t i = 0 ; i < count; i++) {
    if (mEntries[i]->GetEntryType().Equals(entryType)) {
      retval.AppendElement(mEntries[i]);
    }
  }
}

void
nsPerformance::GetEntriesByName(const nsAString& name,
                                const mozilla::dom::Optional<nsAString>& entryType,
                                nsTArray<nsRefPtr<PerformanceEntry> >& retval)
{
  MOZ_ASSERT(NS_IsMainThread());

  retval.Clear();
  uint32_t count = mEntries.Length();
  for (uint32_t i = 0 ; i < count; i++) {
    if (mEntries[i]->GetName().Equals(name) &&
        (!entryType.WasPassed() ||
         mEntries[i]->GetEntryType().Equals(entryType.Value()))) {
      retval.AppendElement(mEntries[i]);
    }
  }
}

void
nsPerformance::ClearResourceTimings()
{
  MOZ_ASSERT(NS_IsMainThread());
  mEntries.Clear();
}

void
nsPerformance::SetResourceTimingBufferSize(uint64_t maxSize)
{
  MOZ_ASSERT(NS_IsMainThread());
  mPrimaryBufferSize = maxSize;
}

/**
 * An entry should be added only after the resource is loaded.
 * This method is not thread safe and can only be called on the main thread.
 */
void
nsPerformance::AddEntry(nsIHttpChannel* channel,
                        nsITimedChannel* timedChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Check if resource timing is prefed off.
  if (!nsContentUtils::IsResourceTimingEnabled()) {
    return;
  }

  // Don't add the entry if the buffer is full
  if (mEntries.Length() >= mPrimaryBufferSize) {
    return;
  }

  if (channel && timedChannel) {
    nsAutoCString name;
    nsAutoString initiatorType;
    nsCOMPtr<nsIURI> originalURI;

    timedChannel->GetInitiatorType(initiatorType);

    // According to the spec, "The name attribute must return the resolved URL
    // of the requested resource. This attribute must not change even if the
    // fetch redirected to a different URL."
    channel->GetOriginalURI(getter_AddRefs(originalURI));
    originalURI->GetSpec(name);
    NS_ConvertUTF8toUTF16 entryName(name);

    // The nsITimedChannel argument will be used to gather all the timings.
    // The nsIHttpChannel argument will be used to check if any cross-origin
    // redirects occurred.
    // The last argument is the "zero time" (offset). Since we don't want
    // any offset for the resource timing, this will be set to "0" - the
    // resource timing returns a relative timing (no offset).
    nsRefPtr<nsPerformanceTiming> performanceTiming =
        new nsPerformanceTiming(this, timedChannel, channel,
            0);

    // The PerformanceResourceTiming object will use the nsPerformanceTiming
    // object to get all the required timings.
    nsRefPtr<dom::PerformanceResourceTiming> performanceEntry =
        new dom::PerformanceResourceTiming(performanceTiming, this);

    performanceEntry->SetName(entryName);
    performanceEntry->SetEntryType(NS_LITERAL_STRING("resource"));
    // If the initiator type had no valid value, then set it to the default
    // ("other") value.
    if (initiatorType.IsEmpty()) {
      initiatorType = NS_LITERAL_STRING("other");
    }
    performanceEntry->SetInitiatorType(initiatorType);

    mEntries.InsertElementSorted(performanceEntry,
        PerformanceEntryComparator());
    if (mEntries.Length() >= mPrimaryBufferSize) {
      // call onresourcetimingbufferfull
      DispatchBufferFullEvent();
    }
  }
}

bool
nsPerformance::PerformanceEntryComparator::Equals(
    const PerformanceEntry* aElem1,
    const PerformanceEntry* aElem2) const
{
  NS_ABORT_IF_FALSE(aElem1 && aElem2,
      "Trying to compare null performance entries");
  return aElem1->StartTime() == aElem2->StartTime();
}

bool
nsPerformance::PerformanceEntryComparator::LessThan(
    const PerformanceEntry* aElem1,
    const PerformanceEntry* aElem2) const
{
  NS_ABORT_IF_FALSE(aElem1 && aElem2,
      "Trying to compare null performance entries");
  return aElem1->StartTime() < aElem2->StartTime();
}
