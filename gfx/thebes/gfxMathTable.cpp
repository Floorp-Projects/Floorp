/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxMathTable.h"

#include "harfbuzz/hb.h"
#include "harfbuzz/hb-ot.h"

#define FloatToFixed(f) (65536 * (f))
#define FixedToFloat(f) ((f) * (1.0 / 65536.0))

using namespace mozilla;

gfxMathTable::gfxMathTable(hb_face_t* aFace, gfxFloat aSize) {
  mMathVariantCache.vertical = false;
  mHBFont = hb_font_create(aFace);
  if (mHBFont) {
    hb_font_set_ppem(mHBFont, aSize, aSize);
    uint32_t scale = FloatToFixed(aSize);
    hb_font_set_scale(mHBFont, scale, scale);
  }

  mMathVariantCache.glyphID = 0;
  ClearCache();
}

gfxMathTable::~gfxMathTable() {
  if (mHBFont) {
    hb_font_destroy(mHBFont);
  }
}

gfxFloat gfxMathTable::Constant(MathConstant aConstant) const {
  int32_t value = hb_ot_math_get_constant(
      mHBFont, static_cast<hb_ot_math_constant_t>(aConstant));
  if (aConstant == ScriptPercentScaleDown ||
      aConstant == ScriptScriptPercentScaleDown ||
      aConstant == RadicalDegreeBottomRaisePercent) {
    return value / 100.0;
  }
  return FixedToFloat(value);
}

gfxFloat gfxMathTable::ItalicsCorrection(uint32_t aGlyphID) const {
  return FixedToFloat(
      hb_ot_math_get_glyph_italics_correction(mHBFont, aGlyphID));
}

uint32_t gfxMathTable::VariantsSize(uint32_t aGlyphID, bool aVertical,
                                    uint16_t aSize) const {
  UpdateMathVariantCache(aGlyphID, aVertical);
  if (aSize < kMaxCachedSizeCount) {
    return mMathVariantCache.sizes[aSize];
  }

  // If the size index exceeds the cache size, we just read the value with
  // hb_ot_math_get_glyph_variants.
  hb_direction_t direction = aVertical ? HB_DIRECTION_BTT : HB_DIRECTION_LTR;
  hb_ot_math_glyph_variant_t variant;
  unsigned int count = 1;
  hb_ot_math_get_glyph_variants(mHBFont, aGlyphID, direction, aSize, &count,
                                &variant);
  return count > 0 ? variant.glyph : 0;
}

bool gfxMathTable::VariantsParts(uint32_t aGlyphID, bool aVertical,
                                 uint32_t aGlyphs[4]) const {
  UpdateMathVariantCache(aGlyphID, aVertical);
  memcpy(aGlyphs, mMathVariantCache.parts, sizeof(mMathVariantCache.parts));
  return mMathVariantCache.arePartsValid;
}

void gfxMathTable::ClearCache() const {
  memset(mMathVariantCache.sizes, 0, sizeof(mMathVariantCache.sizes));
  memset(mMathVariantCache.parts, 0, sizeof(mMathVariantCache.parts));
  mMathVariantCache.arePartsValid = false;
}

void gfxMathTable::UpdateMathVariantCache(uint32_t aGlyphID,
                                          bool aVertical) const {
  if (aGlyphID == mMathVariantCache.glyphID &&
      aVertical == mMathVariantCache.vertical)
    return;

  mMathVariantCache.glyphID = aGlyphID;
  mMathVariantCache.vertical = aVertical;
  ClearCache();

  // Cache the first size variants.
  hb_direction_t direction = aVertical ? HB_DIRECTION_BTT : HB_DIRECTION_LTR;
  hb_ot_math_glyph_variant_t variant[kMaxCachedSizeCount];
  unsigned int count = kMaxCachedSizeCount;
  hb_ot_math_get_glyph_variants(mHBFont, aGlyphID, direction, 0, &count,
                                variant);
  for (unsigned int i = 0; i < count; i++) {
    mMathVariantCache.sizes[i] = variant[i].glyph;
  }

  // Try and cache the parts of the glyph assembly.
  // XXXfredw The structure of the Open Type Math table is a bit more general
  // than the one currently used by the nsMathMLChar code, so we try to fallback
  // in reasonable way. We use the approach of the copyComponents function in
  // github.com/mathjax/MathJax-dev/blob/master/fonts/OpenTypeMath/fontUtil.py
  //
  // The nsMathMLChar code can use at most 3 non extender pieces (aGlyphs[0],
  // aGlyphs[1] and aGlyphs[2]) and the extenders between these pieces should
  // all be the same (aGlyphs[4]). Also, the parts of vertical assembly are
  // stored from bottom to top in the Open Type MATH table while they are
  // stored from top to bottom in nsMathMLChar.

  hb_ot_math_glyph_part_t parts[5];
  count = MOZ_ARRAY_LENGTH(parts);
  unsigned int offset = 0;
  if (hb_ot_math_get_glyph_assembly(mHBFont, aGlyphID, direction, offset,
                                    &count, parts,
                                    NULL) > MOZ_ARRAY_LENGTH(parts))
    return;                // Not supported: Too many pieces.
  if (count <= 0) return;  // Not supported: No pieces.

  // Count the number of non extender pieces
  uint16_t nonExtenderCount = 0;
  for (uint16_t i = 0; i < count; i++) {
    if (!(parts[i].flags & HB_MATH_GLYPH_PART_FLAG_EXTENDER)) {
      nonExtenderCount++;
    }
  }
  if (nonExtenderCount > 3) {
    // Not supported: too many pieces
    return;
  }

  // Now browse the list of pieces

  // 0 = look for a left/bottom glyph
  // 1 = look for an extender between left/bottom and mid
  // 2 = look for a middle glyph
  // 3 = look for an extender between middle and right/top
  // 4 = look for a right/top glyph
  // 5 = no more piece expected
  uint8_t state = 0;

  // First extender char found.
  uint32_t extenderChar = 0;

  for (uint16_t i = 0; i < count; i++) {
    bool isExtender = parts[i].flags & HB_MATH_GLYPH_PART_FLAG_EXTENDER;
    uint32_t glyph = parts[i].glyph;

    if ((state == 1 || state == 2) && nonExtenderCount < 3) {
      // do not try to find a middle glyph
      state += 2;
    }

    if (isExtender) {
      if (!extenderChar) {
        extenderChar = glyph;
        mMathVariantCache.parts[3] = extenderChar;
      } else if (extenderChar != glyph) {
        // Not supported: different extenders
        return;
      }

      if (state == 0) {  // or state == 1
        // ignore left/bottom piece and multiple successive extenders
        state = 1;
      } else if (state == 2) {  // or state == 3
        // ignore middle piece and multiple successive extenders
        state = 3;
      } else if (state >= 4) {
        // Not supported: unexpected extender
        return;
      }

      continue;
    }

    if (state == 0) {
      // copy left/bottom part
      mMathVariantCache.parts[aVertical ? 2 : 0] = glyph;
      state = 1;
      continue;
    }

    if (state == 1 || state == 2) {
      // copy middle part
      mMathVariantCache.parts[1] = glyph;
      state = 3;
      continue;
    }

    if (state == 3 || state == 4) {
      // copy right/top part
      mMathVariantCache.parts[aVertical ? 0 : 2] = glyph;
      state = 5;
    }
  }

  mMathVariantCache.arePartsValid = true;
}
