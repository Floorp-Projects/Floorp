/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Performance.h"

#include "GeckoProfiler.h"
#include "nsRFPService.h"
#include "ProfilerMarkerPayload.h"
#include "PerformanceEntry.h"
#include "PerformanceMainThread.h"
#include "PerformanceMark.h"
#include "PerformanceMeasure.h"
#include "PerformanceObserver.h"
#include "PerformanceResourceTiming.h"
#include "PerformanceService.h"
#include "PerformanceWorker.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceEntryEvent.h"
#include "mozilla/dom/PerformanceNavigationBinding.h"
#include "mozilla/dom/PerformanceObserverBinding.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Preferences.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

#define PERFLOG(msg, ...) printf_stderr(msg, ##__VA_ARGS__)

namespace mozilla {
namespace dom {

using namespace workers;

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

class PrefEnabledRunnable final
  : public WorkerCheckAPIExposureOnMainThreadRunnable
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

} // anonymous namespace

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Performance)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(Performance,
                                   DOMEventTargetHelper,
                                   mUserEntries,
                                   mResourceEntries);

NS_IMPL_ADDREF_INHERITED(Performance, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Performance, DOMEventTargetHelper)

/* static */ already_AddRefed<Performance>
Performance::CreateForMainThread(nsPIDOMWindowInner* aWindow,
                                 nsDOMNavigationTiming* aDOMTiming,
                                 nsITimedChannel* aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Performance> performance =
    PerformanceMainThread::Create(aWindow, aDOMTiming, aChannel);
  return performance.forget();
}

/* static */ already_AddRefed<Performance>
Performance::CreateForWorker(workers::WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<Performance> performance = new PerformanceWorker(aWorkerPrivate);
  return performance.forget();
}

Performance::Performance()
  : mResourceTimingBufferSize(kDefaultResourceTimingBufferSize)
  , mPendingNotificationObserversTask(false)
{
  MOZ_ASSERT(!NS_IsMainThread());
}

Performance::Performance(nsPIDOMWindowInner* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mResourceTimingBufferSize(kDefaultResourceTimingBufferSize)
  , mPendingNotificationObserversTask(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

Performance::~Performance()
{}

DOMHighResTimeStamp
Performance::Now() const
{
  TimeDuration duration = TimeStamp::Now() - CreationTimeStamp();
  return RoundTime(duration.ToMilliseconds());
}

DOMHighResTimeStamp
Performance::TimeOrigin()
{
  if (!mPerformanceService) {
    mPerformanceService = PerformanceService::GetOrCreate();
  }

  MOZ_ASSERT(mPerformanceService);
  return mPerformanceService->TimeOrigin(CreationTimeStamp());
}

JSObject*
Performance::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceBinding::Wrap(aCx, this, aGivenProto);
}

void
Performance::GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    aRetval.Clear();
    return;
  }

  aRetval = mResourceEntries;
  aRetval.AppendElements(mUserEntries);
  aRetval.Sort(PerformanceEntryComparator());
}

