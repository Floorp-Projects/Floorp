/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Performance.h"

#include "GeckoProfiler.h"
#include "nsRFPService.h"
#include "PerformanceEntry.h"
#include "PerformanceMainThread.h"
#include "PerformanceMark.h"
#include "PerformanceMeasure.h"
#include "PerformanceObserver.h"
#include "PerformanceResourceTiming.h"
#include "PerformanceService.h"
#include "PerformanceWorker.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceEntryEvent.h"
#include "mozilla/dom/PerformanceNavigationBinding.h"
#include "mozilla/dom/PerformanceObserverBinding.h"
#include "mozilla/dom/PerformanceNavigationTiming.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"

#define PERFLOG(msg, ...) printf_stderr(msg, ##__VA_ARGS__)

namespace mozilla::dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Performance)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(Performance, DOMEventTargetHelper,
                                   mUserEntries, mResourceEntries,
                                   mSecondaryResourceEntries, mObservers);

NS_IMPL_ADDREF_INHERITED(Performance, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Performance, DOMEventTargetHelper)

/* static */
already_AddRefed<Performance> Performance::CreateForMainThread(
    nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
    nsDOMNavigationTiming* aDOMTiming, nsITimedChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aWindow->AsGlobal());
  RefPtr<Performance> performance = new PerformanceMainThread(
      aWindow, aDOMTiming, aChannel, aPrincipal->IsSystemPrincipal());
  return performance.forget();
}

/* static */
already_AddRefed<Performance> Performance::CreateForWorker(
    WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<Performance> performance = new PerformanceWorker(aWorkerPrivate);
  return performance.forget();
}

Performance::Performance(nsIGlobalObject* aGlobal, bool aSystemPrincipal)
    : DOMEventTargetHelper(aGlobal),
      mResourceTimingBufferSize(kDefaultResourceTimingBufferSize),
      mPendingNotificationObserversTask(false),
      mPendingResourceTimingBufferFullEvent(false),
      mSystemPrincipal(aSystemPrincipal) {
  MOZ_ASSERT(!NS_IsMainThread());
}

Performance::Performance(nsPIDOMWindowInner* aWindow, bool aSystemPrincipal)
    : DOMEventTargetHelper(aWindow),
      mResourceTimingBufferSize(kDefaultResourceTimingBufferSize),
      mPendingNotificationObserversTask(false),
      mPendingResourceTimingBufferFullEvent(false),
      mSystemPrincipal(aSystemPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
}

Performance::~Performance() = default;

DOMHighResTimeStamp Performance::TimeStampToDOMHighResForRendering(
    TimeStamp aTimeStamp) const {
  DOMHighResTimeStamp stamp = GetDOMTiming()->TimeStampToDOMHighRes(aTimeStamp);
  if (!IsSystemPrincipal()) {
    // 0 is an inappropriate mixin for this this area; however CSS Animations
    // needs to have it's Time Reduction Logic refactored, so it's currently
    // only clamping for RFP mode. RFP mode gives a much lower time precision,
    // so we accept the security leak here for now.
    return nsRFPService::ReduceTimePrecisionAsMSecsRFPOnly(stamp, 0);
  }
  return stamp;
}

DOMHighResTimeStamp Performance::Now() {
  DOMHighResTimeStamp rawTime = NowUnclamped();

  // XXX: Remove this would cause functions in pkcs11f.h to fail.
  // Bug 1628021 will find out the root cause.
  if (mSystemPrincipal) {
    return rawTime;
  }

  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawTime, GetRandomTimelineSeed(), mSystemPrincipal,
      CrossOriginIsolated());
}

DOMHighResTimeStamp Performance::NowUnclamped() const {
  TimeDuration duration = TimeStamp::NowUnfuzzed() - CreationTimeStamp();
  return duration.ToMilliseconds();
}

DOMHighResTimeStamp Performance::TimeOrigin() {
  if (!mPerformanceService) {
    mPerformanceService = PerformanceService::GetOrCreate();
  }

  MOZ_ASSERT(mPerformanceService);
  DOMHighResTimeStamp rawTimeOrigin =
      mPerformanceService->TimeOrigin(CreationTimeStamp());
  // Time Origin is an absolute timestamp, so we supply a 0 context mix-in
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawTimeOrigin, 0, mSystemPrincipal, CrossOriginIsolated());
}

