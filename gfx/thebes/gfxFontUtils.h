/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_UTILS_H
#define GFX_FONT_UTILS_H

#include "gfxTypes.h"
#include "gfxPlatform.h"

#include "nsAlgorithm.h"
#include "prcpucfg.h"

#include "nsDataHashtable.h"

#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/Likely.h"
#include "mozilla/Endian.h"

#include "zlib.h"
#include <algorithm>

/* Bug 341128 - w32api defines min/max which causes problems with <bitset> */
#ifdef __MINGW32__
#undef min
#undef max
#endif

typedef struct hb_blob_t hb_blob_t;

class gfxSparseBitSet {
private:
    enum { BLOCK_SIZE = 32 };   // ==> 256 codepoints per block
    enum { BLOCK_SIZE_BITS = BLOCK_SIZE * 8 };
    enum { BLOCK_INDEX_SHIFT = 8 };

    struct Block {
        Block(const Block& aBlock) { memcpy(mBits, aBlock.mBits, sizeof(mBits)); }
        Block(unsigned char memsetValue = 0) { memset(mBits, memsetValue, BLOCK_SIZE); }
        uint8_t mBits[BLOCK_SIZE];
    };

public:
    gfxSparseBitSet() { }
    gfxSparseBitSet(const gfxSparseBitSet& aBitset) {
        uint32_t len = aBitset.mBlocks.Length();
        mBlocks.AppendElements(len);
        for (uint32_t i = 0; i < len; ++i) {
            Block *block = aBitset.mBlocks[i];
            if (block)
                mBlocks[i] = new Block(*block);
        }
    }

    bool Equals(const gfxSparseBitSet *aOther) const {
        if (mBlocks.Length() != aOther->mBlocks.Length()) {
            return false;
        }
        size_t n = mBlocks.Length();
        for (size_t i = 0; i < n; ++i) {
            const Block *b1 = mBlocks[i];
            const Block *b2 = aOther->mBlocks[i];
            if (!b1 != !b2) {
                return false;
            }
            if (!b1) {
                continue;
            }
            if (memcmp(&b1->mBits, &b2->mBits, BLOCK_SIZE) != 0) {
                return false;
            }
        }
        return true;
    }

    bool test(uint32_t aIndex) const {
        NS_ASSERTION(mBlocks.DebugGetHeader(), "mHdr is null, this is bad");
        uint32_t blockIndex = aIndex/BLOCK_SIZE_BITS;
        if (blockIndex >= mBlocks.Length())
            return false;
        Block *block = mBlocks[blockIndex];
        if (!block)
            return false;
        return ((block->mBits[(aIndex>>3) & (BLOCK_SIZE - 1)]) & (1 << (aIndex & 0x7))) != 0;
    }

#if PR_LOGGING
    // dump out contents of bitmap
    void Dump(const char* aPrefix, eGfxLog aWhichLog) const;
#endif

    bool TestRange(uint32_t aStart, uint32_t aEnd) {
        uint32_t startBlock, endBlock, blockLen;
        
        // start point is beyond the end of the block array? return false immediately
        startBlock = aStart >> BLOCK_INDEX_SHIFT;
        blockLen = mBlocks.Length();
        if (startBlock >= blockLen) return false;
        
        // check for blocks in range, if none, return false
        uint32_t blockIndex;
        bool hasBlocksInRange = false;

        endBlock = aEnd >> BLOCK_INDEX_SHIFT;
        blockIndex = startBlock;
        for (blockIndex = startBlock; blockIndex <= endBlock; blockIndex++) {
            if (blockIndex < blockLen && mBlocks[blockIndex])
                hasBlocksInRange = true;
        }
        if (!hasBlocksInRange) return false;

        Block *block;
        uint32_t i, start, end;
        
        // first block, check bits
        if ((block = mBlocks[startBlock])) {
            start = aStart;
            end = std::min(aEnd, ((startBlock+1) << BLOCK_INDEX_SHIFT) - 1);
            for (i = start; i <= end; i++) {
                if ((block->mBits[(i>>3) & (BLOCK_SIZE - 1)]) & (1 << (i & 0x7)))
                    return true;
            }
        }
        if (endBlock == startBlock) return false;

        // [2..n-1] blocks check bytes
        for (blockIndex = startBlock + 1; blockIndex < endBlock; blockIndex++) {
            uint32_t index;
            
            if (blockIndex >= blockLen || !(block = mBlocks[blockIndex])) continue;
            for (index = 0; index < BLOCK_SIZE; index++) {
                if (block->mBits[index]) 
                    return true;
            }
        }
        
        // last block, check bits
        if (endBlock < blockLen && (block = mBlocks[endBlock])) {
            start = endBlock << BLOCK_INDEX_SHIFT;
            end = aEnd;
            for (i = start; i <= end; i++) {
                if ((block->mBits[(i>>3) & (BLOCK_SIZE - 1)]) & (1 << (i & 0x7)))
                    return true;
            }
        }
        
        return false;
    }
    
