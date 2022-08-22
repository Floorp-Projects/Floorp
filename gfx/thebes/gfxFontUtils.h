/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_UTILS_H
#define GFX_FONT_UTILS_H

#include <string.h>
#include <algorithm>
#include <new>
#include <utility>
#include "gfxPlatform.h"
#include "harfbuzz/hb.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Casting.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ServoStyleConstsInlines.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nscore.h"
#include "zlib.h"

class PickleIterator;
class gfxFontEntry;
struct gfxFontVariationAxis;
struct gfxFontVariationInstance;

namespace mozilla {
class Encoding;
class ServoStyleSet;
}  // namespace mozilla

/* Bug 341128 - w32api defines min/max which causes problems with <bitset> */
#ifdef __MINGW32__
#  undef min
#  undef max
#endif

#undef ERROR /* defined by Windows.h, conflicts with some generated bindings \
                code when this gets indirectly included via shared font list \
              */

typedef struct hb_blob_t hb_blob_t;

class SharedBitSet;

namespace IPC {
template <typename T>
struct ParamTraits;
}

class gfxSparseBitSet {
 private:
  friend class SharedBitSet;

  enum { BLOCK_SIZE = 32 };  // ==> 256 codepoints per block
  enum { BLOCK_SIZE_BITS = BLOCK_SIZE * 8 };
  enum { NO_BLOCK = 0xffff };  // index value indicating missing (empty) block

  struct Block {
    explicit Block(unsigned char memsetValue = 0) {
      memset(mBits, memsetValue, BLOCK_SIZE);
    }
    uint8_t mBits[BLOCK_SIZE];
  };

  friend struct IPC::ParamTraits<gfxSparseBitSet>;
  friend struct IPC::ParamTraits<Block>;

 public:
  gfxSparseBitSet() = default;

  bool Equals(const gfxSparseBitSet* aOther) const {
    if (mBlockIndex.Length() != aOther->mBlockIndex.Length()) {
      return false;
    }
    size_t n = mBlockIndex.Length();
    for (size_t i = 0; i < n; ++i) {
      uint32_t b1 = mBlockIndex[i];
      uint32_t b2 = aOther->mBlockIndex[i];
      if ((b1 == NO_BLOCK) != (b2 == NO_BLOCK)) {
        return false;
      }
      if (b1 == NO_BLOCK) {
        continue;
      }
      if (memcmp(&mBlocks[b1].mBits, &aOther->mBlocks[b2].mBits, BLOCK_SIZE) !=
          0) {
        return false;
      }
    }
    return true;
  }

  bool test(uint32_t aIndex) const {
    uint32_t i = aIndex / BLOCK_SIZE_BITS;
    if (i >= mBlockIndex.Length() || mBlockIndex[i] == NO_BLOCK) {
      return false;
    }
    const Block& block = mBlocks[mBlockIndex[i]];
    return ((block.mBits[(aIndex >> 3) & (BLOCK_SIZE - 1)]) &
            (1 << (aIndex & 0x7))) != 0;
  }

  // dump out contents of bitmap
  void Dump(const char* aPrefix, eGfxLog aWhichLog) const;

  bool TestRange(uint32_t aStart, uint32_t aEnd) {
    // start point is beyond the end of the block array? return false
    // immediately
    uint32_t startBlock = aStart / BLOCK_SIZE_BITS;
    uint32_t blockLen = mBlockIndex.Length();
    if (startBlock >= blockLen) {
      return false;
    }

    // check for blocks in range, if none, return false
    bool hasBlocksInRange = false;
    uint32_t endBlock = aEnd / BLOCK_SIZE_BITS;
    for (uint32_t bi = startBlock; bi <= endBlock; bi++) {
      if (bi < blockLen && mBlockIndex[bi] != NO_BLOCK) {
        hasBlocksInRange = true;
        break;
      }
    }
    if (!hasBlocksInRange) {
      return false;
    }

    // first block, check bits
    if (mBlockIndex[startBlock] != NO_BLOCK) {
      const Block& block = mBlocks[mBlockIndex[startBlock]];
      uint32_t start = aStart;
      uint32_t end = std::min(aEnd, ((startBlock + 1) * BLOCK_SIZE_BITS) - 1);
      for (uint32_t i = start; i <= end; i++) {
        if ((block.mBits[(i >> 3) & (BLOCK_SIZE - 1)]) & (1 << (i & 0x7))) {
          return true;
        }
      }
    }
    if (endBlock == startBlock) {
      return false;
    }

    // [2..n-1] blocks check bytes
    for (uint32_t i = startBlock + 1; i < endBlock; i++) {
      if (i >= blockLen || mBlockIndex[i] == NO_BLOCK) {
        continue;
      }
      const Block& block = mBlocks[mBlockIndex[i]];
      for (uint32_t index = 0; index < BLOCK_SIZE; index++) {
        if (block.mBits[index]) {
          return true;
        }
      }
    }

    // last block, check bits
    if (endBlock < blockLen && mBlockIndex[endBlock] != NO_BLOCK) {
      const Block& block = mBlocks[mBlockIndex[endBlock]];
      uint32_t start = endBlock * BLOCK_SIZE_BITS;
      uint32_t end = aEnd;
      for (uint32_t i = start; i <= end; i++) {
        if ((block.mBits[(i >> 3) & (BLOCK_SIZE - 1)]) & (1 << (i & 0x7))) {
          return true;
        }
      }
    }

    return false;
  }

  void set(uint32_t aIndex) {
    uint32_t i = aIndex / BLOCK_SIZE_BITS;
    while (i >= mBlockIndex.Length()) {
      mBlockIndex.AppendElement(NO_BLOCK);
    }
    if (mBlockIndex[i] == NO_BLOCK) {
      mBlocks.AppendElement();
      MOZ_ASSERT(mBlocks.Length() < 0xffff, "block index overflow!");
      mBlockIndex[i] = static_cast<uint16_t>(mBlocks.Length() - 1);
    }
    Block& block = mBlocks[mBlockIndex[i]];
    block.mBits[(aIndex >> 3) & (BLOCK_SIZE - 1)] |= 1 << (aIndex & 0x7);
  }

  void set(uint32_t aIndex, bool aValue) {
    if (aValue) {
      set(aIndex);
    } else {
      clear(aIndex);
    }
  }

