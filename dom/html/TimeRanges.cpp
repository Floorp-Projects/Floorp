/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TimeRanges.h"
#include "mozilla/dom/TimeRangesBinding.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "nsError.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(TimeRanges, nsIDOMTimeRanges)

TimeRanges::TimeRanges()
{
  MOZ_COUNT_CTOR(TimeRanges);
}

TimeRanges::~TimeRanges()
{
  MOZ_COUNT_DTOR(TimeRanges);
}

NS_IMETHODIMP
TimeRanges::GetLength(uint32_t* aLength)
{
  *aLength = Length();
  return NS_OK;
}

double
TimeRanges::Start(uint32_t aIndex, ErrorResult& aRv)
{
  if (aIndex >= mRanges.Length()) {
    aRv = NS_ERROR_DOM_INDEX_SIZE_ERR;
    return 0;
  }

  return mRanges[aIndex].mStart;
}

NS_IMETHODIMP
TimeRanges::Start(uint32_t aIndex, double* aTime)
{
  ErrorResult rv;
  *aTime = Start(aIndex, rv);
  return rv.ErrorCode();
}

double
TimeRanges::End(uint32_t aIndex, ErrorResult& aRv)
{
  if (aIndex >= mRanges.Length()) {
    aRv = NS_ERROR_DOM_INDEX_SIZE_ERR;
    return 0;
  }

  return mRanges[aIndex].mEnd;
}

NS_IMETHODIMP
TimeRanges::End(uint32_t aIndex, double* aTime)
{
  ErrorResult rv;
  *aTime = End(aIndex, rv);
  return rv.ErrorCode();
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
TimeRanges::Normalize(double aError)
{
  if (mRanges.Length() >= 2) {
    nsAutoTArray<TimeRange,4> normalized;

    mRanges.Sort(CompareTimeRanges());

    // This merges the intervals.
    TimeRange current(mRanges[0]);
    for (uint32_t i = 1; i < mRanges.Length(); i++) {
      if (current.mStart <= mRanges[i].mStart &&
          current.mEnd >= mRanges[i].mEnd) {
        continue;
      }
      if (current.mEnd + aError >= mRanges[i].mStart) {
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
TimeRanges::Union(const TimeRanges* aOtherRanges, double aError)
{
  mRanges.AppendElements(aOtherRanges->mRanges);
  Normalize(aError);
}

void
TimeRanges::Intersection(const TimeRanges* aOtherRanges)
{
  nsAutoTArray<TimeRange,4> intersection;

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
TimeRanges::Find(double aTime, double aError /* = 0 */)
{
  for (index_type i = 0; i < mRanges.Length(); ++i) {
    if (aTime < mRanges[i].mEnd && (aTime + aError) >= mRanges[i].mStart) {
      return i;
    }
  }
  return NoIndex;
}

JSObject*
TimeRanges::WrapObject(JSContext* aCx)
{
  return TimeRangesBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