    void set(uint32_t aIndex) {
        uint32_t blockIndex = aIndex/BLOCK_SIZE_BITS;
        if (blockIndex >= mBlocks.Length()) {
            nsAutoPtr<Block> *blocks = mBlocks.AppendElements(blockIndex + 1 - mBlocks.Length());
            if (MOZ_UNLIKELY(!blocks)) // OOM
                return;
        }
        Block *block = mBlocks[blockIndex];
        if (!block) {
            block = new Block;
            mBlocks[blockIndex] = block;
        }
        block->mBits[(aIndex>>3) & (BLOCK_SIZE - 1)] |= 1 << (aIndex & 0x7);
    }

    void set(uint32_t aIndex, bool aValue) {
        if (aValue)
            set(aIndex);
        else
            clear(aIndex);
    }

    void SetRange(uint32_t aStart, uint32_t aEnd) {
        const uint32_t startIndex = aStart/BLOCK_SIZE_BITS;
        const uint32_t endIndex = aEnd/BLOCK_SIZE_BITS;

        if (endIndex >= mBlocks.Length()) {
            uint32_t numNewBlocks = endIndex + 1 - mBlocks.Length();
            nsAutoPtr<Block> *blocks = mBlocks.AppendElements(numNewBlocks);
            if (MOZ_UNLIKELY(!blocks)) // OOM
                return;
        }

        for (uint32_t i = startIndex; i <= endIndex; ++i) {
            const uint32_t blockFirstBit = i * BLOCK_SIZE_BITS;
            const uint32_t blockLastBit = blockFirstBit + BLOCK_SIZE_BITS - 1;

            Block *block = mBlocks[i];
            if (!block) {
                bool fullBlock = false;
                if (aStart <= blockFirstBit && aEnd >= blockLastBit)
                    fullBlock = true;

                block = new Block(fullBlock ? 0xFF : 0);
                mBlocks[i] = block;

                if (fullBlock)
                    continue;
            }

            const uint32_t start = aStart > blockFirstBit ? aStart - blockFirstBit : 0;
            const uint32_t end = std::min<uint32_t>(aEnd - blockFirstBit, BLOCK_SIZE_BITS - 1);

            for (uint32_t bit = start; bit <= end; ++bit) {
                block->mBits[bit>>3] |= 1 << (bit & 0x7);
            }
        }
    }

    void clear(uint32_t aIndex) {
        uint32_t blockIndex = aIndex/BLOCK_SIZE_BITS;
        if (blockIndex >= mBlocks.Length()) {
            nsAutoPtr<Block> *blocks = mBlocks.AppendElements(blockIndex + 1 - mBlocks.Length());
            if (MOZ_UNLIKELY(!blocks)) // OOM
                return;
        }
        Block *block = mBlocks[blockIndex];
        if (!block) {
            return;
        }
        block->mBits[(aIndex>>3) & (BLOCK_SIZE - 1)] &= ~(1 << (aIndex & 0x7));
    }

    void ClearRange(uint32_t aStart, uint32_t aEnd) {
        const uint32_t startIndex = aStart/BLOCK_SIZE_BITS;
        const uint32_t endIndex = aEnd/BLOCK_SIZE_BITS;

        if (endIndex >= mBlocks.Length()) {
            uint32_t numNewBlocks = endIndex + 1 - mBlocks.Length();
            nsAutoPtr<Block> *blocks = mBlocks.AppendElements(numNewBlocks);
            if (MOZ_UNLIKELY(!blocks)) // OOM
                return;
        }

        for (uint32_t i = startIndex; i <= endIndex; ++i) {
            const uint32_t blockFirstBit = i * BLOCK_SIZE_BITS;

            Block *block = mBlocks[i];
            if (!block) {
                // any nonexistent block is implicitly all clear,
                // so there's no need to even create it
                continue;
            }

            const uint32_t start = aStart > blockFirstBit ? aStart - blockFirstBit : 0;
            const uint32_t end = std::min<uint32_t>(aEnd - blockFirstBit, BLOCK_SIZE_BITS - 1);

            for (uint32_t bit = start; bit <= end; ++bit) {
                block->mBits[bit>>3] &= ~(1 << (bit & 0x7));
            }
        }
    }

    size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const {
        size_t total = mBlocks.SizeOfExcludingThis(aMallocSizeOf);
        for (uint32_t i = 0; i < mBlocks.Length(); i++) {
            if (mBlocks[i]) {
                total += aMallocSizeOf(mBlocks[i]);
            }
        }
        return total;
    }

    size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const {
        return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
    }

    // clear out all blocks in the array
    void reset() {
        uint32_t i;
        for (i = 0; i < mBlocks.Length(); i++)
            mBlocks[i] = nullptr;    
    }