  void SetRange(uint32_t aStart, uint32_t aEnd) {
    const uint32_t startIndex = aStart / BLOCK_SIZE_BITS;
    const uint32_t endIndex = aEnd / BLOCK_SIZE_BITS;

    while (endIndex >= mBlockIndex.Length()) {
      mBlockIndex.AppendElement(NO_BLOCK);
    }

    for (uint32_t i = startIndex; i <= endIndex; ++i) {
      const uint32_t blockFirstBit = i * BLOCK_SIZE_BITS;
      const uint32_t blockLastBit = blockFirstBit + BLOCK_SIZE_BITS - 1;

      if (mBlockIndex[i] == NO_BLOCK) {
        bool fullBlock = (aStart <= blockFirstBit && aEnd >= blockLastBit);
        mBlocks.AppendElement(Block(fullBlock ? 0xFF : 0));
        MOZ_ASSERT(mBlocks.Length() < 0xffff, "block index overflow!");
        mBlockIndex[i] = static_cast<uint16_t>(mBlocks.Length() - 1);
        if (fullBlock) {
          continue;
        }
      }

      Block& block = mBlocks[mBlockIndex[i]];
      const uint32_t start =
          aStart > blockFirstBit ? aStart - blockFirstBit : 0;
      const uint32_t end =
          std::min<uint32_t>(aEnd - blockFirstBit, BLOCK_SIZE_BITS - 1);

      for (uint32_t bit = start; bit <= end; ++bit) {
        block.mBits[bit >> 3] |= 1 << (bit & 0x7);
      }
    }
  }

  void clear(uint32_t aIndex) {
    uint32_t i = aIndex / BLOCK_SIZE_BITS;
    if (i >= mBlockIndex.Length()) {
      return;
    }
    if (mBlockIndex[i] == NO_BLOCK) {
      mBlocks.AppendElement();
      MOZ_ASSERT(mBlocks.Length() < 0xffff, "block index overflow!");
      mBlockIndex[i] = static_cast<uint16_t>(mBlocks.Length() - 1);
    }
    Block& block = mBlocks[mBlockIndex[i]];
    block.mBits[(aIndex >> 3) & (BLOCK_SIZE - 1)] &= ~(1 << (aIndex & 0x7));
  }

  void ClearRange(uint32_t aStart, uint32_t aEnd) {
    const uint32_t startIndex = aStart / BLOCK_SIZE_BITS;
    const uint32_t endIndex = aEnd / BLOCK_SIZE_BITS;

    for (uint32_t i = startIndex; i <= endIndex; ++i) {
      if (i >= mBlockIndex.Length()) {
        return;
      }
      if (mBlockIndex[i] == NO_BLOCK) {
        continue;
      }

      const uint32_t blockFirstBit = i * BLOCK_SIZE_BITS;
      Block& block = mBlocks[mBlockIndex[i]];

      const uint32_t start =
          aStart > blockFirstBit ? aStart - blockFirstBit : 0;
      const uint32_t end =
          std::min<uint32_t>(aEnd - blockFirstBit, BLOCK_SIZE_BITS - 1);

      for (uint32_t bit = start; bit <= end; ++bit) {
        block.mBits[bit >> 3] &= ~(1 << (bit & 0x7));
      }
    }
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return mBlocks.ShallowSizeOfExcludingThis(aMallocSizeOf) +
           mBlockIndex.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  // clear out all blocks in the array
  void reset() {
    mBlocks.Clear();
    mBlockIndex.Clear();
  }

  // set this bitset to the union of its current contents and another
  void Union(const gfxSparseBitSet& aBitset) {
    // ensure mBlocks is large enough
    uint32_t blockCount = aBitset.mBlockIndex.Length();
    while (blockCount > mBlockIndex.Length()) {
      mBlockIndex.AppendElement(NO_BLOCK);
    }
    // for each block that may be present in aBitset...
    for (uint32_t i = 0; i < blockCount; ++i) {
      // if it is missing (implicitly empty), just skip
      if (aBitset.mBlockIndex[i] == NO_BLOCK) {
        continue;
      }
      // if the block is missing in this set, just copy the other
      if (mBlockIndex[i] == NO_BLOCK) {
        mBlocks.AppendElement(aBitset.mBlocks[aBitset.mBlockIndex[i]]);
        MOZ_ASSERT(mBlocks.Length() < 0xffff, "block index overflow!");
        mBlockIndex[i] = static_cast<uint16_t>(mBlocks.Length() - 1);
        continue;
      }
      // else set existing block to the union of both
      uint32_t* dst =
          reinterpret_cast<uint32_t*>(&mBlocks[mBlockIndex[i]].mBits);
      const uint32_t* src = reinterpret_cast<const uint32_t*>(
          &aBitset.mBlocks[aBitset.mBlockIndex[i]].mBits);
      for (uint32_t j = 0; j < BLOCK_SIZE / 4; ++j) {
        dst[j] |= src[j];
      }
    }
  }

  inline void Union(const SharedBitSet& aBitset);

  void Compact() {
    // TODO: Discard any empty blocks, and adjust index accordingly.
    // (May not be worth doing, though, because we so rarely clear bits
    // that were previously set.)
    mBlocks.Compact();
    mBlockIndex.Compact();
  }

  uint32_t GetChecksum() const {
    uint32_t check =
        adler32(0, reinterpret_cast<const uint8_t*>(mBlockIndex.Elements()),
                mBlockIndex.Length() * sizeof(uint16_t));
    check = adler32(check, reinterpret_cast<const uint8_t*>(mBlocks.Elements()),
                    mBlocks.Length() * sizeof(Block));
    return check;
  }

 private:
  CopyableTArray<uint16_t> mBlockIndex;
  CopyableTArray<Block> mBlocks;
};

/**
 * SharedBitSet is a version of gfxSparseBitSet that is intended to be used
 * in a shared-memory block, and can be used regardless of the address at which
 * the block has been mapped. The SharedBitSet cannot be modified once it has
 * been created.
 *
 * Max size of a SharedBitSet = 4352 * 32  ; blocks
 *                              + 4352 * 2 ; index
 *                              + 4        ; counts
 *   = 147972 bytes
 *
 * Therefore, SharedFontList must be able to allocate a contiguous block of at
 * least this size.
 */
class SharedBitSet {
 private:
  // We use the same Block type as gfxSparseBitSet.
  typedef gfxSparseBitSet::Block Block;

  enum { BLOCK_SIZE = gfxSparseBitSet::BLOCK_SIZE };
  enum { BLOCK_SIZE_BITS = gfxSparseBitSet::BLOCK_SIZE_BITS };
  enum { NO_BLOCK = gfxSparseBitSet::NO_BLOCK };

 public:
  static const size_t kMaxSize = 147972;  // see above

  // Returns the size needed for a SharedBitSet version of the given
  // gfxSparseBitSet.
  static size_t RequiredSize(const gfxSparseBitSet& aBitset) {
    size_t total = sizeof(SharedBitSet);
    size_t len = aBitset.mBlockIndex.Length();
    total += len * sizeof(uint16_t);  // add size for index array
    // add size for blocks, excluding any missing ones
    for (uint16_t i = 0; i < len; i++) {
      if (aBitset.mBlockIndex[i] != NO_BLOCK) {
        total += sizeof(Block);
      }
    }
    MOZ_ASSERT(total <= kMaxSize);
    return total;
  }

