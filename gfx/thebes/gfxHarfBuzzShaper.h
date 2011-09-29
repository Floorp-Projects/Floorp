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
 * The Original Code is thebes gfx code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
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

#ifndef GFX_HARFBUZZSHAPER_H
#define GFX_HARFBUZZSHAPER_H

#include "gfxTypes.h"
#include "gfxFont.h"
#include "nsDataHashtable.h"
#include "nsPoint.h"

#include "harfbuzz/hb.h"

class gfxHarfBuzzShaper : public gfxFontShaper {
public:
    gfxHarfBuzzShaper(gfxFont *aFont);
    virtual ~gfxHarfBuzzShaper();

    virtual bool InitTextRun(gfxContext *aContext,
                               gfxTextRun *aTextRun,
                               const PRUnichar *aString,
                               PRUint32 aRunStart,
                               PRUint32 aRunLength,
                               PRInt32 aRunScript);

    // get a given font table in harfbuzz blob form
    hb_blob_t * GetFontTable(hb_tag_t aTag) const;

    // map unicode character to glyph ID
    hb_codepoint_t GetGlyph(hb_codepoint_t unicode,
                            hb_codepoint_t variation_selector) const;

    // get harfbuzz glyph advance, in font design units
    void GetGlyphAdvance(gfxContext *aContext,
                         hb_codepoint_t glyph,
                         hb_position_t *x_advance,
                         hb_position_t *y_advance) const;

    hb_position_t GetKerning(PRUint16 aFirstGlyph,
                             PRUint16 aSecondGlyph) const;

protected:
    // extract glyphs from HarfBuzz buffer and store into the gfxTextRun
    nsresult SetGlyphsFromRun(gfxContext *aContext,
                              gfxTextRun *aTextRun,
                              hb_buffer_t *aBuffer,
                              PRUint32 aTextRunOffset,
                              PRUint32 aRunLength);

    // retrieve glyph positions, applying advance adjustments and attachments
    // returns results in appUnits
    nscoord GetGlyphPositions(gfxContext *aContext,
                              hb_buffer_t *aBuffer,
                              nsTArray<nsPoint>& aPositions,
                              PRUint32 aAppUnitsPerDevUnit);

    // harfbuzz face object, created on first use (caches font tables)
    hb_face_t         *mHBFace;

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
    mutable PRInt32    mNumLongMetrics;

    // Cached pointer to cmap subtable to be used for char-to-glyph mapping.
    // This comes from GetFontTablePtr; if it is non-null, our destructor
    // must call ReleaseFontTablePtr to avoid permanently caching the table.
    mutable hb_blob_t *mCmapTable;
    mutable PRInt32    mCmapFormat;
    mutable PRUint32   mSubtableOffset;
    mutable PRUint32   mUVSTableOffset;

    // Whether the font implements GetGlyph, or we should read tables
    // directly
    bool mUseFontGetGlyph;
    // Whether the font implements GetGlyphWidth, or we should read tables
    // directly to get ideal widths
    bool mUseFontGlyphWidths;
};

#endif /* GFX_HARFBUZZSHAPER_H */
