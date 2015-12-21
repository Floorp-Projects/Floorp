/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_SFNTNameTable_h
#define mozilla_gfx_SFNTNameTable_h

#include "mozilla/Function.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "u16string.h"

namespace mozilla {
namespace gfx {

struct NameHeader;
struct NameRecord;

typedef Vector<Function<bool(const NameRecord*)>> NameRecordMatchers;

class SFNTNameTable final
{
public:

  /**
   * Creates a SFNTNameTable if the header data is valid. Note that the data is
   * NOT copied, so must exist for the lifetime of the table.
   *
   * @param aNameData the Name Table data.
   * @param aDataLength length
   * @return UniquePtr to a SFNTNameTable or nullptr if the header is invalid.
   */
  static UniquePtr<SFNTNameTable> Create(const uint8_t *aNameData,
                                         uint32_t aDataLength);

  /**
   * Gets the full name from the name table. If the full name string is not
   * present it will use the family space concatenated with the style.
   * This will only read names that are already UTF16.
   *
   * @param aU16FullName string to be populated with the full name.
   * @return true if the full name is successfully read.
   */
  bool GetU16FullName(mozilla::u16string& aU16FullName);

private:

  SFNTNameTable(const NameHeader *aNameHeader, const uint8_t *aNameData,
                uint32_t aDataLength);

  bool ReadU16Name(const NameRecordMatchers& aMatchers, mozilla::u16string& aU16Name);

  bool ReadU16NameFromRecord(const NameRecord *aNameRecord,
                             mozilla::u16string& aU16Name);

  const NameRecord *mFirstRecord;
  const NameRecord *mEndOfRecords;
  const uint8_t *mStringData;
  const uint32_t mStringDataLength;
};

} // gfx
} // mozilla

#endif // mozilla_gfx_SFNTNameTable_h
