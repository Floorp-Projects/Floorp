/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTFONTCONFIG_H_
#define MOZILLA_GFX_SCALEDFONTFONTCONFIG_H_

#include "ScaledFontBase.h"

#include <fontconfig/fontconfig.h>
#include <cairo.h>

namespace mozilla {
namespace gfx {

class ScaledFontFontconfig : public ScaledFontBase
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontFontconfig)
  ScaledFontFontconfig(cairo_scaled_font_t* aScaledFont, FcPattern* aPattern, Float aSize);
  ~ScaledFontFontconfig();

  virtual FontType GetType() const { return FontType::FONTCONFIG; }

#ifdef USE_SKIA
  virtual SkTypeface* GetSkTypeface();
#endif

private:
  FcPattern* mPattern;
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTFONTCONFIG_H_ */