JSObject* Performance::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return Performance_Binding::Wrap(aCx, this, aGivenProto);
}

void Performance::GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    aRetval.Clear();
    return;
  }

  aRetval = mResourceEntries.Clone();
  aRetval.AppendElements(mUserEntries);
  aRetval.Sort(PerformanceEntryComparator());
}

void Performance::GetEntriesByType(
    const nsAString& aEntryType, nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    aRetval.Clear();
    return;
  }

  if (aEntryType.EqualsLiteral("resource")) {
    aRetval = mResourceEntries.Clone();
    return;
  }

  aRetval.Clear();

  if (aEntryType.EqualsLiteral("mark") || aEntryType.EqualsLiteral("measure")) {
    RefPtr<nsAtom> entryType = NS_Atomize(aEntryType);
    for (PerformanceEntry* entry : mUserEntries) {
      if (entry->GetEntryType() == entryType) {
        aRetval.AppendElement(entry);
      }
    }
  }
}

void Performance::GetEntriesByName(
    const nsAString& aName, const Optional<nsAString>& aEntryType,
    nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  aRetval.Clear();

  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return;
  }

  RefPtr<nsAtom> name = NS_Atomize(aName);
  RefPtr<nsAtom> entryType =
      aEntryType.WasPassed() ? NS_Atomize(aEntryType.Value()) : nullptr;

  // ::Measure expects that results from this function are already
  // passed through ReduceTimePrecision. mResourceEntries and mUserEntries
  // are, so the invariant holds.
  for (PerformanceEntry* entry : mResourceEntries) {
    if (entry->GetName() == name &&
        (!entryType || entry->GetEntryType() == entryType)) {
      aRetval.AppendElement(entry);
    }
  }

  for (PerformanceEntry* entry : mUserEntries) {
    if (entry->GetName() == name &&
        (!entryType || entry->GetEntryType() == entryType)) {
      aRetval.AppendElement(entry);
    }
  }

  aRetval.Sort(PerformanceEntryComparator());
}

void Performance::GetEntriesByTypeForObserver(
    const nsAString& aEntryType, nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  GetEntriesByType(aEntryType, aRetval);
}

void Performance::ClearUserEntries(const Optional<nsAString>& aEntryName,
                                   const nsAString& aEntryType) {
  MOZ_ASSERT(!aEntryType.IsEmpty());
  RefPtr<nsAtom> name =
      aEntryName.WasPassed() ? NS_Atomize(aEntryName.Value()) : nullptr;
  RefPtr<nsAtom> entryType = NS_Atomize(aEntryType);
  mUserEntries.RemoveElementsBy([name, entryType](const auto& entry) {
    return (!name || entry->GetName() == name) &&
           (entry->GetEntryType() == entryType);
  });
}

void Performance::ClearResourceTimings() { mResourceEntries.Clear(); }

struct UserTimingMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("UserTiming");
  }
  static void StreamJSONMarkerData(
      baseprofiler::SpliceableJSONWriter& aWriter,
      const ProfilerString16View& aName, bool aIsMeasure,
      const Maybe<ProfilerString16View>& aStartMark,
      const Maybe<ProfilerString16View>& aEndMark) {
    aWriter.StringProperty("name", NS_ConvertUTF16toUTF8(aName.Data()));
    if (aIsMeasure) {
      aWriter.StringProperty("entryType", "measure");
    } else {
      aWriter.StringProperty("entryType", "mark");
    }

    if (aStartMark.isSome()) {
      aWriter.StringProperty("startMark",
                             NS_ConvertUTF16toUTF8(aStartMark->Data()));
    } else {
      aWriter.NullProperty("startMark");
    }
    if (aEndMark.isSome()) {
      aWriter.StringProperty("endMark",
                             NS_ConvertUTF16toUTF8(aEndMark->Data()));
    } else {
      aWriter.NullProperty("endMark");
    }
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::markerChart, MS::Location::markerTable};
    schema.SetAllLabels("{marker.data.name}");
    schema.AddStaticLabelValue("Marker", "UserTiming");
    schema.AddKeyLabelFormat("entryType", "Entry Type", MS::Format::string);
    schema.AddKeyLabelFormat("name", "Name", MS::Format::string);
    schema.AddKeyLabelFormat("startMark", "Start Mark", MS::Format::string);
    schema.AddKeyLabelFormat("endMark", "End Mark", MS::Format::string);
    schema.AddStaticLabelValue("Description",
                               "UserTimingMeasure is created using the DOM API "
                               "performance.measure().");
    return schema;
  }
};

