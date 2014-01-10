/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxSkipChars.h"

void
gfxSkipCharsIterator::SetOriginalOffset(int32_t aOffset)
{
    aOffset += mOriginalStringToSkipCharsOffset;
    NS_ASSERTION(uint32_t(aOffset) <= mSkipChars->mCharCount,
                 "Invalid offset");

    mOriginalStringOffset = aOffset;

    uint32_t rangeCount = mSkipChars->mRanges.Length();
    if (rangeCount == 0) {
        mSkippedStringOffset = aOffset;
        return;
    }

    // at start of string?
    if (aOffset == 0) {
        mSkippedStringOffset = 0;
        mCurrentRangeIndex =
            rangeCount && mSkipChars->mRanges[0].Start() == 0 ? 0 : -1;
        return;
    }

    // find the range that includes or precedes aOffset
    uint32_t lo = 0, hi = rangeCount;
    const gfxSkipChars::SkippedRange* ranges = mSkipChars->mRanges.Elements();
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (uint32_t(aOffset) < ranges[mid].Start()) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }

    if (lo == rangeCount) {
        mCurrentRangeIndex = rangeCount - 1;
    } else if (uint32_t(aOffset) < ranges[lo].Start()) {
        mCurrentRangeIndex = lo - 1;
        if (mCurrentRangeIndex == -1) {
            mSkippedStringOffset = aOffset;
            return;
        }
    } else {
        mCurrentRangeIndex = lo;
    }

    const gfxSkipChars::SkippedRange& r = ranges[mCurrentRangeIndex];
    if (uint32_t(aOffset) < r.End()) {
        mSkippedStringOffset = r.SkippedOffset();
        return;
    }

    mSkippedStringOffset = aOffset - r.NextDelta();
}

void
gfxSkipCharsIterator::SetSkippedOffset(uint32_t aOffset)
{
    NS_ASSERTION((mSkipChars->mRanges.IsEmpty() &&
                  aOffset <= mSkipChars->mCharCount) ||
                 (aOffset <= mSkipChars->LastRange().SkippedOffset() +
                                 mSkipChars->mCharCount -
                                 mSkipChars->LastRange().End()),
                 "Invalid skipped offset");
    mSkippedStringOffset = aOffset;

    uint32_t rangeCount = mSkipChars->mRanges.Length();
    if (rangeCount == 0) {
        mOriginalStringOffset = aOffset;
        return;
    }

    uint32_t lo = 0, hi = rangeCount;
    const gfxSkipChars::SkippedRange* ranges = mSkipChars->mRanges.Elements();
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (aOffset < ranges[mid].SkippedOffset()) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }

    if (lo == rangeCount) {
        mCurrentRangeIndex = rangeCount - 1;
    } else if (aOffset < ranges[lo].SkippedOffset()) {
        mCurrentRangeIndex = lo - 1;
        if (mCurrentRangeIndex == -1) {
            mOriginalStringOffset = aOffset;
            return;
        }
    } else {
        mCurrentRangeIndex = lo;
    }

    const gfxSkipChars::SkippedRange& r = ranges[mCurrentRangeIndex];
    mOriginalStringOffset = r.End() + aOffset - r.SkippedOffset();
}

bool
gfxSkipCharsIterator::IsOriginalCharSkipped(int32_t* aRunLength) const
{
    if (mCurrentRangeIndex == -1) {
        // we're before the first skipped range (if any)
        if (aRunLength) {
            uint32_t end = mSkipChars->mRanges.IsEmpty() ?
                mSkipChars->mCharCount : mSkipChars->mRanges[0].Start();
            *aRunLength = end - mOriginalStringOffset;
        }
        return mSkipChars->mCharCount == uint32_t(mOriginalStringOffset);
    }

    const gfxSkipChars::SkippedRange& range =
        mSkipChars->mRanges[mCurrentRangeIndex];

    if (uint32_t(mOriginalStringOffset) < range.End()) {
        if (aRunLength) {
            *aRunLength = range.End() - mOriginalStringOffset;
        }
        return true;
    }

    if (aRunLength) {
        uint32_t end =
            uint32_t(mCurrentRangeIndex) + 1 < mSkipChars->mRanges.Length() ?
                mSkipChars->mRanges[mCurrentRangeIndex + 1].Start() :
                mSkipChars->mCharCount;
        *aRunLength = end - mOriginalStringOffset;
    }

    return mSkipChars->mCharCount == uint32_t(mOriginalStringOffset);
}
