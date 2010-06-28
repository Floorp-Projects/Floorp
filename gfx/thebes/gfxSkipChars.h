/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_SKIP_CHARS_H
#define GFX_SKIP_CHARS_H

#include "prtypes.h"
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
class THEBES_API gfxSkipCharsBuilder {
public:
    gfxSkipCharsBuilder() :
        mCharCount(0), mRunCharCount(0), mRunSkipped(PR_FALSE), mInErrorState(PR_FALSE)
    {}
  
    void SkipChars(PRUint32 aChars) {
        DoChars(aChars, PR_TRUE);
    }
    void KeepChars(PRUint32 aChars) {
        DoChars(aChars, PR_FALSE);
    }
    void SkipChar() {
        SkipChars(1);
    }
    void KeepChar() {
        KeepChars(1);
    }
    void DoChars(PRUint32 aChars, PRBool aSkipped) {
        if (aSkipped != mRunSkipped && aChars > 0) {
            FlushRun();
        }
        NS_ASSERTION(mRunCharCount + aChars > mRunCharCount,
                     "Character count overflow");
        mRunCharCount += aChars;
    }

    PRBool IsOK() { return !mInErrorState; }

    PRUint32 GetCharCount() { return mCharCount + mRunCharCount; }
    PRBool GetAllCharsKept() { return mBuffer.Length() == 0; }

    friend class gfxSkipChars;

private:
    typedef nsAutoTArray<PRUint8,256> Buffer;

    /**
     * Moves mRunCharCount/mRunSkipped to the buffer (updating mCharCount),
     * sets mRunCharCount to zero and toggles mRunSkipped.
     */
    void FlushRun();
  
    Buffer       mBuffer;
    PRUint32     mCharCount;
    PRUint32     mRunCharCount;
    PRPackedBool mRunSkipped; // == mBuffer.Length()&1
    PRPackedBool mInErrorState;
};

/**
 * The gfxSkipChars list is represented as a list of bytes of the form
 * [chars to keep, chars to skip, chars to keep, chars to skip, ...]
 * In the special case where all chars are to be kept, the list is length
 * zero.
 * 
 * A freshly-created gfxSkipChars means "all chars kept".
 */
class THEBES_API gfxSkipChars {
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
            mList = nsnull;
            mListLength = 0;
        } else {
            aSkipCharsBuilder->FlushRun();
            mCharCount = aSkipCharsBuilder->mCharCount;
            mList = new PRUint8[aSkipCharsBuilder->mBuffer.Length()];
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
        aSkipCharsBuilder->mRunSkipped = PR_FALSE;
        BuildShortcuts();
    }
  
    void SetAllKeep(PRUint32 aLength) {
        mCharCount = aLength;
        mList = nsnull;
        mListLength = 0;
    }
  
    PRInt32 GetOriginalCharCount() const { return mCharCount; }

    friend class gfxSkipCharsIterator;

private:
    struct Shortcut {
        PRUint32 mListPrefixLength;
        PRUint32 mListPrefixCharCount;
        PRUint32 mListPrefixKeepCharCount;
    
        Shortcut() {}
        Shortcut(PRUint32 aListPrefixLength, PRUint32 aListPrefixCharCount,
                 PRUint32 aListPrefixKeepCharCount) :
            mListPrefixLength(aListPrefixLength),
            mListPrefixCharCount(aListPrefixCharCount),
            mListPrefixKeepCharCount(aListPrefixKeepCharCount) {}
    };
  
    void BuildShortcuts();

    nsAutoArrayPtr<PRUint8>  mList;
    nsAutoArrayPtr<Shortcut> mShortcuts;
    PRUint32                 mListLength;
    PRUint32                 mCharCount;
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
class THEBES_API gfxSkipCharsIterator {
public:
    /**
     * @param aOriginalStringToSkipCharsOffset add this to all incoming and
     * outgoing original string offsets
     */
    gfxSkipCharsIterator(const gfxSkipChars& aSkipChars,
                         PRInt32 aOriginalStringToSkipCharsOffset,
                         PRInt32 aOriginalStringOffset)
        : mSkipChars(&aSkipChars),
          mOriginalStringToSkipCharsOffset(aOriginalStringToSkipCharsOffset),
          mListPrefixLength(0), mListPrefixCharCount(0), mListPrefixKeepCharCount(0) {
          SetOriginalOffset(aOriginalStringOffset);
    }

    gfxSkipCharsIterator(const gfxSkipChars& aSkipChars,
                         PRInt32 aOriginalStringToSkipCharsOffset = 0)
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
    gfxSkipCharsIterator() : mSkipChars(nsnull) {}

    /**
     * Return true if this iterator is properly initialized and usable.
     */  
    PRBool IsInitialized() { return mSkipChars != nsnull; }

    /**
     * Set the iterator to aOriginalStringOffset in the original string.
     * This can efficiently move forward or backward from the current position.
     * aOriginalStringOffset is clamped to [0,originalStringLength].
     */
    void SetOriginalOffset(PRInt32 aOriginalStringOffset) {
        SetOffsets(aOriginalStringOffset + mOriginalStringToSkipCharsOffset, PR_TRUE);
    }
    
    /**
     * Set the iterator to aSkippedStringOffset in the skipped string.
     * This can efficiently move forward or backward from the current position.
     * aSkippedStringOffset is clamped to [0,skippedStringLength].
     */
    void SetSkippedOffset(PRUint32 aSkippedStringOffset) {
        SetOffsets(aSkippedStringOffset, PR_FALSE);
    }
    
    PRUint32 ConvertOriginalToSkipped(PRInt32 aOriginalStringOffset) {
        SetOriginalOffset(aOriginalStringOffset);
        return GetSkippedOffset();
    }
    PRUint32 ConvertSkippedToOriginal(PRInt32 aSkippedStringOffset) {
        SetSkippedOffset(aSkippedStringOffset);
        return GetOriginalOffset();
    }
  
    /**
     * Test if the character at the current position in the original string
     * is skipped or not. If aRunLength is non-null, then *aRunLength is set
     * to a number of characters all of which are either skipped or not, starting
     * at this character. When the current position is at the end of the original
     * string, we return PR_TRUE and *aRunLength is set to zero.
     */
    PRBool IsOriginalCharSkipped(PRInt32* aRunLength = nsnull) const;
    
    void AdvanceOriginal(PRInt32 aDelta) {
        SetOffsets(mOriginalStringOffset + aDelta, PR_TRUE);
    }
    void AdvanceSkipped(PRInt32 aDelta) {
        SetOffsets(mSkippedStringOffset + aDelta, PR_FALSE);
    }
  
    /**
     * @return the offset within the original string
     */
    PRInt32 GetOriginalOffset() const {
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
    PRUint32 GetSkippedOffset() const { return mSkippedStringOffset; }

    PRInt32 GetOriginalEnd() const {
        return mSkipChars->GetOriginalCharCount() -
            mOriginalStringToSkipCharsOffset;
    }

private:
    void SetOffsets(PRUint32 aOffset, PRBool aInOriginalString);
  
    const gfxSkipChars* mSkipChars;
    PRInt32 mOriginalStringOffset;
    PRUint32 mSkippedStringOffset;

    // This offset is added to map from "skipped+unskipped characters in
    // the original DOM string" character space to "skipped+unskipped
    // characters in the textrun's gfxSkipChars" character space
    PRInt32 mOriginalStringToSkipCharsOffset;

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
    PRUint32 mListPrefixLength;
    PRUint32 mListPrefixCharCount;
    PRUint32 mListPrefixKeepCharCount;
};

#endif /*GFX_SKIP_CHARS_H*/