void Performance::Mark(const nsAString& aName, ErrorResult& aRv) {
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

  if (profiler_can_accept_markers()) {
    Maybe<uint64_t> innerWindowId;
    if (GetOwner()) {
      innerWindowId = Some(GetOwner()->WindowID());
    }
    profiler_add_marker("UserTiming", geckoprofiler::category::DOM,
                        MarkerInnerWindowId(innerWindowId), UserTimingMarker{},
                        aName, /* aIsMeasure */ false, Nothing{}, Nothing{});
  }
}

void Performance::ClearMarks(const Optional<nsAString>& aName) {
  ClearUserEntries(aName, u"mark"_ns);
}

DOMHighResTimeStamp Performance::ResolveTimestampFromName(
    const nsAString& aName, ErrorResult& aRv) {
  AutoTArray<RefPtr<PerformanceEntry>, 1> arr;
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

  DOMHighResTimeStamp ts = GetPerformanceTimingFromString(aName);
  if (!ts) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return 0;
  }

  return ts - CreationTime();
}

void Performance::Measure(const nsAString& aName,
                          const Optional<nsAString>& aStartMark,
                          const Optional<nsAString>& aEndMark,
                          ErrorResult& aRv) {
  // We add nothing when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return;
  }

  DOMHighResTimeStamp startTime;
  DOMHighResTimeStamp endTime;

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

  if (profiler_can_accept_markers()) {
    TimeStamp startTimeStamp =
        CreationTimeStamp() + TimeDuration::FromMilliseconds(startTime);
    TimeStamp endTimeStamp =
        CreationTimeStamp() + TimeDuration::FromMilliseconds(endTime);

    // Convert to Maybe values so that Optional types do not need to be used in
    // the profiler.
    Maybe<nsString> startMark;
    if (aStartMark.WasPassed()) {
      startMark.emplace(aStartMark.Value());
    }
    Maybe<nsString> endMark;
    if (aEndMark.WasPassed()) {
      endMark.emplace(aEndMark.Value());
    }

    Maybe<uint64_t> innerWindowId;
    if (GetOwner()) {
      innerWindowId = Some(GetOwner()->WindowID());
    }
    profiler_add_marker("UserTiming", geckoprofiler::category::DOM,
                        {MarkerTiming::Interval(startTimeStamp, endTimeStamp),
                         MarkerInnerWindowId(innerWindowId)},
                        UserTimingMarker{}, aName, /* aIsMeasure */ true,
                        startMark, endMark);
  }
}

void Performance::ClearMeasures(const Optional<nsAString>& aName) {
  ClearUserEntries(aName, u"measure"_ns);
}

void Performance::LogEntry(PerformanceEntry* aEntry,
                           const nsACString& aOwner) const {
  PERFLOG("Performance Entry: %s|%s|%s|%f|%f|%" PRIu64 "\n",
          aOwner.BeginReading(),
          NS_ConvertUTF16toUTF8(aEntry->GetEntryType()->GetUTF16String()).get(),
          NS_ConvertUTF16toUTF8(aEntry->GetName()->GetUTF16String()).get(),
          aEntry->StartTime(), aEntry->Duration(),
          static_cast<uint64_t>(PR_Now() / PR_USEC_PER_MSEC));
}

void Performance::TimingNotification(PerformanceEntry* aEntry,
                                     const nsACString& aOwner,
                                     uint64_t aEpoch) {
  PerformanceEntryEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  aEntry->GetName(init.mName);
  aEntry->GetEntryType(init.mEntryType);
  init.mStartTime = aEntry->StartTime();
  init.mDuration = aEntry->Duration();
  init.mEpoch = aEpoch;
  CopyUTF8toUTF16(aOwner, init.mOrigin);

  RefPtr<PerformanceEntryEvent> perfEntryEvent =
      PerformanceEntryEvent::Constructor(this, u"performanceentry"_ns, init);

  nsCOMPtr<EventTarget> et = do_QueryInterface(GetOwner());
  if (et) {
    et->DispatchEvent(*perfEntryEvent);
  }
}

