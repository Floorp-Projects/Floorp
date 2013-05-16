/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontCairo.h"
#include "Logging.h"

#include "gfxFont.h"

#ifdef MOZ_ENABLE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include "cairo-ft.h"
#endif

#if defined(USE_SKIA) && defined(MOZ_ENABLE_FREETYPE)
#include "skia/SkTypeface.h"
#include "skia/SkTypeface_cairo.h"
#endif

#include <string>

typedef struct FT_FaceRec_* FT_Face;

using namespace std;

namespace mozilla {
namespace gfx {

// On Linux and Android our "platform" font is a cairo_scaled_font_t and we use
// an SkFontHost implementation that allows Skia to render using this.
// This is mainly because FT_Face is not good for sharing between libraries, which
// is a requirement when we consider runtime switchable backends and so on
ScaledFontCairo::ScaledFontCairo(cairo_scaled_font_t* aScaledFont, Float aSize)
  : ScaledFontBase(aSize)
{
  mScaledFont = aScaledFont;
#if defined(USE_SKIA) && defined(MOZ_ENABLE_FREETYPE)
  cairo_font_face_t* fontFace = cairo_scaled_font_get_font_face(aScaledFont);
  FT_Face face = cairo_ft_scaled_font_lock_face(aScaledFont);

  int style = SkTypeface::kNormal;

  if (face->style_flags & FT_STYLE_FLAG_ITALIC)
    style |= SkTypeface::kItalic;

  if (face->style_flags & FT_STYLE_FLAG_BOLD)
    style |= SkTypeface::kBold;

  bool isFixedWidth = face->face_flags & FT_FACE_FLAG_FIXED_WIDTH;
  cairo_ft_scaled_font_unlock_face(aScaledFont);

  mTypeface = SkCreateTypefaceFromCairoFont(fontFace, (SkTypeface::Style)style, isFixedWidth);
#endif
}

}
}
