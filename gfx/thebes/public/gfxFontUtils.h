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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   John Daggett <jdaggett@mozilla.com>
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

#ifndef GFX_FONT_UTILS_H
#define GFX_FONT_UTILS_H

#include "gfxTypes.h"

#include "prtypes.h"
#include "prcpucfg.h"

#include "nsDataHashtable.h"

#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"

/* Bug 341128 - w32api defines min/max which causes problems with <bitset> */
#ifdef __MINGW32__
#undef min
#undef max
#endif

#include <bitset>

// code from gfxWindowsFonts.h

class gfxSparseBitSet {
private:
    enum { BLOCK_SIZE = 32 };   // ==> 256 codepoints per block
    enum { BLOCK_SIZE_BITS = BLOCK_SIZE * 8 };
    enum { BLOCK_INDEX_SHIFT = 8 };

    struct Block {
        Block(const Block& aBlock) { memcpy(mBits, aBlock.mBits, sizeof(mBits)); }
        Block(unsigned char memsetValue = 0) { memset(mBits, memsetValue, BLOCK_SIZE); }
        PRUint8 mBits[BLOCK_SIZE];
    };

public:
    gfxSparseBitSet() { }
    gfxSparseBitSet(const gfxSparseBitSet& aBitset) {
        PRUint32 len = aBitset.mBlocks.Length();
        mBlocks.AppendElements(len);
        for (PRUint32 i = 0; i < len; ++i) {
            Block *block = aBitset.mBlocks[i];
            if (block)
                mBlocks[i] = new Block(*block);
        }
    }
    PRBool test(PRUint32 aIndex) {
        PRUint32 blockIndex = aIndex/BLOCK_SIZE_BITS;
        if (blockIndex >= mBlocks.Length())
            return PR_FALSE;
        Block *block = mBlocks[blockIndex];
        if (!block)
            return PR_FALSE;
        return ((block->mBits[(aIndex>>3) & (BLOCK_SIZE - 1)]) & (1 << (aIndex & 0x7))) != 0;
    }

    PRBool TestRange(PRUint32 aStart, PRUint32 aEnd) {
        PRUint32 startBlock, endBlock, blockLen;
        
        // start point is beyond the end of the block array? return false immediately
        startBlock = aStart >> BLOCK_INDEX_SHIFT;
        blockLen = mBlocks.Length();
        if (startBlock >= blockLen) return PR_FALSE;
        
        // check for blocks in range, if none, return false
        PRUint32 blockIndex;
        PRBool hasBlocksInRange = PR_FALSE;

        endBlock = aEnd >> BLOCK_INDEX_SHIFT;
        blockIndex = startBlock;
        for (blockIndex = startBlock; blockIndex <= endBlock; blockIndex++) {
            if (blockIndex < blockLen && mBlocks[blockIndex])
                hasBlocksInRange = PR_TRUE;
        }
        if (!hasBlocksInRange) return PR_FALSE;

        Block *block;
        PRUint32 i, start, end;
        
        // first block, check bits
        if ((block = mBlocks[startBlock])) {
            start = aStart;
            end = PR_MIN(aEnd, ((startBlock+1) << BLOCK_INDEX_SHIFT) - 1);
            for (i = start; i <= end; i++) {
                if ((block->mBits[(i>>3) & (BLOCK_SIZE - 1)]) & (1 << (i & 0x7)))
                    return PR_TRUE;
            }
        }
        if (endBlock == startBlock) return PR_FALSE;

        // [2..n-1] blocks check bytes
        for (blockIndex = startBlock + 1; blockIndex < endBlock; blockIndex++) {
            PRUint32 index;
            
            if (blockIndex >= blockLen || !(block = mBlocks[blockIndex])) continue;
            for (index = 0; index < BLOCK_SIZE; index++) {
                if (block->mBits[index]) 
                    return PR_TRUE;
            }
        }
        
        // last block, check bits
        if (endBlock < blockLen && (block = mBlocks[endBlock])) {
            start = endBlock << BLOCK_INDEX_SHIFT;
            end = aEnd;
            for (i = start; i <= end; i++) {
                if ((block->mBits[(i>>3) & (BLOCK_SIZE - 1)]) & (1 << (i & 0x7)))
                    return PR_TRUE;
            }
        }
        
        return PR_FALSE;
    }
    