void Performance::InsertUserEntry(PerformanceEntry* aEntry) {
  mUserEntries.InsertElementSorted(aEntry, PerformanceEntryComparator());

  QueueEntry(aEntry);
}

/*
 * Steps are labeled according to the description found at
 * https://w3c.github.io/resource-timing/#sec-extensions-performance-interface.
 *
 * Buffer Full Event
 */
void Performance::BufferEvent() {
  /*
   * While resource timing secondary buffer is not empty,
   * run the following substeps:
   */
  while (!mSecondaryResourceEntries.IsEmpty()) {
    uint32_t secondaryResourceEntriesBeforeCount = 0;
    uint32_t secondaryResourceEntriesAfterCount = 0;

    /*
     * Let number of excess entries before be resource
     * timing secondary buffer current size.
     */
    secondaryResourceEntriesBeforeCount = mSecondaryResourceEntries.Length();

    /*
     * If can add resource timing entry returns false,
     * then fire an event named resourcetimingbufferfull
     * at the Performance object.
     */
    if (!CanAddResourceTimingEntry()) {
      DispatchBufferFullEvent();
    }

    /*
     * Run copy secondary buffer.
     *
     * While resource timing secondary buffer is not
     * empty and can add resource timing entry returns
     * true ...
     */
    while (!mSecondaryResourceEntries.IsEmpty() &&
           CanAddResourceTimingEntry()) {
      /*
       * Let entry be the oldest PerformanceResourceTiming
       * in resource timing secondary buffer. Add entry to
       * the end of performance entry buffer. Increment
       * resource timing buffer current size by 1.
       */
      mResourceEntries.InsertElementSorted(
          mSecondaryResourceEntries.ElementAt(0), PerformanceEntryComparator());
      /*
       * Remove entry from resource timing secondary buffer.
       * Decrement resource timing secondary buffer current
       * size by 1.
       */
      mSecondaryResourceEntries.RemoveElementAt(0);
    }

    /*
     * Let number of excess entries after be resource
     * timing secondary buffer current size.
     */
    secondaryResourceEntriesAfterCount = mSecondaryResourceEntries.Length();

    /*
     * If number of excess entries before is lower than
     * or equals number of excess entries after, then
     * remove all entries from resource timing secondary
     * buffer, set resource timing secondary buffer current
     * size to 0, and abort these steps.
     */
    if (secondaryResourceEntriesBeforeCount <=
        secondaryResourceEntriesAfterCount) {
      mSecondaryResourceEntries.Clear();
      break;
    }
  }
  /*
   * Set resource timing buffer full event pending flag
   * to false.
   */
  mPendingResourceTimingBufferFullEvent = false;
}

void Performance::SetResourceTimingBufferSize(uint64_t aMaxSize) {
  mResourceTimingBufferSize = aMaxSize;
}

/*
 * Steps are labeled according to the description found at
 * https://w3c.github.io/resource-timing/#sec-extensions-performance-interface.
 *
 * Can Add Resource Timing Entry
 */
MOZ_ALWAYS_INLINE bool Performance::CanAddResourceTimingEntry() {
  /*
   * If resource timing buffer current size is smaller than resource timing
   * buffer size limit, return true. [Otherwise,] [r]eturn false.
   */
  return mResourceEntries.Length() < mResourceTimingBufferSize;
}

/*
 * Steps are labeled according to the description found at
 * https://w3c.github.io/resource-timing/#sec-extensions-performance-interface.
 *
 * Add a PerformanceResourceTiming Entry
 */
