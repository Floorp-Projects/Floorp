/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontFontconfig.h"
#include "Logging.h"

#ifdef USE_SKIA
#include "skia/include/ports/SkTypeface_cairo.h"
#endif

namespace mozilla {
namespace gfx {

// On Linux and Android our "platform" font is a cairo_scaled_font_t and we use
// an SkFontHost implementation that allows Skia to render using this.
// This is mainly because FT_Face is not good for sharing between libraries, which
// is a requirement when we consider runtime switchable backends and so on
ScaledFontFontconfig::ScaledFontFontconfig(cairo_scaled_font_t* aScaledFont,
                                           FcPattern* aPattern,
                                           Float aSize)
  : ScaledFontBase(aSize),
    mPattern(aPattern)
{
  SetCairoScaledFont(aScaledFont);
  FcPatternReference(aPattern);
}

ScaledFontFontconfig::~ScaledFontFontconfig()
{
  FcPatternDestroy(mPattern);
}

#ifdef USE_SKIA
SkTypeface* ScaledFontFontconfig::GetSkTypeface()
{
  if (!mTypeface) {
    mTypeface = SkCreateTypefaceFromCairoFTFontWithFontconfig(mScaledFont, mPattern);
  }

  return mTypeface;
}
#endif

} // namespace gfx
} // namespace mozilla
