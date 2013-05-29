/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SKIP_CHARS_H
#define GFX_SKIP_CHARS_H

#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "gfxTypes.h"

/*
 * gfxSkipChars is a data structure representing a list of characters that
 * have been skipped. The initial string is called the "original string"
 * and after skipping some characters, the result is called the "skipped string".
 * gfxSkipChars provides efficient ways to translate between offsets in the
 * original string and the skipped string. It is used by textrun code to keep
 * track of offsets before and after text transformations such as whitespace
 * compression and control code deletion.
 */

/**
 * gfxSkipCharsBuilder is a helper class that accumulates a list of (skip, keep)
 * commands and can eventually be used to construct a real gfxSkipChars.
 * gfxSkipCharsBuilder objects are quite large so don't keep these around.
 * On the positive side, the Skip/KeepChar(s) methods are very efficient,
 * especially when you have runs of all-kept or all-skipped characters.
 * 
 * mBuffer is an array of bytes; even numbered bytes represent characters kept,
 * odd numbered bytes represent characters skipped. After those characters
 * are accounted for, we have mRunCharCount characters which are kept or
 * skipped depending on the value of mRunSkipped.
 * 
 * mCharCount is the sum of counts of all skipped and kept characters, i.e.,
 * the length of the original string.
 */
class gfxSkipCharsBuilder {
public:
    gfxSkipCharsBuilder() :
        mCharCount(0), mRunCharCount(0), mRunSkipped(false), mInErrorState(false)
    {}
  
    void SkipChars(uint32_t aChars) {
        DoChars(aChars, true);
    }
    void KeepChars(uint32_t aChars) {
        DoChars(aChars, false);
    }
    void SkipChar() {
        SkipChars(1);
    }
    void KeepChar() {
        KeepChars(1);
    }
    void DoChars(uint32_t aChars, bool aSkipped) {
        if (aSkipped != mRunSkipped && aChars > 0) {
            FlushRun();
        }
        NS_ASSERTION(mRunCharCount + aChars > mRunCharCount,
                     "Character count overflow");
        mRunCharCount += aChars;
    }

    bool IsOK() { return !mInErrorState; }

    uint32_t GetCharCount() { return mCharCount + mRunCharCount; }
    bool GetAllCharsKept() { return mBuffer.Length() == 0; }

    friend class gfxSkipChars;

private:
    typedef AutoFallibleTArray<uint8_t,256> Buffer;

    /**
     * Moves mRunCharCount/mRunSkipped to the buffer (updating mCharCount),
     * sets mRunCharCount to zero and toggles mRunSkipped.
     */
    void FlushRun();
  
    Buffer       mBuffer;
    uint32_t     mCharCount;
    uint32_t     mRunCharCount;
    bool mRunSkipped; // == mBuffer.Length()&1
    bool mInErrorState;
};

/**
 * The gfxSkipChars list is represented as a list of bytes of the form
 * [chars to keep, chars to skip, chars to keep, chars to skip, ...]
 * In the special case where all chars are to be kept, the list is length
 * zero.
 * 
 * A freshly-created gfxSkipChars means "all chars kept".
 */
class gfxSkipChars {
public:
    gfxSkipChars() : mListLength(0), mCharCount(0) {}
  
    void TakeFrom(gfxSkipChars* aSkipChars) {
        mList = aSkipChars->mList.forget();
        mListLength = aSkipChars->mListLength;
        mCharCount = aSkipChars->mCharCount;
        aSkipChars->mCharCount = 0;
        aSkipChars->mListLength = 0;
        BuildShortcuts();
    }
  
