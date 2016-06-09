/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "PerformanceObserver.h"
#include "PerformanceResourceTiming.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceEntryEvent.h"
#include "mozilla/dom/PerformanceNavigationBinding.h"
#include "mozilla/dom/PerformanceObserverBinding.h"
#include "mozilla/Preferences.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/TimeStamp.h"
#include "js/HeapAPI.h"
#include "GeckoProfiler.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

#ifdef MOZ_WIDGET_GONK
#define PERFLOG(msg, ...)  __android_log_print(ANDROID_LOG_INFO, "PerformanceTiming", msg, ##__VA_ARGS__)
#else
#define PERFLOG(msg, ...) printf_stderr(msg, ##__VA_ARGS__)
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::workers;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PerformanceNavigation, mPerformance)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(PerformanceNavigation, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(PerformanceNavigation, Release)

PerformanceNavigation::PerformanceNavigation(nsPerformance* aPerformance)
  : mPerformance(aPerformance)
{
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");
}

PerformanceNavigation::~PerformanceNavigation()
{
}

JSObject*
PerformanceNavigation::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceNavigationBinding::Wrap(cx, this, aGivenProto);
}

uint16_t
PerformanceNavigation::RedirectCount() const
{
  return GetPerformanceTiming()->GetRedirectCount();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsPerformance)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsPerformance, PerformanceBase)
NS_IMPL_CYCLE_COLLECTION_UNLINK(mTiming,
                                mNavigation,
                                mParentPerformance)
  tmp->mMozMemory = nullptr;
  mozilla::DropJSObjects(this);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsPerformance, PerformanceBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTiming,
                                    mNavigation,
                                    mParentPerformance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsPerformance, PerformanceBase)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMozMemory)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(nsPerformance, PerformanceBase)
NS_IMPL_RELEASE_INHERITED(nsPerformance, PerformanceBase)

nsPerformance::nsPerformance(nsPIDOMWindowInner* aWindow,
                             nsDOMNavigationTiming* aDOMTiming,
                             nsITimedChannel* aChannel,
                             nsPerformance* aParentPerformance)
  : PerformanceBase(aWindow),
    mDOMTiming(aDOMTiming),
    mChannel(aChannel),
    mParentPerformance(aParentPerformance)
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

PerformanceTiming*
nsPerformance::Timing()
{
  if (!mTiming) {
    // For navigation timing, the third argument (an nsIHtttpChannel) is null
    // since the cross-domain redirect were already checked.
    // The last argument (zero time) for performance.timing is the navigation
    // start value.
    mTiming = new PerformanceTiming(this, mChannel, nullptr,
        mDOMTiming->GetNavigationStart());
  }
  return mTiming;
}

