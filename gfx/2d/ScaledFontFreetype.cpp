/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontFreetype.h"

#include "gfxFont.h"

#ifdef USE_SKIA
#include "skia/SkTypeface.h"
#endif

using namespace std;

namespace mozilla {
namespace gfx {

#ifdef USE_SKIA
static SkTypeface::Style
gfxFontStyleToSkia(const gfxFontStyle* aStyle)
{
  if (aStyle->style == NS_FONT_STYLE_ITALIC) {
    if (aStyle->weight == NS_FONT_WEIGHT_BOLD) {
      return SkTypeface::kBoldItalic;
    }
    return SkTypeface::kItalic;
  }
  if (aStyle->weight == NS_FONT_WEIGHT_BOLD) {
    return SkTypeface::kBold;
  }
  return SkTypeface::kNormal;
}
#endif

// Ideally we want to use FT_Face here but as there is currently no way to get
// an SkTypeface from an FT_Face we do this.
ScaledFontFreetype::ScaledFontFreetype(gfxFont* aFont, Float aSize)
  : ScaledFontBase(aSize)
{
#ifdef USE_SKIA
  NS_LossyConvertUTF16toASCII name(aFont->GetName());
  mTypeface = SkTypeface::CreateFromName(name.get(), gfxFontStyleToSkia(aFont->GetStyle()));
#endif
}

}
}
