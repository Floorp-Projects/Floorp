/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTMAC_H_
#define MOZILLA_GFX_SCALEDFONTMAC_H_

#import <ApplicationServices/ApplicationServices.h>
#include "2D.h"

#include "ScaledFontBase.h"

namespace mozilla {
namespace gfx {

class ScaledFontMac : public ScaledFontBase
{
public:
  ScaledFontMac(CGFontRef aFont, Float aSize);
  virtual ~ScaledFontMac();

  virtual FontType GetType() const { return FONT_MAC; }
#ifdef USE_SKIA
  virtual SkTypeface* GetSkTypeface();
#endif
  virtual TemporaryRef<Path> GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget);
private:
  friend class DrawTargetCG;
  CGFontRef mFont;
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTMAC_H_ */
