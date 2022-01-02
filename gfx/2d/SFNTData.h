/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_SFNTData_h
#define mozilla_gfx_SFNTData_h

#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

namespace mozilla {
namespace gfx {

class SFNTData final {
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
  static UniquePtr<SFNTData> Create(const uint8_t* aFontData,
                                    uint32_t aDataLength);

  /**
   * Creates a unique key for the given font data and variation settings.
   *
   * @param aFontData the SFNT data
   * @param aDataLength length
   * @return unique key to be used for caching
   */
  static uint64_t GetUniqueKey(const uint8_t* aFontData, uint32_t aDataLength,
                               uint32_t aVarDataSize, const void* aVarData);

  ~SFNTData();

 private:
  SFNTData() = default;

  bool AddFont(const uint8_t* aFontData, uint32_t aDataLength,
               uint32_t aOffset);

  uint32_t HashHeadTables();

  // Internal representation of single font in font file.
  class Font;

  Vector<Font*> mFonts;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_SFNTData_h
