/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceEventTiming_h___
#define mozilla_dom_PerformanceEventTiming_h___

#include "mozilla/dom/PerformanceEntry.h"
#include "mozilla/EventForwards.h"
#include "nsRFPService.h"
#include "Performance.h"
#include "nsIWeakReferenceUtils.h"
#include "nsINode.h"

namespace mozilla {
class WidgetEvent;
namespace dom {

class PerformanceEventTiming final
    : public PerformanceEntry,
      public LinkedListElement<RefPtr<PerformanceEventTiming>> {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PerformanceEventTiming,
                                           PerformanceEntry)

  static already_AddRefed<PerformanceEventTiming> TryGenerateEventTiming(
      const EventTarget* aTarget, const WidgetEvent* aEvent);

  already_AddRefed<PerformanceEventTiming> Clone() {
    RefPtr<PerformanceEventTiming> eventTiming =
        new PerformanceEventTiming(*this);
    return eventTiming.forget();
  }

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  DOMHighResTimeStamp ProcessingStart() const {
    if (mCachedProcessingStart.isNothing()) {
      mCachedProcessingStart.emplace(nsRFPService::ReduceTimePrecisionAsMSecs(
          mProcessingStart, mPerformance->GetRandomTimelineSeed(),
          mPerformance->IsSystemPrincipal(),
          mPerformance->CrossOriginIsolated()));
    }
    return mCachedProcessingStart.value();
  }

  DOMHighResTimeStamp ProcessingEnd() const {
    if (mCachedProcessingEnd.isNothing()) {
      mCachedProcessingEnd.emplace(nsRFPService::ReduceTimePrecisionAsMSecs(
          mProcessingEnd, mPerformance->GetRandomTimelineSeed(),
          mPerformance->IsSystemPrincipal(),
          mPerformance->CrossOriginIsolated()));
    }
    return mCachedProcessingEnd.value();
  }

  bool Cancelable() const { return mCancelable; }

  nsINode* GetTarget() const;

  void SetDuration(const DOMHighResTimeStamp aDuration) {
    mDuration = aDuration;
  }

  // nsRFPService::ReduceTimePrecisionAsMSecs might causes
  // some memory overhead, using the raw timestamp internally
  // to avoid calling in unnecessarily.
  DOMHighResTimeStamp RawDuration() const { return mDuration; }

  DOMHighResTimeStamp Duration() const override {
    if (mCachedDuration.isNothing()) {
      mCachedDuration.emplace(nsRFPService::ReduceTimePrecisionAsMSecs(
          mDuration, mPerformance->GetRandomTimelineSeed(),
          mPerformance->IsSystemPrincipal(),
          mPerformance->CrossOriginIsolated()));
    }
    return mCachedDuration.value();
  }

  // Similar as RawDuration; Used to avoid calling
  // nsRFPService::ReduceTimePrecisionAsMSecs unnecessarily.
  DOMHighResTimeStamp RawStartTime() const { return mStartTime; }

  DOMHighResTimeStamp StartTime() const override {
    if (mCachedStartTime.isNothing()) {
      mCachedStartTime.emplace(nsRFPService::ReduceTimePrecisionAsMSecs(
          mStartTime, mPerformance->GetRandomTimelineSeed(),
          mPerformance->IsSystemPrincipal(),
          mPerformance->CrossOriginIsolated()));
    }
    return mCachedStartTime.value();
  }

  bool ShouldAddEntryToBuffer(double aDuration) const;
  bool ShouldAddEntryToObserverBuffer(PerformanceObserverInit&) const override;

  void BufferEntryIfNeeded() override;

  void FinalizeEventTiming(EventTarget* aTarget);

  EventMessage GetMessage() const { return mMessage; }

 private:
  PerformanceEventTiming(Performance* aPerformance, const nsAString& aName,
                         const TimeStamp& aStartTime, bool aIsCacelable,
                         EventMessage aMessage);

  PerformanceEventTiming(const PerformanceEventTiming& aEventTimingEntry);

  ~PerformanceEventTiming() = default;

  RefPtr<Performance> mPerformance;

  DOMHighResTimeStamp mProcessingStart;
  mutable Maybe<DOMHighResTimeStamp> mCachedProcessingStart;

  DOMHighResTimeStamp mProcessingEnd;
  mutable Maybe<DOMHighResTimeStamp> mCachedProcessingEnd;

  nsWeakPtr mTarget;

  DOMHighResTimeStamp mStartTime;
  mutable Maybe<DOMHighResTimeStamp> mCachedStartTime;

  DOMHighResTimeStamp mDuration;
  mutable Maybe<DOMHighResTimeStamp> mCachedDuration;

  bool mCancelable;

  EventMessage mMessage;
};
}  // namespace dom
}  // namespace mozilla

#endif
