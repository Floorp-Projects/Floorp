/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTimeRanges.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"

NS_IMPL_ADDREF(nsTimeRanges)
NS_IMPL_RELEASE(nsTimeRanges)

DOMCI_DATA(TimeRanges, nsTimeRanges)

NS_INTERFACE_MAP_BEGIN(nsTimeRanges)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTimeRanges)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TimeRanges)
NS_INTERFACE_MAP_END

nsTimeRanges::nsTimeRanges()
{
  MOZ_COUNT_CTOR(nsTimeRanges);
}

nsTimeRanges::~nsTimeRanges()
{
  MOZ_COUNT_DTOR(nsTimeRanges);
}

NS_IMETHODIMP
nsTimeRanges::GetLength(PRUint32* aLength)
{
  *aLength = mRanges.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsTimeRanges::Start(PRUint32 aIndex, double* aTime)
{
  if (aIndex >= mRanges.Length())
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  *aTime = mRanges[aIndex].mStart;
  return NS_OK;
}

NS_IMETHODIMP
nsTimeRanges::End(PRUint32 aIndex, double* aTime)
{
  if (aIndex >= mRanges.Length())
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  *aTime = mRanges[aIndex].mEnd;
  return NS_OK;
}

void
nsTimeRanges::Add(double aStart, double aEnd)
{
  if (aStart > aEnd) {
    NS_WARNING("Can't add a range if the end is older that the start.");
    return;
  }
  mRanges.AppendElement(TimeRange(aStart,aEnd));
}

void
nsTimeRanges::Normalize()
{
  if (mRanges.Length() >= 2) {
    nsAutoTArray<TimeRange,4> normalized;

    mRanges.Sort(CompareTimeRanges());

    // This merges the intervals.
    TimeRange current(mRanges[0]);
    for (PRUint32 i = 1; i < mRanges.Length(); i++) {
      if (current.mStart <= mRanges[i].mStart &&
          current.mEnd >= mRanges[i].mEnd) {
        continue;
      }
      if (current.mEnd >= mRanges[i].mStart) {
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
