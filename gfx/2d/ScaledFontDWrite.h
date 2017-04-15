/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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
  {}

  ScaledFontDWrite(IDWriteFontFace *aFontFace,
                   const RefPtr<UnscaledFont>& aUnscaledFont,
                   Float aSize,
                   bool aUseEmbeddedBitmap,
                   bool aForceGDIMode,
                   const gfxFontStyle* aStyle);

  FontType GetType() const override { return FontType::DWRITE; }

  already_AddRefed<Path> GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget) override;
  void CopyGlyphsToBuilder(const GlyphBuffer &aBuffer, PathBuilder *aBuilder, const Matrix *aTransformHint) override;

  void CopyGlyphsToSink(const GlyphBuffer &aBuffer, ID2D1GeometrySink *aSink);

  void GetGlyphDesignMetrics(const uint16_t* aGlyphIndices, uint32_t aNumGlyphs, GlyphMetrics* aGlyphMetrics) override;

  bool CanSerialize() override { return true; }

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

protected:
#ifdef USE_CAIRO_SCALED_FONT
  cairo_font_face_t* GetCairoFontFace() override;
#endif
};

class GlyphRenderingOptionsDWrite : public GlyphRenderingOptions
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GlyphRenderingOptionsDWrite, override)
  explicit GlyphRenderingOptionsDWrite(IDWriteRenderingParams *aParams)
    : mParams(aParams)
  {
  }

  FontType GetType() const override { return FontType::DWRITE; }

private:
  friend class DrawTargetD2D;
  friend class DrawTargetD2D1;

  RefPtr<IDWriteRenderingParams> mParams;
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTDWRITE_H_ */
