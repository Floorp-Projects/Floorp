/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_HARFBUZZSHAPER_H
#define GFX_HARFBUZZSHAPER_H

#include "gfxFont.h"

#include "harfbuzz/hb.h"

class gfxHarfBuzzShaper : public gfxFontShaper {
public:
    gfxHarfBuzzShaper(gfxFont *aFont);
    virtual ~gfxHarfBuzzShaper();

    /*
     * For HarfBuzz font callback functions, font_data is a ptr to a
     * FontCallbackData struct
     */
    struct FontCallbackData {
        gfxHarfBuzzShaper *mShaper;
        gfxContext        *mContext;
    };

    virtual bool ShapeText(gfxContext      *aContext,
                           const char16_t *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           gfxShapedText   *aShapedText);

    // get a given font table in harfbuzz blob form
    hb_blob_t * GetFontTable(hb_tag_t aTag) const;

    // map unicode character to glyph ID
    hb_codepoint_t GetGlyph(hb_codepoint_t unicode,
                            hb_codepoint_t variation_selector) const;

    // get harfbuzz glyph advance, in font design units
    hb_position_t GetGlyphHAdvance(gfxContext *aContext,
                                   hb_codepoint_t glyph) const;

    hb_position_t GetHKerning(uint16_t aFirstGlyph,
                              uint16_t aSecondGlyph) const;

protected:
    nsresult SetGlyphsFromRun(gfxContext      *aContext,
                              gfxShapedText   *aShapedText,
                              uint32_t         aOffset,
                              uint32_t         aLength,
                              const char16_t *aText,
                              hb_buffer_t     *aBuffer);

    // retrieve glyph positions, applying advance adjustments and attachments
    // returns results in appUnits
    nscoord GetGlyphPositions(gfxContext *aContext,
                              hb_buffer_t *aBuffer,
                              nsTArray<nsPoint>& aPositions,
                              uint32_t aAppUnitsPerDevUnit);

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

    // Cached copy of the hmtx table and numLongMetrics field from hhea,
    // for use when looking up glyph metrics; initialized to 0 by the
    // constructor so we can tell it hasn't been set yet.
    // This is a signed value so that we can use -1 to indicate
    // an error (if the hhea table was not available).
    mutable hb_blob_t *mHmtxTable;
    mutable int32_t    mNumLongMetrics;

    // Cached pointer to cmap subtable to be used for char-to-glyph mapping.
    // This comes from GetFontTablePtr; if it is non-null, our destructor
    // must call ReleaseFontTablePtr to avoid permanently caching the table.
    mutable hb_blob_t *mCmapTable;
    mutable int32_t    mCmapFormat;
    mutable uint32_t   mSubtableOffset;
    mutable uint32_t   mUVSTableOffset;

    // Whether the font implements GetGlyph, or we should read tables
    // directly
    bool mUseFontGetGlyph;
    // Whether the font implements GetGlyphWidth, or we should read tables
    // directly to get ideal widths
    bool mUseFontGlyphWidths;

    bool mInitialized;
};

#endif /* GFX_HARFBUZZSHAPER_H */
