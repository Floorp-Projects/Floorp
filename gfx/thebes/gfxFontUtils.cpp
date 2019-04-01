/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/BinarySearch.h"

#include "gfxFontUtils.h"
#include "gfxFontEntry.h"
#include "gfxFontVariations.h"

#include "nsServiceManagerUtils.h"

#include "mozilla/Preferences.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"

#include "nsCOMPtr.h"
#include "nsIUUIDGenerator.h"
#include "mozilla/Encoding.h"

#include "harfbuzz/hb.h"

#include "plbase64.h"
#include "mozilla/Logging.h"

#ifdef XP_MACOSX
#  include <CoreFoundation/CoreFoundation.h>
#endif

#define LOG(log, args) MOZ_LOG(gfxPlatform::GetLog(log), LogLevel::Debug, args)

#define UNICODE_BMP_LIMIT 0x10000

using namespace mozilla;

#pragma pack(1)

typedef struct {
  AutoSwap_PRUint16 format;
  AutoSwap_PRUint16 reserved;
  AutoSwap_PRUint32 length;
  AutoSwap_PRUint32 language;
  AutoSwap_PRUint32 startCharCode;
  AutoSwap_PRUint32 numChars;
} Format10CmapHeader;

typedef struct {
  AutoSwap_PRUint16 format;
  AutoSwap_PRUint16 reserved;
  AutoSwap_PRUint32 length;
  AutoSwap_PRUint32 language;
  AutoSwap_PRUint32 numGroups;
} Format12CmapHeader;

typedef struct {
  AutoSwap_PRUint32 startCharCode;
  AutoSwap_PRUint32 endCharCode;
  AutoSwap_PRUint32 startGlyphId;
} Format12Group;

#pragma pack()

void gfxSparseBitSet::Dump(const char* aPrefix, eGfxLog aWhichLog) const {
  uint32_t numBlocks = mBlockIndex.Length();

  for (uint32_t b = 0; b < numBlocks; b++) {
    if (mBlockIndex[b] == NO_BLOCK) {
      continue;
    }
    const Block* block = &mBlocks[mBlockIndex[b]];
    const int BUFSIZE = 256;
    char outStr[BUFSIZE];
    int index = 0;
    index += snprintf(&outStr[index], BUFSIZE - index, "%s u+%6.6x [", aPrefix,
                      (b * BLOCK_SIZE_BITS));
    for (int i = 0; i < 32; i += 4) {
      for (int j = i; j < i + 4; j++) {
        uint8_t bits = block->mBits[j];
        uint8_t flip1 = ((bits & 0xaa) >> 1) | ((bits & 0x55) << 1);
        uint8_t flip2 = ((flip1 & 0xcc) >> 2) | ((flip1 & 0x33) << 2);
        uint8_t flipped = ((flip2 & 0xf0) >> 4) | ((flip2 & 0x0f) << 4);

        index += snprintf(&outStr[index], BUFSIZE - index, "%2.2x", flipped);
      }
      if (i + 4 != 32) index += snprintf(&outStr[index], BUFSIZE - index, " ");
    }
    Unused << snprintf(&outStr[index], BUFSIZE - index, "]");
    LOG(aWhichLog, ("%s", outStr));
  }
}

nsresult gfxFontUtils::ReadCMAPTableFormat10(const uint8_t* aBuf,
                                             uint32_t aLength,
                                             gfxSparseBitSet& aCharacterMap) {
  // Ensure table is large enough that we can safely read the header
  NS_ENSURE_TRUE(aLength >= sizeof(Format10CmapHeader),
                 NS_ERROR_GFX_CMAP_MALFORMED);

  // Sanity-check header fields
  const Format10CmapHeader* cmap10 =
      reinterpret_cast<const Format10CmapHeader*>(aBuf);
  NS_ENSURE_TRUE(uint16_t(cmap10->format) == 10, NS_ERROR_GFX_CMAP_MALFORMED);
  NS_ENSURE_TRUE(uint16_t(cmap10->reserved) == 0, NS_ERROR_GFX_CMAP_MALFORMED);

  uint32_t tablelen = cmap10->length;
  NS_ENSURE_TRUE(tablelen >= sizeof(Format10CmapHeader) && tablelen <= aLength,
                 NS_ERROR_GFX_CMAP_MALFORMED);

  NS_ENSURE_TRUE(cmap10->language == 0, NS_ERROR_GFX_CMAP_MALFORMED);

  uint32_t numChars = cmap10->numChars;
  NS_ENSURE_TRUE(
      tablelen == sizeof(Format10CmapHeader) + numChars * sizeof(uint16_t),
      NS_ERROR_GFX_CMAP_MALFORMED);

  uint32_t charCode = cmap10->startCharCode;
  NS_ENSURE_TRUE(charCode <= CMAP_MAX_CODEPOINT &&
                     charCode + numChars <= CMAP_MAX_CODEPOINT,
                 NS_ERROR_GFX_CMAP_MALFORMED);

  // glyphs[] array immediately follows the subtable header
  const AutoSwap_PRUint16* glyphs =
      reinterpret_cast<const AutoSwap_PRUint16*>(cmap10 + 1);

  for (uint32_t i = 0; i < numChars; ++i) {
    if (uint16_t(*glyphs) != 0) {
      aCharacterMap.set(charCode);
    }
    ++charCode;
    ++glyphs;
  }

  aCharacterMap.Compact();

  return NS_OK;
}

nsresult gfxFontUtils::ReadCMAPTableFormat12or13(
    const uint8_t* aBuf, uint32_t aLength, gfxSparseBitSet& aCharacterMap) {
  // Format 13 has the same structure as format 12, the only difference is
  // the interpretation of the glyphID field. So we can share the code here
  // that reads the table and just records character coverage.

  // Ensure table is large enough that we can safely read the header
  NS_ENSURE_TRUE(aLength >= sizeof(Format12CmapHeader),
                 NS_ERROR_GFX_CMAP_MALFORMED);

  // Sanity-check header fields
  const Format12CmapHeader* cmap12 =
      reinterpret_cast<const Format12CmapHeader*>(aBuf);
  NS_ENSURE_TRUE(
      uint16_t(cmap12->format) == 12 || uint16_t(cmap12->format) == 13,
      NS_ERROR_GFX_CMAP_MALFORMED);
  NS_ENSURE_TRUE(uint16_t(cmap12->reserved) == 0, NS_ERROR_GFX_CMAP_MALFORMED);

  uint32_t tablelen = cmap12->length;
  NS_ENSURE_TRUE(tablelen >= sizeof(Format12CmapHeader) && tablelen <= aLength,
                 NS_ERROR_GFX_CMAP_MALFORMED);

  NS_ENSURE_TRUE(cmap12->language == 0, NS_ERROR_GFX_CMAP_MALFORMED);

  // Check that the table is large enough for the group array
  const uint32_t numGroups = cmap12->numGroups;
  NS_ENSURE_TRUE(
      (tablelen - sizeof(Format12CmapHeader)) / sizeof(Format12Group) >=
          numGroups,
      NS_ERROR_GFX_CMAP_MALFORMED);

  // The array of groups immediately follows the subtable header.
  const Format12Group* group =
      reinterpret_cast<const Format12Group*>(aBuf + sizeof(Format12CmapHeader));

  // Check that groups are in correct order and do not overlap,
  // and record character coverage in aCharacterMap.
  uint32_t prevEndCharCode = 0;
  for (uint32_t i = 0; i < numGroups; i++, group++) {
    uint32_t startCharCode = group->startCharCode;
    const uint32_t endCharCode = group->endCharCode;
    NS_ENSURE_TRUE((prevEndCharCode < startCharCode || i == 0) &&
                       startCharCode <= endCharCode &&
                       endCharCode <= CMAP_MAX_CODEPOINT,
                   NS_ERROR_GFX_CMAP_MALFORMED);
    // don't include a character that maps to glyph ID 0 (.notdef)
    if (group->startGlyphId == 0) {
      startCharCode++;
    }
    if (startCharCode <= endCharCode) {
      aCharacterMap.SetRange(startCharCode, endCharCode);
    }
    prevEndCharCode = endCharCode;
  }

  aCharacterMap.Compact();

  return NS_OK;
}