    // set this bitset to the union of its current contents and another
    void Union(const gfxSparseBitSet& aBitset) {
        // ensure mBlocks is large enough
        uint32_t blockCount = aBitset.mBlocks.Length();
        if (blockCount > mBlocks.Length()) {
            uint32_t needed = blockCount - mBlocks.Length();
            nsAutoPtr<Block> *blocks = mBlocks.AppendElements(needed);
            if (MOZ_UNLIKELY(!blocks)) { // OOM
                return;
            }
        }
        // for each block that may be present in aBitset...
        for (uint32_t i = 0; i < blockCount; ++i) {
            // if it is missing (implicitly empty), just skip
            if (!aBitset.mBlocks[i]) {
                continue;
            }
            // if the block is missing in this set, just copy the other
            if (!mBlocks[i]) {
                mBlocks[i] = new Block(*aBitset.mBlocks[i]);
                continue;
            }
            // else set existing block to the union of both
            uint32_t *dst = reinterpret_cast<uint32_t*>(mBlocks[i]->mBits);
            const uint32_t *src =
                reinterpret_cast<const uint32_t*>(aBitset.mBlocks[i]->mBits);
            for (uint32_t j = 0; j < BLOCK_SIZE / 4; ++j) {
                dst[j] |= src[j];
            }
        }
    }

    void Compact() {
        mBlocks.Compact();
    }

    uint32_t GetChecksum() const {
        uint32_t check = adler32(0, Z_NULL, 0);
        for (uint32_t i = 0; i < mBlocks.Length(); i++) {
            if (mBlocks[i]) {
                const Block *block = mBlocks[i];
                check = adler32(check, (uint8_t*) (&i), 4);
                check = adler32(check, (uint8_t*) block, sizeof(Block));
            }
        }
        return check;
    }

private:
    nsTArray< nsAutoPtr<Block> > mBlocks;
};

#define TRUETYPE_TAG(a, b, c, d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))

namespace mozilla {

// Byte-swapping types and name table structure definitions moved from
// gfxFontUtils.cpp to .h file so that gfxFont.cpp can also refer to them
#pragma pack(1)

struct AutoSwap_PRUint16 {
#ifdef __SUNPRO_CC
    AutoSwap_PRUint16& operator = (const uint16_t aValue)
    {
        this->value = mozilla::NativeEndian::swapToBigEndian(aValue);
        return *this;
    }
#else
    AutoSwap_PRUint16(uint16_t aValue)
    {
        value = mozilla::NativeEndian::swapToBigEndian(aValue);
    }
#endif
    operator uint16_t() const
    {
        return mozilla::NativeEndian::swapFromBigEndian(value);
    }

    operator uint32_t() const
    {
        return mozilla::NativeEndian::swapFromBigEndian(value);
    }

    operator uint64_t() const
    {
        return mozilla::NativeEndian::swapFromBigEndian(value);
    }

private:
    uint16_t value;
};

struct AutoSwap_PRInt16 {
#ifdef __SUNPRO_CC
    AutoSwap_PRInt16& operator = (const int16_t aValue)
    {
        this->value = mozilla::NativeEndian::swapToBigEndian(aValue);
        return *this;
    }
#else
    AutoSwap_PRInt16(int16_t aValue)
    {
        value = mozilla::NativeEndian::swapToBigEndian(aValue);
    }
#endif
    operator int16_t() const
    {
        return mozilla::NativeEndian::swapFromBigEndian(value);
    }

