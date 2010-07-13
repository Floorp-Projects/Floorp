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
 * The Original Code is Mozilla Corporation code.
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

#include "prtypes.h"
#include "prmem.h"
#include "nsString.h"
#include "nsBidiUtils.h"
#include "nsMathUtils.h"

#include "gfxTypes.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxFontUtils.h"
#include "gfxUnicodeProperties.h"

#include "harfbuzz/hb-unicode.h"
#include "harfbuzz/hb-ot.h"

#include "nsUnicodeRange.h"
#include "nsCRT.h"

#define FloatToFixed(f) (65536 * (f))
#define FixedToFloat(f) ((f) * (1.0 / 65536.0))

using namespace mozilla; // for AutoSwap_* types

/*
 * Creation and destruction; on deletion, release any font tables we're holding
 */

gfxHarfBuzzShaper::gfxHarfBuzzShaper(gfxFont *aFont)
    : gfxFontShaper(aFont),
      mHBFace(nsnull),
      mHBLanguage(nsnull),
      mHmtxTable(nsnull),
      mNumLongMetrics(0),
      mCmapTable(nsnull),
      mCmapFormat(-1),
      mSubtableOffset(0),
      mUVSTableOffset(0),
      mUseHintedWidths(aFont->ProvidesHintedWidths())
{
}

gfxHarfBuzzShaper::~gfxHarfBuzzShaper()
{
    hb_blob_destroy(mCmapTable);
    hb_blob_destroy(mHmtxTable);
    hb_face_destroy(mHBFace);
}

/*
 * HarfBuzz callback access to font table data
 */

// callback for HarfBuzz to get a font table (in hb_blob_t form)
// from the shaper (passed as aUserData)
static hb_blob_t *
HBGetTable(hb_tag_t aTag, void *aUserData)
{
    gfxHarfBuzzShaper *shaper = static_cast<gfxHarfBuzzShaper*>(aUserData);
    return shaper->GetFont()->GetFontTable(aTag);
}

/*
 * HarfBuzz font callback functions; user_data is a ptr to a
 * FontCallbackData struct
 */

struct FontCallbackData {
    FontCallbackData(gfxHarfBuzzShaper *aShaper, gfxContext *aContext)
        : mShaper(aShaper), mContext(aContext)
    { }

    gfxHarfBuzzShaper *mShaper;
    gfxContext        *mContext;
};

#define UNICODE_BMP_LIMIT 0x10000

hb_codepoint_t
gfxHarfBuzzShaper::GetGlyph(hb_codepoint_t unicode,
                            hb_codepoint_t variation_selector) const
{
    // we only instantiate a harfbuzz shaper if there's a cmap available
    NS_ASSERTION(mFont->GetFontEntry()->HasCmapTable(),
                 "we cannot be using this font!");

    NS_ASSERTION(mCmapTable && (mCmapFormat > 0) && (mSubtableOffset > 0),
                 "cmap data not correctly set up, expect disaster");

    const PRUint8* data = (const PRUint8*)hb_blob_lock(mCmapTable);

    hb_codepoint_t gid;
    switch (mCmapFormat) {
    case 4:
        gid = unicode < UNICODE_BMP_LIMIT ?
            gfxFontUtils::MapCharToGlyphFormat4(data + mSubtableOffset, unicode) : 0;
        break;
    case 12:
        gid = gfxFontUtils::MapCharToGlyphFormat12(data + mSubtableOffset, unicode);
        break;
    default:
        NS_WARNING("unsupported cmap format, glyphs will be missing");
        gid = 0;
        break;
    }

    if (gid && variation_selector && mUVSTableOffset) {
        hb_codepoint_t varGID =
            gfxFontUtils::MapUVSToGlyphFormat14(data + mUVSTableOffset,
                                                unicode, variation_selector);
        if (varGID) {
            gid = varGID;
        }
        // else the variation sequence was not supported, use default mapping
        // of the character code alone
    }

    hb_blob_unlock(mCmapTable);

    return gid;
}

static hb_codepoint_t
HBGetGlyph(hb_font_t *font, hb_face_t *face, const void *user_data,
           hb_codepoint_t unicode, hb_codepoint_t variation_selector)
{
    const FontCallbackData *fcd =
        static_cast<const FontCallbackData*>(user_data);
    return fcd->mShaper->GetGlyph(unicode, variation_selector);
}

