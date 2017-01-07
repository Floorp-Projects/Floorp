/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_SFNTData_h
#define mozilla_gfx_SFNTData_h

#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "u16string.h"

namespace mozilla {
namespace gfx {

class SFNTData final
{
public:

  /**
   * Creates an SFNTData if the header is a format that we understand and
   * aDataLength is sufficient for the length information in the header data.
   * Note that the data is NOT copied, so must exist the SFNTData's lifetime.
   *
   * @param aFontData the SFNT data.
   * @param aDataLength length
   * @return UniquePtr to a SFNTData or nullptr if the header is invalid.
   */
  static UniquePtr<SFNTData> Create(const uint8_t *aFontData,
                                    uint32_t aDataLength);

  /**
   * Creates a unique key for the given font data and variation settings.
   *
   * @param aFontData the SFNT data
   * @param aDataLength length
   * @return unique key to be used for caching
   */
  static uint64_t GetUniqueKey(const uint8_t *aFontData, uint32_t aDataLength,
                               uint32_t aVarDataSize, const void* aVarData);

  ~SFNTData();

  /**
   * Gets the full name from the name table of the font corresponding to the
   * index. If the full name string is not present it will use the family space
   * concatenated with the style.
   * This will only read names that are already UTF16.
   *
   * @param aFontData SFNT data.
   * @param aDataLength length of aFontData.
   * @param aU16FullName string to be populated with the full name.
   * @return true if the full name is successfully read.
   */
  bool GetU16FullName(uint32_t aIndex, mozilla::u16string& aU16FullName);

  /**
   * Populate a Vector with the first UTF16 full name from each name table of
   * the fonts. If the full name string is not present it will use the family
   * space concatenated with the style.
   * This will only read names that are already UTF16.
   *
   * @param aU16FullNames the Vector to be populated.
   * @return true if at least one name found otherwise false.
   */
  bool GetU16FullNames(Vector<mozilla::u16string>& aU16FullNames);

  /**
   * Returns the index for the first UTF16 name matching aU16FullName.
   *
   * @param aU16FullName full name to find.
   * @param aIndex out param for the index if found.
   * @param aTruncatedLen length to truncate the compared font name to.
   * @return true if the full name is successfully read.
   */
  bool GetIndexForU16Name(const mozilla::u16string& aU16FullName, uint32_t* aIndex,
                          size_t aTruncatedLen = 0);

private:

  SFNTData() {}

  bool AddFont(const uint8_t *aFontData, uint32_t aDataLength,
               uint32_t aOffset);

  // Internal representation of single font in font file.
  class Font;

  Vector<Font*> mFonts;
};

} // gfx
} // mozilla

#endif // mozilla_gfx_SFNTData_h
