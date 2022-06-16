/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTCAIRO_H_
#define MOZILLA_GFX_SCALEDFONTCAIRO_H_

#include "ScaledFontBase.h"

#include <cairo-ft.h>

namespace mozilla {
namespace gfx {

class UnscaledFontFreeType;

class ScaledFontFreeType : public ScaledFontBase {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontFreeType, override)

  ScaledFontFreeType(RefPtr<SharedFTFace>&& aFace,
                     const RefPtr<UnscaledFont>& aUnscaledFont, Float aSize,
                     bool aApplySyntheticBold = false);

  FontType GetType() const override { return FontType::FREETYPE; }

  SkTypeface* CreateSkTypeface() override;
  void SetupSkFontDrawOptions(SkFont& aFont) override;

  AntialiasMode GetDefaultAAMode() override { return AntialiasMode::GRAY; }

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
  friend UnscaledFontFreeType;

  RefPtr<SharedFTFace> mFace;

  bool mApplySyntheticBold;

  struct InstanceData {
    explicit InstanceData(ScaledFontFreeType* aScaledFont)
        : mApplySyntheticBold(aScaledFont->mApplySyntheticBold) {}

    InstanceData(const wr::FontInstanceOptions* aOptions,
                 const wr::FontInstancePlatformOptions* aPlatformOptions);

    bool mApplySyntheticBold;
  };
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SCALEDFONTCAIRO_H_ */
