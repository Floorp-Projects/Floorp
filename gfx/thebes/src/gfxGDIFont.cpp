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
 * Portions created by the Initial Developer are Copyright (C) 2005-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   John Daggett <jdaggett@mozilla.com>
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

#include "gfxGDIFont.h"
#include "gfxGDIShaper.h"
#include "gfxUniscribeShaper.h"
#include "gfxWindowsPlatform.h"
#include "gfxContext.h"

#include "cairo-win32.h"

#define ROUND(x) floor((x) + 0.5)

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

gfxGDIFont::gfxGDIFont(GDIFontEntry *aFontEntry,
                       const gfxFontStyle *aFontStyle,
                       PRBool aNeedsBold,
                       AntialiasOption anAAOption)
    : gfxFont(aFontEntry, aFontStyle, anAAOption),
      mNeedsBold(aNeedsBold),
      mFont(NULL),
      mFontFace(nsnull),
      mScaledFont(nsnull),
      mAdjustedSize(0.0)
{
    // InitMetrics will handle the sizeAdjust factor and set mAdjustedSize,
    // fill in our mLogFont structure and create mFont
    InitMetrics();
    if (!mIsValid) {
        return;
    }

    mFontFace = cairo_win32_font_face_create_for_logfontw_hfont(&mLogFont,
                                                                mFont);

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    if (anAAOption != kAntialiasDefault) {
        cairo_font_options_set_antialias(fontOptions,
                                         GetCairoAntialiasOption(anAAOption));
    }
    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix,
                                           &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);

    cairo_status_t cairoerr = cairo_scaled_font_status(mScaledFont);
    if (cairoerr != CAIRO_STATUS_SUCCESS) {
        mIsValid = PR_FALSE;
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Failed to create scaled font: %s status: %d",
                NS_ConvertUTF16toUTF8(mFontEntry->Name()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
    }

    if (aFontEntry->mForceGDI) {
        mShaper = new gfxGDIShaper(this);
    } else {
        mShaper = new gfxUniscribeShaper(this);
    }
}

gfxGDIFont::~gfxGDIFont()
{
    if (mScaledFont) {
        cairo_scaled_font_destroy(mScaledFont);
    }
    if (mFontFace) {
        cairo_font_face_destroy(mFontFace);
    }
    if (mFont) {
        ::DeleteObject(mFont);
    }
}

gfxFont*
gfxGDIFont::CopyWithAntialiasOption(AntialiasOption anAAOption)
{
    return new gfxGDIFont(static_cast<GDIFontEntry*>(mFontEntry.get()),
                          &mStyle, mNeedsBold, anAAOption);
}

void
gfxGDIFont::InitTextRun(gfxContext *aContext,
                        gfxTextRun *aTextRun,
                        const PRUnichar *aString,
                        PRUint32 aRunStart,
                        PRUint32 aRunLength)
{
    PRBool ok = mShaper->InitTextRun(aContext, aTextRun, aString,
                                     aRunStart, aRunLength);
    if (!ok) {
        // shaping failed; if we were using uniscribe, fall back to GDI
        GDIFontEntry *fe = static_cast<GDIFontEntry*>(GetFontEntry());
        if (!fe->mForceGDI) {
            NS_WARNING("uniscribe failed, switching to GDI shaper");
            fe->mForceGDI = PR_TRUE;
            mShaper = new gfxGDIShaper(this);
            ok = mShaper->InitTextRun(aContext, aTextRun, aString,
                                      aRunStart, aRunLength);
        }
    }
    NS_WARN_IF_FALSE(ok, "shaper failed, expect broken or missing text");
}

const gfxFont::Metrics&
gfxGDIFont::GetMetrics()
{
    return mMetrics;
}

PRUint32
gfxGDIFont::GetSpaceGlyph()
{
    return mSpaceGlyph;
}

