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
#include "nsILoadInfo.h"
#include "nsIURI.h"
#include "nsThreadUtils.h"
#include "PerformanceEntry.h"
#include "PerformanceMark.h"
#include "PerformanceMeasure.h"
#include "PerformanceResourceTiming.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceTimingBinding.h"
#include "mozilla/dom/PerformanceNavigationBinding.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/TimeStamp.h"
#include "js/HeapAPI.h"

#ifdef MOZ_WIDGET_GONK
#define PERFLOG(msg, ...)  __android_log_print(ANDROID_LOG_INFO, "PerformanceTiming", msg, ##__VA_ARGS__)
#else
#define PERFLOG(msg, ...) printf_stderr(msg, ##__VA_ARGS__)
#endif

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsPerformanceTiming, mPerformance)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsPerformanceTiming, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsPerformanceTiming, Release)

nsPerformanceTiming::nsPerformanceTiming(nsPerformance* aPerformance,
                                         nsITimedChannel* aChannel,
                                         nsIHttpChannel* aHttpChannel,
                                         DOMHighResTimeStamp aZeroTime)
  : mPerformance(aPerformance),
    mFetchStart(0.0),
    mZeroTime(aZeroTime),
    mRedirectCount(0),
    mTimingAllowed(true),
    mAllRedirectsSameOrigin(true),
    mInitialized(!!aChannel),
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
    mTimingAllowed = CheckAllowedOrigin(aHttpChannel, aChannel);
    bool redirectsPassCheck = false;
    aChannel->GetAllRedirectsPassTimingAllowCheck(&redirectsPassCheck);
    mReportCrossOriginRedirect = mTimingAllowed && redirectsPassCheck;
  }

  InitializeTimingInfo(aChannel);
}

// Copy the timing info from the channel so we don't need to keep the channel
// alive just to get the timestamps.
void
nsPerformanceTiming::InitializeTimingInfo(nsITimedChannel* aChannel)
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
    aChannel->GetConnectEnd(&mConnectEnd);
    aChannel->GetRequestStart(&mRequestStart);
    aChannel->GetResponseStart(&mResponseStart);
    aChannel->GetCacheReadStart(&mCacheReadStart);
    aChannel->GetResponseEnd(&mResponseEnd);
    aChannel->GetCacheReadEnd(&mCacheReadEnd);
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
    MOZ_ASSERT(!mAsyncOpen.IsNull(), "The fetch start time stamp should always be "
        "valid if the performance timing is enabled");
    mFetchStart = (!mAsyncOpen.IsNull())
        ? TimeStampToDOMHighRes(mAsyncOpen)
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
nsPerformanceTiming::CheckAllowedOrigin(nsIHttpChannel* aResourceChannel,
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
  nsCOMPtr<nsIPrincipal> principal = loadInfo->LoadingPrincipal();

  // Check if the resource is either same origin as the page that started
  // the load, or if the response contains the proper Timing-Allow-Origin
  // header with the domain of the page that started the load.
  return aChannel->TimingAllowCheck(principal);
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
  if (!mAllRedirectsSameOrigin) {
    return 0;
  }
  return mRedirectCount;
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
  return (mRedirectCount != 0) && mReportCrossOriginRedirect;
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
  return TimeStampToDOMHighResOrFetchStart(mRedirectStart);
}

DOMTimeMilliSec
nsPerformanceTiming::RedirectStart()
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
  return TimeStampToDOMHighResOrFetchStart(mRedirectEnd);
}

DOMTimeMilliSec
nsPerformanceTiming::RedirectEnd()
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
nsPerformanceTiming::DomainLookupStartHighRes()
{
  if (!nsContentUtils::IsPerformanceTimingEnabled() || !IsInitialized()) {
    return mZeroTime;
  }
  return TimeStampToDOMHighResOrFetchStart(mDomainLookupStart);
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
  // Bug 1155008 - nsHttpTransaction is racy. Return DomainLookupStart when null
  return mDomainLookupEnd.IsNull() ? DomainLookupStartHighRes()
                                   : TimeStampToDOMHighRes(mDomainLookupEnd);
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
  return mConnectStart.IsNull() ? DomainLookupEndHighRes()
                                : TimeStampToDOMHighRes(mConnectStart);
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
  // Bug 1155008 - nsHttpTransaction is racy. Return ConnectStart when null
  return mConnectEnd.IsNull() ? ConnectStartHighRes()
                              : TimeStampToDOMHighRes(mConnectEnd);
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
  return TimeStampToDOMHighResOrFetchStart(mRequestStart);
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
  if (mResponseStart.IsNull() ||
     (!mCacheReadStart.IsNull() && mCacheReadStart < mResponseStart)) {
    mResponseStart = mCacheReadStart;
  }
  return TimeStampToDOMHighResOrFetchStart(mResponseStart);
}

