/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_VARIATIONS_H
#define GFX_FONT_VARIATIONS_H

#include "mozilla/gfx/FontVariation.h"
#include "nsString.h"
#include "nsTArray.h"

typedef mozilla::gfx::FontVariation gfxFontVariation;

// Structure that describes a single axis of variation in an
// OpenType Variation or Multiple-Master font.
struct gfxFontVariationAxis {
  uint32_t mTag;
  nsCString mName;  // may be empty
  float mMinValue;
  float mMaxValue;
  float mDefaultValue;
};

// A single <axis, value> pair that may be applied to a variation font.
struct gfxFontVariationValue {
  uint32_t mAxis;
  float mValue;
};

// Structure that describes a named instance of a variation font:
// a name like "Light Condensed" or "Black Ultra Extended" etc.,
// and a list of the corresponding <variation-axis, value> pairs
// to be used.
struct gfxFontVariationInstance {
  nsCString mName;
  CopyableTArray<gfxFontVariationValue> mValues;
};

#endif
