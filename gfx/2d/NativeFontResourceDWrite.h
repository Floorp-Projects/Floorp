/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_NativeFontResourceDWrite_h
#define mozilla_gfx_NativeFontResourceDWrite_h

#include <dwrite.h>

#include "2D.h"
#include "mozilla/AlreadyAddRefed.h"

namespace mozilla {
namespace gfx {

class NativeFontResourceDWrite final : public NativeFontResource {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(NativeFontResourceDWrite, override)

  /**
   * Creates a NativeFontResourceDWrite if data is valid. Note aFontData will be
   * copied if required and so can be released after calling.
   *
   * @param aFontData the SFNT data.
   * @param aDataLength length of data.
   * @param aNeedsCairo whether the ScaledFont created needs a cairo scaled font
   * @return Referenced NativeFontResourceDWrite or nullptr if invalid.
   */
  static already_AddRefed<NativeFontResourceDWrite> Create(uint8_t* aFontData,
                                                           uint32_t aDataLength,
                                                           bool aNeedsCairo);

  already_AddRefed<UnscaledFont> CreateUnscaledFont(
      uint32_t aIndex, const uint8_t* aInstanceData,
      uint32_t aInstanceDataLength) final;

 private:
  NativeFontResourceDWrite(
      IDWriteFactory* aFactory, already_AddRefed<IDWriteFontFile> aFontFile,
      already_AddRefed<IDWriteFontFileStream> aFontFileStream,
      DWRITE_FONT_FACE_TYPE aFaceType, uint32_t aNumberOfFaces,
      bool aNeedsCairo)
      : mFactory(aFactory),
        mFontFile(aFontFile),
        mFontFileStream(aFontFileStream),
        mFaceType(aFaceType),
        mNumberOfFaces(aNumberOfFaces),
        mNeedsCairo(aNeedsCairo) {}

  IDWriteFactory* mFactory;
  RefPtr<IDWriteFontFile> mFontFile;
  RefPtr<IDWriteFontFileStream> mFontFileStream;
  DWRITE_FONT_FACE_TYPE mFaceType;
  uint32_t mNumberOfFaces;
  bool mNeedsCairo;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_NativeFontResourceDWrite_h