DOMTimeMilliSec
nsPerformanceTiming::ResponseStart()
{
  return static_cast<int64_t>(ResponseStartHighRes());
}

DOMHighResTimeStamp
nsPerformanceTiming::ResponseEndHighRes()
{
  if (!IsInitialized()) {
    return mZeroTime;
  }
  if (mResponseEnd.IsNull() ||
     (!mCacheReadEnd.IsNull() && mCacheReadEnd < mResponseEnd)) {
    mResponseEnd = mCacheReadEnd;
  }
  // Bug 1155008 - nsHttpTransaction is racy. Return ResponseStart when null
  return mResponseEnd.IsNull() ? ResponseStartHighRes()
                               : TimeStampToDOMHighRes(mResponseEnd);
}

DOMTimeMilliSec
nsPerformanceTiming::ResponseEnd()
{
  return static_cast<int64_t>(ResponseEndHighRes());
}

bool
nsPerformanceTiming::IsInitialized() const
{
  return mInitialized;
}

JSObject*
nsPerformanceTiming::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceTimingBinding::Wrap(cx, this, aGivenProto);
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
nsPerformanceNavigation::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceNavigationBinding::Wrap(cx, this, aGivenProto);
}


NS_IMPL_CYCLE_COLLECTION_CLASS(nsPerformance)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsPerformance, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow, mTiming,
                                mNavigation, mEntries,
                                mParentPerformance)
  tmp->mMozMemory = nullptr;
  mozilla::DropJSObjects(this);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsPerformance, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow, mTiming,
                                    mNavigation, mEntries,
                                    mParentPerformance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsPerformance, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMozMemory)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

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
  mozilla::DropJSObjects(this);
}

// QueryInterface implementation for nsPerformance
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPerformance)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

void
nsPerformance::GetMozMemory(JSContext *aCx, JS::MutableHandle<JSObject*> aObj)
{
  if (!mMozMemory) {
    mMozMemory = js::gc::NewMemoryInfoObject(aCx);
    if (mMozMemory) {
      mozilla::HoldJSObjects(this);
    }
  }

  aObj.set(mMozMemory);
}

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
  return GetDOMTiming()->TimeStampToDOMHighRes(TimeStamp::Now());
}

JSObject*
nsPerformance::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceBinding::Wrap(cx, this, aGivenProto);
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
                                const Optional<nsAString>& entryType,
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
nsPerformance::ClearEntries(const Optional<nsAString>& aEntryName,
                            const nsAString& aEntryType)
{
  for (uint32_t i = 0; i < mEntries.Length();) {
    if ((!aEntryName.WasPassed() ||
         mEntries[i]->GetName().Equals(aEntryName.Value())) &&
        (aEntryType.IsEmpty() ||
         mEntries[i]->GetEntryType().Equals(aEntryType))) {
      mEntries.RemoveElementAt(i);
    } else {
      ++i;
    }
  }
}

void
nsPerformance::ClearResourceTimings()
{
  MOZ_ASSERT(NS_IsMainThread());
  ClearEntries(Optional<nsAString>(),
               NS_LITERAL_STRING("resource"));
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
    nsRefPtr<PerformanceResourceTiming> performanceEntry =
      new PerformanceResourceTiming(performanceTiming, this, entryName);

    // If the initiator type had no valid value, then set it to the default
    // ("other") value.
    if (initiatorType.IsEmpty()) {
      initiatorType = NS_LITERAL_STRING("other");
    }
    performanceEntry->SetInitiatorType(initiatorType);
    InsertPerformanceEntry(performanceEntry, false);
  }
}

bool
nsPerformance::PerformanceEntryComparator::Equals(
    const PerformanceEntry* aElem1,
    const PerformanceEntry* aElem2) const
{
  MOZ_ASSERT(aElem1 && aElem2,
             "Trying to compare null performance entries");
  return aElem1->StartTime() == aElem2->StartTime();
}

