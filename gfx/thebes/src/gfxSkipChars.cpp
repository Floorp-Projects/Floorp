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

#include "gfxSkipChars.h"

#include <stdlib.h>

#define SHORTCUT_FREQUENCY 256

// Even numbered list entries are "keep" entries
static PRBool
IsKeepEntry(PRUint32 aEntry)
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
  
    PRUint32 i;
    PRUint32 nextShortcutIndex = 0;
    PRUint32 originalCharOffset = 0;
    PRUint32 skippedCharOffset = 0;
    for (i = 0; i < mListLength; ++i) {
        PRUint8 len = mList[i];
    
        while (originalCharOffset + len > (nextShortcutIndex + 1)*SHORTCUT_FREQUENCY) {
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
gfxSkipCharsIterator::SetOffsets(PRUint32 aOffset, PRBool aInOriginalString)
{
    if (mSkipChars->mListLength == 0) {
        // Special case: all chars kept, original and stripped strings are equal
        if (aOffset < 0) {
            aOffset = 0;
        } else if (aOffset > mSkipChars->mCharCount) {
            aOffset = mSkipChars->mCharCount;
        }
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
        abs(PRInt32(aOffset) - PRInt32(mListPrefixCharCount)) > SHORTCUT_FREQUENCY) {
        // Take a shortcut. This makes SetOffsets(..., PR_TRUE) O(1) by bounding
        // the iterations in the loop below to at most SHORTCUT_FREQUENCY iterations
        PRUint32 shortcutIndex = aOffset/SHORTCUT_FREQUENCY;
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
  
    PRInt32 currentRunLength = mSkipChars->mList[mListPrefixLength];
    for (;;) {
        // See if aOffset is in the string segment described by
        // mSkipChars->mList[mListPrefixLength]
        PRUint32 segmentOffset = aInOriginalString ? mListPrefixCharCount : mListPrefixKeepCharCount;
        if ((aInOriginalString || IsKeepEntry(mListPrefixLength)) &&
            aOffset >= segmentOffset && aOffset < segmentOffset + currentRunLength) {
            PRInt32 offsetInSegment = aOffset - segmentOffset;
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

PRBool
gfxSkipCharsIterator::IsOriginalCharSkipped(PRInt32* aRunLength) const
{
    if (mSkipChars->mListLength == 0) {
        if (aRunLength) {
            *aRunLength = mSkipChars->mCharCount - mOriginalStringOffset;
        }
        return mSkipChars->mCharCount == PRUint32(mOriginalStringOffset);
    }
  
    // figure out which segment we're in
    PRUint32 currentRunLength = mSkipChars->mList[mListPrefixLength];
    NS_ASSERTION(PRUint32(mOriginalStringOffset) >= mListPrefixCharCount,
                 "Invariant violation");
    PRUint32 offsetIntoCurrentRun =
      PRUint32(mOriginalStringOffset) - mListPrefixCharCount;
    if (mListPrefixLength >= mSkipChars->mListLength - 1 &&
        offsetIntoCurrentRun >= currentRunLength) {
        NS_ASSERTION(mListPrefixLength == mSkipChars->mListLength - 1 &&
                     offsetIntoCurrentRun == currentRunLength,
                     "Overran end of string");
        // We're at the end of the string
        if (aRunLength) {
            *aRunLength = 0;
        }
        return PR_TRUE;
    }
  
    PRBool isSkipped = !IsKeepEntry(mListPrefixLength);
    if (aRunLength) {
        // Long runs of all-skipped or all-kept characters will be encoded as
        // sequences of 255, 0, 255, 0 etc. Compute the maximum run length by skipping
        // over zero entries.
        PRUint32 runLength = currentRunLength - offsetIntoCurrentRun;
        for (PRUint32 i = mListPrefixLength + 2; i < mSkipChars->mListLength; i += 2) {
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
    PRUint32 charCount = mRunCharCount;
    for (;;) {
        PRUint32 chars = PR_MIN(255, charCount);
        if (!mBuffer.AppendElement(chars)) {
            mInErrorState = PR_TRUE;
            return;
        }
        charCount -= chars;
        if (charCount == 0)
            break;
        if (!mBuffer.AppendElement(0)) {
            mInErrorState = PR_TRUE;
            return;
        }
    }
  
    NS_ASSERTION(mCharCount + mRunCharCount >= mCharCount,
                 "String length overflow");
    mCharCount += mRunCharCount;
    mRunCharCount = 0;
    mRunSkipped = !mRunSkipped;
}
