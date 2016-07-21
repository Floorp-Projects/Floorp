/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SFNTNameTable.h"

#include "BigEndianInts.h"
#include "Logging.h"
#include "mozilla/Move.h"

namespace mozilla {
namespace gfx {

static const BigEndianUint16 FORMAT_0 = 0;

static const BigEndianUint16 NAME_ID_FAMILY = 1;
static const BigEndianUint16 NAME_ID_STYLE = 2;
static const BigEndianUint16 NAME_ID_FULL = 4;

static const BigEndianUint16 PLATFORM_ID_UNICODE = 0;
static const BigEndianUint16 PLATFORM_ID_MAC = 1;
static const BigEndianUint16 PLATFORM_ID_MICROSOFT = 3;

static const BigEndianUint16 ENCODING_ID_MICROSOFT_SYMBOL = 0;
static const BigEndianUint16 ENCODING_ID_MICROSOFT_UNICODEBMP = 1;
static const BigEndianUint16 ENCODING_ID_MICROSOFT_UNICODEFULL = 10;

static const BigEndianUint16 LANG_ID_MAC_ENGLISH = 0;

static const BigEndianUint16 LANG_ID_MICROSOFT_EN_US = 0x0409;

#pragma pack(push, 1)

// Name table has a header, followed by name records, followed by string data.
struct NameHeader
{
  BigEndianUint16 format;       // Format selector (=0).
  BigEndianUint16 count;        // Number of name records.
  BigEndianUint16 stringOffset; // Offset to string storage from start of table.
};

struct NameRecord
{
  BigEndianUint16 platformID;
  BigEndianUint16 encodingID; // Platform-specific encoding ID
  BigEndianUint16 languageID;
  BigEndianUint16 nameID;
  BigEndianUint16 length;     // String length in bytes.
  BigEndianUint16 offset;     // String offset from start of storage in bytes.
};

#pragma pack(pop)

/* static */
UniquePtr<SFNTNameTable>
SFNTNameTable::Create(const uint8_t *aNameData, uint32_t aDataLength)
{
  MOZ_ASSERT(aNameData);

  if (aDataLength < sizeof(NameHeader)) {
    gfxWarning() << "Name data too short to contain NameHeader.";
    return nullptr;
  }

  const NameHeader *nameHeader = reinterpret_cast<const NameHeader*>(aNameData);
  if (nameHeader->format != FORMAT_0) {
    gfxWarning() << "Only Name Table Format 0 is supported.";
    return nullptr;
  }

  uint16_t stringOffset = nameHeader->stringOffset;

  if (stringOffset !=
      sizeof(NameHeader) + (nameHeader->count * sizeof(NameRecord))) {
    gfxWarning() << "Name table string offset is incorrect.";
    return nullptr;
  }

  if (aDataLength < stringOffset) {
    gfxWarning() << "Name data too short to contain name records.";
    return nullptr;
  }

  return UniquePtr<SFNTNameTable>(
    new SFNTNameTable(nameHeader, aNameData, aDataLength));
}

SFNTNameTable::SFNTNameTable(const NameHeader *aNameHeader,
                             const uint8_t *aNameData, uint32_t aDataLength)
  : mFirstRecord(reinterpret_cast<const NameRecord*>(aNameData
                                                     + sizeof(NameHeader)))
  , mEndOfRecords(mFirstRecord + aNameHeader->count)
  , mStringData(aNameData + aNameHeader->stringOffset)
  , mStringDataLength(aDataLength - aNameHeader->stringOffset)
{
  MOZ_ASSERT(reinterpret_cast<const uint8_t*>(aNameHeader) == aNameData);
}

#if defined(XP_MACOSX)
static const BigEndianUint16 CANONICAL_LANG_ID = LANG_ID_MAC_ENGLISH;
static const BigEndianUint16 PLATFORM_ID = PLATFORM_ID_MAC;
#else
static const BigEndianUint16 CANONICAL_LANG_ID = LANG_ID_MICROSOFT_EN_US;
static const BigEndianUint16 PLATFORM_ID = PLATFORM_ID_MICROSOFT;
#endif

static bool
IsUTF16Encoding(const NameRecord *aNameRecord)
{
  if (aNameRecord->platformID == PLATFORM_ID_MICROSOFT &&
      (aNameRecord->encodingID == ENCODING_ID_MICROSOFT_UNICODEBMP ||
       aNameRecord->encodingID == ENCODING_ID_MICROSOFT_SYMBOL)) {
    return true;
  }

  if (aNameRecord->platformID == PLATFORM_ID_UNICODE) {
    return true;
  }

  return false;
}

static NameRecordMatchers*
CreateCanonicalU16Matchers(const BigEndianUint16& aNameID)
{
  NameRecordMatchers *matchers = new NameRecordMatchers();

  // First, look for the English name (this will normally succeed).
  if (!matchers->append(
    [=](const NameRecord *aNameRecord) {
        return aNameRecord->nameID == aNameID &&
               aNameRecord->languageID == CANONICAL_LANG_ID &&
               aNameRecord->platformID == PLATFORM_ID &&
               IsUTF16Encoding(aNameRecord);
    })) {
    MOZ_CRASH();
  }

  // Second, look for all languages.
  if (!matchers->append(
    [=](const NameRecord *aNameRecord) {
        return aNameRecord->nameID == aNameID &&
               aNameRecord->platformID == PLATFORM_ID &&
               IsUTF16Encoding(aNameRecord);
    })) {
    MOZ_CRASH();
  }

#if defined(XP_MACOSX)
  // On Mac may be dealing with font that only has Microsoft name entries.
  if (!matchers->append(
    [=](const NameRecord *aNameRecord) {
        return aNameRecord->nameID == aNameID &&
               aNameRecord->languageID == LANG_ID_MICROSOFT_EN_US &&
               aNameRecord->platformID == PLATFORM_ID_MICROSOFT &&
               IsUTF16Encoding(aNameRecord);
    })) {
    MOZ_CRASH();
  }
  if (!matchers->append(
    [=](const NameRecord *aNameRecord) {
        return aNameRecord->nameID == aNameID &&
               aNameRecord->platformID == PLATFORM_ID_MICROSOFT &&
               IsUTF16Encoding(aNameRecord);
    })) {
    MOZ_CRASH();
  }
#endif

  return matchers;
}

static const NameRecordMatchers&
FullNameMatchers()
{
  static const NameRecordMatchers *sFullNameMatchers =
    CreateCanonicalU16Matchers(NAME_ID_FULL);
  return *sFullNameMatchers;
}

static const NameRecordMatchers&
FamilyMatchers()
{
  static const NameRecordMatchers *sFamilyMatchers =
    CreateCanonicalU16Matchers(NAME_ID_FAMILY);
  return *sFamilyMatchers;
}

static const NameRecordMatchers&
StyleMatchers()
{
  static const NameRecordMatchers *sStyleMatchers =
    CreateCanonicalU16Matchers(NAME_ID_STYLE);
  return *sStyleMatchers;
}

bool
SFNTNameTable::GetU16FullName(mozilla::u16string& aU16FullName)
{
  if (ReadU16Name(FullNameMatchers(), aU16FullName)) {
    return true;
  }

  // If the full name record doesn't exist create the name from the family space
  // concatenated with the style.
  mozilla::u16string familyName;
  if (!ReadU16Name(FamilyMatchers(), familyName)) {
    return false;
  }

  mozilla::u16string styleName;
  if (!ReadU16Name(StyleMatchers(), styleName)) {
    return false;
  }

  aU16FullName.assign(Move(familyName));
  aU16FullName.append(u" ");
  aU16FullName.append(styleName);
  return true;
}

bool
SFNTNameTable::ReadU16Name(const NameRecordMatchers& aMatchers,
                           mozilla::u16string& aU16Name)
{
  MOZ_ASSERT(!aMatchers.empty());

  for (size_t i = 0; i < aMatchers.length(); ++i) {
    const NameRecord* record = mFirstRecord;
    while (record != mEndOfRecords) {
      if (aMatchers[i](record)) {
        return ReadU16NameFromRecord(record, aU16Name);
      }
      ++record;
    }
  }

  return false;
}

bool
SFNTNameTable::ReadU16NameFromRecord(const NameRecord *aNameRecord,
                                     mozilla::u16string& aU16Name)
{
  uint32_t offset = aNameRecord->offset;
  uint32_t length = aNameRecord->length;
  if (mStringDataLength < offset + length) {
    gfxWarning() << "Name data too short to contain name string.";
    return false;
  }

  const uint8_t *startOfName = mStringData + offset;
  size_t actualLength = length / sizeof(char16_t);
  UniquePtr<char16_t[]> nameData(new char16_t[actualLength]);
  NativeEndian::copyAndSwapFromBigEndian(nameData.get(), startOfName,
                                         actualLength);

  aU16Name.assign(nameData.get(), actualLength);
  return true;
}

} // gfx
} // mozilla