void
nsPerformance::DispatchBufferFullEvent()
{
  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  // it bubbles, and it isn't cancelable
  event->InitEvent(NS_LITERAL_STRING("resourcetimingbufferfull"), true, false);
  event->SetTrusted(true);
  DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

PerformanceNavigation*
nsPerformance::Navigation()
{
  if (!mNavigation) {
    mNavigation = new PerformanceNavigation(this);
  }
  return mNavigation;
}

DOMHighResTimeStamp
nsPerformance::Now() const
{
  return RoundTime(GetDOMTiming()->TimeStampToDOMHighRes(TimeStamp::Now()));
}

JSObject*
nsPerformance::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceBinding::Wrap(cx, this, aGivenProto);
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
  if (IsResourceEntryLimitReached()) {
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
    RefPtr<PerformanceTiming> performanceTiming =
        new PerformanceTiming(this, timedChannel, channel,
            0);

    // The PerformanceResourceTiming object will use the PerformanceTiming
    // object to get all the required timings.
    RefPtr<PerformanceResourceTiming> performanceEntry =
      new PerformanceResourceTiming(performanceTiming, this, entryName);

    nsAutoCString protocol;
    channel->GetProtocolVersion(protocol);
    performanceEntry->SetNextHopProtocol(NS_ConvertUTF8toUTF16(protocol));

    uint64_t encodedBodySize = 0;
    channel->GetEncodedBodySize(&encodedBodySize);
    performanceEntry->SetEncodedBodySize(encodedBodySize);

    uint64_t transferSize = 0;
    channel->GetTransferSize(&transferSize);
    performanceEntry->SetTransferSize(transferSize);

    uint64_t decodedBodySize = 0;
    channel->GetDecodedBodySize(&decodedBodySize);
    if (decodedBodySize == 0) {
      decodedBodySize = encodedBodySize;
    }
    performanceEntry->SetDecodedBodySize(decodedBodySize);

    // If the initiator type had no valid value, then set it to the default
    // ("other") value.
    if (initiatorType.IsEmpty()) {
      initiatorType = NS_LITERAL_STRING("other");
    }
    performanceEntry->SetInitiatorType(initiatorType);
    InsertResourceEntry(performanceEntry);
  }
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

DOMHighResTimeStamp
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

namespace {

// Helper classes
class MOZ_STACK_CLASS PerformanceEntryComparator final
{
public:
  bool Equals(const PerformanceEntry* aElem1,
              const PerformanceEntry* aElem2) const
  {
    MOZ_ASSERT(aElem1 && aElem2,
               "Trying to compare null performance entries");
    return aElem1->StartTime() == aElem2->StartTime();
  }

  bool LessThan(const PerformanceEntry* aElem1,
                const PerformanceEntry* aElem2) const
  {
    MOZ_ASSERT(aElem1 && aElem2,
               "Trying to compare null performance entries");
    return aElem1->StartTime() < aElem2->StartTime();
  }
};

class PrefEnabledRunnable final : public WorkerCheckAPIExposureOnMainThreadRunnable
{
public:
  PrefEnabledRunnable(WorkerPrivate* aWorkerPrivate,
                      const nsCString& aPrefName)
    : WorkerCheckAPIExposureOnMainThreadRunnable(aWorkerPrivate)
    , mEnabled(false)
    , mPrefName(aPrefName)
  { }

  bool MainThreadRun() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mEnabled = Preferences::GetBool(mPrefName.get(), false);
    return true;
  }

  bool IsEnabled() const
  {
    return mEnabled;
  }

private:
  bool mEnabled;
  nsCString mPrefName;
};

} // namespace

/* static */ bool
nsPerformance::IsEnabled(JSContext* aCx, JSObject* aGlobal)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.enable_user_timing", false);
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<PrefEnabledRunnable> runnable =
    new PrefEnabledRunnable(workerPrivate,
                            NS_LITERAL_CSTRING("dom.enable_user_timing"));
  return runnable->Dispatch() && runnable->IsEnabled();
}

/* static */ bool
nsPerformance::IsObserverEnabled(JSContext* aCx, JSObject* aGlobal)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.enable_performance_observer", false);
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<PrefEnabledRunnable> runnable =
    new PrefEnabledRunnable(workerPrivate,
                            NS_LITERAL_CSTRING("dom.enable_performance_observer"));

  return runnable->Dispatch() && runnable->IsEnabled();
}

void
nsPerformance::InsertUserEntry(PerformanceEntry* aEntry)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString uri;
  uint64_t markCreationEpoch = 0;

  if (nsContentUtils::IsUserTimingLoggingEnabled() ||
      nsContentUtils::SendPerformanceTimingNotifications()) {
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
    if (owner && owner->GetDocumentURI()) {
      rv = owner->GetDocumentURI()->GetHost(uri);
    }

    if(NS_FAILED(rv)) {
      // If we have no URI, just put in "none".
      uri.AssignLiteral("none");
    }
    markCreationEpoch = static_cast<uint64_t>(PR_Now() / PR_USEC_PER_MSEC);

    if (nsContentUtils::IsUserTimingLoggingEnabled()) {
      PerformanceBase::LogEntry(aEntry, uri);
    }
  }

  if (nsContentUtils::SendPerformanceTimingNotifications()) {
    TimingNotification(aEntry, uri, markCreationEpoch);
  }

  PerformanceBase::InsertUserEntry(aEntry);
}

TimeStamp
nsPerformance::CreationTimeStamp() const
{
  return GetDOMTiming()->GetNavigationStartTimeStamp();
}

DOMHighResTimeStamp
nsPerformance::CreationTime() const
{
  return GetDOMTiming()->GetNavigationStart();
}

// PerformanceBase

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PerformanceBase)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(PerformanceBase,
                                   DOMEventTargetHelper,
                                   mUserEntries,
                                   mResourceEntries);

NS_IMPL_ADDREF_INHERITED(PerformanceBase, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PerformanceBase, DOMEventTargetHelper)

PerformanceBase::PerformanceBase()
  : mResourceTimingBufferSize(kDefaultResourceTimingBufferSize)
  , mPendingNotificationObserversTask(false)
{
  MOZ_ASSERT(!NS_IsMainThread());
}