nsresult gfxFontUtils::ReadCMAPTableFormat4(const uint8_t* aBuf,
                                            uint32_t aLength,
                                            gfxSparseBitSet& aCharacterMap) {
  enum {
    OffsetFormat = 0,
    OffsetLength = 2,
    OffsetLanguage = 4,
    OffsetSegCountX2 = 6
  };

  NS_ENSURE_TRUE(ReadShortAt(aBuf, OffsetFormat) == 4,
                 NS_ERROR_GFX_CMAP_MALFORMED);
  uint16_t tablelen = ReadShortAt(aBuf, OffsetLength);
  NS_ENSURE_TRUE(tablelen <= aLength, NS_ERROR_GFX_CMAP_MALFORMED);
  NS_ENSURE_TRUE(tablelen > 16, NS_ERROR_GFX_CMAP_MALFORMED);

  // This field should normally (except for Mac platform subtables) be zero
  // according to the OT spec, but some buggy fonts have lang = 1 (which would
  // be English for MacOS). E.g. Arial Narrow Bold, v. 1.1 (Tiger), Arial
  // Unicode MS (see bug 530614). So accept either zero or one here; the error
  // should be harmless.
  NS_ENSURE_TRUE((ReadShortAt(aBuf, OffsetLanguage) & 0xfffe) == 0,
                 NS_ERROR_GFX_CMAP_MALFORMED);

  uint16_t segCountX2 = ReadShortAt(aBuf, OffsetSegCountX2);
  NS_ENSURE_TRUE(tablelen >= 16 + (segCountX2 * 4),
                 NS_ERROR_GFX_CMAP_MALFORMED);

  const uint16_t segCount = segCountX2 / 2;

  const uint16_t* endCounts = reinterpret_cast<const uint16_t*>(aBuf + 14);
  const uint16_t* startCounts =
      endCounts + 1 /* skip one uint16_t for reservedPad */ + segCount;
  const uint16_t* idDeltas = startCounts + segCount;
  const uint16_t* idRangeOffsets = idDeltas + segCount;
  uint16_t prevEndCount = 0;
  for (uint16_t i = 0; i < segCount; i++) {
    const uint16_t endCount = ReadShortAt16(endCounts, i);
    const uint16_t startCount = ReadShortAt16(startCounts, i);
    const uint16_t idRangeOffset = ReadShortAt16(idRangeOffsets, i);

    // sanity-check range
    // This permits ranges to overlap by 1 character, which is strictly
    // incorrect but occurs in Baskerville on OS X 10.7 (see bug 689087),
    // and appears to be harmless in practice
    NS_ENSURE_TRUE(startCount >= prevEndCount && startCount <= endCount,
                   NS_ERROR_GFX_CMAP_MALFORMED);
    prevEndCount = endCount;

    if (idRangeOffset == 0) {
      // figure out if there's a code in the range that would map to
      // glyph ID 0 (.notdef); if so, we need to skip setting that
      // character code in the map
      const uint16_t skipCode = 65536 - ReadShortAt16(idDeltas, i);
      if (startCount < skipCode) {
        aCharacterMap.SetRange(startCount,
                               std::min<uint16_t>(skipCode - 1, endCount));
      }
      if (skipCode < endCount) {
        aCharacterMap.SetRange(std::max<uint16_t>(startCount, skipCode + 1),
                               endCount);
      }
    } else {
      // Unused: self-documenting.
      // const uint16_t idDelta = ReadShortAt16(idDeltas, i);
      for (uint32_t c = startCount; c <= endCount; ++c) {
        if (c == 0xFFFF) break;

        const uint16_t* gdata =
            (idRangeOffset / 2 + (c - startCount) + &idRangeOffsets[i]);

        NS_ENSURE_TRUE(
            (uint8_t*)gdata > aBuf && (uint8_t*)gdata < aBuf + aLength,
            NS_ERROR_GFX_CMAP_MALFORMED);

        // make sure we have a glyph
        if (*gdata != 0) {
          // The glyph index at this point is:
          uint16_t glyph = ReadShortAt16(idDeltas, i) + *gdata;
          if (glyph) {
            aCharacterMap.set(c);
          }
        }
      }
    }
  }

  aCharacterMap.Compact();

  return NS_OK;
}

nsresult gfxFontUtils::ReadCMAPTableFormat14(const uint8_t* aBuf,
                                             uint32_t aLength,
                                             UniquePtr<uint8_t[]>& aTable) {
  enum {
    OffsetFormat = 0,
    OffsetTableLength = 2,
    OffsetNumVarSelectorRecords = 6,
    OffsetVarSelectorRecords = 10,

    SizeOfVarSelectorRecord = 11,
    VSRecOffsetVarSelector = 0,
    VSRecOffsetDefUVSOffset = 3,
    VSRecOffsetNonDefUVSOffset = 7,

    SizeOfDefUVSTable = 4,
    DefUVSOffsetStartUnicodeValue = 0,
    DefUVSOffsetAdditionalCount = 3,

    SizeOfNonDefUVSTable = 5,
    NonDefUVSOffsetUnicodeValue = 0,
    NonDefUVSOffsetGlyphID = 3
  };
  NS_ENSURE_TRUE(aLength >= OffsetVarSelectorRecords,
                 NS_ERROR_GFX_CMAP_MALFORMED);

  NS_ENSURE_TRUE(ReadShortAt(aBuf, OffsetFormat) == 14,
                 NS_ERROR_GFX_CMAP_MALFORMED);

  uint32_t tablelen = ReadLongAt(aBuf, OffsetTableLength);
  NS_ENSURE_TRUE(tablelen <= aLength, NS_ERROR_GFX_CMAP_MALFORMED);
  NS_ENSURE_TRUE(tablelen >= OffsetVarSelectorRecords,
                 NS_ERROR_GFX_CMAP_MALFORMED);

  const uint32_t numVarSelectorRecords =
      ReadLongAt(aBuf, OffsetNumVarSelectorRecords);
  NS_ENSURE_TRUE(
      (tablelen - OffsetVarSelectorRecords) / SizeOfVarSelectorRecord >=
          numVarSelectorRecords,
      NS_ERROR_GFX_CMAP_MALFORMED);

  const uint8_t* records = aBuf + OffsetVarSelectorRecords;
  for (uint32_t i = 0; i < numVarSelectorRecords;
       i++, records += SizeOfVarSelectorRecord) {
    const uint32_t varSelector = ReadUint24At(records, VSRecOffsetVarSelector);
    const uint32_t defUVSOffset = ReadLongAt(records, VSRecOffsetDefUVSOffset);
    const uint32_t nonDefUVSOffset =
        ReadLongAt(records, VSRecOffsetNonDefUVSOffset);
    NS_ENSURE_TRUE(varSelector <= CMAP_MAX_CODEPOINT &&
                       defUVSOffset <= tablelen - 4 &&
                       nonDefUVSOffset <= tablelen - 4,
                   NS_ERROR_GFX_CMAP_MALFORMED);

    if (defUVSOffset) {
      const uint32_t numUnicodeValueRanges = ReadLongAt(aBuf, defUVSOffset);
      NS_ENSURE_TRUE((tablelen - defUVSOffset) / SizeOfDefUVSTable >=
                         numUnicodeValueRanges,
                     NS_ERROR_GFX_CMAP_MALFORMED);
      const uint8_t* tables = aBuf + defUVSOffset + 4;
      uint32_t prevEndUnicode = 0;
      for (uint32_t j = 0; j < numUnicodeValueRanges;
           j++, tables += SizeOfDefUVSTable) {
        const uint32_t startUnicode =
            ReadUint24At(tables, DefUVSOffsetStartUnicodeValue);
        const uint32_t endUnicode =
            startUnicode + tables[DefUVSOffsetAdditionalCount];
        NS_ENSURE_TRUE((prevEndUnicode < startUnicode || j == 0) &&
                           endUnicode <= CMAP_MAX_CODEPOINT,
                       NS_ERROR_GFX_CMAP_MALFORMED);
        prevEndUnicode = endUnicode;
      }
    }

    if (nonDefUVSOffset) {
      const uint32_t numUVSMappings = ReadLongAt(aBuf, nonDefUVSOffset);
      NS_ENSURE_TRUE(
          (tablelen - nonDefUVSOffset) / SizeOfNonDefUVSTable >= numUVSMappings,
          NS_ERROR_GFX_CMAP_MALFORMED);
      const uint8_t* tables = aBuf + nonDefUVSOffset + 4;
      uint32_t prevUnicode = 0;
      for (uint32_t j = 0; j < numUVSMappings;
           j++, tables += SizeOfNonDefUVSTable) {
        const uint32_t unicodeValue =
            ReadUint24At(tables, NonDefUVSOffsetUnicodeValue);
        NS_ENSURE_TRUE((prevUnicode < unicodeValue || j == 0) &&
                           unicodeValue <= CMAP_MAX_CODEPOINT,
                       NS_ERROR_GFX_CMAP_MALFORMED);
        prevUnicode = unicodeValue;
      }
    }
  }

  aTable = MakeUnique<uint8_t[]>(tablelen);
  memcpy(aTable.get(), aBuf, tablelen);

  return NS_OK;
}

// For fonts with two format-4 tables, the first one (Unicode platform) is
// preferred on the Mac; on other platforms we allow the Microsoft-platform
// subtable to replace it.

#if defined(XP_MACOSX)
#  define acceptableFormat4(p, e, k)                                         \
    (((p) == PLATFORM_ID_MICROSOFT && (e) == EncodingIDMicrosoft && !(k)) || \
     ((p) == PLATFORM_ID_UNICODE))

#  define acceptableUCS4Encoding(p, e, k)           \
    (((p) == PLATFORM_ID_MICROSOFT &&               \
      (e) == EncodingIDUCS4ForMicrosoftPlatform) && \
         (k) != 12 ||                               \
     ((p) == PLATFORM_ID_UNICODE && ((e) != EncodingIDUVSForUnicodePlatform)))
#else
#  define acceptableFormat4(p, e, k)                                 \
    (((p) == PLATFORM_ID_MICROSOFT && (e) == EncodingIDMicrosoft) || \
     ((p) == PLATFORM_ID_UNICODE))

#  define acceptableUCS4Encoding(p, e, k) \
    ((p) == PLATFORM_ID_MICROSOFT && (e) == EncodingIDUCS4ForMicrosoftPlatform)
#endif

#define acceptablePlatform(p) \
  ((p) == PLATFORM_ID_UNICODE || (p) == PLATFORM_ID_MICROSOFT)
#define isSymbol(p, e) ((p) == PLATFORM_ID_MICROSOFT && (e) == EncodingIDSymbol)
#define isUVSEncoding(p, e) \
  ((p) == PLATFORM_ID_UNICODE && (e) == EncodingIDUVSForUnicodePlatform)