  // Create a SharedBitSet in the provided buffer, initializing it with the
  // contents of aBitset.
  static SharedBitSet* Create(void* aBuffer, size_t aBufSize,
                              const gfxSparseBitSet& aBitset) {
    MOZ_ASSERT(aBufSize >= RequiredSize(aBitset));
    return new (aBuffer) SharedBitSet(aBitset);
  }

  bool test(uint32_t aIndex) const {
    const auto i = static_cast<uint16_t>(aIndex / BLOCK_SIZE_BITS);
    if (i >= mBlockIndexCount) {
      return false;
    }
    const uint16_t* const blockIndex =
        reinterpret_cast<const uint16_t*>(this + 1);
    if (blockIndex[i] == NO_BLOCK) {
      return false;
    }
    const Block* const blocks =
        reinterpret_cast<const Block*>(blockIndex + mBlockIndexCount);
    const Block& block = blocks[blockIndex[i]];
    return ((block.mBits[(aIndex >> 3) & (BLOCK_SIZE - 1)]) &
            (1 << (aIndex & 0x7))) != 0;
  }

  bool Equals(const gfxSparseBitSet* aOther) const {
    if (mBlockIndexCount != aOther->mBlockIndex.Length()) {
      return false;
    }
    const uint16_t* const blockIndex =
        reinterpret_cast<const uint16_t*>(this + 1);
    const Block* const blocks =
        reinterpret_cast<const Block*>(blockIndex + mBlockIndexCount);
    for (uint16_t i = 0; i < mBlockIndexCount; ++i) {
      uint16_t index = blockIndex[i];
      uint16_t otherIndex = aOther->mBlockIndex[i];
      if ((index == NO_BLOCK) != (otherIndex == NO_BLOCK)) {
        return false;
      }
      if (index == NO_BLOCK) {
        continue;
      }
      const Block& b1 = blocks[index];
      const Block& b2 = aOther->mBlocks[otherIndex];
      if (memcmp(&b1.mBits, &b2.mBits, BLOCK_SIZE) != 0) {
        return false;
      }
    }
    return true;
  }

 private:
  friend class gfxSparseBitSet;
  SharedBitSet() = delete;

  explicit SharedBitSet(const gfxSparseBitSet& aBitset)
      : mBlockIndexCount(
            mozilla::AssertedCast<uint16_t>(aBitset.mBlockIndex.Length())),
        mBlockCount(0) {
    uint16_t* blockIndex = reinterpret_cast<uint16_t*>(this + 1);
    Block* blocks = reinterpret_cast<Block*>(blockIndex + mBlockIndexCount);
    for (uint16_t i = 0; i < mBlockIndexCount; i++) {
      if (aBitset.mBlockIndex[i] != NO_BLOCK) {
        const Block& srcBlock = aBitset.mBlocks[aBitset.mBlockIndex[i]];
        std::memcpy(&blocks[mBlockCount], &srcBlock, sizeof(Block));
        blockIndex[i] = mBlockCount;
        mBlockCount++;
      } else {
        blockIndex[i] = NO_BLOCK;
      }
    }
  }

  // We never manage SharedBitSet as a "normal" object, it's a view onto a
  // buffer of shared memory. So we should never be trying to call this.
  ~SharedBitSet() = delete;

  uint16_t mBlockIndexCount;
  uint16_t mBlockCount;

  // After the two "header" fields above, we have a block index array
  // of uint16_t[mBlockIndexCount], followed by mBlockCount Block records.
};

// Union the contents of a SharedBitSet with the target gfxSparseBitSet
inline void gfxSparseBitSet::Union(const SharedBitSet& aBitset) {
  // ensure mBlockIndex is large enough
  while (mBlockIndex.Length() < aBitset.mBlockIndexCount) {
    mBlockIndex.AppendElement(NO_BLOCK);
  }
  auto blockIndex = reinterpret_cast<const uint16_t*>(&aBitset + 1);
  auto blocks =
      reinterpret_cast<const Block*>(blockIndex + aBitset.mBlockIndexCount);
  for (uint32_t i = 0; i < aBitset.mBlockIndexCount; ++i) {
    // if it is missing (implicitly empty) in source, just skip
    if (blockIndex[i] == NO_BLOCK) {
      continue;
    }
    // if the block is missing, just copy from source bitset
    if (mBlockIndex[i] == NO_BLOCK) {
      mBlocks.AppendElement(blocks[blockIndex[i]]);
      MOZ_ASSERT(mBlocks.Length() < 0xffff, "block index overflow");
      mBlockIndex[i] = uint16_t(mBlocks.Length() - 1);
      continue;
    }
    // Else set existing target block to the union of both.
    // Note that blocks in SharedBitSet may not be 4-byte aligned, so we don't
    // try to optimize by casting to uint32_t* here and processing 4 bytes at
    // once, as this could result in misaligned access.
    uint8_t* dst = reinterpret_cast<uint8_t*>(&mBlocks[mBlockIndex[i]].mBits);
    const uint8_t* src =
        reinterpret_cast<const uint8_t*>(&blocks[blockIndex[i]].mBits);
    for (uint32_t j = 0; j < BLOCK_SIZE; ++j) {
      dst[j] |= src[j];
    }
  }
}

#define TRUETYPE_TAG(a, b, c, d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))

namespace mozilla {

// Byte-swapping types and name table structure definitions moved from
// gfxFontUtils.cpp to .h file so that gfxFont.cpp can also refer to them
#pragma pack(1)

struct AutoSwap_PRUint16 {
#ifdef __SUNPRO_CC
  AutoSwap_PRUint16& operator=(const uint16_t aValue) {
    this->value = mozilla::NativeEndian::swapToBigEndian(aValue);
    return *this;
  }
#else
  MOZ_IMPLICIT AutoSwap_PRUint16(uint16_t aValue) {
    value = mozilla::NativeEndian::swapToBigEndian(aValue);
  }
#endif
  operator uint16_t() const {
    return mozilla::NativeEndian::swapFromBigEndian(value);
  }

  operator uint32_t() const {
    return mozilla::NativeEndian::swapFromBigEndian(value);
  }

  operator uint64_t() const {
    return mozilla::NativeEndian::swapFromBigEndian(value);
  }

 private:
  uint16_t value;
};

struct AutoSwap_PRInt16 {
#ifdef __SUNPRO_CC
  AutoSwap_PRInt16& operator=(const int16_t aValue) {
    this->value = mozilla::NativeEndian::swapToBigEndian(aValue);
    return *this;
  }
#else
  MOZ_IMPLICIT AutoSwap_PRInt16(int16_t aValue) {
    value = mozilla::NativeEndian::swapToBigEndian(aValue);
  }
#endif
  operator int16_t() const {
    return mozilla::NativeEndian::swapFromBigEndian(value);
  }

  operator uint32_t() const {
    return mozilla::NativeEndian::swapFromBigEndian(value);
  }

