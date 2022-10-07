/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "COLRFonts.h"
#include "gfxFontUtils.h"
#include "harfbuzz/hb.h"
#include "MockDrawTarget.h"
#include "MockScaledFont.h"
#include "FuzzingInterface.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::gfx;

static int FuzzingRunCOLRv1(const uint8_t* data, size_t size) {
  gfxFontUtils::AutoHBBlob hb_data_blob(hb_blob_create(
      (const char*)data, size, HB_MEMORY_MODE_READONLY, nullptr, nullptr));

  hb_face_t* hb_data_face = hb_face_create(hb_data_blob, 0);
  if (!hb_data_face) {
    return 0;
  }

  gfxFontUtils::AutoHBBlob colr(
      hb_face_reference_table(hb_data_face, TRUETYPE_TAG('C', 'O', 'L', 'R')));
  gfxFontUtils::AutoHBBlob cpal(
      hb_face_reference_table(hb_data_face, TRUETYPE_TAG('C', 'P', 'A', 'L')));
  if (!colr || !cpal || !COLRFonts::ValidateColorGlyphs(colr, cpal)) {
    hb_face_destroy(hb_data_face);
    return 0;
  }

  const float kPixelSize = 16.0f;
  unsigned glyph_count = hb_face_get_glyph_count(hb_data_face);
  hb_font_t* hb_data_font = hb_font_create(hb_data_face);
  uint32_t scale = NS_round(kPixelSize * 65536.0);  // size as 16.16 fixed-point
  hb_font_set_scale(hb_data_font, scale, scale);

  RefPtr dt = new MockDrawTarget();
  RefPtr uf = new MockUnscaledFont();
  RefPtr sf = new MockScaledFont(uf, hb_data_font);
  Float f2p = kPixelSize / hb_face_get_upem(hb_data_face);

  for (unsigned i = 0; i <= glyph_count; ++i) {
    if (COLRFonts::GetColrTableVersion(colr) == 1) {
      Rect bounds =
          COLRFonts::GetColorGlyphBounds(colr, hb_data_font, i, dt, sf, f2p);
      const auto* paintGraph = COLRFonts::GetGlyphPaintGraph(colr, i);
      if (paintGraph) {
        dt->PushClipRect(bounds);
        COLRFonts::PaintGlyphGraph(colr, hb_data_font, paintGraph, dt, nullptr,
                                   sf, DrawOptions(), sRGBColor(), Point(), i,
                                   f2p);
        dt->PopClip();
      }
    }
    const auto* layers = COLRFonts::GetGlyphLayers(colr, i);
    if (layers) {
      COLRFonts::PaintGlyphLayers(colr, hb_data_face, layers, dt, nullptr, sf,
                                  DrawOptions(), sRGBColor(), Point());
    }
  }

  hb_font_destroy(hb_data_font);
  hb_face_destroy(hb_data_face);

  return 0;
}

int FuzzingInitCOLRv1(int* argc, char*** argv) {
  Preferences::SetBool("gfx.font_rendering.colr_v1.enabled", true);
  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitCOLRv1, FuzzingRunCOLRv1, GfxCOLRv1);
