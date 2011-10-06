/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Behdad Esfahbod <behdad@gnome.org>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 *   Takuro Ashie <ashie@clear-code.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_FT2UTILS_H
#define GFX_FT2UTILS_H

#include "cairo-ft.h"
#include "gfxFT2FontBase.h"

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
    gfxFT2LockedFace(gfxFT2FontBase *aFont) :
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
    PRUint32 GetGlyph(PRUint32 aCharCode);
    /**
     * Returns 0 if there is no variation selector cmap subtable.
     */
    PRUint32 GetUVSGlyph(PRUint32 aCharCode, PRUint32 aVariantSelector);

    void GetMetrics(gfxFont::Metrics* aMetrics, PRUint32* aSpaceGlyph);

    bool GetFontTable(PRUint32 aTag, FallibleTArray<PRUint8>& aBuffer);

    // A scale factor for use in converting horizontal metrics from font units
    // to pixels.
    gfxFloat XScale()
    {
        if (NS_UNLIKELY(!mFace))
            return 0.0;

        const FT_Size_Metrics& ftMetrics = mFace->size->metrics;

        if (FT_IS_SCALABLE(mFace)) {
            // Prefer FT_Size_Metrics::x_scale to x_ppem as x_ppem does not
            // have subpixel accuracy.
            //
            // FT_Size_Metrics::x_scale is in 16.16 fixed point format.  Its
            // (fractional) value is a factor that converts vertical metrics
            // from design units to units of 1/64 pixels, so that the result
            // may be interpreted as pixels in 26.6 fixed point format.
            return FLOAT_FROM_26_6(FLOAT_FROM_16_16(ftMetrics.x_scale));
        }

        // Not scalable.
        // FT_Size_Metrics doc says x_scale is "only relevant for scalable
        // font formats".
        return gfxFloat(ftMetrics.x_ppem) / gfxFloat(mFace->units_per_EM);
    }

protected:
    /**
     * Get extents for a simple character representable by a single glyph.
     * The return value is the glyph id of that glyph or zero if no such glyph
     * exists.  aExtents is only set when this returns a non-zero glyph id.
     */
    PRUint32 GetCharExtents(char aChar, cairo_text_extents_t* aExtents);

    typedef FT_UInt (*CharVariantFunction)(FT_Face  face,
                                           FT_ULong charcode,
                                           FT_ULong variantSelector);
    CharVariantFunction FindCharVariantFunction();

    nsRefPtr<gfxFT2FontBase> mGfxFont;
    FT_Face mFace;
};

#endif /* GFX_FT2UTILS_H */