 private:
  int16_t value;
};

struct AutoSwap_PRUint32 {
#ifdef __SUNPRO_CC
  AutoSwap_PRUint32& operator=(const uint32_t aValue) {
    this->value = mozilla::NativeEndian::swapToBigEndian(aValue);
    return *this;
  }
#else
  MOZ_IMPLICIT AutoSwap_PRUint32(uint32_t aValue) {
    value = mozilla::NativeEndian::swapToBigEndian(aValue);
  }
#endif
  operator uint32_t() const {
    return mozilla::NativeEndian::swapFromBigEndian(value);
  }

 private:
  uint32_t value;
};

struct AutoSwap_PRInt32 {
#ifdef __SUNPRO_CC
  AutoSwap_PRInt32& operator=(const int32_t aValue) {
    this->value = mozilla::NativeEndian::swapToBigEndian(aValue);
    return *this;
  }
#else
  MOZ_IMPLICIT AutoSwap_PRInt32(int32_t aValue) {
    value = mozilla::NativeEndian::swapToBigEndian(aValue);
  }
#endif
  operator int32_t() const {
    return mozilla::NativeEndian::swapFromBigEndian(value);
  }

 private:
  int32_t value;
};

struct AutoSwap_PRUint64 {
#ifdef __SUNPRO_CC
  AutoSwap_PRUint64& operator=(const uint64_t aValue) {
    this->value = mozilla::NativeEndian::swapToBigEndian(aValue);
    return *this;
  }
#else
  MOZ_IMPLICIT AutoSwap_PRUint64(uint64_t aValue) {
    value = mozilla::NativeEndian::swapToBigEndian(aValue);
  }
#endif
  operator uint64_t() const {
    return mozilla::NativeEndian::swapFromBigEndian(value);
  }

 private:
  uint64_t value;
};

struct AutoSwap_PRUint24 {
  operator uint32_t() const {
    return value[0] << 16 | value[1] << 8 | value[2];
  }

 private:
  AutoSwap_PRUint24() = default;
  uint8_t value[3];
};

struct SFNTHeader {
  AutoSwap_PRUint32 sfntVersion;    // Fixed, 0x00010000 for version 1.0.
  AutoSwap_PRUint16 numTables;      // Number of tables.
  AutoSwap_PRUint16 searchRange;    // (Maximum power of 2 <= numTables) x 16.
  AutoSwap_PRUint16 entrySelector;  // Log2(maximum power of 2 <= numTables).
  AutoSwap_PRUint16 rangeShift;     // NumTables x 16-searchRange.
};

struct TTCHeader {
  AutoSwap_PRUint32 ttcTag;  // 4 -byte identifier 'ttcf'.
  AutoSwap_PRUint16 majorVersion;
  AutoSwap_PRUint16 minorVersion;
  AutoSwap_PRUint32 numFonts;
  // followed by:
  // AutoSwap_PRUint32 offsetTable[numFonts]
};

struct TableDirEntry {
  AutoSwap_PRUint32 tag;       // 4 -byte identifier.
  AutoSwap_PRUint32 checkSum;  // CheckSum for this table.
  AutoSwap_PRUint32 offset;    // Offset from beginning of TrueType font file.
  AutoSwap_PRUint32 length;    // Length of this table.
};

struct HeadTable {
  enum {
    HEAD_VERSION = 0x00010000,
    HEAD_MAGIC_NUMBER = 0x5F0F3CF5,
    HEAD_CHECKSUM_CALC_CONST = 0xB1B0AFBA
  };

