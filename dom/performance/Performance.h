/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Performance_h
#define mozilla_dom_Performance_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCOMPtr.h"
#include "nsDOMNavigationTiming.h"
#include "nsTObserverArray.h"

class nsITimedChannel;

namespace mozilla {

class ErrorResult;

namespace dom {

class OwningStringOrDouble;
class StringOrPerformanceMeasureOptions;
class PerformanceEntry;
class PerformanceMark;
struct PerformanceMarkOptions;
struct PerformanceMeasureOptions;
class PerformanceMeasure;
class PerformanceNavigation;
class PerformancePaintTiming;
class PerformanceObserver;
class PerformanceService;
class PerformanceStorage;
class PerformanceTiming;
class PerformanceEventTiming;
class WorkerPrivate;
class EventCounts;

// Base class for main-thread and worker Performance API
class Performance : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Performance, DOMEventTargetHelper)

  static bool IsObserverEnabled(JSContext* aCx, JSObject* aGlobal);

  static already_AddRefed<Performance> CreateForMainThread(
      nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
      nsDOMNavigationTiming* aDOMTiming, nsITimedChannel* aChannel);

  static already_AddRefed<Performance> CreateForWorker(
      WorkerPrivate* aWorkerPrivate);

  // This will return nullptr if called outside of a Window or Worker.
  static already_AddRefed<Performance> Get(JSContext* aCx,
                                           nsIGlobalObject* aGlobal);

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  virtual void GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  virtual void GetEntriesByType(const nsAString& aEntryType,
                                nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  virtual void GetEntriesByTypeForObserver(
      const nsAString& aEntryType, nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  virtual void GetEntriesByName(const nsAString& aName,
                                const Optional<nsAString>& aEntryType,
                                nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  virtual PerformanceStorage* AsPerformanceStorage() = 0;

  void ClearResourceTimings();

  DOMHighResTimeStamp Now();

  DOMHighResTimeStamp NowUnclamped() const;

  DOMHighResTimeStamp TimeOrigin();

  already_AddRefed<PerformanceMark> Mark(
      JSContext* aCx, const nsAString& aName,
      const PerformanceMarkOptions& aMarkOptions, ErrorResult& aRv);

  void ClearMarks(const Optional<nsAString>& aName);

  already_AddRefed<PerformanceMeasure> Measure(
      JSContext* aCx, const nsAString& aName,
      const StringOrPerformanceMeasureOptions& aStartOrMeasureOptions,
      const Optional<nsAString>& aEndMark, ErrorResult& aRv);

  void ClearMeasures(const Optional<nsAString>& aName);

  void SetResourceTimingBufferSize(uint64_t aMaxSize);

  void AddObserver(PerformanceObserver* aObserver);
  void RemoveObserver(PerformanceObserver* aObserver);
  MOZ_CAN_RUN_SCRIPT void NotifyObservers();
  void CancelNotificationObservers();

  virtual PerformanceTiming* Timing() = 0;

  virtual PerformanceNavigation* Navigation() = 0;

  virtual void SetFCPTimingEntry(PerformancePaintTiming* aEntry) = 0;

  IMPL_EVENT_HANDLER(resourcetimingbufferfull)

  virtual void GetMozMemory(JSContext* aCx,
                            JS::MutableHandle<JSObject*> aObj) = 0;

  virtual nsDOMNavigationTiming* GetDOMTiming() const = 0;

  virtual nsITimedChannel* GetChannel() const = 0;

  virtual TimeStamp CreationTimeStamp() const = 0;

  bool IsSystemPrincipal() const { return mSystemPrincipal; }

  DOMHighResTimeStamp TimeStampToDOMHighResForRendering(TimeStamp) const;

  virtual uint64_t GetRandomTimelineSeed() = 0;

  void MemoryPressure();

  size_t SizeOfUserEntries(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfResourceEntries(mozilla::MallocSizeOf aMallocSizeOf) const;
  virtual size_t SizeOfEventEntries(mozilla::MallocSizeOf aMallocSizeOf) const {
    return 0;
  }

  void InsertResourceEntry(PerformanceEntry* aEntry);

  virtual void InsertEventTimingEntry(PerformanceEventTiming* aEntry) = 0;

  virtual void BufferEventTimingEntryIfNeeded(
      PerformanceEventTiming* aEntry) = 0;

  virtual class EventCounts* EventCounts() = 0;

  virtual void QueueNavigationTimingEntry() = 0;

  virtual void UpdateNavigationTimingEntry() = 0;

  virtual bool CrossOriginIsolated() const = 0;

  virtual void DispatchPendingEventTimingEntries() = 0;

  void QueueNotificationObserversTask();

  bool IsPerformanceTimingAttribute(const nsAString& aName) const;

  virtual bool IsGlobalObjectWindow() const { return false; };

 protected:
  Performance(nsIGlobalObject* aGlobal, bool aSystemPrincipal);
  Performance(nsPIDOMWindowInner* aWindow, bool aSystemPrincipal);

  virtual ~Performance();

  virtual void InsertUserEntry(PerformanceEntry* aEntry);

  void ClearUserEntries(const Optional<nsAString>& aEntryName,
                        const nsAString& aEntryType);

  virtual void DispatchBufferFullEvent() = 0;

  virtual DOMHighResTimeStamp CreationTime() const = 0;

  virtual DOMHighResTimeStamp GetPerformanceTimingFromString(
      const nsAString& aTimingName) {
    return 0;
  }

  void LogEntry(PerformanceEntry* aEntry, const nsACString& aOwner) const;
  void TimingNotification(PerformanceEntry* aEntry, const nsACString& aOwner,
                          const double aEpoch);

  void RunNotificationObserversTask();
  void QueueEntry(PerformanceEntry* aEntry);

  nsTObserverArray<RefPtr<PerformanceObserver>> mObservers;

 protected:
  static const uint64_t kDefaultResourceTimingBufferSize = 250;

  // When kDefaultResourceTimingBufferSize is increased or removed, these should
  // be changed to use SegmentedVector
  AutoTArray<RefPtr<PerformanceEntry>, kDefaultResourceTimingBufferSize>
      mUserEntries;
  AutoTArray<RefPtr<PerformanceEntry>, kDefaultResourceTimingBufferSize>
      mResourceEntries;
  AutoTArray<RefPtr<PerformanceEntry>, kDefaultResourceTimingBufferSize>
      mSecondaryResourceEntries;

  uint64_t mResourceTimingBufferSize;
  bool mPendingNotificationObserversTask;

  bool mPendingResourceTimingBufferFullEvent;

  RefPtr<PerformanceService> mPerformanceService;

  bool mSystemPrincipal;

 private:
  MOZ_ALWAYS_INLINE bool CanAddResourceTimingEntry();
  void BufferEvent();

  // The attributes of a PerformanceMeasureOptions that we call
  // ResolveTimestamp* on.
  enum class ResolveTimestampAttribute;

  DOMHighResTimeStamp ConvertMarkToTimestampWithString(const nsAString& aName,
                                                       ErrorResult& aRv);
  DOMHighResTimeStamp ConvertMarkToTimestampWithDOMHighResTimeStamp(
      const ResolveTimestampAttribute aAttribute, const double aTimestamp,
      ErrorResult& aRv);
  DOMHighResTimeStamp ConvertMarkToTimestamp(
      const ResolveTimestampAttribute aAttribute,
      const OwningStringOrDouble& aMarkNameOrTimestamp, ErrorResult& aRv);

  DOMHighResTimeStamp ConvertNameToTimestamp(const nsAString& aName,
                                             ErrorResult& aRv);

  DOMHighResTimeStamp ResolveEndTimeForMeasure(
      const Optional<nsAString>& aEndMark,
      const Maybe<const PerformanceMeasureOptions&>& aOptions,
      ErrorResult& aRv);
  DOMHighResTimeStamp ResolveStartTimeForMeasure(
      const Maybe<const nsAString&>& aStartMark,
      const Maybe<const PerformanceMeasureOptions&>& aOptions,
      ErrorResult& aRv);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Performance_h
