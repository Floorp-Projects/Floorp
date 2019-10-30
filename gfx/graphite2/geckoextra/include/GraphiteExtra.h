// -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vim: set ts=2 et sw=2 tw=80:
// This Source Code is subject to the terms of the Mozilla Public License
// version 2.0 (the "License"). You can obtain a copy of the License at
// http://mozilla.org/MPL/2.0/.

#ifndef GraphiteExtra_h__
#define GraphiteExtra_h__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "graphite2/Segment.h"

typedef struct {
  uint32_t baseChar;  // in UTF16 code units, not Unicode character indices
  uint32_t baseGlyph;
  uint32_t nChars;  // UTF16 code units
  uint32_t nGlyphs;
} gr_glyph_to_char_cluster;

typedef struct {
  gr_glyph_to_char_cluster* clusters;
  uint16_t* gids;
  float* xLocs;
  float* yLocs;
  uint32_t cIndex;
} gr_glyph_to_char_association;

gr_glyph_to_char_association* gr_get_glyph_to_char_association(
    gr_segment* aSegment, uint32_t aLength, const char16_t* aText);

void gr_free_char_association(gr_glyph_to_char_association* aData);

#ifdef __cplusplus
}
#endif

#endif