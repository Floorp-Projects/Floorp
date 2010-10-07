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
#include "gfxDWriteShaper.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxDWriteFontList.h"
#include "gfxContext.h"
#include <dwrite.h>

#include "gfxDWriteTextAnalysis.h"

#include "harfbuzz/hb-blob.h"

// Chosen this as to resemble DWrite's own oblique face style.
#define OBLIQUE_SKEW_FACTOR 0.3

// This is also in gfxGDIFont.cpp. Would be nice to put it somewhere common,
// but we can't declare it in the gfxFont.h or gfxFontUtils.h headers
// because those are exported, and the cairo headers aren't.
static inline cairo_antialias_t
GetCairoAntialiasOption(gfxFont::AntialiasOption anAntialiasOption)
{
    switch (anAntialiasOption) {
    default:
    case gfxFont::kAntialiasDefault:
        return CAIRO_ANTIALIAS_DEFAULT;
    case gfxFont::kAntialiasNone:
        return CAIRO_ANTIALIAS_NONE;
    case gfxFont::kAntialiasGrayscale:
        return CAIRO_ANTIALIAS_GRAY;
    case gfxFont::kAntialiasSubpixel:
        return CAIRO_ANTIALIAS_SUBPIXEL;
    }
}

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFont
gfxDWriteFont::gfxDWriteFont(gfxFontEntry *aFontEntry,
                             const gfxFontStyle *aFontStyle,
                             PRBool aNeedsBold,
                             AntialiasOption anAAOption)
    : gfxFont(aFontEntry, aFontStyle, anAAOption)
    , mCairoFontFace(nsnull)
    , mCairoScaledFont(nsnull)
    , mNeedsOblique(PR_FALSE)
    , mNeedsBold(aNeedsBold)
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

    if (FontCanSupportHarfBuzz()) {
        mHarfBuzzShaper = new gfxHarfBuzzShaper(this);
    }
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

gfxFont*
gfxDWriteFont::CopyWithAntialiasOption(AntialiasOption anAAOption)
{
    return new gfxDWriteFont(static_cast<gfxDWriteFontEntry*>(mFontEntry.get()),
                             &mStyle, mNeedsBold, anAAOption);
}

