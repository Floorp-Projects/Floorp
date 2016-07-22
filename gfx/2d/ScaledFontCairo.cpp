/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontCairo.h"
#include "Logging.h"

#if defined(USE_SKIA) && defined(MOZ_ENABLE_FREETYPE)
#include "skia/include/ports/SkTypeface_cairo.h"
#endif

namespace mozilla {
namespace gfx {

// On Linux and Android our "platform" font is a cairo_scaled_font_t and we use
// an SkFontHost implementation that allows Skia to render using this.
// This is mainly because FT_Face is not good for sharing between libraries, which
// is a requirement when we consider runtime switchable backends and so on
ScaledFontCairo::ScaledFontCairo(cairo_scaled_font_t* aScaledFont, Float aSize)
  : ScaledFontBase(aSize)
{ 
  SetCairoScaledFont(aScaledFont);
}

#if defined(USE_SKIA) && defined(MOZ_ENABLE_FREETYPE)
SkTypeface* ScaledFontCairo::GetSkTypeface()
{
  if (!mTypeface) {
    mTypeface = SkCreateTypefaceFromCairoFTFont(mScaledFont);
  }

  return mTypeface;
}
#endif

} // namespace gfx
} // namespace mozilla