void
Performance::GetEntriesByType(const nsAString& aEntryType,
                              nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    aRetval.Clear();
    return;
  }

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
Performance::GetEntriesByName(const nsAString& aName,
                              const Optional<nsAString>& aEntryType,
                              nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  aRetval.Clear();

  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return;
  }

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
Performance::ClearUserEntries(const Optional<nsAString>& aEntryName,
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
Performance::ClearResourceTimings()
{
  MOZ_ASSERT(NS_IsMainThread());
  mResourceEntries.Clear();
}

DOMHighResTimeStamp
Performance::RoundTime(double aTime) const
{
  // Round down to the nearest 5us, because if the timer is too accurate people
  // can do nasty timing attacks with it.  See similar code in the worker
  // Performance implementation.
  const double maxResolutionMs = 0.005;
  return nsRFPService::ReduceTimePrecisionAsMSecs(
    floor(aTime / maxResolutionMs) * maxResolutionMs);
}


void
Performance::Mark(const nsAString& aName, ErrorResult& aRv)
{
  // We add nothing when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return;
  }

  if (IsPerformanceTimingAttribute(aName)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  RefPtr<PerformanceMark> performanceMark =
    new PerformanceMark(GetParentObject(), aName, Now());
  InsertUserEntry(performanceMark);

  if (profiler_is_active()) {
    profiler_add_marker(
      "UserTiming",
      MakeUnique<UserTimingMarkerPayload>(aName, TimeStamp::Now()));
  }
}

void
Performance::ClearMarks(const Optional<nsAString>& aName)
{
  ClearUserEntries(aName, NS_LITERAL_STRING("mark"));
}

DOMHighResTimeStamp
Performance::ResolveTimestampFromName(const nsAString& aName,
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
Performance::Measure(const nsAString& aName,
                     const Optional<nsAString>& aStartMark,
                     const Optional<nsAString>& aEndMark,
                     ErrorResult& aRv)
{
  // We add nothing when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
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
    new PerformanceMeasure(GetParentObject(), aName, startTime, endTime);
  InsertUserEntry(performanceMeasure);

  if (profiler_is_active()) {
    TimeStamp startTimeStamp = CreationTimeStamp() +
                               TimeDuration::FromMilliseconds(startTime);
    TimeStamp endTimeStamp = CreationTimeStamp() +
                             TimeDuration::FromMilliseconds(endTime);
    profiler_add_marker(
      "UserTiming",
      MakeUnique<UserTimingMarkerPayload>(aName, startTimeStamp, endTimeStamp));
  }
}

void
Performance::ClearMeasures(const Optional<nsAString>& aName)
{
  ClearUserEntries(aName, NS_LITERAL_STRING("measure"));
}

void
Performance::LogEntry(PerformanceEntry* aEntry, const nsACString& aOwner) const
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
Performance::TimingNotification(PerformanceEntry* aEntry,
                                const nsACString& aOwner, uint64_t aEpoch)
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
Performance::InsertUserEntry(PerformanceEntry* aEntry)
{
  mUserEntries.InsertElementSorted(aEntry,
                                   PerformanceEntryComparator());

  QueueEntry(aEntry);
}

void
Performance::SetResourceTimingBufferSize(uint64_t aMaxSize)
{
  mResourceTimingBufferSize = aMaxSize;
}

void
Performance::InsertResourceEntry(PerformanceEntry* aEntry)
{
  MOZ_ASSERT(aEntry);
  MOZ_ASSERT(mResourceEntries.Length() < mResourceTimingBufferSize);

  // We won't add an entry when 'privacy.resistFingerprint' is true.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return;
  }

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
Performance::AddObserver(PerformanceObserver* aObserver)
{
  mObservers.AppendElementUnlessExists(aObserver);
}

void
Performance::RemoveObserver(PerformanceObserver* aObserver)
{
  mObservers.RemoveElement(aObserver);
}

void
Performance::NotifyObservers()
{
  mPendingNotificationObserversTask = false;
  NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers,
                                           PerformanceObserver,
                                           Notify, ());
}

void
Performance::CancelNotificationObservers()
{
  mPendingNotificationObserversTask = false;
}

class NotifyObserversTask final : public CancelableRunnable
{
public:
  explicit NotifyObserversTask(Performance* aPerformance)
    : CancelableRunnable("dom::NotifyObserversTask")
    , mPerformance(aPerformance)
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

  RefPtr<Performance> mPerformance;
};

void
Performance::RunNotificationObserversTask()
{
  mPendingNotificationObserversTask = true;
  nsCOMPtr<nsIRunnable> task = new NotifyObserversTask(this);
  nsresult rv = NS_DispatchToCurrentThread(task);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPendingNotificationObserversTask = false;
  }
}

void
Performance::QueueEntry(PerformanceEntry* aEntry)
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

/* static */ bool
Performance::IsObserverEnabled(JSContext* aCx, JSObject* aGlobal)
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
Performance::MemoryPressure()
{
  mUserEntries.Clear();
}

} // dom namespace
} // mozilla namespace
