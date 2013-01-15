/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxSkipChars.h"

#include <stdlib.h>
#include <algorithm>

#define SHORTCUT_FREQUENCY 256

// Even numbered list entries are "keep" entries
static bool
IsKeepEntry(uint32_t aEntry)
{
    return !(aEntry & 1);
}

void
gfxSkipChars::BuildShortcuts()
{
    if (!mList || mCharCount < SHORTCUT_FREQUENCY)
        return;
  
    mShortcuts = new Shortcut[mCharCount/SHORTCUT_FREQUENCY];
    if (!mShortcuts)
        return;
  
    uint32_t i;
    uint32_t nextShortcutIndex = 0;
    uint32_t originalCharOffset = 0;
    uint32_t skippedCharOffset = 0;
    for (i = 0; i < mListLength; ++i) {
        uint8_t len = mList[i];
    
        // We use >= here to ensure that when mCharCount is a multiple of
        // SHORTCUT_FREQUENCY, we fill in the final shortcut with a reference
        // to the last element of mList. This means that in general when a list
        // element ends on an offset that's a multiple of SHORTCUT_FREQUENCY,
        // that list element is the shortcut for that offset, which is
        // slightly suboptimal (the *next* element is the one we really want),
        // but it's all correct and simpler this way.
        while (originalCharOffset + len >= (nextShortcutIndex + 1)*SHORTCUT_FREQUENCY) {
            mShortcuts[nextShortcutIndex] =
                Shortcut(i, originalCharOffset, skippedCharOffset);
            ++nextShortcutIndex;
        }
    
        originalCharOffset += len;
        if (IsKeepEntry(i)) {
            skippedCharOffset += len;
        }
    }
}

void
gfxSkipCharsIterator::SetOffsets(uint32_t aOffset, bool aInOriginalString)
{
    NS_ASSERTION(aOffset <= mSkipChars->mCharCount,
                 "Invalid offset");

    if (mSkipChars->mListLength == 0) {
        mOriginalStringOffset = mSkippedStringOffset = aOffset;
        return;
    }
  
    if (aOffset == 0) {
        // Start from the beginning of the string.
        mSkippedStringOffset = 0;
        mOriginalStringOffset = 0;
        mListPrefixLength = 0;
        mListPrefixKeepCharCount = 0;
        mListPrefixCharCount = 0;
        if (aInOriginalString) {
            // Nothing more to do!
            return;
        }
    }
  
    if (aInOriginalString && mSkipChars->mShortcuts &&
        abs(int32_t(aOffset) - int32_t(mListPrefixCharCount)) > SHORTCUT_FREQUENCY) {
        // Take a shortcut. This makes SetOffsets(..., true) O(1) by bounding
        // the iterations in the loop below to at most SHORTCUT_FREQUENCY iterations
        uint32_t shortcutIndex = aOffset/SHORTCUT_FREQUENCY;
        if (shortcutIndex == 0) {
            mListPrefixLength = 0;
            mListPrefixKeepCharCount = 0;
            mListPrefixCharCount = 0;
        } else {
            const gfxSkipChars::Shortcut& shortcut = mSkipChars->mShortcuts[shortcutIndex - 1];
            mListPrefixLength = shortcut.mListPrefixLength;
            mListPrefixKeepCharCount = shortcut.mListPrefixKeepCharCount;
            mListPrefixCharCount = shortcut.mListPrefixCharCount;
        }
    }
  
    int32_t currentRunLength = mSkipChars->mList[mListPrefixLength];
    for (;;) {
        // See if aOffset is in the string segment described by
        // mSkipChars->mList[mListPrefixLength]
        uint32_t segmentOffset = aInOriginalString ? mListPrefixCharCount : mListPrefixKeepCharCount;
        if ((aInOriginalString || IsKeepEntry(mListPrefixLength)) &&
            aOffset >= segmentOffset && aOffset < segmentOffset + currentRunLength) {
            int32_t offsetInSegment = aOffset - segmentOffset;
            mOriginalStringOffset = mListPrefixCharCount + offsetInSegment;
            mSkippedStringOffset = mListPrefixKeepCharCount;
            if (IsKeepEntry(mListPrefixLength)) {
                mSkippedStringOffset += offsetInSegment;
            }
            return;
        }
        
        if (aOffset < segmentOffset) {
            // We need to move backwards
            if (mListPrefixLength <= 0) {
                // nowhere to go backwards
                mOriginalStringOffset = mSkippedStringOffset = 0;
                return;
            }
            // Go backwards one segment and restore invariants
            --mListPrefixLength;
            currentRunLength = mSkipChars->mList[mListPrefixLength];
            mListPrefixCharCount -= currentRunLength;
            if (IsKeepEntry(mListPrefixLength)) {
                mListPrefixKeepCharCount -= currentRunLength;
            }
        } else {
            // We need to move forwards
            if (mListPrefixLength >= mSkipChars->mListLength - 1) {
                // nowhere to go forwards
                mOriginalStringOffset = mListPrefixCharCount + currentRunLength;
                mSkippedStringOffset = mListPrefixKeepCharCount;
                if (IsKeepEntry(mListPrefixLength)) {
                    mSkippedStringOffset += currentRunLength;
                }
                return;
            }
            // Go forwards one segment and restore invariants
            mListPrefixCharCount += currentRunLength;
            if (IsKeepEntry(mListPrefixLength)) {
                mListPrefixKeepCharCount += currentRunLength;
            }
            ++mListPrefixLength;
            currentRunLength = mSkipChars->mList[mListPrefixLength];
        }
    }
}

