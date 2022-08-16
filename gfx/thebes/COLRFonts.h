/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COLR_FONTS_H
#define COLR_FONTS_H

#include "nsTArray.h"

struct hb_blob_t;

namespace mozilla {
namespace gfx {
struct DeviceColor;

class COLRFonts {
 public:
  // for color layer from glyph using COLR and CPAL tables
  static bool ValidateColorGlyphs(hb_blob_t* aCOLR, hb_blob_t* aCPAL);
  static bool GetColorGlyphLayers(
      hb_blob_t* aCOLR, hb_blob_t* aCPAL, uint32_t aGlyphId,
      const mozilla::gfx::DeviceColor& aDefaultColor,
      nsTArray<uint16_t>& aGlyphs,
      nsTArray<mozilla::gfx::DeviceColor>& aColors);
  static bool HasColorLayersForGlyph(hb_blob_t* aCOLR, uint32_t aGlyphId);
};

}  // namespace gfx
}  // namespace mozilla

#endif  // COLR_FONTS_H
