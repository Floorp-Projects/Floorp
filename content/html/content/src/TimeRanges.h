/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TimeRanges_h_
#define mozilla_dom_TimeRanges_h_

#include "nsIDOMTimeRanges.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {

// Implements media TimeRanges:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#timeranges
class TimeRanges MOZ_FINAL : public nsIDOMTimeRanges
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMTIMERANGES

  TimeRanges();
  ~TimeRanges();

  void Add(double aStart, double aEnd);

  // Returns the end time of the last range, or -1 if no ranges added.
  double GetFinalEndTime();

  // See http://www.whatwg.org/html/#normalized-timeranges-object
  void Normalize();

  JSObject* WrapObject(JSContext* aCx);

  uint32_t Length() const
  {
    return mRanges.Length();
  }

  virtual double Start(uint32_t aIndex, ErrorResult& aRv);

  virtual double End(uint32_t aIndex, ErrorResult& aRv);

private:

  // Comparator which orders TimeRanges by start time. Used by Normalize().
  struct TimeRange
  {
    TimeRange(double aStart, double aEnd)
      : mStart(aStart),
        mEnd(aEnd) {}
    double mStart;
    double mEnd;
  };

  struct CompareTimeRanges
  {
    bool Equals(const TimeRange& aTr1, const TimeRange& aTr2) const {
      return aTr1.mStart == aTr2.mStart && aTr1.mEnd == aTr2.mEnd;
    }

    bool LessThan(const TimeRange& aTr1, const TimeRange& aTr2) const {
      return aTr1.mStart < aTr2.mStart;
    }
  };

  nsAutoTArray<TimeRange,4> mRanges;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TimeRanges_h_