uint32_t gfxFontUtils::FindPreferredSubtable(const uint8_t* aBuf,
                                             uint32_t aBufLength,
                                             uint32_t* aTableOffset,
                                             uint32_t* aUVSTableOffset) {
  enum {
    OffsetVersion = 0,
    OffsetNumTables = 2,
    SizeOfHeader = 4,

    TableOffsetPlatformID = 0,
    TableOffsetEncodingID = 2,
    TableOffsetOffset = 4,
    SizeOfTable = 8,

    SubtableOffsetFormat = 0
  };
  enum {
    EncodingIDSymbol = 0,
    EncodingIDMicrosoft = 1,
    EncodingIDDefaultForUnicodePlatform = 0,
    EncodingIDUCS4ForUnicodePlatform = 3,
    EncodingIDUVSForUnicodePlatform = 5,
    EncodingIDUCS4ForMicrosoftPlatform = 10
  };

  if (aUVSTableOffset) {
    *aUVSTableOffset = 0;
  }

  if (!aBuf || aBufLength < SizeOfHeader) {
    // cmap table is missing, or too small to contain header fields!
    return 0;
  }

  // uint16_t version = ReadShortAt(aBuf, OffsetVersion); // Unused:
  // self-documenting.
  uint16_t numTables = ReadShortAt(aBuf, OffsetNumTables);
  if (aBufLength < uint32_t(SizeOfHeader + numTables * SizeOfTable)) {
    return 0;
  }

  // save the format we want here
  uint32_t keepFormat = 0;

  const uint8_t* table = aBuf + SizeOfHeader;
  for (uint16_t i = 0; i < numTables; ++i, table += SizeOfTable) {
    const uint16_t platformID = ReadShortAt(table, TableOffsetPlatformID);
    if (!acceptablePlatform(platformID)) continue;

    const uint16_t encodingID = ReadShortAt(table, TableOffsetEncodingID);
    const uint32_t offset = ReadLongAt(table, TableOffsetOffset);
    if (aBufLength - 2 < offset) {
      // this subtable is not valid - beyond end of buffer
      return 0;
    }

    const uint8_t* subtable = aBuf + offset;
    const uint16_t format = ReadShortAt(subtable, SubtableOffsetFormat);

    if (isSymbol(platformID, encodingID)) {
      keepFormat = format;
      *aTableOffset = offset;
      break;
    } else if (format == 4 &&
               acceptableFormat4(platformID, encodingID, keepFormat)) {
      keepFormat = format;
      *aTableOffset = offset;
    } else if ((format == 10 || format == 12 || format == 13) &&
               acceptableUCS4Encoding(platformID, encodingID, keepFormat)) {
      keepFormat = format;
      *aTableOffset = offset;
      if (platformID > PLATFORM_ID_UNICODE || !aUVSTableOffset ||
          *aUVSTableOffset) {
        break;  // we don't want to try anything else when this format is
                // available.
      }
    } else if (format == 14 && isUVSEncoding(platformID, encodingID) &&
               aUVSTableOffset) {
      *aUVSTableOffset = offset;
      if (keepFormat == 10 || keepFormat == 12) {
        break;
      }
    }
  }

  return keepFormat;
}

nsresult gfxFontUtils::ReadCMAP(const uint8_t* aBuf, uint32_t aBufLength,
                                gfxSparseBitSet& aCharacterMap,
                                uint32_t& aUVSOffset) {
  uint32_t offset;
  uint32_t format =
      FindPreferredSubtable(aBuf, aBufLength, &offset, &aUVSOffset);

  switch (format) {
    case 4:
      return ReadCMAPTableFormat4(aBuf + offset, aBufLength - offset,
                                  aCharacterMap);

    case 10:
      return ReadCMAPTableFormat10(aBuf + offset, aBufLength - offset,
                                   aCharacterMap);

    case 12:
    case 13:
      return ReadCMAPTableFormat12or13(aBuf + offset, aBufLength - offset,
                                       aCharacterMap);

    default:
      break;
  }

  return NS_ERROR_FAILURE;
}

#pragma pack(1)

typedef struct {
  AutoSwap_PRUint16 format;
  AutoSwap_PRUint16 length;
  AutoSwap_PRUint16 language;
  AutoSwap_PRUint16 segCountX2;
  AutoSwap_PRUint16 searchRange;
  AutoSwap_PRUint16 entrySelector;
  AutoSwap_PRUint16 rangeShift;

  AutoSwap_PRUint16 arrays[1];
} Format4Cmap;

typedef struct {
  AutoSwap_PRUint16 format;
  AutoSwap_PRUint32 length;
  AutoSwap_PRUint32 numVarSelectorRecords;

  typedef struct {
    AutoSwap_PRUint24 varSelector;
    AutoSwap_PRUint32 defaultUVSOffset;
    AutoSwap_PRUint32 nonDefaultUVSOffset;
  } VarSelectorRecord;

  VarSelectorRecord varSelectorRecords[1];
} Format14Cmap;

typedef struct {
  AutoSwap_PRUint32 numUVSMappings;

  typedef struct {
    AutoSwap_PRUint24 unicodeValue;
    AutoSwap_PRUint16 glyphID;
  } UVSMapping;

  UVSMapping uvsMappings[1];
} NonDefUVSTable;

#pragma pack()

uint32_t gfxFontUtils::MapCharToGlyphFormat4(const uint8_t* aBuf,
                                             uint32_t aLength, char16_t aCh) {
  const Format4Cmap* cmap4 = reinterpret_cast<const Format4Cmap*>(aBuf);

  uint16_t segCount = (uint16_t)(cmap4->segCountX2) / 2;

  const AutoSwap_PRUint16* endCodes = &cmap4->arrays[0];
  const AutoSwap_PRUint16* startCodes = &cmap4->arrays[segCount + 1];
  const AutoSwap_PRUint16* idDelta = &startCodes[segCount];
  const AutoSwap_PRUint16* idRangeOffset = &idDelta[segCount];

  // Sanity-check that the fixed-size arrays don't exceed the buffer.
  const uint8_t* const limit = aBuf + aLength;
  if ((const uint8_t*)(&idRangeOffset[segCount]) > limit) {
    return 0;  // broken font, just bail out safely
  }

  // For most efficient binary search, we want to work on a range of segment
  // indexes that is a power of 2 so that we can always halve it by shifting.
  // So we find the largest power of 2 that is <= segCount.
  // We will offset this range by segOffset so as to reach the end
  // of the table, provided that doesn't put us beyond the target
  // value from the outset.
  uint32_t powerOf2 = mozilla::FindHighestBit(segCount);
  uint32_t segOffset = segCount - powerOf2;
  uint32_t idx = 0;

  if (uint16_t(startCodes[segOffset]) <= aCh) {
    idx = segOffset;
  }

  // Repeatedly halve the size of the range until we find the target group
  while (powerOf2 > 1) {
    powerOf2 >>= 1;
    if (uint16_t(startCodes[idx + powerOf2]) <= aCh) {
      idx += powerOf2;
    }
  }

  if (aCh >= uint16_t(startCodes[idx]) && aCh <= uint16_t(endCodes[idx])) {
    uint16_t result;
    if (uint16_t(idRangeOffset[idx]) == 0) {
      result = aCh;
    } else {
      uint16_t offset = aCh - uint16_t(startCodes[idx]);
      const AutoSwap_PRUint16* glyphIndexTable =
          (const AutoSwap_PRUint16*)((const char*)&idRangeOffset[idx] +
                                     uint16_t(idRangeOffset[idx]));
      if ((const uint8_t*)(glyphIndexTable + offset + 1) > limit) {
        return 0;  // broken font, just bail out safely
      }
      result = glyphIndexTable[offset];
    }

    // Note that this is unsigned 16-bit arithmetic, and may wrap around
    // (which is required behavior per spec)
    result += uint16_t(idDelta[idx]);
    return result;
  }

  return 0;
}

uint32_t gfxFontUtils::MapCharToGlyphFormat10(const uint8_t* aBuf,
                                              uint32_t aCh) {
  const Format10CmapHeader* cmap10 =
      reinterpret_cast<const Format10CmapHeader*>(aBuf);

  uint32_t startChar = cmap10->startCharCode;
  uint32_t numChars = cmap10->numChars;

  if (aCh < startChar || aCh >= startChar + numChars) {
    return 0;
  }

  const AutoSwap_PRUint16* glyphs =
      reinterpret_cast<const AutoSwap_PRUint16*>(cmap10 + 1);

  uint16_t glyph = glyphs[aCh - startChar];
  return glyph;
}

uint32_t gfxFontUtils::MapCharToGlyphFormat12or13(const uint8_t* aBuf,
                                                  uint32_t aCh) {
  // The only difference between formats 12 and 13 is the interpretation of
  // the glyphId field. So the code here uses the same "Format12" structures,
  // etc., to handle both subtable formats.

  const Format12CmapHeader* cmap12 =
      reinterpret_cast<const Format12CmapHeader*>(aBuf);

  // We know that numGroups is within range for the subtable size
  // because it was checked by ReadCMAPTableFormat12or13.
  uint32_t numGroups = cmap12->numGroups;

  // The array of groups immediately follows the subtable header.
  const Format12Group* groups =
      reinterpret_cast<const Format12Group*>(aBuf + sizeof(Format12CmapHeader));

  // For most efficient binary search, we want to work on a range that
  // is a power of 2 so that we can always halve it by shifting.
  // So we find the largest power of 2 that is <= numGroups.
  // We will offset this range by rangeOffset so as to reach the end
  // of the table, provided that doesn't put us beyond the target
  // value from the outset.
  uint32_t powerOf2 = mozilla::FindHighestBit(numGroups);
  uint32_t rangeOffset = numGroups - powerOf2;
  uint32_t range = 0;
  uint32_t startCharCode;

  if (groups[rangeOffset].startCharCode <= aCh) {
    range = rangeOffset;
  }

  // Repeatedly halve the size of the range until we find the target group
  while (powerOf2 > 1) {
    powerOf2 >>= 1;
    if (groups[range + powerOf2].startCharCode <= aCh) {
      range += powerOf2;
    }
  }

  // Check if the character is actually present in the range and return
  // the corresponding glyph ID. Here is where formats 12 and 13 interpret
  // the startGlyphId (12) or glyphId (13) field differently
  startCharCode = groups[range].startCharCode;
  if (startCharCode <= aCh && groups[range].endCharCode >= aCh) {
    return uint16_t(cmap12->format) == 12
               ? uint16_t(groups[range].startGlyphId) + aCh - startCharCode
               : uint16_t(groups[range].startGlyphId);
  }

  // Else it's not present, so return the .notdef glyph
  return 0;
}