    void TakeFrom(gfxSkipCharsBuilder* aSkipCharsBuilder) {
        if (!aSkipCharsBuilder->mBuffer.Length()) {
            NS_ASSERTION(!aSkipCharsBuilder->mRunSkipped, "out of sync");
            // all characters kept
            mCharCount = aSkipCharsBuilder->mRunCharCount;
            mList = nullptr;
            mListLength = 0;
        } else {
            aSkipCharsBuilder->FlushRun();
            mCharCount = aSkipCharsBuilder->mCharCount;
            mList = new uint8_t[aSkipCharsBuilder->mBuffer.Length()];
            if (!mList) {
                mListLength = 0;
            } else {
                mListLength = aSkipCharsBuilder->mBuffer.Length();
                memcpy(mList, aSkipCharsBuilder->mBuffer.Elements(), mListLength);
            }
        }
        aSkipCharsBuilder->mBuffer.Clear();
        aSkipCharsBuilder->mCharCount = 0;
        aSkipCharsBuilder->mRunCharCount = 0;    
        aSkipCharsBuilder->mRunSkipped = false;
        BuildShortcuts();
    }
  
    void SetAllKeep(uint32_t aLength) {
        mCharCount = aLength;
        mList = nullptr;
        mListLength = 0;
    }
  
    int32_t GetOriginalCharCount() const { return mCharCount; }

    friend class gfxSkipCharsIterator;

private:
    struct Shortcut {
        uint32_t mListPrefixLength;
        uint32_t mListPrefixCharCount;
        uint32_t mListPrefixKeepCharCount;
    
        Shortcut() {}
        Shortcut(uint32_t aListPrefixLength, uint32_t aListPrefixCharCount,
                 uint32_t aListPrefixKeepCharCount) :
            mListPrefixLength(aListPrefixLength),
            mListPrefixCharCount(aListPrefixCharCount),
            mListPrefixKeepCharCount(aListPrefixKeepCharCount) {}
    };
  
    void BuildShortcuts();

    nsAutoArrayPtr<uint8_t>  mList;
    nsAutoArrayPtr<Shortcut> mShortcuts;
    uint32_t                 mListLength;
    uint32_t                 mCharCount;
};

/**
 * A gfxSkipCharsIterator represents a position in the original string. It lets you
 * map efficiently to and from positions in the string after skipped characters
 * have been removed. You can also specify an offset that is added to all
 * incoming original string offsets and subtracted from all outgoing original
 * string offsets --- useful when the gfxSkipChars corresponds to something
 * offset from the original DOM coordinates, which it often does for gfxTextRuns.
 * 
 * The current positions (in both the original and skipped strings) are
 * always constrained to be >= 0 and <= the string length. When the position
 * is equal to the string length, it is at the end of the string. The current
 * positions do not include any aOriginalStringToSkipCharsOffset.
 * 
 * When the position in the original string corresponds to a skipped character,
 * the skipped-characters offset is the offset of the next unskipped character,
 * or the skipped-characters string length if there is no next unskipped character.
 */
class gfxSkipCharsIterator {
public:
    /**
     * @param aOriginalStringToSkipCharsOffset add this to all incoming and
     * outgoing original string offsets
     */
    gfxSkipCharsIterator(const gfxSkipChars& aSkipChars,
                         int32_t aOriginalStringToSkipCharsOffset,
                         int32_t aOriginalStringOffset)
        : mSkipChars(&aSkipChars),
          mOriginalStringToSkipCharsOffset(aOriginalStringToSkipCharsOffset),
          mListPrefixLength(0), mListPrefixCharCount(0), mListPrefixKeepCharCount(0) {
          SetOriginalOffset(aOriginalStringOffset);
    }

    gfxSkipCharsIterator(const gfxSkipChars& aSkipChars,
                         int32_t aOriginalStringToSkipCharsOffset = 0)
        : mSkipChars(&aSkipChars),
          mOriginalStringOffset(0), mSkippedStringOffset(0),
          mOriginalStringToSkipCharsOffset(aOriginalStringToSkipCharsOffset),
          mListPrefixLength(0), mListPrefixCharCount(0), mListPrefixKeepCharCount(0) {
    }

    gfxSkipCharsIterator(const gfxSkipCharsIterator& aIterator)
        : mSkipChars(aIterator.mSkipChars),
          mOriginalStringOffset(aIterator.mOriginalStringOffset),
          mSkippedStringOffset(aIterator.mSkippedStringOffset),
          mOriginalStringToSkipCharsOffset(aIterator.mOriginalStringToSkipCharsOffset),
          mListPrefixLength(aIterator.mListPrefixLength),
          mListPrefixCharCount(aIterator.mListPrefixCharCount),
          mListPrefixKeepCharCount(aIterator.mListPrefixKeepCharCount)
    {}
  