    operator uint32_t() const
    {
        return mozilla::NativeEndian::swapFromBigEndian(value);
    }

private:
    int16_t  value;
};

struct AutoSwap_PRUint32 {
#ifdef __SUNPRO_CC
    AutoSwap_PRUint32& operator = (const uint32_t aValue)
    {
        this->value = mozilla::NativeEndian::swapToBigEndian(aValue);
        return *this;
    }
#else
    AutoSwap_PRUint32(uint32_t aValue)
    {
        value = mozilla::NativeEndian::swapToBigEndian(aValue);
    }
#endif
    operator uint32_t() const
    {
        return mozilla::NativeEndian::swapFromBigEndian(value);
    }

private:
    uint32_t  value;
};

struct AutoSwap_PRInt32 {
#ifdef __SUNPRO_CC
    AutoSwap_PRInt32& operator = (const int32_t aValue)
    {
        this->value = mozilla::NativeEndian::swapToBigEndian(aValue);
        return *this;
    }
#else
    AutoSwap_PRInt32(int32_t aValue)
    {
        value = mozilla::NativeEndian::swapToBigEndian(aValue);
    }
#endif
    operator int32_t() const
    {
        return mozilla::NativeEndian::swapFromBigEndian(value);
    }

private:
    int32_t  value;
};

struct AutoSwap_PRUint64 {
#ifdef __SUNPRO_CC
    AutoSwap_PRUint64& operator = (const uint64_t aValue)
    {
        this->value = mozilla::NativeEndian::swapToBigEndian(aValue);
        return *this;
    }
#else
    AutoSwap_PRUint64(uint64_t aValue)
    {
        value = mozilla::NativeEndian::swapToBigEndian(aValue);
    }
#endif
    operator uint64_t() const
    {
        return mozilla::NativeEndian::swapFromBigEndian(value);
    }

private:
    uint64_t  value;
};

struct AutoSwap_PRUint24 {
    operator uint32_t() const { return value[0] << 16 | value[1] << 8 | value[2]; }
private:
    AutoSwap_PRUint24() { }
    uint8_t  value[3];
};

struct SFNTHeader {
    AutoSwap_PRUint32    sfntVersion;            // Fixed, 0x00010000 for version 1.0.
    AutoSwap_PRUint16    numTables;              // Number of tables.
    AutoSwap_PRUint16    searchRange;            // (Maximum power of 2 <= numTables) x 16.
    AutoSwap_PRUint16    entrySelector;          // Log2(maximum power of 2 <= numTables).
    AutoSwap_PRUint16    rangeShift;             // NumTables x 16-searchRange.        
};

struct TableDirEntry {
    AutoSwap_PRUint32    tag;                    // 4 -byte identifier.
    AutoSwap_PRUint32    checkSum;               // CheckSum for this table.
    AutoSwap_PRUint32    offset;                 // Offset from beginning of TrueType font file.
    AutoSwap_PRUint32    length;                 // Length of this table.        
};

struct HeadTable {
    enum {
        HEAD_VERSION = 0x00010000,
        HEAD_MAGIC_NUMBER = 0x5F0F3CF5,
        HEAD_CHECKSUM_CALC_CONST = 0xB1B0AFBA
    };