PerformanceBase::PerformanceBase(nsPIDOMWindowInner* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mResourceTimingBufferSize(kDefaultResourceTimingBufferSize)
  , mPendingNotificationObserversTask(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

PerformanceBase::~PerformanceBase()
{}

void
PerformanceBase::GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  aRetval = mResourceEntries;
  aRetval.AppendElements(mUserEntries);
  aRetval.Sort(PerformanceEntryComparator());
}

void
PerformanceBase::GetEntriesByType(const nsAString& aEntryType,
                                  nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  if (aEntryType.EqualsLiteral("resource")) {
    aRetval = mResourceEntries;
    return;
  }

  aRetval.Clear();

  if (aEntryType.EqualsLiteral("mark") ||
      aEntryType.EqualsLiteral("measure")) {
    for (PerformanceEntry* entry : mUserEntries) {
      if (entry->GetEntryType().Equals(aEntryType)) {
        aRetval.AppendElement(entry);
      }
    }
  }
}

void
PerformanceBase::GetEntriesByName(const nsAString& aName,
                                  const Optional<nsAString>& aEntryType,
                                  nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  aRetval.Clear();

  for (PerformanceEntry* entry : mResourceEntries) {
    if (entry->GetName().Equals(aName) &&
        (!aEntryType.WasPassed() ||
         entry->GetEntryType().Equals(aEntryType.Value()))) {
      aRetval.AppendElement(entry);
    }
  }

  for (PerformanceEntry* entry : mUserEntries) {
    if (entry->GetName().Equals(aName) &&
        (!aEntryType.WasPassed() ||
         entry->GetEntryType().Equals(aEntryType.Value()))) {
      aRetval.AppendElement(entry);
    }
  }

  aRetval.Sort(PerformanceEntryComparator());
}

void
PerformanceBase::ClearUserEntries(const Optional<nsAString>& aEntryName,
                                  const nsAString& aEntryType)
{
  for (uint32_t i = 0; i < mUserEntries.Length();) {
    if ((!aEntryName.WasPassed() ||
         mUserEntries[i]->GetName().Equals(aEntryName.Value())) &&
        (aEntryType.IsEmpty() ||
         mUserEntries[i]->GetEntryType().Equals(aEntryType))) {
      mUserEntries.RemoveElementAt(i);
    } else {
      ++i;
    }
  }
}

void
PerformanceBase::ClearResourceTimings()
{
  MOZ_ASSERT(NS_IsMainThread());
  mResourceEntries.Clear();
}

DOMHighResTimeStamp
PerformanceBase::RoundTime(double aTime) const
{
  // Round down to the nearest 5us, because if the timer is too accurate people
  // can do nasty timing attacks with it.  See similar code in the worker
  // Performance implementation.
  const double maxResolutionMs = 0.005;
  return floor(aTime / maxResolutionMs) * maxResolutionMs;
}


void
PerformanceBase::Mark(const nsAString& aName, ErrorResult& aRv)
{
  // Don't add the entry if the buffer is full. XXX should be removed by bug 1159003.
  if (mUserEntries.Length() >= mResourceTimingBufferSize) {
    return;
  }

  if (IsPerformanceTimingAttribute(aName)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  RefPtr<PerformanceMark> performanceMark =
    new PerformanceMark(GetAsISupports(), aName, Now());
  InsertUserEntry(performanceMark);

  if (profiler_is_active()) {
    PROFILER_MARKER(NS_ConvertUTF16toUTF8(aName).get());
  }
}

void
PerformanceBase::ClearMarks(const Optional<nsAString>& aName)
{
  ClearUserEntries(aName, NS_LITERAL_STRING("mark"));
}

DOMHighResTimeStamp
PerformanceBase::ResolveTimestampFromName(const nsAString& aName,
                                          ErrorResult& aRv)
{
  AutoTArray<RefPtr<PerformanceEntry>, 1> arr;
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

  return ts - CreationTime();
}

void
PerformanceBase::Measure(const nsAString& aName,
                         const Optional<nsAString>& aStartMark,
                         const Optional<nsAString>& aEndMark,
                         ErrorResult& aRv)
{
  // Don't add the entry if the buffer is full. XXX should be removed by bug
  // 1159003.
  if (mUserEntries.Length() >= mResourceTimingBufferSize) {
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

  RefPtr<PerformanceMeasure> performanceMeasure =
    new PerformanceMeasure(GetAsISupports(), aName, startTime, endTime);
  InsertUserEntry(performanceMeasure);
}

void
PerformanceBase::ClearMeasures(const Optional<nsAString>& aName)
{
  ClearUserEntries(aName, NS_LITERAL_STRING("measure"));
}

void
PerformanceBase::LogEntry(PerformanceEntry* aEntry, const nsACString& aOwner) const
{
  PERFLOG("Performance Entry: %s|%s|%s|%f|%f|%" PRIu64 "\n",
          aOwner.BeginReading(),
          NS_ConvertUTF16toUTF8(aEntry->GetEntryType()).get(),
          NS_ConvertUTF16toUTF8(aEntry->GetName()).get(),
          aEntry->StartTime(),
          aEntry->Duration(),
          static_cast<uint64_t>(PR_Now() / PR_USEC_PER_MSEC));
}

void
PerformanceBase::TimingNotification(PerformanceEntry* aEntry, const nsACString& aOwner, uint64_t aEpoch)
{
  PerformanceEntryEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mName = aEntry->GetName();
  init.mEntryType = aEntry->GetEntryType();
  init.mStartTime = aEntry->StartTime();
  init.mDuration = aEntry->Duration();
  init.mEpoch = aEpoch;
  init.mOrigin = NS_ConvertUTF8toUTF16(aOwner.BeginReading());

  RefPtr<PerformanceEntryEvent> perfEntryEvent =
    PerformanceEntryEvent::Constructor(this, NS_LITERAL_STRING("performanceentry"), init);

  nsCOMPtr<EventTarget> et = do_QueryInterface(GetOwner());
  if (et) {
    bool dummy = false;
    et->DispatchEvent(perfEntryEvent, &dummy);
  }
}

void
PerformanceBase::InsertUserEntry(PerformanceEntry* aEntry)
{
  mUserEntries.InsertElementSorted(aEntry,
                                   PerformanceEntryComparator());

  QueueEntry(aEntry);
}

void
PerformanceBase::SetResourceTimingBufferSize(uint64_t aMaxSize)
{
  mResourceTimingBufferSize = aMaxSize;
}

void
PerformanceBase::InsertResourceEntry(PerformanceEntry* aEntry)
{
  MOZ_ASSERT(aEntry);
  MOZ_ASSERT(mResourceEntries.Length() < mResourceTimingBufferSize);
  if (mResourceEntries.Length() >= mResourceTimingBufferSize) {
    return;
  }

  mResourceEntries.InsertElementSorted(aEntry,
                                       PerformanceEntryComparator());
  if (mResourceEntries.Length() == mResourceTimingBufferSize) {
    // call onresourcetimingbufferfull
    DispatchBufferFullEvent();
  }
  QueueEntry(aEntry);
}

void
PerformanceBase::AddObserver(PerformanceObserver* aObserver)
{
  mObservers.AppendElementUnlessExists(aObserver);
}

void
PerformanceBase::RemoveObserver(PerformanceObserver* aObserver)
{
  mObservers.RemoveElement(aObserver);
}

void
PerformanceBase::NotifyObservers()
{
  mPendingNotificationObserversTask = false;
  NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers,
                                           PerformanceObserver,
                                           Notify, ());
}

void
PerformanceBase::CancelNotificationObservers()
{
  mPendingNotificationObserversTask = false;
}

class NotifyObserversTask final : public CancelableRunnable
{
public:
  explicit NotifyObserversTask(PerformanceBase* aPerformance)
    : mPerformance(aPerformance)
  {
    MOZ_ASSERT(mPerformance);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mPerformance);
    mPerformance->NotifyObservers();
    return NS_OK;
  }

  nsresult Cancel() override
  {
    mPerformance->CancelNotificationObservers();
    mPerformance = nullptr;
    return NS_OK;
  }

private:
  ~NotifyObserversTask()
  {
  }

  RefPtr<PerformanceBase> mPerformance;
};

void
PerformanceBase::RunNotificationObserversTask()
{
  mPendingNotificationObserversTask = true;
  nsCOMPtr<nsIRunnable> task = new NotifyObserversTask(this);
  nsresult rv = NS_DispatchToCurrentThread(task);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPendingNotificationObserversTask = false;
  }
}

void
PerformanceBase::QueueEntry(PerformanceEntry* aEntry)
{
  if (mObservers.IsEmpty()) {
    return;
  }
  NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers,
                                           PerformanceObserver,
                                           QueueEntry, (aEntry));

  if (!mPendingNotificationObserversTask) {
    RunNotificationObserversTask();
  }
}
