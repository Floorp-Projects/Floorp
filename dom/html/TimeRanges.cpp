/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TimeRanges.h"
#include "mozilla/dom/TimeRangesBinding.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "TimeUnits.h"
#include "nsError.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TimeRanges, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TimeRanges)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TimeRanges)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TimeRanges)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TimeRanges::TimeRanges()
  : mParent(nullptr)
{
}

TimeRanges::TimeRanges(nsISupports* aParent)
  : mParent(aParent)
{
}

TimeRanges::TimeRanges(nsISupports* aParent,
                       const media::TimeIntervals& aTimeIntervals)
  : TimeRanges(aParent)
{
  if (aTimeIntervals.IsInvalid()) {
    return;
  }
  for (const media::TimeInterval& interval : aTimeIntervals) {
    Add(interval.mStart.ToSeconds(), interval.mEnd.ToSeconds());
  }
}

TimeRanges::TimeRanges(const media::TimeIntervals& aTimeIntervals)
  : TimeRanges(nullptr, aTimeIntervals)
{
}

media::TimeIntervals
TimeRanges::ToTimeIntervals() const
{
  media::TimeIntervals t;
  for (uint32_t i = 0; i < Length(); i++) {
    t += media::TimeInterval(media::TimeUnit::FromSeconds(Start(i)),
                             media::TimeUnit::FromSeconds(End(i)));
  }
  return t;
}

TimeRanges::~TimeRanges()
{
}

double
TimeRanges::Start(uint32_t aIndex, ErrorResult& aRv) const
{
  if (aIndex >= mRanges.Length()) {
    aRv = NS_ERROR_DOM_INDEX_SIZE_ERR;
    return 0;
  }

  return Start(aIndex);
}

double
TimeRanges::End(uint32_t aIndex, ErrorResult& aRv) const
{
  if (aIndex >= mRanges.Length()) {
    aRv = NS_ERROR_DOM_INDEX_SIZE_ERR;
    return 0;
  }

  return End(aIndex);
}

void
TimeRanges::Add(double aStart, double aEnd)
{
  if (aStart > aEnd) {
    NS_WARNING("Can't add a range if the end is older that the start.");
    return;
  }
  mRanges.AppendElement(TimeRange(aStart,aEnd));
}

double
TimeRanges::GetStartTime()
{
  if (mRanges.IsEmpty()) {
    return -1.0;
  }
  return mRanges[0].mStart;
}

double
TimeRanges::GetEndTime()
{
  if (mRanges.IsEmpty()) {
    return -1.0;
  }
  return mRanges[mRanges.Length() - 1].mEnd;
}

void
TimeRanges::Normalize(double aTolerance)
{
  if (mRanges.Length() >= 2) {
    AutoTArray<TimeRange,4> normalized;

    mRanges.Sort(CompareTimeRanges());

    // This merges the intervals.
    TimeRange current(mRanges[0]);
    for (uint32_t i = 1; i < mRanges.Length(); i++) {
      if (current.mStart <= mRanges[i].mStart &&
          current.mEnd >= mRanges[i].mEnd) {
        continue;
      }
      if (current.mEnd + aTolerance >= mRanges[i].mStart) {
        current.mEnd = mRanges[i].mEnd;
      } else {
        normalized.AppendElement(current);
        current = mRanges[i];
      }
    }

    normalized.AppendElement(current);

    mRanges = normalized;
  }
}

void
TimeRanges::Union(const TimeRanges* aOtherRanges, double aTolerance)
{
  mRanges.AppendElements(aOtherRanges->mRanges);
  Normalize(aTolerance);
}

void
TimeRanges::Intersection(const TimeRanges* aOtherRanges)
{
  AutoTArray<TimeRange,4> intersection;

  const nsTArray<TimeRange>& otherRanges = aOtherRanges->mRanges;
  for (index_type i = 0, j = 0; i < mRanges.Length() && j < otherRanges.Length();) {
    double start = std::max(mRanges[i].mStart, otherRanges[j].mStart);
    double end = std::min(mRanges[i].mEnd, otherRanges[j].mEnd);
    if (start < end) {
      intersection.AppendElement(TimeRange(start, end));
    }
    if (mRanges[i].mEnd < otherRanges[j].mEnd) {
      i += 1;
    } else {
      j += 1;
    }
  }

  mRanges = intersection;
}

TimeRanges::index_type
TimeRanges::Find(double aTime, double aTolerance /* = 0 */)
{
  for (index_type i = 0; i < mRanges.Length(); ++i) {
    if (aTime < mRanges[i].mEnd && (aTime + aTolerance) >= mRanges[i].mStart) {
      return i;
    }
  }
  return NoIndex;
}

JSObject*
TimeRanges::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TimeRanges_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports*
TimeRanges::GetParentObject() const
{
  return mParent;
}

void
TimeRanges::Shift(double aOffset)
{
  for (index_type i = 0; i < mRanges.Length(); ++i) {
    mRanges[i].mStart += aOffset;
    mRanges[i].mEnd += aOffset;
  }
}

} // namespace dom
} // namespace mozilla