    /**
     * The empty constructor creates an object that is useless until it is assigned.
     */
    gfxSkipCharsIterator() : mSkipChars(nullptr) {}

    /**
     * Return true if this iterator is properly initialized and usable.
     */  
    bool IsInitialized() { return mSkipChars != nullptr; }

    /**
     * Set the iterator to aOriginalStringOffset in the original string.
     * This can efficiently move forward or backward from the current position.
     * aOriginalStringOffset is clamped to [0,originalStringLength].
     */
    void SetOriginalOffset(int32_t aOriginalStringOffset) {
        SetOffsets(aOriginalStringOffset + mOriginalStringToSkipCharsOffset, true);
    }
    
    /**
     * Set the iterator to aSkippedStringOffset in the skipped string.
     * This can efficiently move forward or backward from the current position.
     * aSkippedStringOffset is clamped to [0,skippedStringLength].
     */
    void SetSkippedOffset(uint32_t aSkippedStringOffset) {
        SetOffsets(aSkippedStringOffset, false);
    }
    
    uint32_t ConvertOriginalToSkipped(int32_t aOriginalStringOffset) {
        SetOriginalOffset(aOriginalStringOffset);
        return GetSkippedOffset();
    }
    uint32_t ConvertSkippedToOriginal(int32_t aSkippedStringOffset) {
        SetSkippedOffset(aSkippedStringOffset);
        return GetOriginalOffset();
    }
  
    /**
     * Test if the character at the current position in the original string
     * is skipped or not. If aRunLength is non-null, then *aRunLength is set
     * to a number of characters all of which are either skipped or not, starting
     * at this character. When the current position is at the end of the original
     * string, we return true and *aRunLength is set to zero.
     */
    bool IsOriginalCharSkipped(int32_t* aRunLength = nullptr) const;
    
    void AdvanceOriginal(int32_t aDelta) {
        SetOffsets(mOriginalStringOffset + aDelta, true);
    }
    void AdvanceSkipped(int32_t aDelta) {
        SetOffsets(mSkippedStringOffset + aDelta, false);
    }
  
    /**
     * @return the offset within the original string
     */
    int32_t GetOriginalOffset() const {
        return mOriginalStringOffset - mOriginalStringToSkipCharsOffset;
    }
    /**
     * @return the offset within the skipped string corresponding to the
     * current position in the original string. If the current position
     * in the original string is a character that is skipped, then we return
     * the position corresponding to the first non-skipped character in the
     * original string after the current position, or the length of the skipped
     * string if there is no such character.
     */
    uint32_t GetSkippedOffset() const { return mSkippedStringOffset; }

    int32_t GetOriginalEnd() const {
        return mSkipChars->GetOriginalCharCount() -
            mOriginalStringToSkipCharsOffset;
    }

private:
    void SetOffsets(uint32_t aOffset, bool aInOriginalString);
  
    const gfxSkipChars* mSkipChars;
    int32_t mOriginalStringOffset;
    uint32_t mSkippedStringOffset;

    // This offset is added to map from "skipped+unskipped characters in
    // the original DOM string" character space to "skipped+unskipped
    // characters in the textrun's gfxSkipChars" character space
    int32_t mOriginalStringToSkipCharsOffset;

    /*
     * This is used to speed up cursor-style traversal. The invariant is that
     * the first mListPrefixLength bytes of mSkipChars.mList sum to
     * mListPrefixCharCount, and the even-indexed bytes in that prefix sum to
     * mListPrefixKeepCharCount.
     * Also, 0 <= mListPrefixLength < mSkipChars.mListLength, or else
     * mSkipChars.mListLength is zero.
     * Also, mListPrefixCharCount <= mOriginalStringOffset (and therefore
     * mListPrefixKeepCharCount < mSkippedStringOffset).
     */
    uint32_t mListPrefixLength;
    uint32_t mListPrefixCharCount;
    uint32_t mListPrefixKeepCharCount;
};

#endif /*GFX_SKIP_CHARS_H*/
