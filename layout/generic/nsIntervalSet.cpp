/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a set of ranges on a number-line */

#include "nsIntervalSet.h"
#include "nsAlgorithm.h"
#include NEW_H

nsIntervalSet::nsIntervalSet(IntervalSetAlloc aAlloc, IntervalSetFree aFree,
                             void* aAllocatorClosure)
    : mList(nullptr),
      mAlloc(aAlloc),
      mFree(aFree),
      mAllocatorClosure(aAllocatorClosure)
{
    NS_ASSERTION(mAlloc && mFree, "null callback params");
}

nsIntervalSet::~nsIntervalSet()
{
    Interval *current = mList;
    while (current) {
        Interval *trash = current;
        current = current->mNext;
        FreeInterval(trash);
    }
}

void nsIntervalSet::FreeInterval(nsIntervalSet::Interval *aInterval)
{
    NS_ASSERTION(aInterval, "null interval");

    aInterval->Interval::~Interval();
    (*mFree)(sizeof(Interval), aInterval, mAllocatorClosure);
}

void nsIntervalSet::IncludeInterval(coord_type aBegin, coord_type aEnd)
{
    Interval *newInterval = static_cast<Interval*>
                                       ((*mAlloc)(sizeof(Interval), mAllocatorClosure));
    if (!newInterval) {
        NS_NOTREACHED("allocation failure");
        return;
    }
    new(newInterval) Interval(aBegin, aEnd);

    Interval **current = &mList;
    while (*current && (*current)->mEnd < aBegin)
        current = &(*current)->mNext;

    newInterval->mNext = *current;
    *current = newInterval;

    Interval *subsumed = newInterval->mNext;
    while (subsumed && subsumed->mBegin <= aEnd) {
        newInterval->mBegin = NS_MIN(newInterval->mBegin, subsumed->mBegin);
        newInterval->mEnd = NS_MAX(newInterval->mEnd, subsumed->mEnd);
        newInterval->mNext = subsumed->mNext;
        FreeInterval(subsumed);
        subsumed = newInterval->mNext;
    }
}

bool nsIntervalSet::Intersects(coord_type aBegin, coord_type aEnd) const
{
    Interval *current = mList;
    while (current && current->mBegin <= aEnd) {
        if (current->mEnd >= aBegin)
            return true;
        current = current->mNext;
    }
    return false;
}

bool nsIntervalSet::Contains(coord_type aBegin, coord_type aEnd) const
{
    Interval *current = mList;
    while (current && current->mBegin <= aBegin) {
        if (current->mEnd >= aEnd)
            return true;
        current = current->mNext;
    }
    return false;
}
