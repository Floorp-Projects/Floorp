/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontMac.h"
#ifdef USE_SKIA
#include "PathSkia.h"
#include "skia/SkPaint.h"
#include "skia/SkPath.h"
#include "skia/SkTypeface_mac.h"
#endif
#include "DrawTargetCG.h"
#include <vector>
#include <dlfcn.h>

// prototype for private API
extern "C" {
CGPathRef CGFontGetGlyphPath(CGFontRef fontRef, CGAffineTransform *textTransform, int unknown, CGGlyph glyph);
};


namespace mozilla {
namespace gfx {

ScaledFontMac::CTFontDrawGlyphsFuncT* ScaledFontMac::CTFontDrawGlyphsPtr = nullptr;
bool ScaledFontMac::sSymbolLookupDone = false;

ScaledFontMac::ScaledFontMac(CGFontRef aFont, Float aSize)
  : ScaledFontBase(aSize)
{
  if (!sSymbolLookupDone) {
    CTFontDrawGlyphsPtr =
      (CTFontDrawGlyphsFuncT*)dlsym(RTLD_DEFAULT, "CTFontDrawGlyphs");
    sSymbolLookupDone = true;
  }

  // XXX: should we be taking a reference
  mFont = CGFontRetain(aFont);
  if (CTFontDrawGlyphsPtr != nullptr) {
    // only create mCTFont if we're going to be using the CTFontDrawGlyphs API
    mCTFont = CTFontCreateWithGraphicsFont(aFont, aSize, nullptr, nullptr);
  } else {
    mCTFont = nullptr;
  }
}

ScaledFontMac::~ScaledFontMac()
{
  if (mCTFont) {
    CFRelease(mCTFont);
  }
  CGFontRelease(mFont);
}

#ifdef USE_SKIA
SkTypeface* ScaledFontMac::GetSkTypeface()
{
  if (!mTypeface) {
    if (mCTFont) {
      mTypeface = SkCreateTypefaceFromCTFont(mCTFont);
    } else {
      CTFontRef fontFace = CTFontCreateWithGraphicsFont(mFont, mSize, nullptr, nullptr);
      mTypeface = SkCreateTypefaceFromCTFont(fontFace);
      CFRelease(fontFace);
    }
  }
  return mTypeface;
}
#endif

// private API here are the public options on OS X
// CTFontCreatePathForGlyph
// ATSUGlyphGetCubicPaths
// we've used this in cairo sucessfully for some time.
// Note: cairo dlsyms it. We could do that but maybe it's
// safe just to use?

TemporaryRef<Path>
ScaledFontMac::GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget)
{
  if (aTarget->GetType() == BACKEND_COREGRAPHICS || aTarget->GetType() == BACKEND_COREGRAPHICS_ACCELERATED) {
      CGMutablePathRef path = CGPathCreateMutable();

      for (unsigned int i = 0; i < aBuffer.mNumGlyphs; i++) {
          // XXX: we could probably fold both of these transforms together to avoid extra work
          CGAffineTransform flip = CGAffineTransformMakeScale(1, -1);
          CGPathRef glyphPath = ::CGFontGetGlyphPath(mFont, &flip, 0, aBuffer.mGlyphs[i].mIndex);

          CGAffineTransform matrix = CGAffineTransformMake(mSize, 0, 0, mSize,
                                                           aBuffer.mGlyphs[i].mPosition.x,
                                                           aBuffer.mGlyphs[i].mPosition.y);
          CGPathAddPath(path, &matrix, glyphPath);
          CGPathRelease(glyphPath);
      }
      TemporaryRef<Path> ret = new PathCG(path, FILL_WINDING);
      CGPathRelease(path);
      return ret;
  } else {
      return ScaledFontBase::GetPathForGlyphs(aBuffer, aTarget);
  }
}

}
}
