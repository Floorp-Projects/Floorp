/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformancePaintTiming_h___
#define mozilla_dom_PerformancePaintTiming_h___

#include "mozilla/dom/PerformanceEntry.h"
#include "mozilla/dom/PerformancePaintTimingBinding.h"

namespace mozilla {
namespace dom {

class Performance;

// https://w3c.github.io/paint-timing/#sec-PerformancePaintTiming
// Unlike timeToContentfulPaint, this timing is generated during
// displaylist building, when a frame is contentful, we collect
// the timestamp. Whereas, timeToContentfulPaint uses a compositor-side
// timestamp.
class PerformancePaintTiming final : public PerformanceEntry {
 public:
  PerformancePaintTiming(Performance* aPerformance, const nsAString& aName,
                         const TimeStamp& aStartTime);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PerformancePaintTiming,
                                           PerformanceEntry)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  DOMHighResTimeStamp StartTime() const override;

  size_t SizeOfIncludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const override;

 private:
  ~PerformancePaintTiming();
  RefPtr<Performance> mPerformance;

  const TimeStamp mRawStarTime;
  mutable Maybe<DOMHighResTimeStamp> mCachedStartTime;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_PerformanacePaintTiming_h___ */
