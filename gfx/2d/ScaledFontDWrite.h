/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTDWRITE_H_
#define MOZILLA_GFX_SCALEDFONTDWRITE_H_

#include <dwrite.h>
#include "DWriteSettings.h"
#include "ScaledFontBase.h"

struct ID2D1GeometrySink;
struct gfxFontStyle;

namespace mozilla {
namespace gfx {

class NativeFontResourceDWrite;
class UnscaledFontDWrite;

class ScaledFontDWrite final : public ScaledFontBase {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontDWrite, override)
  ScaledFontDWrite(IDWriteFontFace* aFontFace,
                   const RefPtr<UnscaledFont>& aUnscaledFont, Float aSize,
                   bool aUseEmbeddedBitmap, bool aUseMultistrikeBold,
                   bool aGDIForced, const gfxFontStyle* aStyle);

  FontType GetType() const override { return FontType::DWRITE; }

  already_AddRefed<Path> GetPathForGlyphs(const GlyphBuffer& aBuffer,
                                          const DrawTarget* aTarget) override;
  void CopyGlyphsToBuilder(const GlyphBuffer& aBuffer, PathBuilder* aBuilder,
                           const Matrix* aTransformHint) override;

  void CopyGlyphsToSink(const GlyphBuffer& aBuffer,
                        ID2D1SimplifiedGeometrySink* aSink);

  bool CanSerialize() override { return true; }

  bool MayUseBitmaps() override;

  bool GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton) override;

  bool GetWRFontInstanceOptions(
      Maybe<wr::FontInstanceOptions>* aOutOptions,
      Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
      std::vector<FontVariation>* aOutVariations) override;

  DWriteSettings& DWriteSettings() const;

  AntialiasMode GetDefaultAAMode() override;

  bool UseEmbeddedBitmaps() const { return mUseEmbeddedBitmap; }
  bool UseMultistrikeBold() const { return mUseMultistrikeBold; }
  bool ForceGDIMode() const { return mGDIForced; }

  bool UseSubpixelPosition() const override { return !ForceGDIMode(); }

  bool HasBoldSimulation() const {
    return (mFontFace->GetSimulations() & DWRITE_FONT_SIMULATIONS_BOLD) != 0;
  }

  SkTypeface* CreateSkTypeface() override;
  void SetupSkFontDrawOptions(SkFont& aFont) override;
  SkFontStyle mStyle;

  RefPtr<IDWriteFontFace> mFontFace;
  bool mUseEmbeddedBitmap;
  bool mUseMultistrikeBold = false;
  bool mGDIForced = false;

  cairo_font_face_t* CreateCairoFontFace(
      cairo_font_options_t* aFontOptions) override;
  void PrepareCairoScaledFont(cairo_scaled_font_t* aFont) override;

 private:
  friend class NativeFontResourceDWrite;
  friend class UnscaledFontDWrite;

  struct InstanceData {
    explicit InstanceData(ScaledFontDWrite* aScaledFont)
        : mUseEmbeddedBitmap(aScaledFont->mUseEmbeddedBitmap),
          mUseBoldSimulation(aScaledFont->HasBoldSimulation()),
          mUseMultistrikeBold(aScaledFont->UseMultistrikeBold()),
          mGDIForced(aScaledFont->mGDIForced) {}

    InstanceData(const wr::FontInstanceOptions* aOptions,
                 const wr::FontInstancePlatformOptions* aPlatformOptions);

    bool mUseEmbeddedBitmap = false;
    bool mUseBoldSimulation = false;
    bool mUseMultistrikeBold = false;
    bool mGDIForced = false;
  };
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SCALEDFONTDWRITE_H_ */