    void set(PRUint32 aIndex) {
        PRUint32 blockIndex = aIndex/BLOCK_SIZE_BITS;
        if (blockIndex >= mBlocks.Length()) {
            nsAutoPtr<Block> *blocks = mBlocks.AppendElements(blockIndex + 1 - mBlocks.Length());
            if (NS_UNLIKELY(!blocks)) // OOM
                return;
        }
        Block *block = mBlocks[blockIndex];
        if (!block) {
            block = new Block;
            if (NS_UNLIKELY(!block)) // OOM
                return;
            mBlocks[blockIndex] = block;
        }
        block->mBits[(aIndex>>3) & (BLOCK_SIZE - 1)] |= 1 << (aIndex & 0x7);
    }

    void SetRange(PRUint32 aStart, PRUint32 aEnd) {
        const PRUint32 startIndex = aStart/BLOCK_SIZE_BITS;
        const PRUint32 endIndex = aEnd/BLOCK_SIZE_BITS;

        if (endIndex >= mBlocks.Length()) {
            PRUint32 numNewBlocks = endIndex + 1 - mBlocks.Length();
            nsAutoPtr<Block> *blocks = mBlocks.AppendElements(numNewBlocks);
            if (NS_UNLIKELY(!blocks)) // OOM
                return;
        }

        for (PRUint32 i = startIndex; i <= endIndex; ++i) {
            const PRUint32 blockFirstBit = i * BLOCK_SIZE_BITS;
            const PRUint32 blockLastBit = blockFirstBit + BLOCK_SIZE_BITS - 1;

            Block *block = mBlocks[i];
            if (!block) {
                PRBool fullBlock = PR_FALSE;
                if (aStart <= blockFirstBit && aEnd >= blockLastBit)
                    fullBlock = PR_TRUE;

                block = new Block(fullBlock ? 0xFF : 0);

                if (NS_UNLIKELY(!block)) // OOM
                    return;
                mBlocks[i] = block;

                if (fullBlock)
                    continue;
            }

            const PRUint32 start = aStart > blockFirstBit ? aStart - blockFirstBit : 0;
            const PRUint32 end = PR_MIN(aEnd - blockFirstBit, BLOCK_SIZE_BITS - 1);

            for (PRUint32 bit = start; bit <= end; ++bit) {
                block->mBits[bit>>3] |= 1 << (bit & 0x7);
            }
        }
    }

    void clear(PRUint32 aIndex) {
        PRUint32 blockIndex = aIndex/BLOCK_SIZE_BITS;
        if (blockIndex >= mBlocks.Length()) {
            nsAutoPtr<Block> *blocks = mBlocks.AppendElements(blockIndex + 1 - mBlocks.Length());
            if (NS_UNLIKELY(!blocks)) // OOM
                return;
        }
        Block *block = mBlocks[blockIndex];
        if (!block) {
            block = new Block;
            if (NS_UNLIKELY(!block)) // OOM
                return;
            mBlocks[blockIndex] = block;
        }
        block->mBits[(aIndex>>3) & (BLOCK_SIZE - 1)] &= ~(1 << (aIndex & 0x7));
    }

