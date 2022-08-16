/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COLR_FONTS_H
#define COLR_FONTS_H

#include "mozilla/gfx/2D.h"

struct hb_blob_t;

namespace mozilla {

namespace layout {
class TextDrawTarget;
}

namespace gfx {
struct COLRBaseGlyphRecord;

class COLRFonts {
 public:
  // for color layer from glyph using COLR and CPAL tables
  static bool ValidateColorGlyphs(hb_blob_t* aCOLR, hb_blob_t* aCPAL);

  static const COLRBaseGlyphRecord* GetGlyphLayers(hb_blob_t* aCOLR,
                                                   uint32_t aGlyphId);

  static bool PaintGlyphLayers(
      hb_blob_t* aCOLR, hb_blob_t* aCPAL, const COLRBaseGlyphRecord* aLayers,
      DrawTarget* aDrawTarget, layout::TextDrawTarget* aTextDrawer,
      ScaledFont* aScaledFont, DrawOptions aDrawOptions,
      const sRGBColor& aCurrentColor, const Point& aPoint);
};

}  // namespace gfx

}  // namespace mozilla

#endif  // COLR_FONTS_H
