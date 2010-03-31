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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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

#include "gfxDWriteFonts.h"
#include "gfxDWriteFontList.h"
#include "gfxContext.h"
#include <dwrite.h>

#include "gfxDWriteTextAnalysis.h"

// Chosen this as to resemble DWrite's own oblique face style.
#define OBLIQUE_SKEW_FACTOR 0.3

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFont
gfxDWriteFont::gfxDWriteFont(gfxFontEntry *aFontEntry,
                             const gfxFontStyle *aFontStyle,
                             PRBool aNeedsBold)
    : gfxFont(aFontEntry, aFontStyle)
    , mAdjustedSize(0.0f)
    , mCairoFontFace(nsnull)
    , mCairoScaledFont(nsnull)
    , mNeedsOblique(PR_FALSE)
{
    gfxDWriteFontEntry *fe =
        static_cast<gfxDWriteFontEntry*>(aFontEntry);
    nsresult rv;
    DWRITE_FONT_SIMULATIONS sims = DWRITE_FONT_SIMULATIONS_NONE;
    if ((GetStyle()->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) &&
        !fe->IsItalic()) {
            // For this we always use the font_matrix for uniformity. Not the
            // DWrite simulation.
            mNeedsOblique = PR_TRUE;
    }
    PRInt8 baseWeight, weightDistance;
    GetStyle()->ComputeWeightAndOffset(&baseWeight, &weightDistance);
    if (aNeedsBold) {
        sims |= DWRITE_FONT_SIMULATIONS_BOLD;
    }

    rv = fe->CreateFontFace(getter_AddRefs(mFontFace), sims);

    if (NS_FAILED(rv)) {
        mIsValid = PR_FALSE;
        return;
    }

    ComputeMetrics();
}

gfxDWriteFont::~gfxDWriteFont()
{
    if (mCairoFontFace) {
        cairo_font_face_destroy(mCairoFontFace);
    }
    if (mCairoScaledFont) {
        cairo_scaled_font_destroy(mCairoScaledFont);
    }
}

nsString
gfxDWriteFont::GetUniqueName()
{
    return mFontEntry->Name();
}

const gfxFont::Metrics&
gfxDWriteFont::GetMetrics()
{
    return mMetrics;
}