    void ClearRange(PRUint32 aStart, PRUint32 aEnd) {
        const PRUint32 startIndex = aStart/BLOCK_SIZE_BITS;
        const PRUint32 endIndex = aEnd/BLOCK_SIZE_BITS;

        if (endIndex >= mBlocks.Length()) {
            PRUint32 numNewBlocks = endIndex + 1 - mBlocks.Length();
            nsAutoPtr<Block> *blocks = mBlocks.AppendElements(numNewBlocks);
            if (NS_UNLIKELY(!blocks)) // OOM
                return;
        }

        for (PRUint32 i = startIndex; i <= endIndex; ++i) {
            const PRUint32 blockFirstBit = i * BLOCK_SIZE_BITS;
            const PRUint32 blockLastBit = blockFirstBit + BLOCK_SIZE_BITS - 1;

            Block *block = mBlocks[i];
            if (!block) {
                PRBool fullBlock = PR_FALSE;
                if (aStart <= blockFirstBit && aEnd >= blockLastBit)
                    fullBlock = PR_TRUE;

                block = new Block(fullBlock ? 0xFF : 0);

                if (NS_UNLIKELY(!block)) // OOM
                    return;
                mBlocks[i] = block;

                if (fullBlock)
                    continue;
            }

            const PRUint32 start = aStart > blockFirstBit ? aStart - blockFirstBit : 0;
            const PRUint32 end = PR_MIN(aEnd - blockFirstBit, BLOCK_SIZE_BITS - 1);

            for (PRUint32 bit = start; bit <= end; ++bit) {
                block->mBits[bit>>3] &= ~(1 << (bit & 0x7));
            }
        }
    }

    PRUint32 GetSize() {
        PRUint32 size = 0;
        for (PRUint32 i = 0; i < mBlocks.Length(); i++) {
            if (mBlocks[i])
                size += sizeof(Block);
            size += sizeof(nsAutoPtr<Block>);
        }
        return size;
    }

    // clear out all blocks in the array
    void reset() {
        PRUint32 i;
        for (i = 0; i < mBlocks.Length(); i++)
            mBlocks[i] = nsnull;    
    }
    
    nsTArray< nsAutoPtr<Block> > mBlocks;
};

class THEBES_API gfxFontUtils {

public:

    // for reading big-endian font data on either big or little-endian platforms
    
    static inline PRUint16
    ReadShortAt(const PRUint8 *aBuf, PRUint32 aIndex)
    {
        return (aBuf[aIndex] << 8) | aBuf[aIndex + 1];
    }
    
    static inline PRUint16
    ReadShortAt16(const PRUint16 *aBuf, PRUint32 aIndex)
    {
        const PRUint8 *buf = (PRUint8*) aBuf;
        PRUint32 index = aIndex << 1;
        return (buf[index] << 8) | buf[index+1];
    }
    
    static inline PRUint32
    ReadLongAt(const PRUint8 *aBuf, PRUint32 aIndex)
    {
        return ((aBuf[aIndex] << 24) | (aBuf[aIndex + 1] << 16) | 
                (aBuf[aIndex + 2] << 8) | (aBuf[aIndex + 3]));
    }
    
    static nsresult
    ReadCMAPTableFormat12(PRUint8 *aBuf, PRUint32 aLength, 
                          gfxSparseBitSet& aCharacterMap);
    
    static nsresult 
    ReadCMAPTableFormat4(PRUint8 *aBuf, PRUint32 aLength, 
                         gfxSparseBitSet& aCharacterMap);

    static nsresult
    ReadCMAP(PRUint8 *aBuf, PRUint32 aBufLength, gfxSparseBitSet& aCharacterMap,
             PRPackedBool& aUnicodeFont, PRPackedBool& aSymbolFont);
      
#ifdef XP_WIN
    // given a TrueType/OpenType data file, produce a EOT-format header
    // for use with Windows T2Embed API AddFontResource type API's
    // effectively hide existing fonts with matching names aHeaderLen is
    // the size of the header buffer on input, the actual size of the
    // EOT header on output aIsCFF returns whether the font has PS style
    // glyphs or not (as opposed to TrueType glyphs)
    static nsresult
    MakeEOTHeader(const PRUint8 *aFontData, PRUint32 aFontDataLength,
                            nsTArray<PRUint8> *aHeader, PRBool *aIsCFF);
#endif

