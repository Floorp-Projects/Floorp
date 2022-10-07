/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COLR_FONTS_H
#define COLR_FONTS_H

#include "mozilla/gfx/2D.h"

struct hb_blob_t;
struct hb_face_t;
struct hb_font_t;

namespace mozilla {

namespace layout {
class TextDrawTarget;
}

namespace gfx {

class COLRFonts {
 public:
  static bool ValidateColorGlyphs(hb_blob_t* aCOLR, hb_blob_t* aCPAL);

  // COLRv0: color glyph is represented as a simple list of colored layers.
  // (This is used only as an opaque pointer; the internal type is private.)
  struct GlyphLayers;

  static const GlyphLayers* GetGlyphLayers(hb_blob_t* aCOLR, uint32_t aGlyphId);

  static bool PaintGlyphLayers(
      hb_blob_t* aCOLR, hb_face_t* aFace, const GlyphLayers* aLayers,
      DrawTarget* aDrawTarget, layout::TextDrawTarget* aTextDrawer,
      ScaledFont* aScaledFont, DrawOptions aDrawOptions,
      const sRGBColor& aCurrentColor, const Point& aPoint);

  // COLRv1 support: color glyph is represented by a directed acyclic graph of
  // paint records.
  // (This is used only as an opaque pointer; the internal type is private.)
  struct GlyphPaintGraph;

  static const GlyphPaintGraph* GetGlyphPaintGraph(hb_blob_t* aCOLR,
                                                   uint32_t aGlyphId);

  static bool PaintGlyphGraph(hb_blob_t* aCOLR, hb_font_t* aFont,
                              const GlyphPaintGraph* aPaintGraph,
                              DrawTarget* aDrawTarget,
                              layout::TextDrawTarget* aTextDrawer,
                              ScaledFont* aScaledFont, DrawOptions aDrawOptions,
                              const sRGBColor& aCurrentColor,
                              const Point& aPoint, uint32_t aGlyphId,
                              float aFontUnitsToPixels);

  static Rect GetColorGlyphBounds(hb_blob_t* aCOLR, hb_font_t* aFont,
                                  uint32_t aGlyphId, DrawTarget* aDrawTarget,
                                  ScaledFont* aScaledFont,
                                  float aFontUnitsToPixels);

  static uint16_t GetColrTableVersion(hb_blob_t* aCOLR);
};

}  // namespace gfx

}  // namespace mozilla

#endif  // COLR_FONTS_H