bool
nsPerformance::PerformanceEntryComparator::LessThan(
    const PerformanceEntry* aElem1,
    const PerformanceEntry* aElem2) const
{
  MOZ_ASSERT(aElem1 && aElem2,
             "Trying to compare null performance entries");
  return aElem1->StartTime() < aElem2->StartTime();
}

void
nsPerformance::InsertPerformanceEntry(PerformanceEntry* aEntry,
                                      bool aShouldPrint)
{
  MOZ_ASSERT(aEntry);
  MOZ_ASSERT(mEntries.Length() < mPrimaryBufferSize);
  if (mEntries.Length() == mPrimaryBufferSize) {
    NS_WARNING("Performance Entry buffer size maximum reached!");
    return;
  }
  if (aShouldPrint && nsContentUtils::IsUserTimingLoggingEnabled()) {
    nsAutoCString uri;
    nsresult rv = mWindow->GetDocumentURI()->GetHost(uri);
    if(NS_FAILED(rv)) {
      // If we have no URI, just put in "none".
      uri.AssignLiteral("none");
    }
    PERFLOG("Performance Entry: %s|%s|%s|%f|%f|%" PRIu64 "\n",
            uri.get(),
            NS_ConvertUTF16toUTF8(aEntry->GetEntryType()).get(),
            NS_ConvertUTF16toUTF8(aEntry->GetName()).get(),
            aEntry->StartTime(),
            aEntry->Duration(),
            static_cast<uint64_t>(PR_Now() / PR_USEC_PER_MSEC));
  }
  mEntries.InsertElementSorted(aEntry,
                               PerformanceEntryComparator());
  if (mEntries.Length() == mPrimaryBufferSize) {
    // call onresourcetimingbufferfull
    DispatchBufferFullEvent();
  }
}

void
nsPerformance::Mark(const nsAString& aName, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Don't add the entry if the buffer is full
  if (mEntries.Length() >= mPrimaryBufferSize) {
    NS_WARNING("Performance Entry buffer size maximum reached!");
    return;
  }
  if (IsPerformanceTimingAttribute(aName)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }
  nsRefPtr<PerformanceMark> performanceMark =
    new PerformanceMark(this, aName);
  InsertPerformanceEntry(performanceMark, true);
}

void
nsPerformance::ClearMarks(const Optional<nsAString>& aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  ClearEntries(aName, NS_LITERAL_STRING("mark"));
}

DOMHighResTimeStamp
nsPerformance::ResolveTimestampFromName(const nsAString& aName,
                                        ErrorResult& aRv)
{
  nsAutoTArray<nsRefPtr<PerformanceEntry>, 1> arr;
  DOMHighResTimeStamp ts;
  Optional<nsAString> typeParam;
  nsAutoString str;
  str.AssignLiteral("mark");
  typeParam = &str;
  GetEntriesByName(aName, typeParam, arr);
  if (!arr.IsEmpty()) {
    return arr.LastElement()->StartTime();
  }
  if (!IsPerformanceTimingAttribute(aName)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return 0;
  }
  ts = GetPerformanceTimingFromString(aName);
  if (!ts) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return 0;
  }
  return ConvertDOMMilliSecToHighRes(ts);
}