    // checks for valid SFNT table structure, returns true if valid
    // does *not* guarantee that all font data is valid
    static PRBool
    ValidateSFNTHeaders(const PRUint8 *aFontData, PRUint32 aFontDataLength);
    
    static inline bool IsJoiner(PRUint32 ch) {
        return (ch == 0x200C ||
                ch == 0x200D ||
                ch == 0x2060);
    }

    static inline bool IsInvalid(PRUint32 ch) {
        return (ch == 0xFFFD);
    }

    static PRUint8 CharRangeBit(PRUint32 ch);
    
    // for a given font list pref name, set up a list of font names
    static void GetPrefsFontList(const char *aPrefName, 
                                 nsTArray<nsString>& aFontList);

};

// helper class for loading in font info spaced out at regular intervals

class gfxFontInfoLoader {
public:

    // state transitions:
    //   initial ---StartLoader with delay---> timer on delay
    //   initial ---StartLoader without delay---> timer on interval
    //   timer on delay ---LoaderTimerFire---> timer on interval
    //   timer on delay ---CancelLoader---> timer off
    //   timer on interval ---CancelLoader---> timer off
    //   timer off ---StartLoader with delay---> timer on delay
    //   timer off ---StartLoader without delay---> timer on interval
    typedef enum {
        stateInitial,
        stateTimerOnDelay,
        stateTimerOnInterval,
        stateTimerOff
    } TimerState;

    gfxFontInfoLoader() :
        mInterval(0), mState(stateInitial)
    {
    }

    virtual ~gfxFontInfoLoader() {}

    // start timer with an initial delay, then call Run method at regular intervals
    void StartLoader(PRUint32 aDelay, PRUint32 aInterval) {
        mInterval = aInterval;

        // sanity check
        if (mState != stateInitial && mState != stateTimerOff)
            CancelLoader();

        // set up timer
        if (!mTimer) {
            mTimer = do_CreateInstance("@mozilla.org/timer;1");
            if (!mTimer) {
                NS_WARNING("Failure to create font info loader timer");
                return;
            }
        }

        // need an initial delay?
        PRUint32 timerInterval;

        if (aDelay) {
            mState = stateTimerOnDelay;
            timerInterval = aDelay;
        } else {
            mState = stateTimerOnInterval;
            timerInterval = mInterval;
        }

        InitLoader();

        // start timer
        mTimer->InitWithFuncCallback(LoaderTimerCallback, this, aDelay, 
                                     nsITimer::TYPE_REPEATING_SLACK);
    }

    // cancel the timer and cleanup
    void CancelLoader() {
        if (mState == stateInitial)
            return;
        mState = stateTimerOff;
        if (mTimer) {
            mTimer->Cancel();
        }
        FinishLoader();
    }

protected:

    // Init - initialization at start time after initial delay
    virtual void InitLoader() = 0;

    // Run - called at intervals, return true to indicate done
    virtual PRBool RunLoader() = 0;

    // Finish - cleanup after done
    virtual void FinishLoader() = 0;

    static void LoaderTimerCallback(nsITimer *aTimer, void *aThis) {
        gfxFontInfoLoader *loader = (gfxFontInfoLoader*) aThis;
        loader->LoaderTimerFire();
    }

    // start the timer, interval callbacks
    void LoaderTimerFire() {
        if (mState == stateTimerOnDelay) {
            mState = stateTimerOnInterval;
            mTimer->SetDelay(mInterval);
        }

        PRBool done = RunLoader();
        if (done) {
            CancelLoader();
        }
    }

    nsCOMPtr<nsITimer> mTimer;
    PRUint32 mInterval;
    TimerState mState;
};

#endif /* GFX_FONT_UTILS_H */
