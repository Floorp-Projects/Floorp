/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Woodrow <mwoodrow@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "ScaledFontMac.h"
#ifdef USE_SKIA
#include "PathSkia.h"
#include "skia/SkPaint.h"
#include "skia/SkPath.h"
#include "skia/SkTypeface_mac.h"
#endif
#include "DrawTargetCG.h"
#include <vector>

// prototype for private API
extern "C" {
CGPathRef CGFontGetGlyphPath(CGFontRef fontRef, CGAffineTransform *textTransform, int unknown, CGGlyph glyph);
};


namespace mozilla {
namespace gfx {

ScaledFontMac::ScaledFontMac(CGFontRef aFont, Float aSize)
  : ScaledFontBase(aSize)
{
  // XXX: should we be taking a reference
  mFont = CGFontRetain(aFont);
}

ScaledFontMac::~ScaledFontMac()
{
  CGFontRelease(mFont);
}

#ifdef USE_SKIA
SkTypeface* ScaledFontMac::GetSkTypeface()
{
  if (!mTypeface) {
    CTFontRef fontFace = CTFontCreateWithGraphicsFont(mFont, mSize, NULL, NULL);
    mTypeface = SkCreateTypefaceFromCTFont(fontFace);
    CFRelease(fontFace);
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
  if (aTarget->GetType() == BACKEND_COREGRAPHICS) {
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
      return new PathCG(path, FILL_WINDING);
  } else {
      return ScaledFontBase::GetPathForGlyphs(aBuffer, aTarget);
  }
}

}
}
