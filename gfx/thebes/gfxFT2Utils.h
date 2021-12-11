/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2UTILS_H
#define GFX_FT2UTILS_H

#include "cairo-ft.h"
#include "gfxFT2FontBase.h"
#include "mozilla/Likely.h"

// Rounding and truncation functions for a FreeType fixed point number
// (FT26Dot6) stored in a 32bit integer with high 26 bits for the integer
// part and low 6 bits for the fractional part.
#define FLOAT_FROM_26_6(x) ((x) / 64.0)
#define FLOAT_FROM_16_16(x) ((x) / 65536.0)
#define ROUND_26_6_TO_INT(x) ((x) >= 0 ? ((32 + (x)) >> 6) : -((32 - (x)) >> 6))

typedef struct FT_FaceRec_* FT_Face;

/**
 * BEWARE: Recursively locking with gfxFT2LockedFace is not supported.
 * Do not instantiate gfxFT2LockedFace within the scope of another instance.
 * Do not attempt to call into Cairo within the scope of gfxFT2LockedFace,
 * as that may accidentally try to re-lock the face within Cairo itself
 * and thus deadlock.
 */
class MOZ_STACK_CLASS gfxFT2LockedFace {
 public:
  explicit gfxFT2LockedFace(gfxFT2FontBase* aFont)
      : mGfxFont(aFont), mFace(aFont->LockFTFace()) {}
  ~gfxFT2LockedFace() {
    if (mFace) {
      mGfxFont->UnlockFTFace();
    }
  }

  FT_Face get() { return mFace; };

  /**
   * Get the glyph id for a Unicode character representable by a single
   * glyph, or return zero if there is no such glyph.  This does no caching,
   * so you probably want gfxFcFont::GetGlyph.
   */
  uint32_t GetGlyph(uint32_t aCharCode);
  /**
   * Returns 0 if there is no variation selector cmap subtable.
   */
  uint32_t GetUVSGlyph(uint32_t aCharCode, uint32_t aVariantSelector);

 protected:
  typedef FT_UInt (*CharVariantFunction)(FT_Face face, FT_ULong charcode,
                                         FT_ULong variantSelector);
  CharVariantFunction FindCharVariantFunction();

  gfxFT2FontBase* MOZ_NON_OWNING_REF mGfxFont;  // owned by caller
  FT_Face mFace;
};

// A couple of FreeType-based utilities shared by gfxFontconfigFontEntry
// and FT2FontEntry.

typedef struct FT_MM_Var_ FT_MM_Var;

class gfxFT2Utils {
 public:
  static void GetVariationAxes(const FT_MM_Var* aMMVar,
                               nsTArray<gfxFontVariationAxis>& aAxes);

  static void GetVariationInstances(
      gfxFontEntry* aFontEntry, const FT_MM_Var* aMMVar,
      nsTArray<gfxFontVariationInstance>& aInstances);
};

#endif /* GFX_FT2UTILS_H */