void
gfxDWriteFont::ComputeMetrics()
{
    DWRITE_FONT_METRICS fontMetrics;
    mFontFace->GetMetrics(&fontMetrics);

    if (mStyle.sizeAdjust != 0.0) {
        gfxFloat aspect = (gfxFloat)fontMetrics.xHeight /
                   fontMetrics.designUnitsPerEm;
        mAdjustedSize = mStyle.GetAdjustedSize(aspect);
    } else {
        mAdjustedSize = mStyle.size;
    }

    mMetrics.xHeight =
        ((gfxFloat)fontMetrics.xHeight /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize;
    mMetrics.emAscent = 
        ((gfxFloat)fontMetrics.ascent /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize;
    mMetrics.emDescent = 
        ((gfxFloat)fontMetrics.descent /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize;
    mMetrics.emHeight = mAdjustedSize;
    mMetrics.maxAscent = mMetrics.emAscent;
    mMetrics.maxDescent = mMetrics.emDescent;
    mMetrics.maxHeight = mMetrics.emHeight;
    mMetrics.maxAdvance = mAdjustedSize;
    mMetrics.internalLeading = 
        ((gfxFloat)(fontMetrics.ascent + 
                    fontMetrics.descent - 
                    fontMetrics.designUnitsPerEm) / 
                    fontMetrics.designUnitsPerEm) * mAdjustedSize;
    
    mMetrics.externalLeading = 
        ((gfxFloat)fontMetrics.lineGap /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize;

    UINT16 glyph = (PRUint16)GetSpaceGlyph();
    DWRITE_GLYPH_METRICS metrics;
    mFontFace->GetDesignGlyphMetrics(&glyph, 1, &metrics);
    mMetrics.spaceWidth = 
        ((gfxFloat)metrics.advanceWidth /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize;
    UINT32 ucs = L'x';
    if (SUCCEEDED(mFontFace->GetGlyphIndicesA(&ucs, 1, &glyph)) &&
        SUCCEEDED(mFontFace->GetDesignGlyphMetrics(&glyph, 1, &metrics))) {    
        mMetrics.aveCharWidth = 
            ((gfxFloat)metrics.advanceWidth /
                       fontMetrics.designUnitsPerEm) * mAdjustedSize;
    } else {
        // Let's just assume the X is square.
        mMetrics.aveCharWidth = 
            ((gfxFloat)fontMetrics.xHeight /
                       fontMetrics.designUnitsPerEm) * mAdjustedSize;
    }
    ucs = L'0';
    if (FAILED(mFontFace->GetGlyphIndicesA(&ucs, 1, &glyph)) &&
        SUCCEEDED(mFontFace->GetDesignGlyphMetrics(&glyph, 1, &metrics))) {
        mMetrics.zeroOrAveCharWidth = 
            ((gfxFloat)metrics.advanceWidth /
                       fontMetrics.designUnitsPerEm) * mAdjustedSize;
    } else {
        mMetrics.zeroOrAveCharWidth = mMetrics.aveCharWidth;
    }
    mMetrics.underlineOffset = 
        ((gfxFloat)fontMetrics.underlinePosition /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize;
    mMetrics.underlineSize = 
        ((gfxFloat)fontMetrics.underlineThickness /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize;
    mMetrics.strikeoutOffset = 
        ((gfxFloat)fontMetrics.strikethroughPosition /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize;
    mMetrics.strikeoutSize = 
        ((gfxFloat)fontMetrics.strikethroughThickness /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize;
    mMetrics.subscriptOffset = 0;
    mMetrics.subscriptOffset = 0;

    SanitizeMetrics(&mMetrics, PR_FALSE);
 }

PRUint32
gfxDWriteFont::GetSpaceGlyph()
{
    UINT32 ucs = L' ';
    UINT16 glyph;
    HRESULT hr;
    hr = mFontFace->GetGlyphIndicesA(&ucs, 1, &glyph);
    if (FAILED(hr)) {
        return 0;
    }
    return glyph;
}

PRBool
gfxDWriteFont::SetupCairoFont(gfxContext *aContext)
{
    cairo_scaled_font_t *scaledFont = CairoScaledFont();
    if (cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(aContext->GetCairo(), scaledFont);
    return PR_TRUE;
}

cairo_font_face_t *
gfxDWriteFont::CairoFontFace()
{
    if (!mCairoFontFace) {
#ifdef CAIRO_HAS_DWRITE_FONT
        mCairoFontFace = 
            cairo_dwrite_font_face_create_for_dwrite_fontface(
            ((gfxDWriteFontEntry*)mFontEntry.get())->mFont, mFontFace);
#endif
    }
    return mCairoFontFace;
}


cairo_scaled_font_t *
gfxDWriteFont::CairoScaledFont()
{
    if (!mCairoScaledFont) {
        cairo_matrix_t sizeMatrix;
        cairo_matrix_t identityMatrix;

        cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);
        cairo_matrix_init_identity(&identityMatrix);

        cairo_font_options_t *fontOptions = cairo_font_options_create();
        if (mNeedsOblique) {
            double skewfactor = OBLIQUE_SKEW_FACTOR;

            cairo_matrix_t style;
            cairo_matrix_init(&style,
                              1,                //xx
                              0,                //yx
                              -1 * skewfactor,  //xy
                              1,                //yy
                              0,                //x0
                              0);               //y0
            cairo_matrix_multiply(&sizeMatrix, &sizeMatrix, &style);
        }

        mCairoScaledFont = cairo_scaled_font_create(CairoFontFace(),
                                                    &sizeMatrix,
                                                    &identityMatrix,
                                                    fontOptions);
        cairo_font_options_destroy(fontOptions);
    }

    NS_ASSERTION(mAdjustedSize == 0.0 ||
                 cairo_scaled_font_status(mCairoScaledFont) 
                   == CAIRO_STATUS_SUCCESS,
                 "Failed to make scaled font");

    return mCairoScaledFont;
}

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFontGroup
gfxDWriteFontGroup::gfxDWriteFontGroup(const nsAString& aFamilies,
                                       const gfxFontStyle *aStyle,
                                       gfxUserFontSet *aUserFontSet)
    : gfxFontGroup(aFamilies, aStyle, aUserFontSet)
{
}

gfxDWriteFontGroup::~gfxDWriteFontGroup()
{
}

gfxDWriteFont *
gfxDWriteFontGroup::GetFontAt(PRInt32 i) 
{
    // If it turns out to be hard for all clients that cache font
    // groups to call UpdateFontList at appropriate times, we could
    // instead consider just calling UpdateFontList from someplace
    // more central (such as here).
    NS_ASSERTION(!mUserFontSet || mCurrGeneration == GetGeneration(),
                 "Whoever was caching this font group should have "
                 "called UpdateFontList on it");
    NS_ASSERTION(mFonts.Length() > PRUint32(i), 
                 "Requesting a font index that doesn't exist");

    return static_cast<gfxDWriteFont*>(mFonts[i].get());
}

gfxFontGroup *
gfxDWriteFontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxDWriteFontGroup(mFamilies, aStyle, mUserFontSet);
}

#define MAX_RANGE_LENGTH 25000
gfxTextRun *
gfxDWriteFontGroup::MakeTextRun(const PRUnichar *aString,
                                PRUint32 aLength,
                                const Parameters *aParams,
                                PRUint32 aFlags)
{
    HRESULT hr;
    // TODO: Handle TEST_DISABLE_OPTIONAL_LIGATURES

    gfxTextRun *textRun = gfxTextRun::Create(aParams,
                                             aString,
                                             aLength,
                                             this,
                                             aFlags);

    gfxPlatform::GetPlatform()->SetupClusterBoundaries(textRun, aString);

    DWRITE_READING_DIRECTION readingDirection = 
        DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    if (textRun->IsRightToLeft()) {
        readingDirection = DWRITE_READING_DIRECTION_RIGHT_TO_LEFT;
    }
    nsRefPtr<gfxDWriteFont> font = GetFontAt(0);

    textRun->AddGlyphRun(font, 0);
    gfxTextRun::CompressedGlyph g;

    nsRefPtr<IDWriteTextAnalyzer> analyzer;

    hr = gfxWindowsPlatform::GetPlatform()->GetDWriteFactory()->
        CreateTextAnalyzer(getter_AddRefs(analyzer));

    nsTArray<gfxTextRange> ranges;

    ComputeRanges(ranges, aString, 0, aLength);

    /**
     * There's an internal 16-bit limit on some things inside the analyzer.
     * to be on the safe side here we split up any ranges over MAX_RANGE_LENGTH 
     * characters.
     * TODO: Figure out what exactly is going on, and what is a safe number, and 
     * why.
     * TODO: Figure out good places to split this.
     */
    for (unsigned int i = 0; i < ranges.Length(); i++) {
        if (ranges[i].Length() > MAX_RANGE_LENGTH) {
            ranges.InsertElementAt(i + 1, 
                                   gfxTextRange(ranges[i].start 
                                     + MAX_RANGE_LENGTH,
                                   ranges[i].end));
            ranges[i + 1].font = ranges[i].font;
            ranges[i].end = ranges[i].start + MAX_RANGE_LENGTH;
        }
    }
    UINT32 rangeOffset = 0;
    for (unsigned int i = 0; i < ranges.Length(); i++) {
        gfxTextRange &range = ranges[i];
        TextAnalysis analysis(
            aString + range.start, range.Length(),
            NULL, 
            readingDirection);
        TextAnalysis::Run *runHead;
        DWRITE_LINE_BREAKPOINT *linebreaks;
        hr = analysis.GenerateResults(analyzer, &runHead, &linebreaks);

        if (FAILED(hr)) {
            NS_WARNING("Analyzer failed to generate results.");
            continue;
        }

        if (range.font) {
            font = static_cast<gfxDWriteFont*>(range.font.get());
        } else {
            // XXX missing glyph info needs to be set
            font = GetFontAt(0);
        }

        textRun->AddGlyphRun(font, range.start);

        PRUint32 appUnitsPerDevPixel = textRun->GetAppUnitsPerDevUnit();

        UINT32 maxGlyphs = 0;
trymoreglyphs:
        if ((PR_UINT32_MAX - 3 * range.Length() / 2 + 16) < maxGlyphs) {
            // This isn't going to work, we're going to cross the UINT32 upper
            // limit. Next range it is.
            continue;
        }
        maxGlyphs += 3 * range.Length() / 2 + 16;

        nsAutoTArray<UINT16, 400> clusters;
        nsAutoTArray<UINT16, 400> indices;
        nsAutoTArray<DWRITE_SHAPING_TEXT_PROPERTIES, 400> textProperties;
        nsAutoTArray<DWRITE_SHAPING_GLYPH_PROPERTIES, 400> glyphProperties;
        if (!clusters.SetLength(range.Length()) ||
            !indices.SetLength(maxGlyphs) || 
            !textProperties.SetLength(maxGlyphs) ||
            !glyphProperties.SetLength(maxGlyphs)) {
                continue;
        }

        UINT32 actualGlyphs;

        hr = analyzer->GetGlyphs(aString + range.start, range.Length(),
            font->mFontFace, FALSE, 
            readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT,
            &runHead->mScript, NULL, NULL, NULL, NULL, 0,
            maxGlyphs, clusters.Elements(), textProperties.Elements(),
            indices.Elements(), glyphProperties.Elements(), &actualGlyphs);

        if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
            // We increase the amount of glyphs and try again.
            goto trymoreglyphs;
        }
        if (FAILED(hr)) {
            NS_WARNING("Analyzer failed to get glyphs.");
            continue;
        }

        WORD gID = indices[0];
        nsAutoTArray<FLOAT, 400> advances;
        nsAutoTArray<DWRITE_GLYPH_OFFSET, 400> glyphOffsets;
        if (!advances.SetLength(actualGlyphs) || 
            !glyphOffsets.SetLength(actualGlyphs)) {
            continue;
        }

        hr = analyzer->GetGlyphPlacements(aString + range.start, 
                                          clusters.Elements(),
                                          textProperties.Elements(),
                                          range.Length(),
                                          indices.Elements(),
                                          glyphProperties.Elements(),
                                          actualGlyphs,
                                          font->mFontFace,
                                          (float)font->GetAdjustedSize(),
                                          FALSE,
                                          FALSE,
                                          &runHead->mScript,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0,
                                          advances.Elements(),
                                          glyphOffsets.Elements());
        if (FAILED(hr)) {
            NS_WARNING("Analyzer failed to get glyph placements.");
            continue;
        }

        nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;

        for (unsigned int c = 0; c < range.Length(); c++) {
            PRUint32 k = clusters[c];
            PRUint32 absC = range.start + c;

            if (c > 0 && k == clusters[c - 1]) {
                g.SetComplex(textRun->IsClusterStart(absC), PR_FALSE, 0);
                textRun->SetGlyphs(absC, g, nsnull);
                // This is a cluster continuation. No glyph here.
                continue;
            }

            // Count glyphs for this character
            PRUint32 glyphCount = actualGlyphs - k;
            PRUint32 nextClusterOffset;
            for (nextClusterOffset = c + 1; 
                nextClusterOffset < range.Length(); ++nextClusterOffset) {
                if (clusters[nextClusterOffset] > k) {
                    glyphCount = clusters[nextClusterOffset] - k;
                    break;
                }
            }
            PRInt32 advance = 
                (PRInt32)(advances[k] * aParams->mAppUnitsPerDevUnit);
            if (glyphCount == 1 && advance >= 0 &&
                glyphOffsets[k].advanceOffset == 0 &&
                glyphOffsets[k].ascenderOffset == 0 &&
                textRun->IsClusterStart(absC) &&
                gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
                gfxTextRun::CompressedGlyph::IsSimpleGlyphID(indices[k])) {
                  textRun->SetSimpleGlyph(absC, 
                                          g.SetSimpleGlyph(advance, 
                                                           indices[k]));
            } else {
                if (detailedGlyphs.Length() < glyphCount) {
                    if (!detailedGlyphs.AppendElements(
                        glyphCount - detailedGlyphs.Length())) {
                        continue;
                    }
                }
                float totalAdvance = 0;
                for (unsigned int z = 0; z < glyphCount; z++) {
                    detailedGlyphs[z].mGlyphID = indices[k + z];
                    detailedGlyphs[z].mAdvance = 
                        (PRInt32)(advances[k + z]
                           * aParams->mAppUnitsPerDevUnit);
                    if (readingDirection == 
                        DWRITE_READING_DIRECTION_RIGHT_TO_LEFT) {
                        detailedGlyphs[z].mXOffset = 
                            (totalAdvance + 
                              glyphOffsets[k + z].advanceOffset)
                            * aParams->mAppUnitsPerDevUnit;
                    } else {
                        detailedGlyphs[z].mXOffset = 
                            glyphOffsets[k + z].advanceOffset *
                            aParams->mAppUnitsPerDevUnit;
                    }
                    detailedGlyphs[z].mYOffset = 
                        -glyphOffsets[k + z].ascenderOffset *
                        aParams->mAppUnitsPerDevUnit;
                    totalAdvance += advances[k + z];
                }
                textRun->SetGlyphs(
                    absC,
                    g.SetComplex(textRun->IsClusterStart(absC),
                                 PR_TRUE,
                                 glyphCount),
                    detailedGlyphs.Elements());
            }
        }
    }

    return textRun;
}

gfxTextRun *
gfxDWriteFontGroup::MakeTextRun(const PRUint8 *aString,
                                PRUint32 aLength,
                                const Parameters *aParams,
                                PRUint32 aFlags)
{
    nsCString string((const char*)aString, aLength);
    return MakeTextRun(NS_ConvertASCIItoUTF16(string).get(),
                       aLength,
                       aParams,
                       aFlags);
}