void Performance::InsertResourceEntry(PerformanceEntry* aEntry) {
  MOZ_ASSERT(aEntry);

  if (nsContentUtils::ShouldResistFingerprinting()) {
    return;
  }

  /*
   * Let new entry be the input PerformanceEntry to be added.
   *
   * If can add resource timing entry returns true and resource
   * timing buffer full event pending flag is false ...
   */
  if (CanAddResourceTimingEntry() && !mPendingResourceTimingBufferFullEvent) {
    /*
     * Add new entry to the performance entry buffer.
     * Increase resource timing buffer current size by 1.
     */
    mResourceEntries.InsertElementSorted(aEntry, PerformanceEntryComparator());
    QueueEntry(aEntry);
    return;
  }

  /*
   * If resource timing buffer full event pending flag is
   * false ...
   */
  if (!mPendingResourceTimingBufferFullEvent) {
    /*
     * Set resource timing buffer full event pending flag
     * to true.
     */
    mPendingResourceTimingBufferFullEvent = true;

    /*
     * Queue a task to run fire a buffer full event.
     */
    NS_DispatchToCurrentThread(NewCancelableRunnableMethod(
        "Performance::BufferEvent", this, &Performance::BufferEvent));
  }
  /*
   * Add new entry to the resource timing secondary buffer.
   * Increase resource timing secondary buffer current size
   * by 1.
   */
  mSecondaryResourceEntries.InsertElementSorted(aEntry,
                                                PerformanceEntryComparator());
}

void Performance::AddObserver(PerformanceObserver* aObserver) {
  mObservers.AppendElementUnlessExists(aObserver);
}

void Performance::RemoveObserver(PerformanceObserver* aObserver) {
  mObservers.RemoveElement(aObserver);
}

void Performance::NotifyObservers() {
  mPendingNotificationObserversTask = false;
  NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers, Notify, ());
}

void Performance::CancelNotificationObservers() {
  mPendingNotificationObserversTask = false;
}

class NotifyObserversTask final : public CancelableRunnable {
 public:
  explicit NotifyObserversTask(Performance* aPerformance)
      : CancelableRunnable("dom::NotifyObserversTask"),
        mPerformance(aPerformance) {
    MOZ_ASSERT(mPerformance);
  }

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY for now until Runnable::Run is
  // MOZ_CAN_RUN_SCRIPT.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD Run() override {
    MOZ_ASSERT(mPerformance);
    RefPtr<Performance> performance(mPerformance);
    performance->NotifyObservers();
    return NS_OK;
  }

  nsresult Cancel() override {
    mPerformance->CancelNotificationObservers();
    mPerformance = nullptr;
    return NS_OK;
  }

 private:
  ~NotifyObserversTask() = default;

  RefPtr<Performance> mPerformance;
};

void Performance::QueueNotificationObserversTask() {
  if (!mPendingNotificationObserversTask) {
    RunNotificationObserversTask();
  }
}

void Performance::RunNotificationObserversTask() {
  mPendingNotificationObserversTask = true;
  nsCOMPtr<nsIRunnable> task = new NotifyObserversTask(this);
  nsresult rv;
  if (GetOwnerGlobal()) {
    rv = GetOwnerGlobal()->Dispatch(TaskCategory::Other, task.forget());
  } else {
    rv = NS_DispatchToCurrentThread(task);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPendingNotificationObserversTask = false;
  }
}

void Performance::QueueEntry(PerformanceEntry* aEntry) {
  nsTObserverArray<PerformanceObserver*> interestedObservers;
  if (!mObservers.IsEmpty()) {
    const auto [begin, end] = mObservers.NonObservingRange();
    std::copy_if(begin, end, MakeBackInserter(interestedObservers),
                 [aEntry](PerformanceObserver* observer) {
                   return observer->ObservesTypeOfEntry(aEntry);
                 });
  }

  NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(interestedObservers, QueueEntry,
                                           (aEntry));

  aEntry->BufferEntryIfNeeded();

  if (!interestedObservers.IsEmpty()) {
    QueueNotificationObserversTask();
  }
}

void Performance::MemoryPressure() { mUserEntries.Clear(); }

size_t Performance::SizeOfUserEntries(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t userEntries = 0;
  for (const PerformanceEntry* entry : mUserEntries) {
    userEntries += entry->SizeOfIncludingThis(aMallocSizeOf);
  }
  return userEntries;
}

size_t Performance::SizeOfResourceEntries(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t resourceEntries = 0;
  for (const PerformanceEntry* entry : mResourceEntries) {
    resourceEntries += entry->SizeOfIncludingThis(aMallocSizeOf);
  }
  return resourceEntries;
}

}  // namespace mozilla::dom