struct HMetricsHeader {
    AutoSwap_PRUint32    tableVersionNumber;
    AutoSwap_PRInt16     ascender;
    AutoSwap_PRInt16     descender;
    AutoSwap_PRInt16     lineGap;
    AutoSwap_PRUint16    advanceWidthMax;
    AutoSwap_PRInt16     minLeftSideBearing;
    AutoSwap_PRInt16     minRightSideBearing;
    AutoSwap_PRInt16     xMaxExtent;
    AutoSwap_PRInt16     caretSlopeRise;
    AutoSwap_PRInt16     caretSlopeRun;
    AutoSwap_PRInt16     caretOffset;
    AutoSwap_PRInt16     reserved[4];
    AutoSwap_PRInt16     metricDataFormat;
    AutoSwap_PRUint16    numberOfHMetrics;
};

struct HLongMetric {
    AutoSwap_PRUint16    advanceWidth;
    AutoSwap_PRInt16     lsb;
};

struct HMetrics {
    HLongMetric          metrics[1]; // actually numberOfHMetrics
// the variable-length metrics[] array is immediately followed by:
//  AutoSwap_PRUint16    leftSideBearing[];
};

void
gfxHarfBuzzShaper::GetGlyphMetrics(gfxContext *aContext,
                                   hb_codepoint_t glyph,
                                   hb_glyph_metrics_t *metrics) const
{
    if (mUseHintedWidths) {
        metrics->x_advance = mFont->GetHintedGlyphWidth(aContext, glyph);
        return;
    }

    // font did not implement GetHintedGlyphWidth, so get an unhinted value
    // directly from the font tables

    NS_ASSERTION((mNumLongMetrics > 0) && mHmtxTable != nsnull,
                 "font is lacking metrics, we shouldn't be here");

    if (glyph >= mNumLongMetrics) {
        glyph = mNumLongMetrics - 1;
    }

    if ((glyph + 1) * sizeof(HLongMetric) <= hb_blob_get_length(mHmtxTable)) {
        const HMetrics* hmtx =
            reinterpret_cast<const HMetrics*>(hb_blob_lock(mHmtxTable));
        metrics->x_advance =
            FloatToFixed(mFont->FUnitsToDevUnitsFactor() *
                         PRUint16(hmtx->metrics[glyph].advanceWidth));
        hb_blob_unlock(mHmtxTable);
    }

    // TODO: set additional metrics if/when harfbuzz needs them
}

static void
HBGetGlyphMetrics(hb_font_t *font, hb_face_t *face, const void *user_data,
                  hb_codepoint_t glyph, hb_glyph_metrics_t *metrics)
{
    const FontCallbackData *fcd =
        static_cast<const FontCallbackData*>(user_data);
    fcd->mShaper->GetGlyphMetrics(fcd->mContext, glyph, metrics);
}

static hb_bool_t
HBGetContourPoint(hb_font_t *font, hb_face_t *face, const void *user_data,
                  unsigned int point_index, hb_codepoint_t glyph,
                  hb_position_t *x, hb_position_t *y)
{
    /* not yet implemented - no support for used of hinted contour points
       to fine-tune anchor positions in GPOS AnchorFormat2 */
    return false;
}

static hb_position_t
HBGetKerning(hb_font_t *font, hb_face_t *face, const void *user_data,
             hb_codepoint_t first_glyph, hb_codepoint_t second_glyph)
{
    /* not yet implemented - no support for legacy TrueType kerning */
    return 0;
}

/*
 * HarfBuzz unicode property callbacks
 */

static hb_codepoint_t
HBGetMirroring(hb_codepoint_t aCh)
{
    return gfxUnicodeProperties::GetMirroredChar(aCh);
}

static hb_category_t
HBGetGeneralCategory(hb_codepoint_t aCh)
{
    return hb_category_t(gfxUnicodeProperties::GetGeneralCategory(aCh));
}

static hb_script_t
HBGetScript(hb_codepoint_t aCh)
{
    return hb_script_t(gfxUnicodeProperties::GetScriptCode(aCh));
}

static unsigned int
HBGetCombiningClass(hb_codepoint_t aCh)
{
    return gfxUnicodeProperties::GetCombiningClass(aCh);
}