    AutoSwap_PRUint32    tableVersionNumber;    // Fixed, 0x00010000 for version 1.0.
    AutoSwap_PRUint32    fontRevision;          // Set by font manufacturer.
    AutoSwap_PRUint32    checkSumAdjustment;    // To compute: set it to 0, sum the entire font as ULONG, then store 0xB1B0AFBA - sum.
    AutoSwap_PRUint32    magicNumber;           // Set to 0x5F0F3CF5.
    AutoSwap_PRUint16    flags;
    AutoSwap_PRUint16    unitsPerEm;            // Valid range is from 16 to 16384. This value should be a power of 2 for fonts that have TrueType outlines.
    AutoSwap_PRUint64    created;               // Number of seconds since 12:00 midnight, January 1, 1904. 64-bit integer
    AutoSwap_PRUint64    modified;              // Number of seconds since 12:00 midnight, January 1, 1904. 64-bit integer
    AutoSwap_PRInt16     xMin;                  // For all glyph bounding boxes.
    AutoSwap_PRInt16     yMin;                  // For all glyph bounding boxes.
    AutoSwap_PRInt16     xMax;                  // For all glyph bounding boxes.
    AutoSwap_PRInt16     yMax;                  // For all glyph bounding boxes.
    AutoSwap_PRUint16    macStyle;              // Bit 0: Bold (if set to 1);
    AutoSwap_PRUint16    lowestRecPPEM;         // Smallest readable size in pixels.
    AutoSwap_PRInt16     fontDirectionHint;
    AutoSwap_PRInt16     indexToLocFormat;
    AutoSwap_PRInt16     glyphDataFormat;
};

struct OS2Table {
    AutoSwap_PRUint16    version;                // 0004 = OpenType 1.5
    AutoSwap_PRInt16     xAvgCharWidth;
    AutoSwap_PRUint16    usWeightClass;
    AutoSwap_PRUint16    usWidthClass;
    AutoSwap_PRUint16    fsType;
    AutoSwap_PRInt16     ySubscriptXSize;
    AutoSwap_PRInt16     ySubscriptYSize;
    AutoSwap_PRInt16     ySubscriptXOffset;
    AutoSwap_PRInt16     ySubscriptYOffset;
    AutoSwap_PRInt16     ySuperscriptXSize;
    AutoSwap_PRInt16     ySuperscriptYSize;
    AutoSwap_PRInt16     ySuperscriptXOffset;
    AutoSwap_PRInt16     ySuperscriptYOffset;
    AutoSwap_PRInt16     yStrikeoutSize;
    AutoSwap_PRInt16     yStrikeoutPosition;
    AutoSwap_PRInt16     sFamilyClass;
    uint8_t              panose[10];
    AutoSwap_PRUint32    unicodeRange1;
    AutoSwap_PRUint32    unicodeRange2;
    AutoSwap_PRUint32    unicodeRange3;
    AutoSwap_PRUint32    unicodeRange4;
    uint8_t              achVendID[4];
    AutoSwap_PRUint16    fsSelection;
    AutoSwap_PRUint16    usFirstCharIndex;
    AutoSwap_PRUint16    usLastCharIndex;
    AutoSwap_PRInt16     sTypoAscender;
    AutoSwap_PRInt16     sTypoDescender;
    AutoSwap_PRInt16     sTypoLineGap;
    AutoSwap_PRUint16    usWinAscent;
    AutoSwap_PRUint16    usWinDescent;
    AutoSwap_PRUint32    codePageRange1;
    AutoSwap_PRUint32    codePageRange2;
    AutoSwap_PRInt16     sxHeight;
    AutoSwap_PRInt16     sCapHeight;
    AutoSwap_PRUint16    usDefaultChar;
    AutoSwap_PRUint16    usBreakChar;
    AutoSwap_PRUint16    usMaxContext;
};

struct PostTable {
    AutoSwap_PRUint32    version;
    AutoSwap_PRInt32     italicAngle;
    AutoSwap_PRInt16     underlinePosition;
    AutoSwap_PRUint16    underlineThickness;
    AutoSwap_PRUint32    isFixedPitch;
    AutoSwap_PRUint32    minMemType42;
    AutoSwap_PRUint32    maxMemType42;
    AutoSwap_PRUint32    minMemType1;
    AutoSwap_PRUint32    maxMemType1;
};

struct HheaTable {
    AutoSwap_PRUint32    version;
    AutoSwap_PRInt16     ascender;
    AutoSwap_PRInt16     descender;
    AutoSwap_PRInt16     lineGap;
    AutoSwap_PRUint16    advanceWidthMax;
    AutoSwap_PRInt16     minLeftSideBearing;
    AutoSwap_PRInt16     minRightSideBearing;
    AutoSwap_PRInt16     xMaxExtent;
    AutoSwap_PRInt16     caretSlopeRise;
    AutoSwap_PRInt16     caretSlopeRun;
    AutoSwap_PRInt16     caretOffset;
    AutoSwap_PRInt16     reserved1;
    AutoSwap_PRInt16     reserved2;
    AutoSwap_PRInt16     reserved3;
    AutoSwap_PRInt16     reserved4;
    AutoSwap_PRInt16     metricDataFormat;
    AutoSwap_PRUint16    numOfLongHorMetrics;
};

struct MaxpTableHeader {
    AutoSwap_PRUint32    version; // CFF: 0x00005000; TrueType: 0x00010000
    AutoSwap_PRUint16    numGlyphs;
// truetype version has additional fields that we don't currently use
};

// old 'kern' table, supported on Windows
// see http://www.microsoft.com/typography/otspec/kern.htm
struct KernTableVersion0 {
    AutoSwap_PRUint16    version; // 0x0000
    AutoSwap_PRUint16    nTables;
};

struct KernTableSubtableHeaderVersion0 {
    AutoSwap_PRUint16    version;
    AutoSwap_PRUint16    length;
    AutoSwap_PRUint16    coverage;
};

// newer Mac-only 'kern' table, ignored by Windows
// see http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6kern.html
struct KernTableVersion1 {
    AutoSwap_PRUint32    version; // 0x00010000
    AutoSwap_PRUint32    nTables;
};

struct KernTableSubtableHeaderVersion1 {
    AutoSwap_PRUint32    length;
    AutoSwap_PRUint16    coverage;
    AutoSwap_PRUint16    tupleIndex;
};

#pragma pack()

// Return just the highest bit of the given value, i.e., the highest
// power of 2 that is <= value, or zero if the input value is zero.
inline uint32_t
FindHighestBit(uint32_t value)
{
    // propagate highest bit into all lower bits of the value
    value |= (value >> 1);
    value |= (value >> 2);
    value |= (value >> 4);
    value |= (value >> 8);
    value |= (value >> 16);
    // isolate the leftmost bit
    return (value & ~(value >> 1));
}

} // namespace mozilla

// used for overlaying name changes without touching original font data
struct FontDataOverlay {
    // overlaySrc != 0 ==> use overlay
    uint32_t  overlaySrc;    // src offset from start of font data
    uint32_t  overlaySrcLen; // src length
    uint32_t  overlayDest;   // dest offset from start of font data
};
    
enum gfxUserFontType {
    GFX_USERFONT_UNKNOWN = 0,
    GFX_USERFONT_OPENTYPE = 1,
    GFX_USERFONT_SVG = 2,
    GFX_USERFONT_WOFF = 3
};

class THEBES_API gfxFontUtils {

public:
    // these are public because gfxFont.cpp also looks into the name table
    enum {
        NAME_ID_FAMILY = 1,
        NAME_ID_STYLE = 2,
        NAME_ID_UNIQUE = 3,
        NAME_ID_FULL = 4,
        NAME_ID_VERSION = 5,
        NAME_ID_POSTSCRIPT = 6,
        NAME_ID_PREFERRED_FAMILY = 16,
        NAME_ID_PREFERRED_STYLE = 17,

