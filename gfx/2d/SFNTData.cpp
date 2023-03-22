/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SFNTData.h"

#include <algorithm>
#include <numeric>

#include "BigEndianInts.h"
#include "Logging.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Span.h"

namespace mozilla {
namespace gfx {

#define TRUETYPE_TAG(a, b, c, d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))

#pragma pack(push, 1)

struct TTCHeader {
  BigEndianUint32 ttcTag;   // Always 'ttcf'
  BigEndianUint32 version;  // Fixed, 0x00010000
  BigEndianUint32 numFonts;
};

struct OffsetTable {
  BigEndianUint32 sfntVersion;  // Fixed, 0x00010000 for version 1.0.
  BigEndianUint16 numTables;
  BigEndianUint16 searchRange;    // (Maximum power of 2 <= numTables) x 16.
  BigEndianUint16 entrySelector;  // Log2(maximum power of 2 <= numTables).
  BigEndianUint16 rangeShift;     // NumTables x 16-searchRange.
};

struct TableDirEntry {
  BigEndianUint32 tag;       // 4 -byte identifier.
  BigEndianUint32 checkSum;  // CheckSum for this table.
  BigEndianUint32 offset;    // Offset from beginning of TrueType font file.
  BigEndianUint32 length;    // Length of this table.

  friend bool operator<(const TableDirEntry& lhs, const uint32_t aTag) {
    return lhs.tag < aTag;
  }
};

#pragma pack(pop)

class SFNTData::Font {
 public:
  Font(const OffsetTable* aOffsetTable, const uint8_t* aFontData,
       uint32_t aDataLength)
      : mFontData(aFontData),
        mFirstDirEntry(
            reinterpret_cast<const TableDirEntry*>(aOffsetTable + 1)),
        mEndOfDirEntries(mFirstDirEntry + aOffsetTable->numTables),
        mDataLength(aDataLength) {}

  Span<const uint8_t> GetHeadTableBytes() const {
    const TableDirEntry* dirEntry =
        GetDirEntry(TRUETYPE_TAG('h', 'e', 'a', 'd'));
    if (!dirEntry) {
      gfxWarning() << "Head table entry not found.";
      return {};
    }

    return {mFontData + dirEntry->offset, dirEntry->length};
  }

  Span<const uint8_t> GetCmapTableBytes() const {
    const TableDirEntry* dirEntry =
        GetDirEntry(TRUETYPE_TAG('c', 'm', 'a', 'p'));
    if (!dirEntry) {
      gfxWarning() << "Cmap table entry not found.";
      return {};
    }

    return {mFontData + dirEntry->offset, dirEntry->length};
  }

 private:
  const TableDirEntry* GetDirEntry(const uint32_t aTag) const {
    const TableDirEntry* foundDirEntry =
        std::lower_bound(mFirstDirEntry, mEndOfDirEntries, aTag);

    if (foundDirEntry == mEndOfDirEntries || foundDirEntry->tag != aTag) {
      gfxWarning() << "Font data does not contain tag.";
      return nullptr;
    }

    if (mDataLength < (foundDirEntry->offset + foundDirEntry->length)) {
      gfxWarning() << "Font data too short to contain table.";
      return nullptr;
    }

    return foundDirEntry;
  }

  const uint8_t* mFontData;
  const TableDirEntry* mFirstDirEntry;
  const TableDirEntry* mEndOfDirEntries;
  uint32_t mDataLength;
};

/* static */
UniquePtr<SFNTData> SFNTData::Create(const uint8_t* aFontData,
                                     uint32_t aDataLength) {
  MOZ_ASSERT(aFontData);

  // Check to see if this is a font collection.
  if (aDataLength < sizeof(TTCHeader)) {
    gfxWarning() << "Font data too short.";
    return nullptr;
  }

  const TTCHeader* ttcHeader = reinterpret_cast<const TTCHeader*>(aFontData);
  if (ttcHeader->ttcTag == TRUETYPE_TAG('t', 't', 'c', 'f')) {
    uint32_t numFonts = ttcHeader->numFonts;
    if (aDataLength <
        sizeof(TTCHeader) + (numFonts * sizeof(BigEndianUint32))) {
      gfxWarning() << "Font data too short to contain full TTC Header.";
      return nullptr;
    }

    UniquePtr<SFNTData> sfntData(new SFNTData);
    const BigEndianUint32* offset =
        reinterpret_cast<const BigEndianUint32*>(aFontData + sizeof(TTCHeader));
    const BigEndianUint32* endOfOffsets = offset + numFonts;
    while (offset != endOfOffsets) {
      if (!sfntData->AddFont(aFontData, aDataLength, *offset)) {
        return nullptr;
      }
      ++offset;
    }

    return sfntData;
  }

  UniquePtr<SFNTData> sfntData(new SFNTData);
  if (!sfntData->AddFont(aFontData, aDataLength, 0)) {
    return nullptr;
  }

  return sfntData;
}

/* static */
uint64_t SFNTData::GetUniqueKey(const uint8_t* aFontData, uint32_t aDataLength,
                                uint32_t aVarDataSize, const void* aVarData) {
  uint64_t hash = 0;
  UniquePtr<SFNTData> sfntData = SFNTData::Create(aFontData, aDataLength);
  if (sfntData) {
    hash = sfntData->HashHeadAndCmapTables();
  } else {
    gfxWarning() << "Failed to create SFNTData from data, hashing whole font.";
    hash = HashBytes(aFontData, aDataLength);
  }

  if (aVarDataSize) {
    hash = AddToHash(hash, HashBytes(aVarData, aVarDataSize));
  }

  return hash << 32 | aDataLength;
}

SFNTData::~SFNTData() {
  for (size_t i = 0; i < mFonts.length(); ++i) {
    delete mFonts[i];
  }
}

bool SFNTData::AddFont(const uint8_t* aFontData, uint32_t aDataLength,
                       uint32_t aOffset) {
  uint32_t remainingLength = aDataLength - aOffset;
  if (remainingLength < sizeof(OffsetTable)) {
    gfxWarning() << "Font data too short to contain OffsetTable " << aOffset;
    return false;
  }

  const OffsetTable* offsetTable =
      reinterpret_cast<const OffsetTable*>(aFontData + aOffset);
  if (remainingLength <
      sizeof(OffsetTable) + (offsetTable->numTables * sizeof(TableDirEntry))) {
    gfxWarning() << "Font data too short to contain tables.";
    return false;
  }

  return mFonts.append(new Font(offsetTable, aFontData, aDataLength));
}

uint32_t SFNTData::HashHeadAndCmapTables() {
  uint32_t tablesHash = std::accumulate(
      mFonts.begin(), mFonts.end(), 0U, [](uint32_t hash, Font* font) {
        Span<const uint8_t> headBytes = font->GetHeadTableBytes();
        hash = AddToHash(hash, HashBytes(headBytes.data(), headBytes.size()));
        Span<const uint8_t> cmapBytes = font->GetCmapTableBytes();
        return AddToHash(hash, HashBytes(cmapBytes.data(), cmapBytes.size()));
      });

  return tablesHash;
}

}  // namespace gfx
}  // namespace mozilla
