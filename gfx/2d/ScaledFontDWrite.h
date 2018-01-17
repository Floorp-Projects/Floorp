/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTDWRITE_H_
#define MOZILLA_GFX_SCALEDFONTDWRITE_H_

#include <dwrite.h>
#include "ScaledFontBase.h"

struct ID2D1GeometrySink;
struct gfxFontStyle;

namespace mozilla {
namespace gfx {

class NativeFontResourceDWrite;
class UnscaledFontDWrite;

class ScaledFontDWrite final : public ScaledFontBase
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontDWrite, override)
  ScaledFontDWrite(IDWriteFontFace *aFont,
                   const RefPtr<UnscaledFont>& aUnscaledFont,
                   Float aSize)
    : ScaledFontBase(aUnscaledFont, aSize)
    , mFontFace(aFont)
    , mUseEmbeddedBitmap(false)
    , mForceGDIMode(false)
    , mGamma(2.2f)
    , mContrast(1.0f)
  {}

  ScaledFontDWrite(IDWriteFontFace *aFontFace,
                   const RefPtr<UnscaledFont>& aUnscaledFont,
                   Float aSize,
                   bool aUseEmbeddedBitmap,
                   bool aForceGDIMode,
                   IDWriteRenderingParams *aParams,
                   Float aGamma,
                   Float aContrast,
                   const gfxFontStyle* aStyle = nullptr);

  FontType GetType() const override { return FontType::DWRITE; }

  already_AddRefed<Path> GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget) override;
  void CopyGlyphsToBuilder(const GlyphBuffer &aBuffer, PathBuilder *aBuilder, const Matrix *aTransformHint) override;

  void CopyGlyphsToSink(const GlyphBuffer &aBuffer, ID2D1GeometrySink *aSink);

  void GetGlyphDesignMetrics(const uint16_t* aGlyphIndices, uint32_t aNumGlyphs, GlyphMetrics* aGlyphMetrics) override;

  bool CanSerialize() override { return true; }

  bool GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton) override;

  bool GetWRFontInstanceOptions(Maybe<wr::FontInstanceOptions>* aOutOptions,
                                Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
                                std::vector<FontVariation>* aOutVariations) override;

  AntialiasMode GetDefaultAAMode() override;

  bool UseEmbeddedBitmaps() { return mUseEmbeddedBitmap; }
  bool ForceGDIMode() { return mForceGDIMode; }

#ifdef USE_SKIA
  SkTypeface* GetSkTypeface() override;
  SkFontStyle mStyle;
#endif

  RefPtr<IDWriteFontFace> mFontFace;
  bool mUseEmbeddedBitmap;
  bool mForceGDIMode;
  // DrawTargetD2D1 requires the IDWriteRenderingParams,
  // but we also separately need to store the gamma and contrast
  // since Skia needs to be able to access these without having
  // to use the full set of DWrite parameters (which would be
  // required to recreate an IDWriteRenderingParams) in a
  // DrawTargetRecording playback.
  RefPtr<IDWriteRenderingParams> mParams;
  Float mGamma;
  Float mContrast;

protected:
#ifdef USE_CAIRO_SCALED_FONT
  cairo_font_face_t* GetCairoFontFace() override;
#endif

private:
  friend class NativeFontResourceDWrite;
  friend class UnscaledFontDWrite;

  struct InstanceData
  {
    explicit InstanceData(ScaledFontDWrite* aScaledFont)
      : mUseEmbeddedBitmap(aScaledFont->mUseEmbeddedBitmap)
      , mForceGDIMode(aScaledFont->mForceGDIMode)
      , mGamma(aScaledFont->mGamma)
      , mContrast(aScaledFont->mContrast)
    {}

    bool mUseEmbeddedBitmap;
    bool mForceGDIMode;
    Float mGamma;
    Float mContrast;
  };
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTDWRITE_H_ */