static unsigned int
HBGetEastAsianWidth(hb_codepoint_t aCh)
{
    return gfxUnicodeProperties::GetEastAsianWidth(aCh);
}

/*
 * gfxFontShaper override to initialize the text run using HarfBuzz
 */

static hb_font_funcs_t * sHBFontFuncs = nsnull;
static hb_unicode_funcs_t * sHBUnicodeFuncs = nsnull;

PRBool
gfxHarfBuzzShaper::InitTextRun(gfxContext *aContext,
                               gfxTextRun *aTextRun,
                               const PRUnichar *aString,
                               PRUint32 aRunStart,
                               PRUint32 aRunLength,
                               PRInt32 aRunScript)
{
    // some font back-ends require this in order to get proper hinted metrics
    mFont->SetupCairoFont(aContext);

    if (!mHBFace) {
        // set up the harfbuzz face etc the first time we use the font

        if (!sHBFontFuncs) {
            // static function callback pointers, initialized by the first
            // harfbuzz shaper used
            sHBFontFuncs = hb_font_funcs_copy(hb_font_funcs_create());
            hb_font_funcs_set_glyph_func(sHBFontFuncs, HBGetGlyph);
            hb_font_funcs_set_glyph_metrics_func(sHBFontFuncs,
                                                 HBGetGlyphMetrics);
            hb_font_funcs_set_contour_point_func(sHBFontFuncs,
                                                 HBGetContourPoint);
            hb_font_funcs_set_kerning_func(sHBFontFuncs, HBGetKerning);

            sHBUnicodeFuncs = hb_unicode_funcs_copy(hb_unicode_funcs_create());
            hb_unicode_funcs_set_mirroring_func(sHBUnicodeFuncs,
                                                HBGetMirroring);
            hb_unicode_funcs_set_script_func(sHBUnicodeFuncs, HBGetScript);
            hb_unicode_funcs_set_general_category_func(sHBUnicodeFuncs,
                                                       HBGetGeneralCategory);
            hb_unicode_funcs_set_combining_class_func(sHBUnicodeFuncs,
                                                      HBGetCombiningClass);
            hb_unicode_funcs_set_eastasian_width_func(sHBUnicodeFuncs,
                                                      HBGetEastAsianWidth);
        }

        mHBFace = hb_face_create_for_tables(HBGetTable, nsnull, this);

        // get the cmap table and find offset to our subtable
        mCmapTable = mFont->GetFontTable(TRUETYPE_TAG('c','m','a','p'));
        if (!mCmapTable) {
            NS_WARNING("failed to load cmap, glyphs will be missing");
            return PR_FALSE;
        }
        const PRUint8* data = (const PRUint8*)hb_blob_lock(mCmapTable);
        PRBool symbol;
        mCmapFormat =
            gfxFontUtils::FindPreferredSubtable(data,
                                                hb_blob_get_length(mCmapTable),
                                                &mSubtableOffset,
                                                &mUVSTableOffset,
                                                &symbol);
        hb_blob_unlock(mCmapTable);

        if (!mUseHintedWidths) {
            // if font doesn't implement hinted widths, we will be reading
            // the hmtx table directly;
            // read mNumLongMetrics from hhea table without caching its blob,
            // and preload/cache the hmtx table
            hb_blob_t *hheaTable =
                mFont->GetFontTable(TRUETYPE_TAG('h','h','e','a'));
            if (hheaTable &&
                hb_blob_get_length(hheaTable) >= sizeof(HMetricsHeader)) {
                const HMetricsHeader* hhea =
                    reinterpret_cast<const HMetricsHeader*>
                        (hb_blob_lock(hheaTable));
                mNumLongMetrics = hhea->numberOfHMetrics;
                hb_blob_unlock(hheaTable);

                mHmtxTable =
                    mFont->GetFontTable(TRUETYPE_TAG('h','m','t','x'));
            }
            hb_blob_destroy(hheaTable);
        }

        if (mFont->GetStyle()->languageOverride) {
            mHBLanguage =
                hb_ot_tag_to_language(mFont->GetStyle()->languageOverride);
        } else if (mFont->GetFontEntry()->mLanguageOverride) {
            mHBLanguage =
                hb_ot_tag_to_language(mFont->GetFontEntry()->mLanguageOverride);
        } else {
            nsCString langString;
            mFont->GetStyle()->language->ToUTF8String(langString);
            mHBLanguage = hb_language_from_string(langString.get());
        }
    }

    if (mCmapFormat <= 0 || (!mUseHintedWidths && !mHmtxTable)) {
        // unable to shape with this font
        return PR_FALSE;
    }

    FontCallbackData fcd(this, aContext);
    hb_font_t *font = hb_font_create();
    hb_font_set_funcs(font, sHBFontFuncs, nsnull, &fcd);
    hb_font_set_ppem(font, mFont->GetAdjustedSize(), mFont->GetAdjustedSize());
    PRUint32 scale = FloatToFixed(mFont->GetAdjustedSize()); // 16.16 fixed-point
    hb_font_set_scale(font, scale, scale);

    // aRunStart and aRunLength define the section of the textRun and of
    // aString that is to be drawn with this particular font

    PRBool disableLigatures =
        (aTextRun->GetFlags() &
         gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES) != 0;

    nsAutoTArray<hb_feature_t,20> features;

    // Ligature features are enabled by default in the generic shaper,
    // so we explicitly turn them off if necessary (for letter-spacing)
    if (disableLigatures) {
        hb_feature_t ligaOff = { HB_TAG('l','i','g','a'), 0, 0, -1 };
        hb_feature_t cligOff = { HB_TAG('c','l','i','g'), 0, 0, -1 };
        features.AppendElement(ligaOff);
        features.AppendElement(cligOff);
    }

    // css features need to be merged with the existing ones, if any
    const nsTArray<gfxFontFeature> *cssFeatures =
        mFont->GetStyle()->featureSettings;
    if (!cssFeatures) {
        cssFeatures = mFont->GetFontEntry()->mFeatureSettings;
    }
    if (cssFeatures) {
        for (PRUint32 i = 0; i < cssFeatures->Length(); ++i) {
            PRUint32 j;
            for (j = 0; j < features.Length(); ++j) {
                if (cssFeatures->ElementAt(i).mTag == features[j].tag) {
                    features[j].value = cssFeatures->ElementAt(i).mValue;
                    break;
                }
            }
            if (j == features.Length()) {
                const gfxFontFeature& f = cssFeatures->ElementAt(i);
                hb_feature_t hbf = { f.mTag, f.mValue, 0, -1 };
                features.AppendElement(hbf);
            }
        }
    }

    hb_buffer_t *buffer = hb_buffer_create(aRunLength);
    hb_buffer_set_unicode_funcs(buffer, sHBUnicodeFuncs);
    hb_buffer_set_direction(buffer,
                            aTextRun->IsRightToLeft() ?
                                HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer, hb_script_t(aRunScript));
    hb_buffer_set_language(buffer, mHBLanguage);

    hb_buffer_add_utf16(buffer, reinterpret_cast<const uint16_t*>(aString + aRunStart),
                        aRunLength, 0, aRunLength);

    hb_shape(font, mHBFace, buffer, features.Elements(), features.Length());

    if (aTextRun->IsRightToLeft()) {
        hb_buffer_reverse(buffer);
    }

    nsresult rv = SetGlyphsFromRun(aContext, aTextRun, buffer,
                                   aRunStart, aRunLength);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "failed to store glyphs into textrun");
    hb_buffer_destroy(buffer);
    hb_font_destroy(font);

    return PR_TRUE;
}

