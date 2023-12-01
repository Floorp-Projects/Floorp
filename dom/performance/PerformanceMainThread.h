/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceMainThread_h
#define mozilla_dom_PerformanceMainThread_h

#include "Performance.h"
#include "PerformanceStorage.h"
#include "LargestContentfulPaint.h"
#include "nsTextFrame.h"

namespace mozilla::dom {

class PerformanceNavigationTiming;
class PerformanceEventTiming;

using ImageLCPEntryMap =
    nsTHashMap<LCPEntryHashEntry, RefPtr<LargestContentfulPaint>>;

using TextFrameUnions = nsTHashMap<nsRefPtrHashKey<Element>, nsRect>;

class PerformanceMainThread final : public Performance,
                                    public PerformanceStorage {
 public:
  PerformanceMainThread(nsPIDOMWindowInner* aWindow,
                        nsDOMNavigationTiming* aDOMTiming,
                        nsITimedChannel* aChannel);

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
  bool HadFCPTimingEntry() const { return mFCPTiming; }

  void InsertEventTimingEntry(PerformanceEventTiming*) override;
  void BufferEventTimingEntryIfNeeded(PerformanceEventTiming*) override;
  void DispatchPendingEventTimingEntries() override;

  void BufferLargestContentfulPaintEntryIfNeeded(LargestContentfulPaint*);

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
  void QueueLargestContentfulPaintEntry(LargestContentfulPaint* aEntry);

  size_t SizeOfEventEntries(mozilla::MallocSizeOf aMallocSizeOf) const override;

  static constexpr uint32_t kDefaultEventTimingBufferSize = 150;
  static constexpr uint32_t kDefaultEventTimingDurationThreshold = 104;
  static constexpr double kDefaultEventTimingMinDuration = 16.0;

  static constexpr uint32_t kMaxLargestContentfulPaintBufferSize = 150;

  class EventCounts* EventCounts() override;

  bool IsGlobalObjectWindow() const override { return true; };

  bool HasDispatchedInputEvent() const { return mHasDispatchedInputEvent; }

  void SetHasDispatchedScrollEvent();
  bool HasDispatchedScrollEvent() const { return mHasDispatchedScrollEvent; }

  void ProcessElementTiming();

  void AddImagesPendingRendering(ImagePendingRendering aImagePendingRendering) {
    mImagesPendingRendering.AppendElement(aImagePendingRendering);
  }

  void StoreImageLCPEntry(Element* aElement, imgRequestProxy* aImgRequestProxy,
                          LargestContentfulPaint* aEntry);

  already_AddRefed<LargestContentfulPaint> GetImageLCPEntry(
      Element* aElement, imgRequestProxy* aImgRequestProxy);

  bool UpdateLargestContentfulPaintSize(double aSize);
  double GetLargestContentfulPaintSize() const {
    return mLargestContentfulPaintSize;
  }

  nsTHashMap<nsRefPtrHashKey<Element>, nsRect>& GetTextFrameUnions() {
    return mTextFrameUnions;
  }

  void FinalizeLCPEntriesForText();

  void ClearGeneratedTempDataForLCP();

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

  nsTArray<RefPtr<PerformanceEventTiming>> mEventTimingEntries;
  nsTArray<RefPtr<LargestContentfulPaint>> mLargestContentfulPaintEntries;

  AutoCleanLinkedList<RefPtr<PerformanceEventTiming>>
      mPendingEventTimingEntries;
  bool mHasDispatchedInputEvent = false;
  bool mHasDispatchedScrollEvent = false;

  RefPtr<PerformanceEventTiming> mFirstInputEvent;
  RefPtr<PerformanceEventTiming> mPendingPointerDown;

 private:
  void SetHasDispatchedInputEvent();

  bool mHasQueuedRefreshdriverObserver = false;

  RefPtr<class EventCounts> mEventCounts;
  void IncEventCount(const nsAtom* aType);

  PresShell* GetPresShell();

  nsTArray<ImagePendingRendering> mImagesPendingRendering;

  // The key is the pair of the element initiates the image loading
  // and the imgRequestProxy of the image, and the value is
  // the LCP entry for this image. When the image is
  // completely loaded, we add it to mImageLCPEntryMap.
  // Later, when the image is painted, we get the LCP entry from it
  // to update the size and queue the entry if needed.
  //
  // When the initiating element is disconnected from the document,
  // we keep the orphan entry because if the same memory address is
  // reused by a different LCP candidate, it'll update
  // mImageLCPEntryMap precedes before it tries to get the LCP entry.
  ImageLCPEntryMap mImageLCPEntryMap;

  // Keeps track of the rendered size of the largest contentful paint that
  // we have processed so far.
  double mLargestContentfulPaintSize = 0.0;

  // When a text frame is painted, its area (relative to the
  // containing block) is unioned with other text frames that
  // belong to the same containing block.
  // mTextFrameUnions's key is the containing block, and
  // the value is the unioned area.
  TextFrameUnions mTextFrameUnions;
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, ImageLCPEntryMap& aField,
    const char* aName, uint32_t aFlags = 0) {
  for (auto& entry : aField) {
    RefPtr<LargestContentfulPaint>* lcpEntry = entry.GetModifiableData();
    ImplCycleCollectionTraverse(aCallback, *lcpEntry, "ImageLCPEntryMap.mData",
                                aCallback.Flags());
  }
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, TextFrameUnions& aField,
    const char* aName, uint32_t aFlags = 0) {
  for (auto& entry : aField) {
    ImplCycleCollectionTraverse(
        aCallback, entry, "TextFrameUnions's key (nsRefPtrHashKey<Element>)",
        aFlags);
  }
}

}  // namespace mozilla::dom

#endif  // mozilla_dom_PerformanceMainThread_h
