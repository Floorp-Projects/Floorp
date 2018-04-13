/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeFontResourceFontconfig.h"
#include "ScaledFontFontconfig.h"
#include "UnscaledFontFreeType.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

NativeFontResourceFontconfig::NativeFontResourceFontconfig(UniquePtr<uint8_t[]>&& aFontData, FT_Face aFace)
  : mFontData(Move(aFontData)),
    mFace(aFace)
{
}

NativeFontResourceFontconfig::~NativeFontResourceFontconfig()
{
  if (mFace) {
    Factory::ReleaseFTFace(mFace);
    mFace = nullptr;
  }
}

already_AddRefed<NativeFontResourceFontconfig>
NativeFontResourceFontconfig::Create(uint8_t *aFontData, uint32_t aDataLength, FT_Library aFTLibrary)
{
  if (!aFontData || !aDataLength) {
    return nullptr;
  }
  UniquePtr<uint8_t[]> fontData(new (fallible) uint8_t[aDataLength]);
  if (!fontData) {
    return nullptr;
  }
  memcpy(fontData.get(), aFontData, aDataLength);

  FT_Face face = Factory::NewFTFaceFromData(aFTLibrary, fontData.get(), aDataLength, 0);
  if (!face) {
    return nullptr;
  }
  if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != FT_Err_Ok) {
    Factory::ReleaseFTFace(face);
    return nullptr;
  }

  RefPtr<NativeFontResourceFontconfig> resource =
    new NativeFontResourceFontconfig(Move(fontData), face);
  return resource.forget();
}

already_AddRefed<UnscaledFont>
NativeFontResourceFontconfig::CreateUnscaledFont(uint32_t aIndex,
                                                 const uint8_t* aInstanceData, uint32_t aInstanceDataLength)
{
  RefPtr<UnscaledFont> unscaledFont = new UnscaledFontFontconfig(mFace, this);
  return unscaledFont.forget();
}

} // gfx
} // mozilla