void
nsPerformance::Measure(const nsAString& aName,
                       const Optional<nsAString>& aStartMark,
                       const Optional<nsAString>& aEndMark,
                       ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Don't add the entry if the buffer is full
  if (mEntries.Length() >= mPrimaryBufferSize) {
    NS_WARNING("Performance Entry buffer size maximum reached!");
    return;
  }
  DOMHighResTimeStamp startTime;
  DOMHighResTimeStamp endTime;

  if (IsPerformanceTimingAttribute(aName)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  if (aStartMark.WasPassed()) {
    startTime = ResolveTimestampFromName(aStartMark.Value(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  } else {
    // Navigation start is used in this case, but since DOMHighResTimeStamp is
    // in relation to navigation start, this will be zero if a name is not
    // passed.
    startTime = 0;
  }
  if (aEndMark.WasPassed()) {
    endTime = ResolveTimestampFromName(aEndMark.Value(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  } else {
    endTime = Now();
  }
  nsRefPtr<PerformanceMeasure> performanceMeasure =
    new PerformanceMeasure(this, aName, startTime, endTime);
  InsertPerformanceEntry(performanceMeasure, true);
}

void
nsPerformance::ClearMeasures(const Optional<nsAString>& aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  ClearEntries(aName, NS_LITERAL_STRING("measure"));
}

DOMHighResTimeStamp
nsPerformance::ConvertDOMMilliSecToHighRes(DOMTimeMilliSec aTime) {
  // If the time we're trying to convert is equal to zero, it hasn't been set
  // yet so just return 0.
  if (aTime == 0) {
    return 0;
  }
  return aTime - GetDOMTiming()->GetNavigationStart();
}

// To be removed once bug 1124165 lands
bool
nsPerformance::IsPerformanceTimingAttribute(const nsAString& aName)
{
  // Note that toJSON is added to this list due to bug 1047848
  static const char* attributes[] =
    {"navigationStart", "unloadEventStart", "unloadEventEnd", "redirectStart",
     "redirectEnd", "fetchStart", "domainLookupStart", "domainLookupEnd",
     "connectStart", "connectEnd", "requestStart", "responseStart",
     "responseEnd", "domLoading", "domInteractive", "domContentLoadedEventStart",
     "domContentLoadedEventEnd", "domComplete", "loadEventStart",
     "loadEventEnd", nullptr};

  for (uint32_t i = 0; attributes[i]; ++i) {
    if (aName.EqualsASCII(attributes[i])) {
      return true;
    }
  }
  return false;
}

DOMTimeMilliSec
nsPerformance::GetPerformanceTimingFromString(const nsAString& aProperty)
{
  if (!IsPerformanceTimingAttribute(aProperty)) {
    return 0;
  }
  if (aProperty.EqualsLiteral("navigationStart")) {
    // DOMHighResTimeStamp is in relation to navigationStart, so this will be
    // zero.
    return GetDOMTiming()->GetNavigationStart();
  }
  if (aProperty.EqualsLiteral("unloadEventStart")) {
    return GetDOMTiming()->GetUnloadEventStart();
  }
  if (aProperty.EqualsLiteral("unloadEventEnd")) {
    return GetDOMTiming()->GetUnloadEventEnd();
  }
  if (aProperty.EqualsLiteral("redirectStart")) {
    return Timing()->RedirectStart();
  }
  if (aProperty.EqualsLiteral("redirectEnd")) {
    return Timing()->RedirectEnd();
  }
  if (aProperty.EqualsLiteral("fetchStart")) {
    return Timing()->FetchStart();
  }
  if (aProperty.EqualsLiteral("domainLookupStart")) {
    return Timing()->DomainLookupStart();
  }
  if (aProperty.EqualsLiteral("domainLookupEnd")) {
    return Timing()->DomainLookupEnd();
  }
  if (aProperty.EqualsLiteral("connectStart")) {
    return Timing()->ConnectStart();
  }
  if (aProperty.EqualsLiteral("connectEnd")) {
    return Timing()->ConnectEnd();
  }
  if (aProperty.EqualsLiteral("requestStart")) {
    return Timing()->RequestStart();
  }
  if (aProperty.EqualsLiteral("responseStart")) {
    return Timing()->ResponseStart();
  }
  if (aProperty.EqualsLiteral("responseEnd")) {
    return Timing()->ResponseEnd();
  }
  if (aProperty.EqualsLiteral("domLoading")) {
    return GetDOMTiming()->GetDomLoading();
  }
  if (aProperty.EqualsLiteral("domInteractive")) {
    return GetDOMTiming()->GetDomInteractive();
  }
  if (aProperty.EqualsLiteral("domContentLoadedEventStart")) {
    return GetDOMTiming()->GetDomContentLoadedEventStart();
  }
  if (aProperty.EqualsLiteral("domContentLoadedEventEnd")) {
    return GetDOMTiming()->GetDomContentLoadedEventEnd();
  }
  if (aProperty.EqualsLiteral("domComplete")) {
    return GetDOMTiming()->GetDomComplete();
  }
  if (aProperty.EqualsLiteral("loadEventStart")) {
    return GetDOMTiming()->GetLoadEventStart();
  }
  if (aProperty.EqualsLiteral("loadEventEnd"))  {
    return GetDOMTiming()->GetLoadEventEnd();
  }
  MOZ_CRASH("IsPerformanceTimingAttribute and GetPerformanceTimingFromString are out of sync");
  return 0;
}

