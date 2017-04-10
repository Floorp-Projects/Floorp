/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_UNSCALEDFONTFREETYPE_H_
#define MOZILLA_GFX_UNSCALEDFONTFREETYPE_H_

#include <cairo-ft.h>

#include "2D.h"

namespace mozilla {
namespace gfx {

class UnscaledFontFreeType : public UnscaledFont
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(UnscaledFontFreeType, override)
  explicit UnscaledFontFreeType(FT_Face aFace)
    : mFace(aFace)
    , mIndex(0)
  {}
  explicit UnscaledFontFreeType(const char* aFile,
                                uint32_t aIndex = 0)
    : mFace(nullptr)
    , mFile(aFile)
    , mIndex(aIndex)
  {}

  FontType GetType() const override { return FontType::FREETYPE; }

  FT_Face GetFace() const { return mFace; }
  const char* GetFile() const { return mFile.c_str(); }
  uint32_t GetIndex() const { return mIndex; }

private:
  FT_Face mFace;
  std::string mFile;
  uint32_t mIndex;
};

#ifdef MOZ_WIDGET_GTK
class UnscaledFontFontconfig : public UnscaledFontFreeType
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(UnscaledFontFontconfig, override)
  explicit UnscaledFontFontconfig(FT_Face aFace)
    : UnscaledFontFreeType(aFace)
  {}
  explicit UnscaledFontFontconfig(const char* aFile,
                                  uint32_t aIndex = 0)
    : UnscaledFontFreeType(aFile, aIndex)
  {}

  FontType GetType() const override { return FontType::FONTCONFIG; }
};
#endif

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_UNSCALEDFONTFREETYPE_H_ */