#define SMALL_GLYPH_RUN 128 // some testing indicates that 90%+ of text runs
                            // will fit without requiring separate allocation
                            // for charToGlyphArray

nsresult
gfxHarfBuzzShaper::SetGlyphsFromRun(gfxContext *aContext,
                                    gfxTextRun *aTextRun,
                                    hb_buffer_t *aBuffer,
                                    PRUint32 aTextRunOffset,
                                    PRUint32 aRunLength)
{
    PRInt32 numGlyphs = hb_buffer_get_length(aBuffer);
    if (numGlyphs == 0)
        return NS_OK;

    const hb_glyph_info_t *ginfo = hb_buffer_get_glyph_infos(aBuffer);

    nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;

    static const PRInt32 NO_GLYPH = -1;
    nsAutoTArray<PRInt32,SMALL_GLYPH_RUN> charToGlyphArray;
    if (!charToGlyphArray.SetLength(aRunLength))
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 *charToGlyph = charToGlyphArray.Elements();
    for (PRUint32 offset = 0; offset < aRunLength; ++offset) {
        charToGlyph[offset] = NO_GLYPH;
    }

    for (PRInt32 i = 0; i < numGlyphs; ++i) {
        PRInt32 loc = ginfo[i].cluster;
        if (loc < aRunLength) {
            charToGlyph[loc] = i;
        }
    }

    nsAutoTArray<nsPoint,SMALL_GLYPH_RUN> positions;
    if (!positions.SetLength(numGlyphs))
        return NS_ERROR_OUT_OF_MEMORY;
    float runWidth = GetGlyphPositions(aContext, aBuffer, positions,
                                       aTextRun->GetAppUnitsPerDevUnit());

    PRInt32 glyphStart = 0; // looking for a clump that starts at this glyph
    PRInt32 charStart = 0; // and this char index within the range of the run

    while (glyphStart < numGlyphs) {

        PRBool inOrder = PR_TRUE;
        PRInt32 charEnd = ginfo[glyphStart].cluster;
        PRInt32 glyphEnd = glyphStart;
        PRInt32 charLimit = aRunLength;
        while (charEnd < charLimit) {
            // This is normally executed once for each iteration of the outer loop,
            // but in unusual cases where the character/glyph association is complex,
            // the initial character range might correspond to a non-contiguous
            // glyph range with "holes" in it. If so, we will repeat this loop to
            // extend the character range until we have a contiguous glyph sequence.
            charEnd += 1;
            while (charEnd != charLimit && charToGlyph[charEnd] == NO_GLYPH) {
                charEnd += 1;
            }

            // find the maximum glyph index covered by the clump so far
            for (PRInt32 i = charStart; i < charEnd; ++i) {
                if (charToGlyph[i] != NO_GLYPH) {
                    glyphEnd = PR_MAX(glyphEnd, charToGlyph[i] + 1);
                    // update extent of glyph range
                }
            }

            if (glyphEnd == glyphStart + 1) {
                // for the common case of a single-glyph clump,
                // we can skip the following checks
                break;
            }

            if (glyphEnd == glyphStart) {
                // no glyphs, try to extend the clump
                continue;
            }

            // check whether all glyphs in the range are associated with the characters
            // in our clump; if not, we have a discontinuous range, and should extend it
            // unless we've reached the end of the text
            PRBool allGlyphsAreWithinCluster = PR_TRUE;
            PRInt32 prevGlyphCharIndex = charStart - 1;
            for (PRInt32 i = glyphStart; i < glyphEnd; ++i) {
                PRInt32 glyphCharIndex = ginfo[i].cluster;
                if (glyphCharIndex < charStart || glyphCharIndex >= charEnd) {
                    allGlyphsAreWithinCluster = PR_FALSE;
                    break;
                }
                if (glyphCharIndex <= prevGlyphCharIndex) {
                    inOrder = PR_FALSE;
                }
                prevGlyphCharIndex = glyphCharIndex;
            }
            if (allGlyphsAreWithinCluster) {
                break;
            }
        }

        NS_ASSERTION(glyphStart < glyphEnd,
                     "character/glyph clump contains no glyphs!");
        NS_ASSERTION(charStart != charEnd,
                     "character/glyph clump contains no characters!");

        // Now charStart..charEnd is a ligature clump, corresponding to glyphStart..glyphEnd;
        // Set baseCharIndex to the char we'll actually attach the glyphs to (1st of ligature),
        // and endCharIndex to the limit (position beyond the last char),
        // adjusting for the offset of the stringRange relative to the textRun.
        PRInt32 baseCharIndex, endCharIndex;
        while (charEnd < aRunLength && charToGlyph[charEnd] == NO_GLYPH)
            charEnd++;
        baseCharIndex = charStart;
        endCharIndex = charEnd;

        // Then we check if the clump falls outside our actual string range;
        // if so, just go to the next.
        if (baseCharIndex >= aRunLength) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }
        // Ensure we won't try to go beyond the valid length of the textRun's text
        endCharIndex = PR_MIN(endCharIndex, aRunLength);

        // Now we're ready to set the glyph info in the textRun;
        // measure the glyph width of the first (perhaps only) glyph,
        // to see if it is "Simple"
        nscoord advance;
        if (glyphStart < numGlyphs-1) {
            advance = positions[glyphStart+1].x - positions[glyphStart].x;
        } else {
            advance = positions[0].x + runWidth - positions[glyphStart].x;
        }

        PRInt32 glyphsInClump = glyphEnd - glyphStart;

        // Check for default-ignorable char that didn't get filtered, combined,
        // etc by the shaping process, and remove from the run.
        // (This may be done within harfbuzz eventually.)
        if (glyphsInClump == 1 && baseCharIndex + 1 == endCharIndex &&
            aTextRun->FilterIfIgnorable(aTextRunOffset + baseCharIndex)) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }

        // Check if it's a simple one-to-one mapping
        if (glyphsInClump == 1 &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(ginfo[glyphStart].codepoint) &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            aTextRun->IsClusterStart(aTextRunOffset + baseCharIndex) &&
            positions[glyphStart].y == 0)
        {
            gfxTextRun::CompressedGlyph g;
            aTextRun->SetSimpleGlyph(aTextRunOffset + baseCharIndex,
                                     g.SetSimpleGlyph(advance,
                                         ginfo[glyphStart].codepoint));
        } else {
            // collect all glyphs in a list to be assigned to the first char;
            // there must be at least one in the clump, and we already measured
            // its advance, hence the placement of the loop-exit test and the
            // measurement of the next glyph
            while (1) {
                gfxTextRun::DetailedGlyph* details =
                    detailedGlyphs.AppendElement();
                details->mGlyphID = ginfo[glyphStart].codepoint;
                details->mXOffset = 0.0f;
                details->mYOffset = -positions[glyphStart].y;
                details->mAdvance = advance;
                if (++glyphStart >= glyphEnd)
                    break;
                if (glyphStart < numGlyphs-1) {
                    advance = positions[glyphStart+1].x -
                                  positions[glyphStart].x;
                } else {
                    advance = positions[0].x + runWidth -
                                  positions[glyphStart].x;
                }
            }

            gfxTextRun::CompressedGlyph g;
            g.SetComplex(aTextRun->IsClusterStart(aTextRunOffset + baseCharIndex),
                         PR_TRUE, detailedGlyphs.Length());
            aTextRun->SetGlyphs(aTextRunOffset + baseCharIndex,
                                g, detailedGlyphs.Elements());

            detailedGlyphs.Clear();
        }

        // the rest of the chars in the group are ligature continuations,
        // no associated glyphs
        while (++baseCharIndex != endCharIndex && baseCharIndex < aRunLength) {
            gfxTextRun::CompressedGlyph g;
            g.SetComplex(inOrder &&
                         aTextRun->IsClusterStart(aTextRunOffset + baseCharIndex),
                         PR_FALSE, 0);
            aTextRun->SetGlyphs(aTextRunOffset + baseCharIndex, g, nsnull);
        }

        glyphStart = glyphEnd;
        charStart = charEnd;
    }

    return NS_OK;
}

