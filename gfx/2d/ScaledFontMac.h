/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTMAC_H_
#define MOZILLA_GFX_SCALEDFONTMAC_H_

#ifdef MOZ_WIDGET_COCOA
#  include <ApplicationServices/ApplicationServices.h>
#else
#  include <CoreGraphics/CoreGraphics.h>
#  include <CoreText/CoreText.h>
#endif

#include "2D.h"

#include "ScaledFontBase.h"

namespace mozilla {
namespace gfx {

class UnscaledFontMac;

class ScaledFontMac : public ScaledFontBase {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontMac, override)
  ScaledFontMac(
      CGFontRef aFont, const RefPtr<UnscaledFont>& aUnscaledFont, Float aSize,
      bool aOwnsFont = false,
      const DeviceColor& aFontSmoothingBackgroundColor = DeviceColor(),
      bool aUseFontSmoothing = true, bool aApplySyntheticBold = false,
      bool aHasColorGlyphs = false);
  ScaledFontMac(
      CTFontRef aFont, const RefPtr<UnscaledFont>& aUnscaledFont,
      const DeviceColor& aFontSmoothingBackgroundColor = DeviceColor(),
      bool aUseFontSmoothing = true, bool aApplySyntheticBold = false,
      bool aHasColorGlyphs = false);
  ~ScaledFontMac();

  FontType GetType() const override { return FontType::MAC; }
  SkTypeface* CreateSkTypeface() override;
  void SetupSkFontDrawOptions(SkFont& aFont) override;
  already_AddRefed<Path> GetPathForGlyphs(const GlyphBuffer& aBuffer,
                                          const DrawTarget* aTarget) override;

  bool GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton) override;

  bool GetWRFontInstanceOptions(
      Maybe<wr::FontInstanceOptions>* aOutOptions,
      Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
      std::vector<FontVariation>* aOutVariations) override;

  bool CanSerialize() override { return true; }

  DeviceColor FontSmoothingBackgroundColor() {
    return mFontSmoothingBackgroundColor;
  }

  cairo_font_face_t* CreateCairoFontFace(
      cairo_font_options_t* aFontOptions) override;

 private:
  friend class DrawTargetSkia;
  friend class UnscaledFontMac;

  CGFontRef mFont;
  CTFontRef
      mCTFont;  // only created if CTFontDrawGlyphs is available, otherwise null

  DeviceColor mFontSmoothingBackgroundColor;
  bool mUseFontSmoothing;
  bool mApplySyntheticBold;
  bool mHasColorGlyphs;

  struct InstanceData {
    explicit InstanceData(ScaledFontMac* aScaledFont)
        : mFontSmoothingBackgroundColor(
              aScaledFont->mFontSmoothingBackgroundColor),
          mUseFontSmoothing(aScaledFont->mUseFontSmoothing),
          mApplySyntheticBold(aScaledFont->mApplySyntheticBold),
          mHasColorGlyphs(aScaledFont->mHasColorGlyphs) {}

    InstanceData(const wr::FontInstanceOptions* aOptions,
                 const wr::FontInstancePlatformOptions* aPlatformOptions);

    DeviceColor mFontSmoothingBackgroundColor;
    bool mUseFontSmoothing;
    bool mApplySyntheticBold;
    bool mHasColorGlyphs;
  };
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SCALEDFONTMAC_H_ */
