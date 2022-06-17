/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTFONTCONFIG_H_
#define MOZILLA_GFX_SCALEDFONTFONTCONFIG_H_

#include "ScaledFontBase.h"

#include <cairo-ft.h>

namespace mozilla {
namespace gfx {

class NativeFontResourceFontconfig;
class UnscaledFontFontconfig;

class ScaledFontFontconfig : public ScaledFontBase {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontFontconfig, override)
  ScaledFontFontconfig(RefPtr<SharedFTFace>&& aFace, FcPattern* aPattern,
                       const RefPtr<UnscaledFont>& aUnscaledFont, Float aSize);

  FontType GetType() const override { return FontType::FONTCONFIG; }

  SkTypeface* CreateSkTypeface() override;
  void SetupSkFontDrawOptions(SkFont& aFont) override;

  AntialiasMode GetDefaultAAMode() override;

  bool UseSubpixelPosition() const override;

  bool CanSerialize() override { return true; }

  bool MayUseBitmaps() override;

  bool GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton) override;

  bool GetWRFontInstanceOptions(
      Maybe<wr::FontInstanceOptions>* aOutOptions,
      Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
      std::vector<FontVariation>* aOutVariations) override;

  bool HasVariationSettings() override;

 protected:
  cairo_font_face_t* CreateCairoFontFace(
      cairo_font_options_t* aFontOptions) override;

 private:
  friend class NativeFontResourceFontconfig;
  friend class UnscaledFontFontconfig;

  struct InstanceData {
    enum {
      AUTOHINT = 1 << 0,
      EMBEDDED_BITMAP = 1 << 1,
      EMBOLDEN = 1 << 2,
      HINT_METRICS = 1 << 3,
      LCD_VERTICAL = 1 << 4,
      SUBPIXEL_BGR = 1 << 5,
    };

    explicit InstanceData(FcPattern* aPattern);
    InstanceData(const wr::FontInstanceOptions* aOptions,
                 const wr::FontInstancePlatformOptions* aPlatformOptions);

    void SetupFontOptions(cairo_font_options_t* aFontOptions,
                          int* aOutLoadFlags,
                          unsigned int* aOutSynthFlags) const;

    uint8_t mFlags;
    AntialiasMode mAntialias;
    FontHinting mHinting;
    uint8_t mLcdFilter;
  };

  ScaledFontFontconfig(RefPtr<SharedFTFace>&& aFace,
                       const InstanceData& aInstanceData,
                       const RefPtr<UnscaledFont>& aUnscaledFont, Float aSize);

  RefPtr<SharedFTFace> mFace;
  InstanceData mInstanceData;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SCALEDFONTFONTCONFIG_H_ */
