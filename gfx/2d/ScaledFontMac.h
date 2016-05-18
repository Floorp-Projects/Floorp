/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTMAC_H_
#define MOZILLA_GFX_SCALEDFONTMAC_H_

#ifdef MOZ_WIDGET_COCOA
#include <ApplicationServices/ApplicationServices.h>
#else
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#endif

#include "2D.h"

#include "ScaledFontBase.h"

namespace mozilla {
namespace gfx {

class ScaledFontMac : public ScaledFontBase
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontMac)
  ScaledFontMac(CGFontRef aFont, Float aSize);
  virtual ~ScaledFontMac();

  virtual FontType GetType() const { return FontType::MAC; }
#ifdef USE_SKIA
  virtual SkTypeface* GetSkTypeface();
#endif
  virtual already_AddRefed<Path> GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget);
  virtual void CopyGlyphsToBuilder(const GlyphBuffer &aBuffer, PathBuilder *aBuilder, BackendType aBackendType, const Matrix *aTransformHint);
  virtual bool GetFontFileData(FontFileDataOutput aDataCallback, void *aBaton);

private:
  friend class DrawTargetCG;
  friend class DrawTargetSkia;
  CGFontRef mFont;
  CTFontRef mCTFont; // only created if CTFontDrawGlyphs is available, otherwise null

  typedef void (CTFontDrawGlyphsFuncT)(CTFontRef,
                                       const CGGlyph[], const CGPoint[],
                                       size_t, CGContextRef);

  static bool sSymbolLookupDone;

public:
  // function pointer for CTFontDrawGlyphs, if available;
  // initialized the first time a ScaledFontMac is created,
  // so it will be valid by the time DrawTargetCG wants to use it
  static CTFontDrawGlyphsFuncT* CTFontDrawGlyphsPtr;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_SCALEDFONTMAC_H_ */