  AutoSwap_PRUint32 tableVersionNumber;  // Fixed, 0x00010000 for version 1.0.
  AutoSwap_PRUint32 fontRevision;        // Set by font manufacturer.
  AutoSwap_PRUint32
      checkSumAdjustment;  // To compute: set it to 0, sum the entire font as
                           // ULONG, then store 0xB1B0AFBA - sum.
  AutoSwap_PRUint32 magicNumber;  // Set to 0x5F0F3CF5.
  AutoSwap_PRUint16 flags;
  AutoSwap_PRUint16
      unitsPerEm;  // Valid range is from 16 to 16384. This value should be a
                   // power of 2 for fonts that have TrueType outlines.
  AutoSwap_PRUint64 created;  // Number of seconds since 12:00 midnight, January
                              // 1, 1904. 64-bit integer
  AutoSwap_PRUint64 modified;       // Number of seconds since 12:00 midnight,
                                    // January 1, 1904. 64-bit integer
  AutoSwap_PRInt16 xMin;            // For all glyph bounding boxes.
  AutoSwap_PRInt16 yMin;            // For all glyph bounding boxes.
  AutoSwap_PRInt16 xMax;            // For all glyph bounding boxes.
  AutoSwap_PRInt16 yMax;            // For all glyph bounding boxes.
  AutoSwap_PRUint16 macStyle;       // Bit 0: Bold (if set to 1);
  AutoSwap_PRUint16 lowestRecPPEM;  // Smallest readable size in pixels.
  AutoSwap_PRInt16 fontDirectionHint;
  AutoSwap_PRInt16 indexToLocFormat;
  AutoSwap_PRInt16 glyphDataFormat;
};

struct OS2Table {
  AutoSwap_PRUint16 version;  // 0004 = OpenType 1.5
  AutoSwap_PRInt16 xAvgCharWidth;
  AutoSwap_PRUint16 usWeightClass;
  AutoSwap_PRUint16 usWidthClass;
  AutoSwap_PRUint16 fsType;
  AutoSwap_PRInt16 ySubscriptXSize;
  AutoSwap_PRInt16 ySubscriptYSize;
  AutoSwap_PRInt16 ySubscriptXOffset;
  AutoSwap_PRInt16 ySubscriptYOffset;
  AutoSwap_PRInt16 ySuperscriptXSize;
  AutoSwap_PRInt16 ySuperscriptYSize;
  AutoSwap_PRInt16 ySuperscriptXOffset;
  AutoSwap_PRInt16 ySuperscriptYOffset;
  AutoSwap_PRInt16 yStrikeoutSize;
  AutoSwap_PRInt16 yStrikeoutPosition;
  AutoSwap_PRInt16 sFamilyClass;
  uint8_t panose[10];
  AutoSwap_PRUint32 unicodeRange1;
  AutoSwap_PRUint32 unicodeRange2;
  AutoSwap_PRUint32 unicodeRange3;
  AutoSwap_PRUint32 unicodeRange4;
  uint8_t achVendID[4];
  AutoSwap_PRUint16 fsSelection;
  AutoSwap_PRUint16 usFirstCharIndex;
  AutoSwap_PRUint16 usLastCharIndex;
  AutoSwap_PRInt16 sTypoAscender;
  AutoSwap_PRInt16 sTypoDescender;
  AutoSwap_PRInt16 sTypoLineGap;
  AutoSwap_PRUint16 usWinAscent;
  AutoSwap_PRUint16 usWinDescent;
  AutoSwap_PRUint32 codePageRange1;
  AutoSwap_PRUint32 codePageRange2;
  AutoSwap_PRInt16 sxHeight;
  AutoSwap_PRInt16 sCapHeight;
  AutoSwap_PRUint16 usDefaultChar;
  AutoSwap_PRUint16 usBreakChar;
  AutoSwap_PRUint16 usMaxContext;
};

struct PostTable {
  AutoSwap_PRUint32 version;
  AutoSwap_PRInt32 italicAngle;
  AutoSwap_PRInt16 underlinePosition;
  AutoSwap_PRUint16 underlineThickness;
  AutoSwap_PRUint32 isFixedPitch;
  AutoSwap_PRUint32 minMemType42;
  AutoSwap_PRUint32 maxMemType42;
  AutoSwap_PRUint32 minMemType1;
  AutoSwap_PRUint32 maxMemType1;
};

// This structure is used for both 'hhea' and 'vhea' tables.
// The field names here are those of the horizontal version; the
// vertical table just exchanges vertical and horizontal coordinates.
struct MetricsHeader {
  AutoSwap_PRUint32 version;
  AutoSwap_PRInt16 ascender;
  AutoSwap_PRInt16 descender;
  AutoSwap_PRInt16 lineGap;
  AutoSwap_PRUint16 advanceWidthMax;
  AutoSwap_PRInt16 minLeftSideBearing;
  AutoSwap_PRInt16 minRightSideBearing;
  AutoSwap_PRInt16 xMaxExtent;
  AutoSwap_PRInt16 caretSlopeRise;
  AutoSwap_PRInt16 caretSlopeRun;
  AutoSwap_PRInt16 caretOffset;
  AutoSwap_PRInt16 reserved1;
  AutoSwap_PRInt16 reserved2;
  AutoSwap_PRInt16 reserved3;
  AutoSwap_PRInt16 reserved4;
  AutoSwap_PRInt16 metricDataFormat;
  AutoSwap_PRUint16 numOfLongMetrics;
};

struct MaxpTableHeader {
  AutoSwap_PRUint32 version;  // CFF: 0x00005000; TrueType: 0x00010000
  AutoSwap_PRUint16 numGlyphs;
  // truetype version has additional fields that we don't currently use
};

// old 'kern' table, supported on Windows
// see http://www.microsoft.com/typography/otspec/kern.htm
struct KernTableVersion0 {
  AutoSwap_PRUint16 version;  // 0x0000
  AutoSwap_PRUint16 nTables;
};

struct KernTableSubtableHeaderVersion0 {
  AutoSwap_PRUint16 version;
  AutoSwap_PRUint16 length;
  AutoSwap_PRUint16 coverage;
};

// newer Mac-only 'kern' table, ignored by Windows
// see http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6kern.html
struct KernTableVersion1 {
  AutoSwap_PRUint32 version;  // 0x00010000
  AutoSwap_PRUint32 nTables;
};

struct KernTableSubtableHeaderVersion1 {
  AutoSwap_PRUint32 length;
  AutoSwap_PRUint16 coverage;
  AutoSwap_PRUint16 tupleIndex;
};

#pragma pack()

// Return just the highest bit of the given value, i.e., the highest
// power of 2 that is <= value, or zero if the input value is zero.
inline uint32_t FindHighestBit(uint32_t value) {
  // propagate highest bit into all lower bits of the value
  value |= (value >> 1);
  value |= (value >> 2);
  value |= (value >> 4);
  value |= (value >> 8);
  value |= (value >> 16);
  // isolate the leftmost bit
  return (value & ~(value >> 1));
}

}  // namespace mozilla

// used for overlaying name changes without touching original font data
struct FontDataOverlay {
  // overlaySrc != 0 ==> use overlay
  uint32_t overlaySrc;     // src offset from start of font data
  uint32_t overlaySrcLen;  // src length
  uint32_t overlayDest;    // dest offset from start of font data
};

enum gfxUserFontType {
  GFX_USERFONT_UNKNOWN = 0,
  GFX_USERFONT_OPENTYPE = 1,
  GFX_USERFONT_SVG = 2,
  GFX_USERFONT_WOFF = 3,
  GFX_USERFONT_WOFF2 = 4
};

extern const uint8_t sCJKCompatSVSTable[];

class gfxFontUtils {
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
    PLATFORM_ID_UNICODE = 0,  // Mac OS uses this typically
    PLATFORM_ID_MAC = 1,
    PLATFORM_ID_ISO = 2,
    PLATFORM_ID_MICROSOFT = 3,

    ENCODING_ID_MAC_ROMAN = 0,  // traditional Mac OS script manager encodings
    ENCODING_ID_MAC_JAPANESE =
        1,  // (there are others defined, but some were never
    ENCODING_ID_MAC_TRAD_CHINESE =
        2,  // implemented by Apple, and I have never seen them
    ENCODING_ID_MAC_KOREAN = 3,  // used in font names)
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
    ENCODING_ID_MICROSOFT_JOHAB = 6,
    ENCODING_ID_MICROSOFT_UNICODEFULL = 10,

    LANG_ALL = -1,
    LANG_ID_MAC_ENGLISH = 0,  // many others are defined, but most don't affect
    LANG_ID_MAC_HEBREW =
        10,  // the charset; should check all the central/eastern
    LANG_ID_MAC_JAPANESE = 11,  // european codes, though
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

    LANG_ID_MICROSOFT_EN_US =
        0x0409,  // with Microsoft platformID, EN US lang code

    CMAP_MAX_CODEPOINT = 0x10ffff  // maximum possible Unicode codepoint
                                   // contained in a cmap
  };

  // name table has a header, followed by name records, followed by string data
  struct NameHeader {
    mozilla::AutoSwap_PRUint16 format;        // Format selector (=0).
    mozilla::AutoSwap_PRUint16 count;         // Number of name records.
    mozilla::AutoSwap_PRUint16 stringOffset;  // Offset to start of string
                                              // storage (from start of table)
  };

  struct NameRecord {
    mozilla::AutoSwap_PRUint16 platformID;  // Platform ID
    mozilla::AutoSwap_PRUint16 encodingID;  // Platform-specific encoding ID
    mozilla::AutoSwap_PRUint16 languageID;  // Language ID
    mozilla::AutoSwap_PRUint16 nameID;      // Name ID.
    mozilla::AutoSwap_PRUint16 length;      // String length (in bytes).
    mozilla::AutoSwap_PRUint16 offset;  // String offset from start of storage
                                        // (in bytes).
  };

  // Helper to ensure we free a font table when we return.
  class AutoHBBlob {
   public:
    explicit AutoHBBlob(hb_blob_t* aBlob) : mBlob(aBlob) {}

    ~AutoHBBlob() { hb_blob_destroy(mBlob); }

    operator hb_blob_t*() { return mBlob; }

   private:
    hb_blob_t* const mBlob;
  };

  // for reading big-endian font data on either big or little-endian platforms