namespace {

struct Format14CmapWrapper {
  const Format14Cmap& mCmap14;
  explicit Format14CmapWrapper(const Format14Cmap& cmap14) : mCmap14(cmap14) {}
  uint32_t operator[](size_t index) const {
    return mCmap14.varSelectorRecords[index].varSelector;
  }
};

struct NonDefUVSTableWrapper {
  const NonDefUVSTable& mTable;
  explicit NonDefUVSTableWrapper(const NonDefUVSTable& table) : mTable(table) {}
  uint32_t operator[](size_t index) const {
    return mTable.uvsMappings[index].unicodeValue;
  }
};

}  // namespace

uint16_t gfxFontUtils::MapUVSToGlyphFormat14(const uint8_t* aBuf, uint32_t aCh,
                                             uint32_t aVS) {
  using mozilla::BinarySearch;
  const Format14Cmap* cmap14 = reinterpret_cast<const Format14Cmap*>(aBuf);

  size_t index;
  if (!BinarySearch(Format14CmapWrapper(*cmap14), 0,
                    cmap14->numVarSelectorRecords, aVS, &index)) {
    return 0;
  }

  const uint32_t nonDefUVSOffset =
      cmap14->varSelectorRecords[index].nonDefaultUVSOffset;
  if (!nonDefUVSOffset) {
    return 0;
  }

  const NonDefUVSTable* table =
      reinterpret_cast<const NonDefUVSTable*>(aBuf + nonDefUVSOffset);

  if (BinarySearch(NonDefUVSTableWrapper(*table), 0, table->numUVSMappings, aCh,
                   &index)) {
    return table->uvsMappings[index].glyphID;
  }

  return 0;
}

uint32_t gfxFontUtils::MapCharToGlyph(const uint8_t* aCmapBuf,
                                      uint32_t aBufLength, uint32_t aUnicode,
                                      uint32_t aVarSelector) {
  uint32_t offset, uvsOffset;
  uint32_t format =
      FindPreferredSubtable(aCmapBuf, aBufLength, &offset, &uvsOffset);

  uint32_t gid;
  switch (format) {
    case 4:
      gid = aUnicode < UNICODE_BMP_LIMIT
                ? MapCharToGlyphFormat4(aCmapBuf + offset, aBufLength - offset,
                                        char16_t(aUnicode))
                : 0;
      break;
    case 10:
      gid = MapCharToGlyphFormat10(aCmapBuf + offset, aUnicode);
      break;
    case 12:
    case 13:
      gid = MapCharToGlyphFormat12or13(aCmapBuf + offset, aUnicode);
      break;
    default:
      NS_WARNING("unsupported cmap format, glyphs will be missing");
      gid = 0;
  }

  if (aVarSelector && uvsOffset && gid) {
    uint32_t varGID = gfxFontUtils::MapUVSToGlyphFormat14(
        aCmapBuf + uvsOffset, aUnicode, aVarSelector);
    if (!varGID) {
      aUnicode = gfxFontUtils::GetUVSFallback(aUnicode, aVarSelector);
      if (aUnicode) {
        switch (format) {
          case 4:
            if (aUnicode < UNICODE_BMP_LIMIT) {
              varGID = MapCharToGlyphFormat4(
                  aCmapBuf + offset, aBufLength - offset, char16_t(aUnicode));
            }
            break;
          case 10:
            varGID = MapCharToGlyphFormat10(aCmapBuf + offset, aUnicode);
            break;
          case 12:
          case 13:
            varGID = MapCharToGlyphFormat12or13(aCmapBuf + offset, aUnicode);
            break;
        }
      }
    }
    if (varGID) {
      gid = varGID;
    }

    // else the variation sequence was not supported, use default mapping
    // of the character code alone
  }

  return gid;
}

void gfxFontUtils::ParseFontList(const nsACString& aFamilyList,
                                 nsTArray<nsCString>& aFontList) {
  const char kComma = ',';

  // append each font name to the list
  nsAutoCString fontname;
  const char *p, *p_end;
  aFamilyList.BeginReading(p);
  aFamilyList.EndReading(p_end);

  while (p < p_end) {
    const char* nameStart = p;
    while (++p != p_end && *p != kComma) /* nothing */
      ;

    // pull out a single name and clean out leading/trailing whitespace
    fontname = Substring(nameStart, p);
    fontname.CompressWhitespace(true, true);

    // append it to the list if it's not empty
    if (!fontname.IsEmpty()) {
      aFontList.AppendElement(fontname);
    }
    ++p;
  }
}

void gfxFontUtils::AppendPrefsFontList(const char* aPrefName,
                                       nsTArray<nsCString>& aFontList) {
  // get the list of single-face font families
  nsAutoCString fontlistValue;
  nsresult rv = Preferences::GetCString(aPrefName, fontlistValue);
  if (NS_FAILED(rv)) {
    return;
  }

  ParseFontList(fontlistValue, aFontList);
}

void gfxFontUtils::GetPrefsFontList(const char* aPrefName,
                                    nsTArray<nsCString>& aFontList) {
  aFontList.Clear();
  AppendPrefsFontList(aPrefName, aFontList);
}

// produce a unique font name that is (1) a valid Postscript name and (2) less
// than 31 characters in length.  Using AddFontMemResourceEx on Windows fails
// for names longer than 30 characters in length.

#define MAX_B64_LEN 32

nsresult gfxFontUtils::MakeUniqueUserFontName(nsAString& aName) {
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1");
  NS_ENSURE_TRUE(uuidgen, NS_ERROR_OUT_OF_MEMORY);

  nsID guid;

  NS_ASSERTION(sizeof(guid) * 2 <= MAX_B64_LEN, "size of nsID has changed!");

  nsresult rv = uuidgen->GenerateUUIDInPlace(&guid);
  NS_ENSURE_SUCCESS(rv, rv);

  char guidB64[MAX_B64_LEN] = {0};

  if (!PL_Base64Encode(reinterpret_cast<char*>(&guid), sizeof(guid), guidB64))
    return NS_ERROR_FAILURE;

  // all b64 characters except for '/' are allowed in Postscript names, so
  // convert / ==> -
  char* p;
  for (p = guidB64; *p; p++) {
    if (*p == '/') *p = '-';
  }

  aName.AssignLiteral(u"uf");
  aName.AppendASCII(guidB64);
  return NS_OK;
}

// TrueType/OpenType table handling code

// need byte aligned structs
#pragma pack(1)

// name table stores set of name record structures, followed by
// large block containing all the strings.  name record offset and length
// indicates the offset and length within that block.
// http://www.microsoft.com/typography/otspec/name.htm
struct NameRecordData {
  uint32_t offset;
  uint32_t length;
};

#pragma pack()

static bool IsValidSFNTVersion(uint32_t version) {
  // normally 0x00010000, CFF-style OT fonts == 'OTTO' and Apple TT fonts =
  // 'true' 'typ1' is also possible for old Type 1 fonts in a SFNT container but
  // not supported
  return version == 0x10000 || version == TRUETYPE_TAG('O', 'T', 'T', 'O') ||
         version == TRUETYPE_TAG('t', 'r', 'u', 'e');
}

gfxUserFontType gfxFontUtils::DetermineFontDataType(const uint8_t* aFontData,
                                                    uint32_t aFontDataLength) {
  // test for OpenType font data
  // problem: EOT-Lite with 0x10000 length will look like TrueType!
  if (aFontDataLength >= sizeof(SFNTHeader)) {
    const SFNTHeader* sfntHeader =
        reinterpret_cast<const SFNTHeader*>(aFontData);
    uint32_t sfntVersion = sfntHeader->sfntVersion;
    if (IsValidSFNTVersion(sfntVersion)) {
      return GFX_USERFONT_OPENTYPE;
    }
  }

  // test for WOFF
  if (aFontDataLength >= sizeof(AutoSwap_PRUint32)) {
    const AutoSwap_PRUint32* version =
        reinterpret_cast<const AutoSwap_PRUint32*>(aFontData);
    if (uint32_t(*version) == TRUETYPE_TAG('w', 'O', 'F', 'F')) {
      return GFX_USERFONT_WOFF;
    }
    if (Preferences::GetBool(GFX_PREF_WOFF2_ENABLED) &&
        uint32_t(*version) == TRUETYPE_TAG('w', 'O', 'F', '2')) {
      return GFX_USERFONT_WOFF2;
    }
  }

  // tests for other formats here

  return GFX_USERFONT_UNKNOWN;
}

static int DirEntryCmp(const void* aKey, const void* aItem) {
  int32_t tag = *static_cast<const int32_t*>(aKey);
  const TableDirEntry* entry = static_cast<const TableDirEntry*>(aItem);
  return tag - int32_t(entry->tag);
}

