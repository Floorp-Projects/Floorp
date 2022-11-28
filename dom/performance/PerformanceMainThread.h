/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceMainThread_h
#define mozilla_dom_PerformanceMainThread_h

#include "Performance.h"
#include "PerformanceStorage.h"

namespace mozilla::dom {

class PerformanceNavigationTiming;
class PerformanceEventTiming;

class PerformanceMainThread final : public Performance,
                                    public PerformanceStorage {
 public:
  PerformanceMainThread(nsPIDOMWindowInner* aWindow,
                        nsDOMNavigationTiming* aDOMTiming,
                        nsITimedChannel* aChannel, bool aPrincipal);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PerformanceMainThread,
                                                         Performance)

  PerformanceStorage* AsPerformanceStorage() override { return this; }

  virtual PerformanceTiming* Timing() override;

  virtual PerformanceNavigation* Navigation() override;

  virtual void AddEntry(nsIHttpChannel* channel,
                        nsITimedChannel* timedChannel) override;

  // aData must be non-null.
  virtual void AddEntry(const nsString& entryName,
                        const nsString& initiatorType,
                        UniquePtr<PerformanceTimingData>&& aData) override;

  // aPerformanceTimingData must be non-null.
  void AddRawEntry(UniquePtr<PerformanceTimingData> aPerformanceTimingData,
                   const nsAString& aInitiatorType,
                   const nsAString& aEntryName);
  virtual void SetFCPTimingEntry(PerformancePaintTiming* aEntry) override;

  void InsertEventTimingEntry(PerformanceEventTiming*) override;
  void BufferEventTimingEntryIfNeeded(PerformanceEventTiming*) override;
  void DispatchPendingEventTimingEntries() override;

  TimeStamp CreationTimeStamp() const override;

  DOMHighResTimeStamp CreationTime() const override;

  virtual void GetMozMemory(JSContext* aCx,
                            JS::MutableHandle<JSObject*> aObj) override;

  virtual nsDOMNavigationTiming* GetDOMTiming() const override {
    return mDOMTiming;
  }

  virtual uint64_t GetRandomTimelineSeed() override {
    return GetDOMTiming()->GetRandomTimelineSeed();
  }

  virtual nsITimedChannel* GetChannel() const override { return mChannel; }

  // The GetEntries* methods need to be overriden in order to add the
  // the document entry of type navigation.
  virtual void GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval) override;

  // Return entries which qualify availableFromTimeline boolean check
  virtual void GetEntriesByType(
      const nsAString& aEntryType,
      nsTArray<RefPtr<PerformanceEntry>>& aRetval) override;

  // There are entries that we don't want expose via performance, however
  // we do want PerformanceObserver to get them
  void GetEntriesByTypeForObserver(
      const nsAString& aEntryType,
      nsTArray<RefPtr<PerformanceEntry>>& aRetval) override;
  virtual void GetEntriesByName(
      const nsAString& aName, const Optional<nsAString>& aEntryType,
      nsTArray<RefPtr<PerformanceEntry>>& aRetval) override;

  void UpdateNavigationTimingEntry() override;
  void QueueNavigationTimingEntry() override;

  bool CrossOriginIsolated() const override;

  size_t SizeOfEventEntries(mozilla::MallocSizeOf aMallocSizeOf) const override;

  static constexpr uint32_t kDefaultEventTimingBufferSize = 150;
  static constexpr uint32_t kDefaultEventTimingDurationThreshold = 104;
  static constexpr double kDefaultEventTimingMinDuration = 16.0;

  class EventCounts* EventCounts() override;

  bool IsGlobalObjectWindow() const override { return true; };

 protected:
  ~PerformanceMainThread();

  void CreateNavigationTimingEntry();

  void InsertUserEntry(PerformanceEntry* aEntry) override;

  DOMHighResTimeStamp GetPerformanceTimingFromString(
      const nsAString& aTimingName) override;

  void DispatchBufferFullEvent() override;

  RefPtr<PerformanceNavigationTiming> mDocEntry;
  RefPtr<nsDOMNavigationTiming> mDOMTiming;
  nsCOMPtr<nsITimedChannel> mChannel;
  RefPtr<PerformanceTiming> mTiming;
  RefPtr<PerformanceNavigation> mNavigation;
  RefPtr<PerformancePaintTiming> mFCPTiming;
  JS::Heap<JSObject*> mMozMemory;

  const bool mCrossOriginIsolated;

  nsTArray<RefPtr<PerformanceEventTiming>> mEventTimingEntries;

  AutoCleanLinkedList<RefPtr<PerformanceEventTiming>>
      mPendingEventTimingEntries;
  bool mHasDispatchedInputEvent = false;

  RefPtr<PerformanceEventTiming> mFirstInputEvent;
  RefPtr<PerformanceEventTiming> mPendingPointerDown;

 private:
  bool mHasQueuedRefreshdriverObserver = false;

  RefPtr<class EventCounts> mEventCounts;
  void IncEventCount(const nsAtom* aType);

  PresShell* GetPresShell();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_PerformanceMainThread_h