        PLATFORM_ALL = -1,
        PLATFORM_ID_UNICODE = 0,           // Mac OS uses this typically
        PLATFORM_ID_MAC = 1,
        PLATFORM_ID_ISO = 2,
        PLATFORM_ID_MICROSOFT = 3,

        ENCODING_ID_MAC_ROMAN = 0,         // traditional Mac OS script manager encodings
        ENCODING_ID_MAC_JAPANESE = 1,      // (there are others defined, but some were never
        ENCODING_ID_MAC_TRAD_CHINESE = 2,  // implemented by Apple, and I have never seen them
        ENCODING_ID_MAC_KOREAN = 3,        // used in font names)
        ENCODING_ID_MAC_ARABIC = 4,
        ENCODING_ID_MAC_HEBREW = 5,
        ENCODING_ID_MAC_GREEK = 6,
        ENCODING_ID_MAC_CYRILLIC = 7,
        ENCODING_ID_MAC_DEVANAGARI = 9,
        ENCODING_ID_MAC_GURMUKHI = 10,
        ENCODING_ID_MAC_GUJARATI = 11,
        ENCODING_ID_MAC_SIMP_CHINESE = 25,

        ENCODING_ID_MICROSOFT_SYMBOL = 0,  // Microsoft platform encoding IDs
        ENCODING_ID_MICROSOFT_UNICODEBMP = 1,
        ENCODING_ID_MICROSOFT_SHIFTJIS = 2,
        ENCODING_ID_MICROSOFT_PRC = 3,
        ENCODING_ID_MICROSOFT_BIG5 = 4,
        ENCODING_ID_MICROSOFT_WANSUNG = 5,
        ENCODING_ID_MICROSOFT_JOHAB  = 6,
        ENCODING_ID_MICROSOFT_UNICODEFULL = 10,

        LANG_ALL = -1,
        LANG_ID_MAC_ENGLISH = 0,      // many others are defined, but most don't affect
        LANG_ID_MAC_HEBREW = 10,      // the charset; should check all the central/eastern
        LANG_ID_MAC_JAPANESE = 11,    // european codes, though
        LANG_ID_MAC_ARABIC = 12,
        LANG_ID_MAC_ICELANDIC = 15,
        LANG_ID_MAC_TURKISH = 17,
        LANG_ID_MAC_TRAD_CHINESE = 19,
        LANG_ID_MAC_URDU = 20,
        LANG_ID_MAC_KOREAN = 23,
        LANG_ID_MAC_POLISH = 25,
        LANG_ID_MAC_FARSI = 31,
        LANG_ID_MAC_SIMP_CHINESE = 33,
        LANG_ID_MAC_ROMANIAN = 37,
        LANG_ID_MAC_CZECH = 38,
        LANG_ID_MAC_SLOVAK = 39,

        LANG_ID_MICROSOFT_EN_US = 0x0409,        // with Microsoft platformID, EN US lang code
        
        CMAP_MAX_CODEPOINT = 0x10ffff     // maximum possible Unicode codepoint 
                                          // contained in a cmap
    };

    // name table has a header, followed by name records, followed by string data
    struct NameHeader {
        mozilla::AutoSwap_PRUint16    format;       // Format selector (=0).
        mozilla::AutoSwap_PRUint16    count;        // Number of name records.
        mozilla::AutoSwap_PRUint16    stringOffset; // Offset to start of string storage
                                                    // (from start of table)
    };

    struct NameRecord {
        mozilla::AutoSwap_PRUint16    platformID;   // Platform ID
        mozilla::AutoSwap_PRUint16    encodingID;   // Platform-specific encoding ID
        mozilla::AutoSwap_PRUint16    languageID;   // Language ID
        mozilla::AutoSwap_PRUint16    nameID;       // Name ID.
        mozilla::AutoSwap_PRUint16    length;       // String length (in bytes).
        mozilla::AutoSwap_PRUint16    offset;       // String offset from start of storage
                                                    // (in bytes).
    };

    // for reading big-endian font data on either big or little-endian platforms

    static inline uint16_t
    ReadShortAt(const uint8_t *aBuf, uint32_t aIndex)
    {
        return (aBuf[aIndex] << 8) | aBuf[aIndex + 1];
    }

    static inline uint16_t
    ReadShortAt16(const uint16_t *aBuf, uint32_t aIndex)
    {
        const uint8_t *buf = reinterpret_cast<const uint8_t*>(aBuf);
        uint32_t index = aIndex << 1;
        return (buf[index] << 8) | buf[index+1];
    }