  static inline uint16_t ReadShortAt(const uint8_t* aBuf, uint32_t aIndex) {
    return static_cast<uint16_t>(aBuf[aIndex] << 8) | aBuf[aIndex + 1];
  }

  static inline uint16_t ReadShortAt16(const uint16_t* aBuf, uint32_t aIndex) {
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(aBuf);
    uint32_t index = aIndex << 1;
    return static_cast<uint16_t>(buf[index] << 8) | buf[index + 1];
  }

  static inline uint32_t ReadUint24At(const uint8_t* aBuf, uint32_t aIndex) {
    return ((aBuf[aIndex] << 16) | (aBuf[aIndex + 1] << 8) |
            (aBuf[aIndex + 2]));
  }

  static inline uint32_t ReadLongAt(const uint8_t* aBuf, uint32_t aIndex) {
    return ((aBuf[aIndex] << 24) | (aBuf[aIndex + 1] << 16) |
            (aBuf[aIndex + 2] << 8) | (aBuf[aIndex + 3]));
  }

  static nsresult ReadCMAPTableFormat10(const uint8_t* aBuf, uint32_t aLength,
                                        gfxSparseBitSet& aCharacterMap);

  static nsresult ReadCMAPTableFormat12or13(const uint8_t* aBuf,
                                            uint32_t aLength,
                                            gfxSparseBitSet& aCharacterMap);

  static nsresult ReadCMAPTableFormat4(const uint8_t* aBuf, uint32_t aLength,
                                       gfxSparseBitSet& aCharacterMap,
                                       bool aIsSymbolFont);

  static nsresult ReadCMAPTableFormat14(const uint8_t* aBuf, uint32_t aLength,
                                        const uint8_t*& aTable);

  static uint32_t FindPreferredSubtable(const uint8_t* aBuf,
                                        uint32_t aBufLength,
                                        uint32_t* aTableOffset,
                                        uint32_t* aUVSTableOffset,
                                        bool* aIsSymbolFont);

  static nsresult ReadCMAP(const uint8_t* aBuf, uint32_t aBufLength,
                           gfxSparseBitSet& aCharacterMap,
                           uint32_t& aUVSOffset);

  static uint32_t MapCharToGlyphFormat4(const uint8_t* aBuf, uint32_t aLength,
                                        char16_t aCh);

  static uint32_t MapCharToGlyphFormat10(const uint8_t* aBuf, uint32_t aCh);

  static uint32_t MapCharToGlyphFormat12or13(const uint8_t* aBuf, uint32_t aCh);

  static uint16_t MapUVSToGlyphFormat14(const uint8_t* aBuf, uint32_t aCh,
                                        uint32_t aVS);

  // sCJKCompatSVSTable is a 'cmap' format 14 subtable that maps
  // <char + var-selector> pairs to the corresponding Unicode
  // compatibility ideograph codepoints.
  static MOZ_ALWAYS_INLINE uint32_t GetUVSFallback(uint32_t aCh, uint32_t aVS) {
    aCh = MapUVSToGlyphFormat14(sCJKCompatSVSTable, aCh, aVS);
    return aCh >= 0xFB00 ? aCh + (0x2F800 - 0xFB00) : aCh;
  }

  static uint32_t MapCharToGlyph(const uint8_t* aCmapBuf, uint32_t aBufLength,
                                 uint32_t aUnicode, uint32_t aVarSelector = 0);

  // For legacy MS Symbol fonts, we try mapping 8-bit character codes to the
  // Private Use range at U+F0xx used by the cmaps in these fonts.
  static MOZ_ALWAYS_INLINE uint32_t MapLegacySymbolFontCharToPUA(uint32_t aCh) {
    return aCh >= 0x20 && aCh <= 0xff ? 0xf000 + aCh : 0;
  }

#ifdef XP_WIN
  // determine whether a font (which has already been sanitized, so is known
  // to be a valid sfnt) is CFF format rather than TrueType
  static bool IsCffFont(const uint8_t* aFontData);
#endif

  // determine the format of font data
  static gfxUserFontType DetermineFontDataType(const uint8_t* aFontData,
                                               uint32_t aFontDataLength);

  // Read the fullname from the sfnt data (used to save the original name
  // prior to renaming the font for installation).
  // This is called with sfnt data that has already been validated,
  // so it should always succeed in finding the name table.
  static nsresult GetFullNameFromSFNT(const uint8_t* aFontData,
                                      uint32_t aLength, nsACString& aFullName);

  // helper to get fullname from name table, constructing from family+style
  // if no explicit fullname is present
  static nsresult GetFullNameFromTable(hb_blob_t* aNameTable,
                                       nsACString& aFullName);

  // helper to get family name from name table
  static nsresult GetFamilyNameFromTable(hb_blob_t* aNameTable,
                                         nsACString& aFamilyName);

  // Find the table directory entry for a given table tag, in a (validated)
  // buffer of 'sfnt' data. Returns null if the tag is not present.
  static mozilla::TableDirEntry* FindTableDirEntry(const void* aFontData,
                                                   uint32_t aTableTag);

  // Return a blob that wraps a table found within a buffer of font data.
  // The blob does NOT own its data; caller guarantees that the buffer
  // will remain valid at least as long as the blob.
  // Returns null if the specified table is not found.
  // This method assumes aFontData is valid 'sfnt' data; before using this,
  // caller is responsible to do any sanitization/validation necessary.
  static hb_blob_t* GetTableFromFontData(const void* aFontData,
                                         uint32_t aTableTag);

  // create a new name table and build a new font with that name table
  // appended on the end, returns true on success
  static nsresult RenameFont(const nsAString& aName, const uint8_t* aFontData,
                             uint32_t aFontDataLength,
                             FallibleTArray<uint8_t>* aNewFont);

  // read all names matching aNameID, returning in aNames array
  static nsresult ReadNames(const char* aNameData, uint32_t aDataLen,
                            uint32_t aNameID, int32_t aPlatformID,
                            nsTArray<nsCString>& aNames);

  // reads English or first name matching aNameID, returning in aName
  // platform based on OS
  static nsresult ReadCanonicalName(hb_blob_t* aNameTable, uint32_t aNameID,
                                    nsCString& aName);

  static nsresult ReadCanonicalName(const char* aNameData, uint32_t aDataLen,
                                    uint32_t aNameID, nsCString& aName);

  // convert a name from the raw name table data into an nsString,
  // provided we know how; return true if successful, or false
  // if we can't handle the encoding
  static bool DecodeFontName(const char* aBuf, int32_t aLength,
                             uint32_t aPlatformCode, uint32_t aScriptCode,
                             uint32_t aLangCode, nsACString& dest);

  static inline bool IsJoinCauser(uint32_t ch) { return (ch == 0x200D); }