PRBool
gfxGDIFont::SetupCairoFont(gfxContext *aContext)
{
    if (cairo_scaled_font_status(mScaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(aContext->GetCairo(), mScaledFont);
    return PR_TRUE;
}

void
gfxGDIFont::InitMetrics()
{
    if (mAdjustedSize == 0.0) {
        mAdjustedSize = mStyle.size;
        if (mStyle.sizeAdjust != 0.0 && mAdjustedSize > 0.0) {
            // to implement font-size-adjust, we first create the "unadjusted" font
            FillLogFont(mAdjustedSize);
            mFont = ::CreateFontIndirectW(&mLogFont);

            // initialize its metrics, then delete the font
            InitMetrics();
            ::DeleteObject(mFont);
            mFont = nsnull;

            // calculate the properly adjusted size, and then proceed
            gfxFloat aspect = mMetrics.xHeight / mMetrics.emHeight;
            mAdjustedSize = mStyle.GetAdjustedSize(aspect);
        }
    }

    if (!mFont) {
        FillLogFont(mAdjustedSize);
        mFont = ::CreateFontIndirectW(&mLogFont);
    }

    AutoDC dc;
    SetGraphicsMode(dc.GetDC(), GM_ADVANCED);
    AutoSelectFont selectFont(dc.GetDC(), mFont);

    // Get font metrics
    OUTLINETEXTMETRIC oMetrics;
    TEXTMETRIC& metrics = oMetrics.otmTextMetrics;

    if (0 < GetOutlineTextMetrics(dc.GetDC(), sizeof(oMetrics), &oMetrics)) {
        mMetrics.superscriptOffset = (double)oMetrics.otmptSuperscriptOffset.y;
        // Some fonts have wrong sign on their subscript offset, bug 410917.
        mMetrics.subscriptOffset = fabs((double)oMetrics.otmptSubscriptOffset.y);
        mMetrics.strikeoutSize = (double)oMetrics.otmsStrikeoutSize;
        mMetrics.strikeoutOffset = (double)oMetrics.otmsStrikeoutPosition;
        mMetrics.underlineSize = (double)oMetrics.otmsUnderscoreSize;
        mMetrics.underlineOffset = (double)oMetrics.otmsUnderscorePosition;

        const MAT2 kIdentityMatrix = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
        GLYPHMETRICS gm;
        DWORD len = GetGlyphOutlineW(dc.GetDC(), PRUnichar('x'), GGO_METRICS, &gm, 0, nsnull, &kIdentityMatrix);
        if (len == GDI_ERROR || gm.gmptGlyphOrigin.y <= 0) {
            // 56% of ascent, best guess for true type
            mMetrics.xHeight = ROUND((double)metrics.tmAscent * 0.56);
        } else {
            mMetrics.xHeight = gm.gmptGlyphOrigin.y;
        }
        mMetrics.emHeight = metrics.tmHeight - metrics.tmInternalLeading;
        gfxFloat typEmHeight = (double)oMetrics.otmAscent - (double)oMetrics.otmDescent;
        mMetrics.emAscent = ROUND(mMetrics.emHeight * (double)oMetrics.otmAscent / typEmHeight);
        mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;
    } else {
        // Make a best-effort guess at extended metrics
        // this is based on general typographic guidelines
        
        // GetTextMetrics can fail if the font file has been removed
        // or corrupted recently.
        BOOL result = GetTextMetrics(dc.GetDC(), &metrics);
        if (!result) {
            NS_WARNING("Missing or corrupt font data, fasten your seatbelt");
            mIsValid = PR_FALSE;
            memset(&mMetrics, 0, sizeof(mMetrics));
            return;
        }

        mMetrics.xHeight = ROUND((float)metrics.tmAscent * 0.56f); // 56% of ascent, best guess for non-true type
        mMetrics.superscriptOffset = mMetrics.xHeight;
        mMetrics.subscriptOffset = mMetrics.xHeight;
        mMetrics.strikeoutSize = 1;
        mMetrics.strikeoutOffset = ROUND(mMetrics.xHeight / 2.0f); // 50% of xHeight
        mMetrics.underlineSize = 1;
        mMetrics.underlineOffset = -ROUND((float)metrics.tmDescent * 0.30f); // 30% of descent
        mMetrics.emHeight = metrics.tmHeight - metrics.tmInternalLeading;
        mMetrics.emAscent = metrics.tmAscent - metrics.tmInternalLeading;
        mMetrics.emDescent = metrics.tmDescent;
    }

    mMetrics.internalLeading = metrics.tmInternalLeading;
    mMetrics.externalLeading = metrics.tmExternalLeading;
    mMetrics.maxHeight = metrics.tmHeight;
    mMetrics.maxAscent = metrics.tmAscent;
    mMetrics.maxDescent = metrics.tmDescent;
    mMetrics.maxAdvance = metrics.tmMaxCharWidth;
    mMetrics.aveCharWidth = PR_MAX(1, metrics.tmAveCharWidth);
    // The font is monospace when TMPF_FIXED_PITCH is *not* set!
    // See http://msdn2.microsoft.com/en-us/library/ms534202(VS.85).aspx
    if (!(metrics.tmPitchAndFamily & TMPF_FIXED_PITCH)) {
      mMetrics.maxAdvance = mMetrics.aveCharWidth;
    }

    // Cache the width of a single space.
    SIZE size;
    GetTextExtentPoint32W(dc.GetDC(), L" ", 1, &size);
    mMetrics.spaceWidth = ROUND(size.cx);

    // Cache the width of digit zero.
    // XXX MSDN (http://msdn.microsoft.com/en-us/library/ms534223.aspx)
    // does not say what the failure modes for GetTextExtentPoint32 are -
    // is it safe to assume it will fail iff the font has no '0'?
    if (GetTextExtentPoint32W(dc.GetDC(), L"0", 1, &size))
        mMetrics.zeroOrAveCharWidth = ROUND(size.cx);
    else
        mMetrics.zeroOrAveCharWidth = mMetrics.aveCharWidth;

    mSpaceGlyph = 0;
    if (metrics.tmPitchAndFamily & TMPF_TRUETYPE) {
        WORD glyph;
        DWORD ret = GetGlyphIndicesW(dc.GetDC(), L" ", 1, &glyph,
                                     GGI_MARK_NONEXISTING_GLYPHS);
        if (ret != GDI_ERROR && glyph != 0xFFFF) {
            mSpaceGlyph = glyph;
        }
    }

    SanitizeMetrics(&mMetrics, GetFontEntry()->mIsBadUnderlineFont);
    mIsValid = PR_TRUE;

#if 0
    printf("Font: %p (%s) size: %f\n", this,
           NS_ConvertUTF16toUTF8(GetName()).get(), mStyle.size);
    printf("    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    printf("    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    printf("    internalLeading: %f externalLeading: %f\n", mMetrics.internalLeading, mMetrics.externalLeading);
    printf("    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    printf("    uOff: %f uSize: %f stOff: %f stSize: %f supOff: %f subOff: %f\n",
           mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize,
           mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif
}

void
gfxGDIFont::FillLogFont(gfxFloat aSize)
{
    GDIFontEntry *fe = static_cast<GDIFontEntry*>(GetFontEntry());

    PRUint16 weight = mNeedsBold ? 700 : fe->Weight();
    PRBool italic = (mStyle.style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE));

    // if user font, disable italics/bold if defined to be italics/bold face
    // this avoids unwanted synthetic italics/bold
    if (fe->mIsUserFont) {
        if (fe->IsItalic())
            italic = PR_FALSE; // avoid synthetic italic
        if (fe->IsBold()) {
            weight = 400; // avoid synthetic bold
        }
    }

    fe->FillLogFont(&mLogFont, italic, weight, aSize);
}