    static inline uint32_t
    ReadUint24At(const uint8_t *aBuf, uint32_t aIndex)
    {
        return ((aBuf[aIndex] << 16) | (aBuf[aIndex + 1] << 8) |
                (aBuf[aIndex + 2]));
    }

    static inline uint32_t
    ReadLongAt(const uint8_t *aBuf, uint32_t aIndex)
    {
        return ((aBuf[aIndex] << 24) | (aBuf[aIndex + 1] << 16) | 
                (aBuf[aIndex + 2] << 8) | (aBuf[aIndex + 3]));
    }

    static nsresult
    ReadCMAPTableFormat12(const uint8_t *aBuf, uint32_t aLength, 
                          gfxSparseBitSet& aCharacterMap);

    static nsresult 
    ReadCMAPTableFormat4(const uint8_t *aBuf, uint32_t aLength, 
                         gfxSparseBitSet& aCharacterMap);

    static nsresult
    ReadCMAPTableFormat14(const uint8_t *aBuf, uint32_t aLength, 
                          uint8_t*& aTable);

    static uint32_t
    FindPreferredSubtable(const uint8_t *aBuf, uint32_t aBufLength,
                          uint32_t *aTableOffset, uint32_t *aUVSTableOffset,
                          bool *aSymbolEncoding);

    static nsresult
    ReadCMAP(const uint8_t *aBuf, uint32_t aBufLength,
             gfxSparseBitSet& aCharacterMap,
             uint32_t& aUVSOffset,
             bool& aUnicodeFont, bool& aSymbolFont);

    static uint32_t
    MapCharToGlyphFormat4(const uint8_t *aBuf, PRUnichar aCh);

    static uint32_t
    MapCharToGlyphFormat12(const uint8_t *aBuf, uint32_t aCh);

    static uint16_t
    MapUVSToGlyphFormat14(const uint8_t *aBuf, uint32_t aCh, uint32_t aVS);

    static uint32_t
    MapCharToGlyph(const uint8_t *aCmapBuf, uint32_t aBufLength,
                   uint32_t aUnicode, uint32_t aVarSelector = 0);

#ifdef XP_WIN

    // given a TrueType/OpenType data file, produce a EOT-format header
    // for use with Windows T2Embed API AddFontResource type API's
    // effectively hide existing fonts with matching names aHeaderLen is
    // the size of the header buffer on input, the actual size of the
    // EOT header on output
    static nsresult
    MakeEOTHeader(const uint8_t *aFontData, uint32_t aFontDataLength,
                  FallibleTArray<uint8_t> *aHeader, FontDataOverlay *aOverlay);

    // determine whether a font (which has already been sanitized, so is known
    // to be a valid sfnt) is CFF format rather than TrueType
    static bool
    IsCffFont(const uint8_t* aFontData, bool& hasVertical);

#endif

    // determine the format of font data
    static gfxUserFontType
    DetermineFontDataType(const uint8_t *aFontData, uint32_t aFontDataLength);

    // Read the fullname from the sfnt data (used to save the original name
    // prior to renaming the font for installation).
    // This is called with sfnt data that has already been validated,
    // so it should always succeed in finding the name table.
    static nsresult
    GetFullNameFromSFNT(const uint8_t* aFontData, uint32_t aLength,
                        nsAString& aFullName);

    // helper to get fullname from name table, constructing from family+style
    // if no explicit fullname is present
    static nsresult
    GetFullNameFromTable(hb_blob_t *aNameTable,
                         nsAString& aFullName);

    // helper to get family name from name table
    static nsresult
    GetFamilyNameFromTable(hb_blob_t *aNameTable,
                           nsAString& aFamilyName);

    // create a new name table and build a new font with that name table
    // appended on the end, returns true on success
    static nsresult
    RenameFont(const nsAString& aName, const uint8_t *aFontData, 
               uint32_t aFontDataLength, FallibleTArray<uint8_t> *aNewFont);
    
    // read all names matching aNameID, returning in aNames array
    static nsresult
    ReadNames(hb_blob_t *aNameTable, uint32_t aNameID, 
              int32_t aPlatformID, nsTArray<nsString>& aNames);
      
    // reads English or first name matching aNameID, returning in aName
    // platform based on OS
    static nsresult
    ReadCanonicalName(hb_blob_t *aNameTable, uint32_t aNameID, 
                      nsString& aName);
      
    // convert a name from the raw name table data into an nsString,
    // provided we know how; return true if successful, or false
    // if we can't handle the encoding
    static bool
    DecodeFontName(const char *aBuf, int32_t aLength, 
                   uint32_t aPlatformCode, uint32_t aScriptCode,
                   uint32_t aLangCode, nsAString& dest);

    static inline bool IsJoinCauser(uint32_t ch) {
        return (ch == 0x200D);
    }