nscoord
gfxHarfBuzzShaper::GetGlyphPositions(gfxContext *aContext,
                                     hb_buffer_t *aBuffer,
                                     nsTArray<nsPoint>& aPositions,
                                     PRUint32 aAppUnitsPerDevUnit)
{
    nsPoint *outPos = aPositions.Elements();
    const nsPoint *limit = outPos + hb_buffer_get_length(aBuffer);

    nsPoint curPos(0, 0);

    // factor to convert 16.16 fixed-point pixels to app units
    float hb2appUnits = aAppUnitsPerDevUnit / 65536.0;

    const hb_glyph_position_t *posInfo = hb_buffer_get_glyph_positions(aBuffer);
    while (outPos < limit) {
        outPos->x = posInfo->x_offset == 0 ? curPos.x :
            curPos.x + NS_roundf(hb2appUnits * posInfo->x_offset);
        outPos->y = posInfo->y_offset == 0 ? curPos.y :
            curPos.y + NS_roundf(hb2appUnits * posInfo->y_offset);
        curPos.x += NS_roundf(hb2appUnits * posInfo->x_advance);
        if (posInfo->y_advance) {
            curPos.y += NS_roundf(hb2appUnits * posInfo->y_advance);
        }
        ++posInfo;
        ++outPos;
    }

    return curPos.x;
}
