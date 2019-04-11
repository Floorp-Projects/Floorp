/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TextRenderer_H
#define GFX_TextRenderer_H

#include "mozilla/EnumeratedArray.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/UniquePtr.h"
#include "nsISupportsImpl.h"
#include <string>

namespace mozilla {
namespace layers {

class Compositor;
class TextureSource;
class TextureSourceProvider;
struct FontBitmapInfo;

class TextRenderer final {
  ~TextRenderer();

 public:
  NS_INLINE_DECL_REFCOUNTING(TextRenderer)

  enum class FontType { Default, FixedWidth, NumTypes };

  TextRenderer() = default;

  RefPtr<TextureSource> RenderText(TextureSourceProvider* aProvider,
                                   const std::string& aText, uint32_t aTextSize,
                                   uint32_t aTargetPixelWidth,
                                   FontType aFontType);

  void RenderText(Compositor* aCompositor, const std::string& aText,
                  const gfx::IntPoint& aOrigin,
                  const gfx::Matrix4x4& aTransform, uint32_t aTextSize,
                  uint32_t aTargetPixelWidth,
                  FontType aFontType = FontType::Default);

  struct FontCache {
    ~FontCache();
    RefPtr<gfx::DataSourceSurface> mGlyphBitmaps;
    gfx::DataSourceSurface::MappedSurface mMap;
    const FontBitmapInfo* mInfo;
  };

 protected:
  // Note that this may still fail to set mGlyphBitmaps to a valid value
  // if the underlying CreateDataSourceSurface fails for some reason.
  bool EnsureInitialized(FontType aType);

  static const FontBitmapInfo* GetFontInfo(FontType aType);

 private:
  EnumeratedArray<FontType, FontType::NumTypes, UniquePtr<FontCache>> mFonts;
};

struct FontBitmapInfo {
  Maybe<unsigned int> mGlyphWidth;
  Maybe<const unsigned short*> mGlyphWidths;
  unsigned int mTextureWidth;
  unsigned int mTextureHeight;
  unsigned int mCellWidth;
  unsigned int mCellHeight;
  unsigned int mFirstChar;
  const unsigned char* mPNG;
  size_t mPNGLength;

  unsigned int GetGlyphWidth(char aGlyph) const {
    if (mGlyphWidth) {
      return mGlyphWidth.value();
    }
    MOZ_ASSERT(unsigned(aGlyph) >= mFirstChar);
    return mGlyphWidths.value()[unsigned(aGlyph) - mFirstChar];
  }
};

}  // namespace layers
}  // namespace mozilla

#endif
