/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#define ROUND_26_6_TO_INT(x) ((x) >= 0 ?  ((32 + (x)) >> 6) \
                                       : -((32 - (x)) >> 6))

typedef struct FT_FaceRec_* FT_Face;

class gfxFT2LockedFace {
public:
    explicit gfxFT2LockedFace(gfxFT2FontBase *aFont) :
        mGfxFont(aFont),
        mFace(cairo_ft_scaled_font_lock_face(aFont->CairoScaledFont()))
    { }
    ~gfxFT2LockedFace()
    {
        if (mFace) {
            cairo_ft_scaled_font_unlock_face(mGfxFont->CairoScaledFont());
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
    typedef FT_UInt (*CharVariantFunction)(FT_Face  face,
                                           FT_ULong charcode,
                                           FT_ULong variantSelector);
    CharVariantFunction FindCharVariantFunction();

    RefPtr<gfxFT2FontBase> mGfxFont;
    FT_Face mFace;
};

#endif /* GFX_FT2UTILS_H */
