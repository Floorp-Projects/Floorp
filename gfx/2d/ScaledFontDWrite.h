/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTDWRITE_H_
#define MOZILLA_GFX_SCALEDFONTDWRITE_H_

#include <dwrite.h>
#include "ScaledFontBase.h"

struct ID2D1GeometrySink;

namespace mozilla {
namespace gfx {

class ScaledFontDWrite MOZ_FINAL : public ScaledFontBase
{
public:
  ScaledFontDWrite(IDWriteFontFace *aFont, Float aSize)
    : mFontFace(aFont)
    , ScaledFontBase(aSize)
  {}
  ScaledFontDWrite(uint8_t *aData, uint32_t aSize, uint32_t aIndex, Float aGlyphSize);

  virtual FontType GetType() const { return FONT_DWRITE; }

  virtual TemporaryRef<Path> GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget);
  virtual void CopyGlyphsToBuilder(const GlyphBuffer &aBuffer, PathBuilder *aBuilder, const Matrix *aTransformHint);

  void CopyGlyphsToSink(const GlyphBuffer &aBuffer, ID2D1GeometrySink *aSink);

  virtual bool GetFontFileData(FontFileDataOutput aDataCallback, void *aBaton);

  virtual AntialiasMode GetDefaultAAMode();

#ifdef USE_SKIA
  virtual SkTypeface* GetSkTypeface()
  {
    MOZ_ASSERT(false, "Skia and DirectWrite do not mix");
    return nullptr;
  }
#endif

  RefPtr<IDWriteFontFace> mFontFace;
};

class GlyphRenderingOptionsDWrite : public GlyphRenderingOptions
{
public:
  GlyphRenderingOptionsDWrite(IDWriteRenderingParams *aParams)
    : mParams(aParams)
  {
  }

  virtual FontType GetType() const { return FONT_DWRITE; }

private:
  friend class DrawTargetD2D;
  friend class DrawTargetD2D1;

  RefPtr<IDWriteRenderingParams> mParams;
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTDWRITE_H_ */
