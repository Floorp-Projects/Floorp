/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DrawMode_h
#define DrawMode_h

#include "mozilla/TypedEnumBits.h"

// Options for how the text should be drawn
enum class DrawMode : int {
  // GLYPH_FILL and GLYPH_STROKE draw into the current context
  //  and may be used together with bitwise OR.
  GLYPH_FILL = 1 << 0,
  // Note: using GLYPH_STROKE will destroy the current path.
  GLYPH_STROKE = 1 << 1,
  // Appends glyphs to the current path. Can NOT be used with
  //  GLYPH_FILL or GLYPH_STROKE.
  GLYPH_PATH = 1 << 2,
  // When GLYPH_FILL and GLYPH_STROKE are both set, draws the
  //  stroke underneath the fill.
  GLYPH_STROKE_UNDERNEATH = 1 << 3
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(DrawMode)
#endif