  // We treat Combining Grapheme Joiner (U+034F) together with the join
  // controls (ZWJ, ZWNJ) here, because (like them) it is an invisible
  // char that will be handled by the shaper even if not explicitly
  // supported by the font. (See bug 1408366.)
  static inline bool IsJoinControl(uint32_t ch) {
    return (ch == 0x200C || ch == 0x200D || ch == 0x034f);
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

  enum {
    kUnicodeRegionalIndicatorA = 0x1F1E6,
    kUnicodeRegionalIndicatorZ = 0x1F1FF
  };

  static inline bool IsRegionalIndicator(uint32_t aCh) {
    return aCh >= kUnicodeRegionalIndicatorA &&
           aCh <= kUnicodeRegionalIndicatorZ;
  }

  static inline bool IsInvalid(uint32_t ch) { return (ch == 0xFFFD); }

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

  static inline bool PotentialRTLChar(char16_t aCh) {
    if (aCh >= kUnicodeBidiScriptsStart && aCh <= kUnicodeBidiScriptsEnd)
      // bidi scripts Hebrew, Arabic, Syriac, Thaana, N'Ko are all encoded
      // together
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

  // parse a simple list of font family names into
  // an array of strings
  static void ParseFontList(const nsACString& aFamilyList,
                            nsTArray<nsCString>& aFontList);

  // for a given pref name, initialize a list of font names
  static void GetPrefsFontList(const char* aPrefName,
                               nsTArray<nsCString>& aFontList,
                               bool aLocalized = false);

  // generate a unique font name
  static nsresult MakeUniqueUserFontName(nsAString& aName);

  // Helper used to implement gfxFontEntry::GetVariation{Axes,Instances} for
  // platforms where the native font APIs don't provide the info we want
  // in a convenient form, or when native APIs are too expensive.
  // (Not used on platforms -- currently, freetype -- where the font APIs
  // expose variation instance details directly.)
  static void GetVariationData(gfxFontEntry* aFontEntry,
                               nsTArray<gfxFontVariationAxis>* aAxes,
                               nsTArray<gfxFontVariationInstance>* aInstances);

  // Helper method for reading localized family names from the name table
  // of a single face.
  static void ReadOtherFamilyNamesForFace(
      const nsACString& aFamilyName, const char* aNameData,
      uint32_t aDataLength, nsTArray<nsCString>& aOtherFamilyNames,
      bool useFullName);

  // Main, DOM worker or servo thread safe method to check if we are performing
  // Servo traversal.
  static bool IsInServoTraversal();

  // Main, DOM worker or servo thread safe method to get the current
  // ServoTypeSet. Always returns nullptr for DOM worker threads.
  static mozilla::ServoStyleSet* CurrentServoStyleSet();

  static void AssertSafeThreadOrServoFontMetricsLocked()
#ifdef DEBUG
      ;
#else
  {
  }
#endif

 protected:
  friend struct MacCharsetMappingComparator;

  static nsresult ReadNames(const char* aNameData, uint32_t aDataLen,
                            uint32_t aNameID, int32_t aLangID,
                            int32_t aPlatformID, nsTArray<nsCString>& aNames);

  // convert opentype name-table platform/encoding/language values to an
  // Encoding object we can use to convert the name data to unicode
  static const mozilla::Encoding* GetCharsetForFontName(uint16_t aPlatform,
                                                        uint16_t aScript,
                                                        uint16_t aLanguage);

  struct MacFontNameCharsetMapping {
    uint16_t mScript;
    uint16_t mLanguage;
    const mozilla::Encoding* mEncoding;

    bool operator<(const MacFontNameCharsetMapping& rhs) const {
      return (mScript < rhs.mScript) ||
             ((mScript == rhs.mScript) && (mLanguage < rhs.mLanguage));
    }
  };
  static const MacFontNameCharsetMapping gMacFontNameCharsets[];
  static const mozilla::Encoding* gISOFontNameCharsets[];
  static const mozilla::Encoding* gMSFontNameCharsets[];
};

// Factors used to weight the distances between the available and target font
// properties during font-matching. These ensure that we respect the CSS-fonts
// requirement that font-stretch >> font-style >> font-weight; and in addition,
// a mismatch between the desired and actual glyph presentation (emoji vs text)
// will take precedence over any of the style attributes.
constexpr double kPresentationMismatch = 1.0e12;
constexpr double kStretchFactor = 1.0e8;
constexpr double kStyleFactor = 1.0e4;
constexpr double kWeightFactor = 1.0e0;

// style distance ==> [0,500]
static inline double StyleDistance(const mozilla::SlantStyleRange& aRange,
                                   mozilla::FontSlantStyle aTargetStyle) {
  const mozilla::FontSlantStyle minStyle = aRange.Min();
  if (aTargetStyle == minStyle) {
    return 0.0;  // styles match exactly ==> 0
  }

  // bias added to angle difference when searching in the non-preferred
  // direction from a target angle
  const double kReverse = 100.0;

  // bias added when we've crossed from positive to negative angles or
  // vice versa
  const double kNegate = 200.0;

  if (aTargetStyle.IsNormal()) {
    if (minStyle.IsOblique()) {
      // to distinguish oblique 0deg from normal, we add 1.0 to the angle
      const double minAngle = minStyle.ObliqueAngle();
      if (minAngle >= 0.0) {
        return 1.0 + minAngle;
      }
      const mozilla::FontSlantStyle maxStyle = aRange.Max();
      const double maxAngle = maxStyle.ObliqueAngle();
      if (maxAngle >= 0.0) {
        // [min,max] range includes 0.0, so just return our minimum
        return 1.0;
      }
      // negative oblique is even worse than italic
      return kNegate - maxAngle;
    }
    // must be italic, which is worse than any non-negative oblique;
    // treat as a match in the wrong search direction
    MOZ_ASSERT(minStyle.IsItalic());
    return kReverse;
  }

  const double kDefaultAngle = mozilla::FontSlantStyle::OBLIQUE.ObliqueAngle();

  if (aTargetStyle.IsItalic()) {
    if (minStyle.IsOblique()) {
      const double minAngle = minStyle.ObliqueAngle();
      if (minAngle >= kDefaultAngle) {
        return 1.0 + (minAngle - kDefaultAngle);
      }
      const mozilla::FontSlantStyle maxStyle = aRange.Max();
      const double maxAngle = maxStyle.ObliqueAngle();
      if (maxAngle >= kDefaultAngle) {
        return 1.0;
      }
      if (maxAngle > 0.0) {
        // wrong direction but still > 0, add bias of 100
        return kReverse + (kDefaultAngle - maxAngle);
      }
      // negative oblique angle, add bias of 300
      return kReverse + kNegate + (kDefaultAngle - maxAngle);
    }
    // normal is worse than oblique > 0, but better than oblique <= 0
    MOZ_ASSERT(minStyle.IsNormal());
    return kNegate;
  }

  // target is oblique <angle>: four different cases depending on
  // the value of the <angle>, which determines the preferred direction
  // of search
  const double targetAngle = aTargetStyle.ObliqueAngle();
  if (targetAngle >= kDefaultAngle) {
    if (minStyle.IsOblique()) {
      const double minAngle = minStyle.ObliqueAngle();
      if (minAngle >= targetAngle) {
        return minAngle - targetAngle;
      }
      const mozilla::FontSlantStyle maxStyle = aRange.Max();
      const double maxAngle = maxStyle.ObliqueAngle();
      if (maxAngle >= targetAngle) {
        return 0.0;
      }
      if (maxAngle > 0.0) {
        return kReverse + (targetAngle - maxAngle);
      }
      return kReverse + kNegate + (targetAngle - maxAngle);
    }
    if (minStyle.IsItalic()) {
      return kReverse + kNegate;
    }
    return kReverse + kNegate + 1.0;
  }

  if (targetAngle <= -kDefaultAngle) {
    if (minStyle.IsOblique()) {
      const mozilla::FontSlantStyle maxStyle = aRange.Max();
      const double maxAngle = maxStyle.ObliqueAngle();
      if (maxAngle <= targetAngle) {
        return targetAngle - maxAngle;
      }
      const double minAngle = minStyle.ObliqueAngle();
      if (minAngle <= targetAngle) {
        return 0.0;
      }
      if (minAngle < 0.0) {
        return kReverse + (minAngle - targetAngle);
      }
      return kReverse + kNegate + (minAngle - targetAngle);
    }
    if (minStyle.IsItalic()) {
      return kReverse + kNegate;
    }
    return kReverse + kNegate + 1.0;
  }

  if (targetAngle >= 0.0) {
    if (minStyle.IsOblique()) {
      const double minAngle = minStyle.ObliqueAngle();
      if (minAngle > targetAngle) {
        return kReverse + (minAngle - targetAngle);
      }
      const mozilla::FontSlantStyle maxStyle = aRange.Max();
      const double maxAngle = maxStyle.ObliqueAngle();
      if (maxAngle >= targetAngle) {
        return 0.0;
      }
      if (maxAngle > 0.0) {
        return targetAngle - maxAngle;
      }
      return kReverse + kNegate + (targetAngle - maxAngle);
    }
    if (minStyle.IsItalic()) {
      return kReverse + kNegate - 2.0;
    }
    return kReverse + kNegate - 1.0;
  }

  // last case: (targetAngle < 0.0 && targetAngle > kDefaultAngle)
  if (minStyle.IsOblique()) {
    const mozilla::FontSlantStyle maxStyle = aRange.Max();
    const double maxAngle = maxStyle.ObliqueAngle();
    if (maxAngle < targetAngle) {
      return kReverse + (targetAngle - maxAngle);
    }
    const double minAngle = minStyle.ObliqueAngle();
    if (minAngle <= targetAngle) {
      return 0.0;
    }
    if (minAngle < 0.0) {
      return minAngle - targetAngle;
    }
    return kReverse + kNegate + (minAngle - targetAngle);
  }
  if (minStyle.IsItalic()) {
    return kReverse + kNegate - 2.0;
  }
  return kReverse + kNegate - 1.0;
}

// stretch distance ==> [0,2000]
static inline double StretchDistance(const mozilla::StretchRange& aRange,
                                     mozilla::FontStretch aTargetStretch) {
  const double kReverseDistance = 1000.0;

  mozilla::FontStretch minStretch = aRange.Min();
  mozilla::FontStretch maxStretch = aRange.Max();

  // The stretch value is a (non-negative) percentage; currently we support
  // values in the range 0 .. 1000. (If the upper limit is ever increased,
  // the kReverseDistance value used here may need to be adjusted.)
  // If aTargetStretch is >100, we prefer larger values if available;
  // if <=100, we prefer smaller values if available.
  if (aTargetStretch < minStretch) {
    if (aTargetStretch > mozilla::FontStretch::NORMAL) {
      return minStretch.ToFloat() - aTargetStretch.ToFloat();
    }
    return (minStretch.ToFloat() - aTargetStretch.ToFloat()) + kReverseDistance;
  }
  if (aTargetStretch > maxStretch) {
    if (aTargetStretch <= mozilla::FontStretch::NORMAL) {
      return aTargetStretch.ToFloat() - maxStretch.ToFloat();
    }
    return (aTargetStretch.ToFloat() - maxStretch.ToFloat()) + kReverseDistance;
  }
  return 0.0;
}

// Calculate weight distance with values in the range (0..1000). In general,
// heavier weights match towards even heavier weights while lighter weights
// match towards even lighter weights. Target weight values in the range
// [400..500] are special, since they will first match up to 500, then down
// towards 0, then up again towards 999.
//
// Example: with target 600 and font weight 800, distance will be 200. With
// target 300 and font weight 600, distance will be 900, since heavier
// weights are farther away than lighter weights. If the target is 5 and the
// font weight 995, the distance would be 1590 for the same reason.

// weight distance ==> [0,1600]
static inline double WeightDistance(const mozilla::WeightRange& aRange,
                                    mozilla::FontWeight aTargetWeight) {
  const double kNotWithinCentralRange = 100.0;
  const double kReverseDistance = 600.0;

  mozilla::FontWeight minWeight = aRange.Min();
  mozilla::FontWeight maxWeight = aRange.Max();

  if (aTargetWeight >= minWeight && aTargetWeight <= maxWeight) {
    // Target is within the face's range, so it's a perfect match
    return 0.0;
  }

  if (aTargetWeight < mozilla::FontWeight::NORMAL) {
    // Requested a lighter-than-400 weight
    if (maxWeight < aTargetWeight) {
      return aTargetWeight.ToFloat() - maxWeight.ToFloat();
    }
    // Add reverse-search penalty for bolder faces
    return (minWeight.ToFloat() - aTargetWeight.ToFloat()) + kReverseDistance;
  }

  if (aTargetWeight > mozilla::FontWeight::FromInt(500)) {
    // Requested a bolder-than-500 weight
    if (minWeight > aTargetWeight) {
      return minWeight.ToFloat() - aTargetWeight.ToFloat();
    }
    // Add reverse-search penalty for lighter faces
    return (aTargetWeight.ToFloat() - maxWeight.ToFloat()) + kReverseDistance;
  }

  // Special case for requested weight in the [400..500] range
  if (minWeight > aTargetWeight) {
    if (minWeight <= mozilla::FontWeight::FromInt(500)) {
      // Bolder weight up to 500 is first choice
      return minWeight.ToFloat() - aTargetWeight.ToFloat();
    }
    // Other bolder weights get a reverse-search penalty
    return (minWeight.ToFloat() - aTargetWeight.ToFloat()) + kReverseDistance;
  }
  // Lighter weights are not as good as bolder ones within [400..500]
  return (aTargetWeight.ToFloat() - maxWeight.ToFloat()) +
         kNotWithinCentralRange;
}

#endif /* GFX_FONT_UTILS_H */