void
gfxDWriteFont::CreatePlatformShaper()
{
    mPlatformShaper = new gfxDWriteShaper(this);
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

    mMetrics.maxAscent = 
        ceil(((gfxFloat)fontMetrics.ascent /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize);
    mMetrics.maxDescent = 
        ceil(((gfxFloat)fontMetrics.descent /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize);
    mMetrics.maxHeight = mMetrics.maxAscent + mMetrics.maxDescent;

    mMetrics.emHeight = mAdjustedSize;
    mMetrics.emAscent = mMetrics.emHeight *
        mMetrics.maxAscent / mMetrics.maxHeight;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    mMetrics.maxAdvance = mAdjustedSize;

    // try to get the true maxAdvance value from 'hhea'
    PRUint8 *tableData;
    PRUint32 len;
    void *tableContext = NULL;
    BOOL exists;
    HRESULT hr =
        mFontFace->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('h', 'h', 'e', 'a'),
                                   (const void**)&tableData,
                                   &len,
                                   &tableContext,
                                   &exists);
    if (SUCCEEDED(hr)) {
        if (exists && len >= sizeof(mozilla::HheaTable)) {
            const mozilla::HheaTable* hhea =
                reinterpret_cast<const mozilla::HheaTable*>(tableData);
            mMetrics.maxAdvance = ((gfxFloat)PRUint16(hhea->advanceWidthMax) /
                       fontMetrics.designUnitsPerEm) * mAdjustedSize;
        }
        mFontFace->ReleaseFontTable(tableContext);
    }

    mMetrics.internalLeading = 
        ceil(((gfxFloat)(fontMetrics.ascent + 
                    fontMetrics.descent - 
                    fontMetrics.designUnitsPerEm) / 
                    fontMetrics.designUnitsPerEm) * mAdjustedSize);
    mMetrics.externalLeading = 
        ceil(((gfxFloat)fontMetrics.lineGap /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize);

    UINT16 glyph = (PRUint16)GetSpaceGlyph();
    DWRITE_GLYPH_METRICS metrics;
    mFontFace->GetDesignGlyphMetrics(&glyph, 1, &metrics);
    mMetrics.spaceWidth = 
        ((gfxFloat)metrics.advanceWidth /
                   fontMetrics.designUnitsPerEm) * mAdjustedSize;

    // try to get aveCharWidth from the OS/2 table, fall back to measuring 'x'
    // if the table is not available
    mMetrics.aveCharWidth = 0;
    hr = mFontFace->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('O', 'S', '/', '2'),
                                    (const void**)&tableData,
                                    &len,
                                    &tableContext,
                                    &exists);
    if (SUCCEEDED(hr)) {
        if (exists && len >= 4) {
            // Not checking against sizeof(mozilla::OS2Table) here because older
            // versions of the table have different sizes; we only need the first
            // two 16-bit fields here.
            const mozilla::OS2Table* os2 =
                reinterpret_cast<const mozilla::OS2Table*>(tableData);
            mMetrics.aveCharWidth = ((gfxFloat)PRInt16(os2->xAvgCharWidth) /
                       fontMetrics.designUnitsPerEm) * mAdjustedSize;
        }
        mFontFace->ReleaseFontTable(tableContext);
    }

    UINT32 ucs;
    if (mMetrics.aveCharWidth < 1) {
        ucs = L'x';
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
    }

    ucs = L'0';
    if (SUCCEEDED(mFontFace->GetGlyphIndicesA(&ucs, 1, &glyph)) &&
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
    mMetrics.superscriptOffset = 0;
    mMetrics.subscriptOffset = 0;

    mFUnitsConvFactor = GetAdjustedSize() / fontMetrics.designUnitsPerEm;

    SanitizeMetrics(&mMetrics, GetFontEntry()->mIsBadUnderlineFont);

#if 0
    printf("Font: %p (%s) size: %f\n", this,
           NS_ConvertUTF16toUTF8(GetName()).get(), mStyle.size);
    printf("    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    printf("    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    printf("    internalLeading: %f externalLeading: %f\n", mMetrics.internalLeading, mMetrics.externalLeading);
    printf("    spaceWidth: %f aveCharWidth: %f zeroOrAve: %f xHeight: %f\n",
           mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.zeroOrAveCharWidth, mMetrics.xHeight);
    printf("    uOff: %f uSize: %f stOff: %f stSize: %f supOff: %f subOff: %f\n",
           mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize,
           mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif
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

        if (mAntialiasOption != kAntialiasDefault) {
            cairo_font_options_set_antialias(fontOptions,
                GetCairoAntialiasOption(mAntialiasOption));
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

// Access to font tables packaged in hb_blob_t form

// object attached to the Harfbuzz blob, used to release
// the table when the blob is destroyed
class FontTableRec {
public:
    FontTableRec(IDWriteFontFace *aFontFace, void *aContext)
        : mFontFace(aFontFace), mContext(aContext)
    { }

    ~FontTableRec() {
        mFontFace->ReleaseFontTable(mContext);
    }

private:
    IDWriteFontFace *mFontFace;
    void            *mContext;
};

/*static*/ void
gfxDWriteFont::DestroyBlobFunc(void* aUserData)
{
    FontTableRec *ftr = static_cast<FontTableRec*>(aUserData);
    delete ftr;
}

hb_blob_t *
gfxDWriteFont::GetFontTable(PRUint32 aTag)
{
    const void *data;
    UINT32      size;
    void       *context;
    BOOL        exists;
    HRESULT hr = mFontFace->TryGetFontTable(NS_SWAP32(aTag),
                                            &data, &size, &context, &exists);
    if (SUCCEEDED(hr) && exists) {
        FontTableRec *ftr = new FontTableRec(mFontFace, context);
        return hb_blob_create(static_cast<const char*>(data), size,
                              HB_MEMORY_MODE_READONLY,
                              DestroyBlobFunc, ftr);
    }

    if (mFontEntry->IsUserFont() && !mFontEntry->IsLocalUserFont()) {
        // for downloaded fonts, there may be layout tables cached in the entry
        // even though they're absent from the sanitized platform font
        return mFontEntry->GetFontTable(aTag);
    }

    return nsnull;
}