/* static */
TableDirEntry* gfxFontUtils::FindTableDirEntry(const void* aFontData,
                                               uint32_t aTableTag) {
  const SFNTHeader* header = reinterpret_cast<const SFNTHeader*>(aFontData);
  const TableDirEntry* dir = reinterpret_cast<const TableDirEntry*>(header + 1);
  return static_cast<TableDirEntry*>(
      bsearch(&aTableTag, dir, uint16_t(header->numTables),
              sizeof(TableDirEntry), DirEntryCmp));
}

/* static */
hb_blob_t* gfxFontUtils::GetTableFromFontData(const void* aFontData,
                                              uint32_t aTableTag) {
  const TableDirEntry* dir = FindTableDirEntry(aFontData, aTableTag);
  if (dir) {
    return hb_blob_create(
        reinterpret_cast<const char*>(aFontData) + dir->offset, dir->length,
        HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  }
  return nullptr;
}

nsresult gfxFontUtils::RenameFont(const nsAString& aName,
                                  const uint8_t* aFontData,
                                  uint32_t aFontDataLength,
                                  FallibleTArray<uint8_t>* aNewFont) {
  NS_ASSERTION(aNewFont, "null font data array");

  uint64_t dataLength(aFontDataLength);

  // new name table
  static const uint32_t neededNameIDs[] = {NAME_ID_FAMILY, NAME_ID_STYLE,
                                           NAME_ID_UNIQUE, NAME_ID_FULL,
                                           NAME_ID_POSTSCRIPT};

  // calculate new name table size
  uint16_t nameCount = ArrayLength(neededNameIDs);

  // leave room for null-terminator
  uint32_t nameStrLength = (aName.Length() + 1) * sizeof(char16_t);
  if (nameStrLength > 65535) {
    // The name length _in bytes_ must fit in an unsigned short field;
    // therefore, a name longer than this cannot be used.
    return NS_ERROR_FAILURE;
  }

  // round name table size up to 4-byte multiple
  uint32_t nameTableSize =
      (sizeof(NameHeader) + sizeof(NameRecord) * nameCount + nameStrLength +
       3) &
      ~3;

  if (dataLength + nameTableSize > UINT32_MAX) return NS_ERROR_FAILURE;

  // bug 505386 - need to handle unpadded font length
  uint32_t paddedFontDataSize = (aFontDataLength + 3) & ~3;
  uint32_t adjFontDataSize = paddedFontDataSize + nameTableSize;

  // create new buffer: old font data plus new name table
  if (!aNewFont->AppendElements(adjFontDataSize, fallible))
    return NS_ERROR_OUT_OF_MEMORY;

  // copy the old font data
  uint8_t* newFontData = reinterpret_cast<uint8_t*>(aNewFont->Elements());

  // null the last four bytes in case the font length is not a multiple of 4
  memset(newFontData + aFontDataLength, 0,
         paddedFontDataSize - aFontDataLength);

  // copy font data
  memcpy(newFontData, aFontData, aFontDataLength);

  // null out the last 4 bytes for checksum calculations
  memset(newFontData + adjFontDataSize - 4, 0, 4);

  NameHeader* nameHeader =
      reinterpret_cast<NameHeader*>(newFontData + paddedFontDataSize);

  // -- name header
  nameHeader->format = 0;
  nameHeader->count = nameCount;
  nameHeader->stringOffset =
      sizeof(NameHeader) + nameCount * sizeof(NameRecord);

  // -- name records
  uint32_t i;
  NameRecord* nameRecord = reinterpret_cast<NameRecord*>(nameHeader + 1);

  for (i = 0; i < nameCount; i++, nameRecord++) {
    nameRecord->platformID = PLATFORM_ID_MICROSOFT;
    nameRecord->encodingID = ENCODING_ID_MICROSOFT_UNICODEBMP;
    nameRecord->languageID = LANG_ID_MICROSOFT_EN_US;
    nameRecord->nameID = neededNameIDs[i];
    nameRecord->offset = 0;
    nameRecord->length = nameStrLength;
  }

  // -- string data, located after the name records, stored in big-endian form
  char16_t* strData = reinterpret_cast<char16_t*>(nameRecord);

  mozilla::NativeEndian::copyAndSwapToBigEndian(strData, aName.BeginReading(),
                                                aName.Length());
  strData[aName.Length()] = 0;  // add null termination

  // adjust name table header to point to the new name table
  SFNTHeader* sfntHeader = reinterpret_cast<SFNTHeader*>(newFontData);

  // table directory entries begin immediately following SFNT header
  TableDirEntry* dirEntry =
      FindTableDirEntry(newFontData, TRUETYPE_TAG('n', 'a', 'm', 'e'));
  // function only called if font validates, so this should always be true
  MOZ_ASSERT(dirEntry, "attempt to rename font with no name table");

  uint32_t numTables = sfntHeader->numTables;

  // note: dirEntry now points to 'name' table record

  // recalculate name table checksum
  uint32_t checkSum = 0;
  AutoSwap_PRUint32* nameData =
      reinterpret_cast<AutoSwap_PRUint32*>(nameHeader);
  AutoSwap_PRUint32* nameDataEnd = nameData + (nameTableSize >> 2);

  while (nameData < nameDataEnd) checkSum = checkSum + *nameData++;

  // adjust name table entry to point to new name table
  dirEntry->offset = paddedFontDataSize;
  dirEntry->length = nameTableSize;
  dirEntry->checkSum = checkSum;

  // fix up checksums
  uint32_t checksum = 0;

  // checksum for font = (checksum of header) + (checksum of tables)
  uint32_t headerLen = sizeof(SFNTHeader) + sizeof(TableDirEntry) * numTables;
  const AutoSwap_PRUint32* headerData =
      reinterpret_cast<const AutoSwap_PRUint32*>(newFontData);

  // header length is in bytes, checksum calculated in longwords
  for (i = 0; i < (headerLen >> 2); i++, headerData++) {
    checksum += *headerData;
  }

  uint32_t headOffset = 0;
  dirEntry = reinterpret_cast<TableDirEntry*>(newFontData + sizeof(SFNTHeader));

  for (i = 0; i < numTables; i++, dirEntry++) {
    if (dirEntry->tag == TRUETYPE_TAG('h', 'e', 'a', 'd')) {
      headOffset = dirEntry->offset;
    }
    checksum += dirEntry->checkSum;
  }

  NS_ASSERTION(headOffset != 0, "no head table for font");

  HeadTable* headData = reinterpret_cast<HeadTable*>(newFontData + headOffset);

  headData->checkSumAdjustment = HeadTable::HEAD_CHECKSUM_CALC_CONST - checksum;

  return NS_OK;
}

// This is only called after the basic validity of the downloaded sfnt
// data has been checked, so it should never fail to find the name table
// (though it might fail to read it, if memory isn't available);
// other checks here are just for extra paranoia.
nsresult gfxFontUtils::GetFullNameFromSFNT(const uint8_t* aFontData,
                                           uint32_t aLength,
                                           nsACString& aFullName) {
  aFullName = "(MISSING NAME)";  // should always get replaced

  const TableDirEntry* dirEntry =
      FindTableDirEntry(aFontData, TRUETYPE_TAG('n', 'a', 'm', 'e'));

  // should never fail, as we're only called after font validation succeeded
  NS_ENSURE_TRUE(dirEntry, NS_ERROR_NOT_AVAILABLE);

  uint32_t len = dirEntry->length;
  NS_ENSURE_TRUE(aLength > len && aLength - len >= dirEntry->offset,
                 NS_ERROR_UNEXPECTED);

  hb_blob_t* nameBlob =
      hb_blob_create((const char*)aFontData + dirEntry->offset, len,
                     HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  nsresult rv = GetFullNameFromTable(nameBlob, aFullName);
  hb_blob_destroy(nameBlob);

  return rv;
}

nsresult gfxFontUtils::GetFullNameFromTable(hb_blob_t* aNameTable,
                                            nsACString& aFullName) {
  nsAutoCString name;
  nsresult rv = gfxFontUtils::ReadCanonicalName(
      aNameTable, gfxFontUtils::NAME_ID_FULL, name);
  if (NS_SUCCEEDED(rv) && !name.IsEmpty()) {
    aFullName = name;
    return NS_OK;
  }
  rv = gfxFontUtils::ReadCanonicalName(aNameTable, gfxFontUtils::NAME_ID_FAMILY,
                                       name);
  if (NS_SUCCEEDED(rv) && !name.IsEmpty()) {
    nsAutoCString styleName;
    rv = gfxFontUtils::ReadCanonicalName(
        aNameTable, gfxFontUtils::NAME_ID_STYLE, styleName);
    if (NS_SUCCEEDED(rv) && !styleName.IsEmpty()) {
      name.Append(' ');
      name.Append(styleName);
      aFullName = name;
    }
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

nsresult gfxFontUtils::GetFamilyNameFromTable(hb_blob_t* aNameTable,
                                              nsACString& aFamilyName) {
  nsAutoCString name;
  nsresult rv = gfxFontUtils::ReadCanonicalName(
      aNameTable, gfxFontUtils::NAME_ID_FAMILY, name);
  if (NS_SUCCEEDED(rv) && !name.IsEmpty()) {
    aFamilyName = name;
    return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

enum {
#if defined(XP_MACOSX)
  CANONICAL_LANG_ID = gfxFontUtils::LANG_ID_MAC_ENGLISH,
  PLATFORM_ID = gfxFontUtils::PLATFORM_ID_MAC
#else
  CANONICAL_LANG_ID = gfxFontUtils::LANG_ID_MICROSOFT_EN_US,
  PLATFORM_ID = gfxFontUtils::PLATFORM_ID_MICROSOFT
#endif
};

nsresult gfxFontUtils::ReadNames(const char* aNameData, uint32_t aDataLen,
                                 uint32_t aNameID, int32_t aPlatformID,
                                 nsTArray<nsCString>& aNames) {
  return ReadNames(aNameData, aDataLen, aNameID, LANG_ALL, aPlatformID, aNames);
}

nsresult gfxFontUtils::ReadCanonicalName(hb_blob_t* aNameTable,
                                         uint32_t aNameID, nsCString& aName) {
  uint32_t nameTableLen;
  const char* nameTable = hb_blob_get_data(aNameTable, &nameTableLen);
  return ReadCanonicalName(nameTable, nameTableLen, aNameID, aName);
}

nsresult gfxFontUtils::ReadCanonicalName(const char* aNameData,
                                         uint32_t aDataLen, uint32_t aNameID,
                                         nsCString& aName) {
  nsresult rv;

  nsTArray<nsCString> names;

  // first, look for the English name (this will succeed 99% of the time)
  rv = ReadNames(aNameData, aDataLen, aNameID, CANONICAL_LANG_ID, PLATFORM_ID,
                 names);
  NS_ENSURE_SUCCESS(rv, rv);

  // otherwise, grab names for all languages
  if (names.Length() == 0) {
    rv = ReadNames(aNameData, aDataLen, aNameID, LANG_ALL, PLATFORM_ID, names);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#if defined(XP_MACOSX)
  // may be dealing with font that only has Microsoft name entries
  if (names.Length() == 0) {
    rv = ReadNames(aNameData, aDataLen, aNameID, LANG_ID_MICROSOFT_EN_US,
                   PLATFORM_ID_MICROSOFT, names);
    NS_ENSURE_SUCCESS(rv, rv);

    // getting really desperate now, take anything!
    if (names.Length() == 0) {
      rv = ReadNames(aNameData, aDataLen, aNameID, LANG_ALL,
                     PLATFORM_ID_MICROSOFT, names);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
#endif

  // return the first name (99.9% of the time names will
  // contain a single English name)
  if (names.Length()) {
    aName.Assign(names[0]);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

// Charsets to use for decoding Mac platform font names.
// This table is sorted by {encoding, language}, with the wildcard "ANY" being
// greater than any defined values for each field; we use a binary search on
// both fields, and fall back to matching only encoding if necessary

// Some "redundant" entries for specific combinations are included such as
// encoding=roman, lang=english, in order that common entries will be found
// on the first search.

const uint16_t ANY = 0xffff;
const gfxFontUtils::MacFontNameCharsetMapping
    gfxFontUtils::gMacFontNameCharsets[] = {
        {ENCODING_ID_MAC_ROMAN, LANG_ID_MAC_ENGLISH, MACINTOSH_ENCODING},
        {ENCODING_ID_MAC_ROMAN, LANG_ID_MAC_ICELANDIC, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_ROMAN, LANG_ID_MAC_TURKISH, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_ROMAN, LANG_ID_MAC_POLISH, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_ROMAN, LANG_ID_MAC_ROMANIAN, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_ROMAN, LANG_ID_MAC_CZECH, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_ROMAN, LANG_ID_MAC_SLOVAK, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_ROMAN, ANY, MACINTOSH_ENCODING},
        {ENCODING_ID_MAC_JAPANESE, LANG_ID_MAC_JAPANESE, SHIFT_JIS_ENCODING},
        {ENCODING_ID_MAC_JAPANESE, ANY, SHIFT_JIS_ENCODING},
        {ENCODING_ID_MAC_TRAD_CHINESE, LANG_ID_MAC_TRAD_CHINESE, BIG5_ENCODING},
        {ENCODING_ID_MAC_TRAD_CHINESE, ANY, BIG5_ENCODING},
        {ENCODING_ID_MAC_KOREAN, LANG_ID_MAC_KOREAN, EUC_KR_ENCODING},
        {ENCODING_ID_MAC_KOREAN, ANY, EUC_KR_ENCODING},
        {ENCODING_ID_MAC_ARABIC, LANG_ID_MAC_ARABIC, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_ARABIC, LANG_ID_MAC_URDU, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_ARABIC, LANG_ID_MAC_FARSI, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_ARABIC, ANY, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_HEBREW, LANG_ID_MAC_HEBREW, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_HEBREW, ANY, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_GREEK, ANY, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_CYRILLIC, ANY, X_MAC_CYRILLIC_ENCODING},
        {ENCODING_ID_MAC_DEVANAGARI, ANY, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_GURMUKHI, ANY, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_GUJARATI, ANY, X_USER_DEFINED_ENCODING},
        {ENCODING_ID_MAC_SIMP_CHINESE, LANG_ID_MAC_SIMP_CHINESE,
         GB18030_ENCODING},
        {ENCODING_ID_MAC_SIMP_CHINESE, ANY, GB18030_ENCODING}};

const Encoding* gfxFontUtils::gISOFontNameCharsets[] = {
    /* 0 */ WINDOWS_1252_ENCODING, /* US-ASCII */
    /* 1 */ nullptr, /* spec says "ISO 10646" but does not specify encoding
                        form! */
    /* 2 */ WINDOWS_1252_ENCODING /* ISO-8859-1 */
};

const Encoding* gfxFontUtils::gMSFontNameCharsets[] = {
    /* [0] ENCODING_ID_MICROSOFT_SYMBOL */ UTF_16BE_ENCODING,
    /* [1] ENCODING_ID_MICROSOFT_UNICODEBMP */ UTF_16BE_ENCODING,
    /* [2] ENCODING_ID_MICROSOFT_SHIFTJIS */ SHIFT_JIS_ENCODING,
    /* [3] ENCODING_ID_MICROSOFT_PRC */ nullptr,
    /* [4] ENCODING_ID_MICROSOFT_BIG5 */ BIG5_ENCODING,
    /* [5] ENCODING_ID_MICROSOFT_WANSUNG */ nullptr,
    /* [6] ENCODING_ID_MICROSOFT_JOHAB */ nullptr,
    /* [7] reserved */ nullptr,
    /* [8] reserved */ nullptr,
    /* [9] reserved */ nullptr,
    /*[10] ENCODING_ID_MICROSOFT_UNICODEFULL */ UTF_16BE_ENCODING};

struct MacCharsetMappingComparator {
  typedef gfxFontUtils::MacFontNameCharsetMapping MacFontNameCharsetMapping;
  const MacFontNameCharsetMapping& mSearchValue;
  explicit MacCharsetMappingComparator(
      const MacFontNameCharsetMapping& aSearchValue)
      : mSearchValue(aSearchValue) {}
  int operator()(const MacFontNameCharsetMapping& aEntry) const {
    if (mSearchValue < aEntry) {
      return -1;
    }
    if (aEntry < mSearchValue) {
      return 1;
    }
    return 0;
  }
};

// Return the Encoding object we should use to decode a font name
// given the name table attributes.
// Special return values:
//    X_USER_DEFINED_ENCODING  One of Mac legacy encodings that is not a part
//                             of Encoding Standard
//    nullptr                  unknown charset, do not attempt conversion
const Encoding* gfxFontUtils::GetCharsetForFontName(uint16_t aPlatform,
                                                    uint16_t aScript,
                                                    uint16_t aLanguage) {
  switch (aPlatform) {
    case PLATFORM_ID_UNICODE:
      return UTF_16BE_ENCODING;

    case PLATFORM_ID_MAC: {
      MacFontNameCharsetMapping searchValue = {aScript, aLanguage, nullptr};
      for (uint32_t i = 0; i < 2; ++i) {
        size_t idx;
        if (BinarySearchIf(gMacFontNameCharsets, 0,
                           ArrayLength(gMacFontNameCharsets),
                           MacCharsetMappingComparator(searchValue), &idx)) {
          return gMacFontNameCharsets[idx].mEncoding;
        }

        // no match, so try again finding one in any language
        searchValue.mLanguage = ANY;
      }
    } break;

    case PLATFORM_ID_ISO:
      if (aScript < ArrayLength(gISOFontNameCharsets)) {
        return gISOFontNameCharsets[aScript];
      }
      break;

    case PLATFORM_ID_MICROSOFT:
      if (aScript < ArrayLength(gMSFontNameCharsets)) {
        return gMSFontNameCharsets[aScript];
      }
      break;
  }

  return nullptr;
}

template <int N>
static bool StartsWith(const nsACString& string, const char (&prefix)[N]) {
  if (N - 1 > string.Length()) {
    return false;
  }
  return memcmp(string.Data(), prefix, N - 1) == 0;
}

// convert a raw name from the name table to an nsString, if possible;
// return value indicates whether conversion succeeded
bool gfxFontUtils::DecodeFontName(const char* aNameData, int32_t aByteLen,
                                  uint32_t aPlatformCode, uint32_t aScriptCode,
                                  uint32_t aLangCode, nsACString& aName) {
  if (aByteLen <= 0) {
    NS_WARNING("empty font name");
    aName.SetLength(0);
    return true;
  }

  auto encoding = GetCharsetForFontName(aPlatformCode, aScriptCode, aLangCode);

  if (!encoding) {
    // nullptr -> unknown charset
#ifdef DEBUG
    char warnBuf[128];
    if (aByteLen > 64) aByteLen = 64;
    SprintfLiteral(warnBuf,
                   "skipping font name, unknown charset %d:%d:%d for <%.*s>",
                   aPlatformCode, aScriptCode, aLangCode, aByteLen, aNameData);
    NS_WARNING(warnBuf);
#endif
    return false;
  }

  if (encoding == X_USER_DEFINED_ENCODING) {
#ifdef XP_MACOSX
    // Special case for macOS only: support legacy Mac encodings
    // that aren't part of the Encoding Standard.
    if (aPlatformCode == PLATFORM_ID_MAC) {
      CFStringRef str =
          CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)aNameData,
                                  aByteLen, aScriptCode, false);
      if (str) {
        CFIndex length = CFStringGetLength(str);
        nsAutoString name16;
        name16.SetLength(length);
        CFStringGetCharacters(str, CFRangeMake(0, length),
                              (UniChar*)name16.BeginWriting());
        CFRelease(str);
        CopyUTF16toUTF8(name16, aName);
        return true;
      }
    }
#endif
    NS_WARNING("failed to get the decoder for a font name string");
    return false;
  }

  auto rv = encoding->DecodeWithoutBOMHandling(
      nsDependentCSubstring(aNameData, aByteLen), aName);
  return NS_SUCCEEDED(rv);
}

nsresult gfxFontUtils::ReadNames(const char* aNameData, uint32_t aDataLen,
                                 uint32_t aNameID, int32_t aLangID,
                                 int32_t aPlatformID,
                                 nsTArray<nsCString>& aNames) {
  NS_ASSERTION(aDataLen != 0, "null name table");

  if (!aDataLen) {
    return NS_ERROR_FAILURE;
  }

  // -- name table data
  const NameHeader* nameHeader = reinterpret_cast<const NameHeader*>(aNameData);

  uint32_t nameCount = nameHeader->count;

  // -- sanity check the number of name records
  if (uint64_t(nameCount) * sizeof(NameRecord) > aDataLen) {
    NS_WARNING("invalid font (name table data)");
    return NS_ERROR_FAILURE;
  }

  // -- iterate through name records
  const NameRecord* nameRecord =
      reinterpret_cast<const NameRecord*>(aNameData + sizeof(NameHeader));
  uint64_t nameStringsBase = uint64_t(nameHeader->stringOffset);

  uint32_t i;
  for (i = 0; i < nameCount; i++, nameRecord++) {
    uint32_t platformID;

    // skip over unwanted nameID's
    if (uint32_t(nameRecord->nameID) != aNameID) {
      continue;
    }

    // skip over unwanted platform data
    platformID = nameRecord->platformID;
    if (aPlatformID != PLATFORM_ALL && platformID != uint32_t(aPlatformID)) {
      continue;
    }

    // skip over unwanted languages
    if (aLangID != LANG_ALL &&
        uint32_t(nameRecord->languageID) != uint32_t(aLangID)) {
      continue;
    }

    // add name to names array

    // -- calculate string location
    uint32_t namelen = nameRecord->length;
    uint32_t nameoff =
        nameRecord->offset;  // offset from base of string storage

    if (nameStringsBase + uint64_t(nameoff) + uint64_t(namelen) > aDataLen) {
      NS_WARNING("invalid font (name table strings)");
      return NS_ERROR_FAILURE;
    }

    // -- decode if necessary and make nsString
    nsAutoCString name;

    DecodeFontName(aNameData + nameStringsBase + nameoff, namelen, platformID,
                   uint32_t(nameRecord->encodingID),
                   uint32_t(nameRecord->languageID), name);

    uint32_t k, numNames;
    bool foundName = false;

    numNames = aNames.Length();
    for (k = 0; k < numNames; k++) {
      if (name.Equals(aNames[k])) {
        foundName = true;
        break;
      }
    }

    if (!foundName) aNames.AppendElement(name);
  }

  return NS_OK;
}

#pragma pack(1)

struct COLRBaseGlyphRecord {
  AutoSwap_PRUint16 glyphId;
  AutoSwap_PRUint16 firstLayerIndex;
  AutoSwap_PRUint16 numLayers;
};

struct COLRLayerRecord {
  AutoSwap_PRUint16 glyphId;
  AutoSwap_PRUint16 paletteEntryIndex;
};

struct CPALColorRecord {
  uint8_t blue;
  uint8_t green;
  uint8_t red;
  uint8_t alpha;
};

#pragma pack()

bool gfxFontUtils::ValidateColorGlyphs(hb_blob_t* aCOLR, hb_blob_t* aCPAL) {
  unsigned int colrLength;
  const COLRHeader* colr =
      reinterpret_cast<const COLRHeader*>(hb_blob_get_data(aCOLR, &colrLength));
  unsigned int cpalLength;
  const CPALHeaderVersion0* cpal = reinterpret_cast<const CPALHeaderVersion0*>(
      hb_blob_get_data(aCPAL, &cpalLength));

  if (!colr || !cpal || !colrLength || !cpalLength) {
    return false;
  }

  if (uint16_t(colr->version) != 0 || uint16_t(cpal->version) != 0) {
    // We only support version 0 headers.
    return false;
  }

  const uint32_t offsetBaseGlyphRecord = colr->offsetBaseGlyphRecord;
  const uint16_t numBaseGlyphRecord = colr->numBaseGlyphRecord;
  const uint32_t offsetLayerRecord = colr->offsetLayerRecord;
  const uint16_t numLayerRecords = colr->numLayerRecords;

  const uint32_t offsetFirstColorRecord = cpal->offsetFirstColorRecord;
  const uint16_t numColorRecords = cpal->numColorRecords;
  const uint32_t numPaletteEntries = cpal->numPaletteEntries;

  if (offsetBaseGlyphRecord >= colrLength) {
    return false;
  }

  if (offsetLayerRecord >= colrLength) {
    return false;
  }

  if (offsetFirstColorRecord >= cpalLength) {
    return false;
  }

  if (!numPaletteEntries) {
    return false;
  }

  if (sizeof(COLRBaseGlyphRecord) * numBaseGlyphRecord >
      colrLength - offsetBaseGlyphRecord) {
    // COLR base glyph record will be overflow
    return false;
  }

  if (sizeof(COLRLayerRecord) * numLayerRecords >
      colrLength - offsetLayerRecord) {
    // COLR layer record will be overflow
    return false;
  }

  if (sizeof(CPALColorRecord) * numColorRecords >
      cpalLength - offsetFirstColorRecord) {
    // CPAL color record will be overflow
    return false;
  }

  if (numPaletteEntries * uint16_t(cpal->numPalettes) != numColorRecords) {
    // palette of CPAL color record will be overflow.
    return false;
  }

  uint16_t lastGlyphId = 0;
  const COLRBaseGlyphRecord* baseGlyph =
      reinterpret_cast<const COLRBaseGlyphRecord*>(
          reinterpret_cast<const uint8_t*>(colr) + offsetBaseGlyphRecord);

  for (uint16_t i = 0; i < numBaseGlyphRecord; i++, baseGlyph++) {
    const uint32_t firstLayerIndex = baseGlyph->firstLayerIndex;
    const uint16_t numLayers = baseGlyph->numLayers;
    const uint16_t glyphId = baseGlyph->glyphId;

    if (lastGlyphId && lastGlyphId >= glyphId) {
      // glyphId must be sorted
      return false;
    }
    lastGlyphId = glyphId;

    if (!numLayers) {
      // no layer
      return false;
    }
    if (firstLayerIndex + numLayers > numLayerRecords) {
      // layer length of target glyph is overflow
      return false;
    }
  }

  const COLRLayerRecord* layer = reinterpret_cast<const COLRLayerRecord*>(
      reinterpret_cast<const uint8_t*>(colr) + offsetLayerRecord);

  for (uint16_t i = 0; i < numLayerRecords; i++, layer++) {
    if (uint16_t(layer->paletteEntryIndex) >= numPaletteEntries &&
        uint16_t(layer->paletteEntryIndex) != 0xFFFF) {
      // CPAL palette entry record is overflow
      return false;
    }
  }

  return true;
}

static int CompareBaseGlyph(const void* key, const void* data) {
  uint32_t glyphId = (uint32_t)(uintptr_t)key;
  const COLRBaseGlyphRecord* baseGlyph =
      reinterpret_cast<const COLRBaseGlyphRecord*>(data);
  uint32_t baseGlyphId = uint16_t(baseGlyph->glyphId);

  if (baseGlyphId == glyphId) {
    return 0;
  }

  return baseGlyphId > glyphId ? -1 : 1;
}

static COLRBaseGlyphRecord* LookForBaseGlyphRecord(const COLRHeader* aCOLR,
                                                   uint32_t aGlyphId) {
  const uint8_t* baseGlyphRecords = reinterpret_cast<const uint8_t*>(aCOLR) +
                                    uint32_t(aCOLR->offsetBaseGlyphRecord);
  // BaseGlyphRecord is sorted by glyphId
  return reinterpret_cast<COLRBaseGlyphRecord*>(
      bsearch((void*)(uintptr_t)aGlyphId, baseGlyphRecords,
              uint16_t(aCOLR->numBaseGlyphRecord), sizeof(COLRBaseGlyphRecord),
              CompareBaseGlyph));
}

bool gfxFontUtils::GetColorGlyphLayers(hb_blob_t* aCOLR, hb_blob_t* aCPAL,
                                       uint32_t aGlyphId,
                                       const mozilla::gfx::Color& aDefaultColor,
                                       nsTArray<uint16_t>& aGlyphs,
                                       nsTArray<mozilla::gfx::Color>& aColors) {
  unsigned int blobLength;
  const COLRHeader* colr =
      reinterpret_cast<const COLRHeader*>(hb_blob_get_data(aCOLR, &blobLength));
  MOZ_ASSERT(colr, "Cannot get COLR raw data");
  MOZ_ASSERT(blobLength, "Found COLR data, but length is 0");

  COLRBaseGlyphRecord* baseGlyph = LookForBaseGlyphRecord(colr, aGlyphId);
  if (!baseGlyph) {
    return false;
  }

  const CPALHeaderVersion0* cpal = reinterpret_cast<const CPALHeaderVersion0*>(
      hb_blob_get_data(aCPAL, &blobLength));
  MOZ_ASSERT(cpal, "Cannot get CPAL raw data");
  MOZ_ASSERT(blobLength, "Found CPAL data, but length is 0");

  const COLRLayerRecord* layer = reinterpret_cast<const COLRLayerRecord*>(
      reinterpret_cast<const uint8_t*>(colr) +
      uint32_t(colr->offsetLayerRecord) +
      sizeof(COLRLayerRecord) * uint16_t(baseGlyph->firstLayerIndex));
  const uint16_t numLayers = baseGlyph->numLayers;
  const uint32_t offsetFirstColorRecord = cpal->offsetFirstColorRecord;

  for (uint16_t layerIndex = 0; layerIndex < numLayers; layerIndex++) {
    aGlyphs.AppendElement(uint16_t(layer->glyphId));
    if (uint16_t(layer->paletteEntryIndex) == 0xFFFF) {
      aColors.AppendElement(aDefaultColor);
    } else {
      const CPALColorRecord* color = reinterpret_cast<const CPALColorRecord*>(
          reinterpret_cast<const uint8_t*>(cpal) + offsetFirstColorRecord +
          sizeof(CPALColorRecord) * uint16_t(layer->paletteEntryIndex));
      aColors.AppendElement(
          mozilla::gfx::Color(color->red / 255.0, color->green / 255.0,
                              color->blue / 255.0, color->alpha / 255.0));
    }
    layer++;
  }
  return true;
}

void gfxFontUtils::GetVariationInstances(
    gfxFontEntry* aFontEntry, nsTArray<gfxFontVariationInstance>& aInstances) {
  MOZ_ASSERT(aInstances.IsEmpty());

  if (!aFontEntry->HasVariations()) {
    return;
  }

  // Some platforms don't offer a simple API to return the list of instances,
  // so we have to interpret the 'fvar' table ourselves.

  // https://www.microsoft.com/typography/otspec/fvar.htm#fvarHeader
  struct FvarHeader {
    AutoSwap_PRUint16 majorVersion;
    AutoSwap_PRUint16 minorVersion;
    AutoSwap_PRUint16 axesArrayOffset;
    AutoSwap_PRUint16 reserved;
    AutoSwap_PRUint16 axisCount;
    AutoSwap_PRUint16 axisSize;
    AutoSwap_PRUint16 instanceCount;
    AutoSwap_PRUint16 instanceSize;
  };

  // https://www.microsoft.com/typography/otspec/fvar.htm#variationAxisRecord
  struct AxisRecord {
    AutoSwap_PRUint32 axisTag;
    AutoSwap_PRInt32 minValue;
    AutoSwap_PRInt32 defaultValue;
    AutoSwap_PRInt32 maxValue;
    AutoSwap_PRUint16 flags;
    AutoSwap_PRUint16 axisNameID;
  };

  // https://www.microsoft.com/typography/otspec/fvar.htm#instanceRecord
  struct InstanceRecord {
    AutoSwap_PRUint16 subfamilyNameID;
    AutoSwap_PRUint16 flags;
    AutoSwap_PRInt32 coordinates[1];  // variable-size array [axisCount]
    // The variable-length 'coordinates' array may be followed by an
    // optional extra field 'postScriptNameID'. We can't directly
    // represent this in the struct, because its offset varies depending
    // on the number of axes present.
    // (Not currently used by our code here anyhow.)
    //  AutoSwap_PRUint16 postScriptNameID;
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

  // Load the two font tables we need as harfbuzz blobs; if either is absent,
  // just bail out.
  AutoHBBlob fvarTable(
      aFontEntry->GetFontTable(TRUETYPE_TAG('f', 'v', 'a', 'r')));
  AutoHBBlob nameTable(
      aFontEntry->GetFontTable(TRUETYPE_TAG('n', 'a', 'm', 'e')));
  if (!fvarTable || !nameTable) {
    return;
  }
  unsigned int len;
  const char* data = hb_blob_get_data(fvarTable, &len);
  if (len < sizeof(FvarHeader)) {
    return;
  }
  // Read the fields of the table header; bail out if it looks broken.
  auto fvar = reinterpret_cast<const FvarHeader*>(data);
  if (uint16_t(fvar->majorVersion) != 1 || uint16_t(fvar->minorVersion) != 0 ||
      uint16_t(fvar->reserved) != 2) {
    return;
  }
  uint16_t axisCount = fvar->axisCount;
  uint16_t axisSize = fvar->axisSize;
  uint16_t instanceCount = fvar->instanceCount;
  uint16_t instanceSize = fvar->instanceSize;
  if (axisCount ==
          0 ||  // no axes?
                // https://www.microsoft.com/typography/otspec/fvar.htm#axisSize
      axisSize != 20 ||      // required value for current table version
      instanceCount == 0 ||  // no instances?
      // https://www.microsoft.com/typography/otspec/fvar.htm#instanceSize
      (instanceSize != axisCount * sizeof(int32_t) + 4 &&
       instanceSize != axisCount * sizeof(int32_t) + 6)) {
    return;
  }
  // Check that axis array will not exceed table size
  uint16_t axesOffset = fvar->axesArrayOffset;
  if (axesOffset + uint32_t(axisCount) * axisSize > len) {
    return;
  }
  // Get pointer to the array of axis records
  auto axes = reinterpret_cast<const AxisRecord*>(data + axesOffset);
  // Get address of instance array, and check it doesn't overflow table size.
  // https://www.microsoft.com/typography/otspec/fvar.htm#axisAndInstanceArrays
  auto instData = data + axesOffset + axisCount * axisSize;
  if (instData + uint32_t(instanceCount) * instanceSize > data + len) {
    return;
  }
  aInstances.SetCapacity(instanceCount);
  for (unsigned i = 0; i < instanceCount; ++i, instData += instanceSize) {
    // Typed pointer to the current instance record, to read its fields.
    auto inst = reinterpret_cast<const InstanceRecord*>(instData);
    // Pointer to the coordinates array within the instance record.
    // This array has axisCount elements, and is included in instanceSize
    // (which depends on axisCount, and was validated above) so we know
    // access to coords[j] below will not be outside the table bounds.
    auto coords = &inst->coordinates[0];
    gfxFontVariationInstance instance;
    uint16_t nameID = inst->subfamilyNameID;
    nsresult rv = ReadCanonicalName(nameTable, nameID, instance.mName);
    if (NS_FAILED(rv)) {
      // If no name was available for the instance, ignore it.
      continue;
    }
    instance.mValues.SetCapacity(axisCount);
    for (unsigned j = 0; j < axisCount; ++j) {
      gfxFontVariationValue value;
      value.mAxis = axes[j].axisTag;
      value.mValue = int32_t(coords[j]) / 65536.0;
      instance.mValues.AppendElement(value);
    }
    aInstances.AppendElement(instance);
  }
}

void gfxFontUtils::ReadOtherFamilyNamesForFace(
    const nsACString& aFamilyName, const char* aNameData, uint32_t aDataLength,
    nsTArray<nsCString>& aOtherFamilyNames, bool useFullName) {
  const NameHeader* nameHeader = reinterpret_cast<const NameHeader*>(aNameData);

  uint32_t nameCount = nameHeader->count;
  if (nameCount * sizeof(NameRecord) > aDataLength) {
    NS_WARNING("invalid font (name records)");
    return;
  }

  const NameRecord* nameRecord =
      reinterpret_cast<const NameRecord*>(aNameData + sizeof(NameHeader));
  uint32_t stringsBase = uint32_t(nameHeader->stringOffset);

  for (uint32_t i = 0; i < nameCount; i++, nameRecord++) {
    uint32_t nameLen = nameRecord->length;
    uint32_t nameOff =
        nameRecord->offset;  // offset from base of string storage

    if (stringsBase + nameOff + nameLen > aDataLength) {
      NS_WARNING("invalid font (name table strings)");
      return;
    }

    uint16_t nameID = nameRecord->nameID;
    if ((useFullName && nameID == NAME_ID_FULL) ||
        (!useFullName &&
         (nameID == NAME_ID_FAMILY || nameID == NAME_ID_PREFERRED_FAMILY))) {
      nsAutoCString otherFamilyName;
      bool ok = DecodeFontName(
          aNameData + stringsBase + nameOff, nameLen,
          uint32_t(nameRecord->platformID), uint32_t(nameRecord->encodingID),
          uint32_t(nameRecord->languageID), otherFamilyName);
      // add if not same as canonical family name
      if (ok && otherFamilyName != aFamilyName &&
          !aOtherFamilyNames.Contains(otherFamilyName)) {
        aOtherFamilyNames.AppendElement(otherFamilyName);
      }
    }
  }
}

#ifdef XP_WIN

/* static */
bool gfxFontUtils::IsCffFont(const uint8_t* aFontData) {
  // this is only called after aFontData has passed basic validation,
  // so we know there is enough data present to allow us to read the version!
  const SFNTHeader* sfntHeader = reinterpret_cast<const SFNTHeader*>(aFontData);
  return (sfntHeader->sfntVersion == TRUETYPE_TAG('O', 'T', 'T', 'O'));
}

#endif

#undef acceptablePlatform
#undef isSymbol
#undef isUVSEncoding
#undef LOG
#undef LOG_ENABLED
