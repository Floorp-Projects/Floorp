/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_HARFBUZZSHAPER_H
#define GFX_HARFBUZZSHAPER_H

#include "gfxFont.h"

#include "harfbuzz/hb.h"
#include "nsUnicodeProperties.h"
#include "mozilla/gfx/2D.h"

class gfxHarfBuzzShaper : public gfxFontShaper {
public:
    explicit gfxHarfBuzzShaper(gfxFont *aFont);
    virtual ~gfxHarfBuzzShaper();

    /*
     * For HarfBuzz font callback functions, font_data is a ptr to a
     * FontCallbackData struct
     */
    struct FontCallbackData {
        gfxHarfBuzzShaper* mShaper;
        mozilla::gfx::DrawTarget* mDrawTarget;
    };

    bool Initialize();
    virtual bool ShapeText(gfxContext      *aContext,
                           const char16_t *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           bool             aVertical,
                           gfxShapedText   *aShapedText);

    // get a given font table in harfbuzz blob form
    hb_blob_t * GetFontTable(hb_tag_t aTag) const;

    // map unicode character to glyph ID
    hb_codepoint_t GetGlyph(hb_codepoint_t unicode,
                            hb_codepoint_t variation_selector) const;

    // get harfbuzz glyph advance, in font design units
    hb_position_t GetGlyphHAdvance(hb_codepoint_t glyph) const;

    hb_position_t GetGlyphVAdvance(hb_codepoint_t glyph) const;

    void GetGlyphVOrigin(hb_codepoint_t aGlyph,
                         hb_position_t *aX, hb_position_t *aY) const;

    // get harfbuzz horizontal advance in 16.16 fixed point format.
    static hb_position_t
    HBGetGlyphHAdvance(hb_font_t *font, void *font_data,
                       hb_codepoint_t glyph, void *user_data);

    // get harfbuzz vertical advance in 16.16 fixed point format.
    static hb_position_t
    HBGetGlyphVAdvance(hb_font_t *font, void *font_data,
                       hb_codepoint_t glyph, void *user_data);

    static hb_bool_t
    HBGetGlyphHOrigin(hb_font_t *font, void *font_data,
                      hb_codepoint_t glyph,
                      hb_position_t *x, hb_position_t *y,
                      void *user_data);
    static hb_bool_t
    HBGetGlyphVOrigin(hb_font_t *font, void *font_data,
                      hb_codepoint_t glyph,
                      hb_position_t *x, hb_position_t *y,
                      void *user_data);

    hb_position_t GetHKerning(uint16_t aFirstGlyph,
                              uint16_t aSecondGlyph) const;

    hb_bool_t GetGlyphExtents(hb_codepoint_t aGlyph,
                              hb_glyph_extents_t *aExtents) const;

    bool UseVerticalPresentationForms() const
    {
        return mUseVerticalPresentationForms;
    }

    static hb_script_t
    GetHBScriptUsedForShaping(int32_t aScript) {
        // Decide what harfbuzz script code will be used for shaping
        hb_script_t hbScript;
        if (aScript <= MOZ_SCRIPT_INHERITED) {
            // For unresolved "common" or "inherited" runs,
            // default to Latin for now.
            hbScript = HB_SCRIPT_LATIN;
        } else {
            hbScript =
                hb_script_t(mozilla::unicode::GetScriptTagForCode(aScript));
        }
        return hbScript;
    }

protected:
    nsresult SetGlyphsFromRun(gfxContext     *aContext,
                              gfxShapedText  *aShapedText,
                              uint32_t        aOffset,
                              uint32_t        aLength,
                              const char16_t *aText,
                              hb_buffer_t    *aBuffer,
                              bool            aVertical);

    // retrieve glyph positions, applying advance adjustments and attachments
    // returns results in appUnits
    nscoord GetGlyphPositions(gfxContext *aContext,
                              hb_buffer_t *aBuffer,
                              nsTArray<nsPoint>& aPositions,
                              uint32_t aAppUnitsPerDevUnit);

    bool InitializeVertical();
    bool LoadHmtxTable();

    struct Glyf { // we only need the bounding-box at the beginning
                  // of the glyph record, not the actual outline data
        AutoSwap_PRInt16 numberOfContours;
        AutoSwap_PRInt16 xMin;
        AutoSwap_PRInt16 yMin;
        AutoSwap_PRInt16 xMax;
        AutoSwap_PRInt16 yMax;
    };

    const Glyf *FindGlyf(hb_codepoint_t aGlyph, bool *aEmptyGlyf) const;

    // harfbuzz face object: we acquire a reference from the font entry
    // on shaper creation, and release it in our destructor
    hb_face_t         *mHBFace;

    // size-specific font object, owned by the gfxHarfBuzzShaper
    hb_font_t         *mHBFont;

    FontCallbackData   mCallbackData;

    // Following table references etc are declared "mutable" because the
    // harfbuzz callback functions take a const ptr to the shaper, but
    // wish to cache tables here to avoid repeatedly looking them up
    // in the font.

    // Old-style TrueType kern table, if we're not doing GPOS kerning
    mutable hb_blob_t *mKernTable;

    // Cached copy of the hmtx table.
    mutable hb_blob_t *mHmtxTable;

    // For vertical fonts, cached vmtx and VORG table, if present.
    mutable hb_blob_t *mVmtxTable;
    mutable hb_blob_t *mVORGTable;
    // And for vertical TrueType (not CFF) fonts that have vmtx,
    // we also use loca and glyf to get glyph bounding boxes.
    mutable hb_blob_t *mLocaTable;
    mutable hb_blob_t *mGlyfTable;

    // Cached pointer to cmap subtable to be used for char-to-glyph mapping.
    // This comes from GetFontTablePtr; if it is non-null, our destructor
    // must call ReleaseFontTablePtr to avoid permanently caching the table.
    mutable hb_blob_t *mCmapTable;
    mutable int32_t    mCmapFormat;
    mutable uint32_t   mSubtableOffset;
    mutable uint32_t   mUVSTableOffset;

    // Cached copy of numLongMetrics field from the hhea table,
    // for use when looking up glyph metrics; initialized to 0 by the
    // constructor so we can tell it hasn't been set yet.
    // This is a signed value so that we can use -1 to indicate
    // an error (if the hhea table was not available).
    mutable int32_t    mNumLongHMetrics;
    // Similarly for vhea if it's a vertical font.
    mutable int32_t    mNumLongVMetrics;

    // Whether the font implements GetGlyph, or we should read tables
    // directly
    bool mUseFontGetGlyph;
    // Whether the font implements GetGlyphWidth, or we should read tables
    // directly to get ideal widths
    bool mUseFontGlyphWidths;

    bool mInitialized;
    bool mVerticalInitialized;

    // Whether to use vertical presentation forms for CJK characters
    // when available (only set if the 'vert' feature is not available).
    bool mUseVerticalPresentationForms;

    // these are set from the FindGlyf callback on first use of the glyf data
    mutable bool mLoadedLocaGlyf;
    mutable bool mLocaLongOffsets;
};

#endif /* GFX_HARFBUZZSHAPER_H */
