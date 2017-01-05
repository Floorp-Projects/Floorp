/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTFONTCONFIG_H_
#define MOZILLA_GFX_SCALEDFONTFONTCONFIG_H_

#include "ScaledFontBase.h"

#include <cairo-ft.h>

namespace mozilla {
namespace gfx {

class NativeFontResourceFontconfig;

class ScaledFontFontconfig : public ScaledFontBase
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontFontconfig, override)
  ScaledFontFontconfig(cairo_scaled_font_t* aScaledFont, FcPattern* aPattern, Float aSize);
  ~ScaledFontFontconfig();

  FontType GetType() const override { return FontType::FONTCONFIG; }

#ifdef USE_SKIA
  SkTypeface* GetSkTypeface() override;
#endif

  bool GetFontFileData(FontFileDataOutput aDataCallback, void* aBaton) override;

  bool GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton) override;

  bool GetFontDescriptor(FontDescriptorOutput aCb, void* aBaton) override;

  static already_AddRefed<ScaledFont>
    CreateFromFontDescriptor(const uint8_t* aData, uint32_t aDataLength, Float aSize);

private:
  friend class NativeFontResourceFontconfig;

  struct InstanceData
  {
    enum {
      ANTIALIAS       = 1 << 0,
      AUTOHINT        = 1 << 1,
      EMBEDDED_BITMAP = 1 << 2,
      EMBOLDEN        = 1 << 3,
      VERTICAL_LAYOUT = 1 << 4,
      HINT_METRICS    = 1 << 5
    };

    InstanceData(cairo_scaled_font_t* aScaledFont, FcPattern* aPattern);

    void SetupPattern(FcPattern* aPattern) const;
    void SetupFontOptions(cairo_font_options_t* aFontOptions) const;
    void SetupFontMatrix(cairo_matrix_t* aFontMatrix) const;

    uint8_t mFlags;
    uint8_t mHintStyle;
    uint8_t mSubpixelOrder;
    uint8_t mLcdFilter;
    Float mScale;
    Float mSkew;
  };

  struct FontDescriptor
  {
    uint32_t mPathLength;
    uint32_t mIndex;
    InstanceData mInstanceData;
  };

  static already_AddRefed<ScaledFont>
    CreateFromInstanceData(const InstanceData& aInstanceData,
                           FT_Face aFace, const char* aPathname, uint32_t aIndex,
                           Float aSize);

  FcPattern* mPattern;
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTFONTCONFIG_H_ */