bool
gfxSkipCharsIterator::IsOriginalCharSkipped(int32_t* aRunLength) const
{
    if (mSkipChars->mListLength == 0) {
        if (aRunLength) {
            *aRunLength = mSkipChars->mCharCount - mOriginalStringOffset;
        }
        return mSkipChars->mCharCount == uint32_t(mOriginalStringOffset);
    }
  
    uint32_t listPrefixLength = mListPrefixLength;
    // figure out which segment we're in
    uint32_t currentRunLength = mSkipChars->mList[listPrefixLength];
    // Zero-length list entries are possible. Advance until mListPrefixLength
    // is pointing to a run with real characters (or we're at the end of the
    // string).
    while (currentRunLength == 0 && listPrefixLength < mSkipChars->mListLength - 1) {
        ++listPrefixLength;
        // This does not break the iterator's invariant because no skipped
        // or kept characters are being added
        currentRunLength = mSkipChars->mList[listPrefixLength];
    }
    NS_ASSERTION(uint32_t(mOriginalStringOffset) >= mListPrefixCharCount,
                 "Invariant violation");
    uint32_t offsetIntoCurrentRun =
      uint32_t(mOriginalStringOffset) - mListPrefixCharCount;
    if (listPrefixLength >= mSkipChars->mListLength - 1 &&
        offsetIntoCurrentRun >= currentRunLength) {
        NS_ASSERTION(listPrefixLength == mSkipChars->mListLength - 1 &&
                     offsetIntoCurrentRun == currentRunLength,
                     "Overran end of string");
        // We're at the end of the string
        if (aRunLength) {
            *aRunLength = 0;
        }
        return true;
    }
  
    bool isSkipped = !IsKeepEntry(listPrefixLength);
    if (aRunLength) {
        // Long runs of all-skipped or all-kept characters will be encoded as
        // sequences of 255, 0, 255, 0 etc. Compute the maximum run length by skipping
        // over zero entries.
        uint32_t runLength = currentRunLength - offsetIntoCurrentRun;
        for (uint32_t i = listPrefixLength + 2; i < mSkipChars->mListLength; i += 2) {
            if (mSkipChars->mList[i - 1] != 0)
                break;
            runLength += mSkipChars->mList[i];
        }
        *aRunLength = runLength;
    }
    return isSkipped;
}

void
gfxSkipCharsBuilder::FlushRun()
{
    NS_ASSERTION((mBuffer.Length() & 1) == mRunSkipped,
                 "out of sync?");
    // Fill in buffer entries starting at mBufferLength, as many as necessary
    uint32_t charCount = mRunCharCount;
    for (;;) {
        uint32_t chars = std::min<uint32_t>(255, charCount);
        if (!mBuffer.AppendElement(chars)) {
            mInErrorState = true;
            return;
        }
        charCount -= chars;
        if (charCount == 0)
            break;
        if (!mBuffer.AppendElement(0)) {
            mInErrorState = true;
            return;
        }
    }
  
    NS_ASSERTION(mCharCount + mRunCharCount >= mCharCount,
                 "String length overflow");
    mCharCount += mRunCharCount;
    mRunCharCount = 0;
    mRunSkipped = !mRunSkipped;
}