    static inline bool IsJoinControl(uint32_t ch) {
        return (ch == 0x200C || ch == 0x200D);
    }

    enum {
        kUnicodeVS1 = 0xFE00,
        kUnicodeVS16 = 0xFE0F,
        kUnicodeVS17 = 0xE0100,
        kUnicodeVS256 = 0xE01EF
    };

    static inline bool IsVarSelector(uint32_t ch) {
        return (ch >= kUnicodeVS1 && ch <= kUnicodeVS16) ||
               (ch >= kUnicodeVS17 && ch <= kUnicodeVS256);
    }

    static inline bool IsInvalid(uint32_t ch) {
        return (ch == 0xFFFD);
    }

    // Font code may want to know if there is the potential for bidi behavior
    // to be triggered by any of the characters in a text run; this can be
    // used to test that possibility.
    enum {
        kUnicodeBidiScriptsStart = 0x0590,
        kUnicodeBidiScriptsEnd = 0x08FF,
        kUnicodeBidiPresentationStart = 0xFB1D,
        kUnicodeBidiPresentationEnd = 0xFEFC,
        kUnicodeFirstHighSurrogateBlock = 0xD800,
        kUnicodeRLM = 0x200F,
        kUnicodeRLE = 0x202B,
        kUnicodeRLO = 0x202E
    };

    static inline bool PotentialRTLChar(PRUnichar aCh) {
        if (aCh >= kUnicodeBidiScriptsStart && aCh <= kUnicodeBidiScriptsEnd)
            // bidi scripts Hebrew, Arabic, Syriac, Thaana, N'Ko are all encoded together
            return true;

        if (aCh == kUnicodeRLM || aCh == kUnicodeRLE || aCh == kUnicodeRLO)
            // directional controls that trigger bidi layout
            return true;

        if (aCh >= kUnicodeBidiPresentationStart &&
            aCh <= kUnicodeBidiPresentationEnd)
            // presentation forms of Arabic and Hebrew letters
            return true;

        if ((aCh & 0xFF00) == kUnicodeFirstHighSurrogateBlock)
            // surrogate that could be part of a bidi supplementary char
            // (Cypriot, Aramaic, Phoenecian, etc)
            return true;

        // otherwise we know this char cannot trigger bidi reordering
        return false;
    }

    static uint8_t CharRangeBit(uint32_t ch);
    
    // for a given font list pref name, set up a list of font names
    static void GetPrefsFontList(const char *aPrefName, 
                                 nsTArray<nsString>& aFontList);

    // generate a unique font name
    static nsresult MakeUniqueUserFontName(nsAString& aName);

protected:
    static nsresult
    ReadNames(hb_blob_t *aNameTable, uint32_t aNameID, 
              int32_t aLangID, int32_t aPlatformID, nsTArray<nsString>& aNames);

    // convert opentype name-table platform/encoding/language values to a charset name
    // we can use to convert the name data to unicode, or "" if data is UTF16BE
    static const char*
    GetCharsetForFontName(uint16_t aPlatform, uint16_t aScript, uint16_t aLanguage);

    struct MacFontNameCharsetMapping {
        uint16_t    mEncoding;
        uint16_t    mLanguage;
        const char *mCharsetName;

        bool operator<(const MacFontNameCharsetMapping& rhs) const {
            return (mEncoding < rhs.mEncoding) ||
                   ((mEncoding == rhs.mEncoding) && (mLanguage < rhs.mLanguage));
        }
    };
    static const MacFontNameCharsetMapping gMacFontNameCharsets[];
    static const char* gISOFontNameCharsets[];
    static const char* gMSFontNameCharsets[];
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
    void StartLoader(uint32_t aDelay, uint32_t aInterval) {
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
        uint32_t timerInterval;

        if (aDelay) {
            mState = stateTimerOnDelay;
            timerInterval = aDelay;
        } else {
            mState = stateTimerOnInterval;
            timerInterval = mInterval;
        }

        InitLoader();

        // start timer
        mTimer->InitWithFuncCallback(LoaderTimerCallback, this, timerInterval,
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
    virtual bool RunLoader() = 0;

    // Finish - cleanup after done
    virtual void FinishLoader() = 0;

    static void LoaderTimerCallback(nsITimer *aTimer, void *aThis) {
        gfxFontInfoLoader *loader = static_cast<gfxFontInfoLoader*>(aThis);
        loader->LoaderTimerFire();
    }

    // start the timer, interval callbacks
    void LoaderTimerFire() {
        if (mState == stateTimerOnDelay) {
            mState = stateTimerOnInterval;
            mTimer->SetDelay(mInterval);
        }

        bool done = RunLoader();
        if (done) {
            CancelLoader();
        }
    }

    nsCOMPtr<nsITimer> mTimer;
    uint32_t mInterval;
    TimerState mState;
};

#endif /* GFX_FONT_UTILS_H */
