/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#include "nsFontMetricsWin2.h"    
#include "nsFont.h"

#include "nsString.h"
#include "nsAutoBuffer.h"
#include <stdio.h>
#include <malloc.h>
#include <Usp10.h>

#include "cairo-win32.h"


// for using the cairo apis for fonts...
#define USE_SCALED_FONTS


NS_IMPL_ISUPPORTS1(nsFontMetricsWin, nsIFontMetrics)

#include <stdlib.h>

nsFontMetricsWin::nsFontMetricsWin()
{
    _putenv("XPCOM_DEBUG_BREAK=warn");
}

nsFontMetricsWin::~nsFontMetricsWin()
{
    cairo_font_face_destroy(mCairoFontFace);
    cairo_scaled_font_destroy(mCairoFont);
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
    nsFontMetricsWin* metrics = (nsFontMetricsWin*) aData;
    metrics->mFonts.AppendString(aFamily);
    if (aGeneric) {
        metrics->mGeneric.Assign(aFamily);
        //ToLowerCase(metrics->mGeneric);
        return PR_FALSE; // stop
    }
    ++metrics->mGenericIndex;

    return PR_TRUE; // don't stop
}

cairo_font_face_t *
nsFontMetricsWin::MakeCairoFontFace(const nsString *aFontName)
{
    cairo_font_face_t *fontFace = nsnull;

    LOGFONTW logFont;
    memset(&logFont, 0, sizeof(LOGFONTW));
    FillLogFont(&logFont, aFontName);

    fontFace = cairo_win32_font_face_create_for_logfontw(&logFont);

    return fontFace;
}
cairo_scaled_font_t *
nsFontMetricsWin::MakeCairoScaledFont(cairo_t *cr, cairo_font_face_t *aFontFace)
{
    cairo_scaled_font_t *font = nsnull;

    float app2dev = mDeviceContext->AppUnitsToDevUnits();
    double size = NSToIntRound(mFont.size * app2dev);
    cairo_matrix_t sizeMatrix, ctm;
    if (cr)
        cairo_get_matrix(cr, &ctm);
    else
        cairo_matrix_init_identity(&ctm);

    cairo_matrix_init_scale(&sizeMatrix, size, size);

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    font = cairo_scaled_font_create(aFontFace, &sizeMatrix, &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);

    return font;
}

NS_IMETHODIMP
nsFontMetricsWin::Init(const nsFont& aFont, nsIAtom* aLangGroup,
                       nsIDeviceContext *aContext)
{
    mFont = aFont;
    mLangGroup = aLangGroup;
    mDeviceContext = (nsThebesDeviceContext*)aContext;
    mDev2App = aContext->DevUnitsToAppUnits();

    // set up the first font in the list as the default, and we'll go from there.
    mFont.EnumerateFamilies(FontEnumCallback, this);

    mCairoFontFace = MakeCairoFontFace(mFonts[0]);
    mCairoFont = MakeCairoScaledFont(nsnull, mCairoFontFace);

    HWND win = (HWND)mDeviceContext->GetWidget();
    HDC dc = ::GetDC(win);

    RealizeFont(dc);

    ::ReleaseDC(win, dc);

    return NS_OK;
}


#define CLIP_TURNOFF_FONTASSOCIATION 0x40
void
nsFontMetricsWin::FillLogFont(LOGFONTW* logFont, const nsString *aFontName)
{
    float app2dev = mDeviceContext->AppUnitsToDevUnits();
    logFont->lfHeight = - NSToIntRound(mFont.size * app2dev);

    if (logFont->lfHeight == 0) {
        logFont->lfHeight = -1;
    }

    // Fill in logFont structure
    logFont->lfWidth          = 0; 
    logFont->lfEscapement     = 0;
    logFont->lfOrientation    = 0;
    logFont->lfUnderline      =
        (mFont.decorations & NS_FONT_DECORATION_UNDERLINE)
        ? TRUE : FALSE;
    logFont->lfStrikeOut      =
        (mFont.decorations & NS_FONT_DECORATION_LINE_THROUGH)
        ? TRUE : FALSE;
    logFont->lfCharSet        = DEFAULT_CHARSET;
#ifndef WINCE
    logFont->lfOutPrecision   = OUT_TT_PRECIS;
#else
    logFont->lfOutPrecision   = OUT_DEFAULT_PRECIS;
#endif
    logFont->lfClipPrecision  = CLIP_TURNOFF_FONTASSOCIATION;
    logFont->lfQuality        = DEFAULT_QUALITY;
    logFont->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    logFont->lfWeight = mFont.weight;
    logFont->lfItalic = (mFont.style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE))
        ? TRUE : FALSE;   // XXX need better oblique support

    int len = PR_MIN(aFontName->Length(), LF_FACESIZE);
    memcpy(logFont->lfFaceName, aFontName->get(), len * 2);
    logFont->lfFaceName[len] = '\0';
}


NS_IMETHODIMP
nsFontMetricsWin::Destroy()
{
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetXHeight(nscoord& aResult)
{
    aResult = mXHeight;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetSuperscriptOffset(nscoord& aResult)
{
    aResult = mSuperscriptOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetSubscriptOffset(nscoord& aResult)
{
    aResult = mSubscriptOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
    aOffset = mStrikeoutOffset;
    aSize = mStrikeoutSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
    aOffset = mUnderlineOffset;
    aSize = mUnderlineSize;

    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetHeight(nscoord &aHeight)
{
    aHeight = mMaxHeight;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetInternalLeading(nscoord &aLeading)
{
    aLeading = mInternalLeading;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetExternalLeading(nscoord &aLeading)
{
    aLeading = mExternalLeading;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetEmHeight(nscoord &aHeight)
{
    aHeight = mEmHeight;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetEmAscent(nscoord &aAscent)
{
    aAscent = mEmAscent;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetEmDescent(nscoord &aDescent)
{
    aDescent = mEmDescent;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetMaxHeight(nscoord &aHeight)
{
    aHeight = mMaxHeight;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetMaxAscent(nscoord &aAscent)
{
    aAscent = mMaxAscent;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetMaxDescent(nscoord &aDescent)
{
    aDescent = mMaxDescent;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetMaxAdvance(nscoord &aAdvance)
{
    aAdvance = mMaxAdvance;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetLangGroup(nsIAtom** aLangGroup)
{
    *aLangGroup = mLangGroup;
    NS_IF_ADDREF(*aLangGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetFontHandle(nsFontHandle &aHandle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFontMetricsWin::GetAveCharWidth(nscoord& aAveCharWidth)
{
    aAveCharWidth = mAveCharWidth;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetSpaceWidth(nscoord& aSpaceCharWidth)
{
    aSpaceCharWidth = mSpaceWidth;

    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetLeading(nscoord& aLeading)
{
    aLeading = mInternalLeading;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsWin::GetNormalLineHeight(nscoord& aLineHeight)
{
    aLineHeight = mEmHeight + mInternalLeading;
    return NS_OK;
}





PRInt32
nsFontMetricsWin::MeasureOrDrawUniscribe(nsThebesRenderingContext *aContext,
                                         const PRUnichar *aString, PRUint32 aLength,
                                         PRBool aDraw, PRInt32 aX, PRInt32 aY, const PRInt32 *aSpacing)
{
    HDC aDC = (HDC)aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_WINDOWS_DC);

    float app2dev = mDeviceContext->AppUnitsToDevUnits();

    int loops = 0;

    cairo_t *cr = (cairo_t*)aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_CAIRO_CONTEXT);

    PRBool isComplex = (::ScriptIsComplex(aString, aLength, SIC_COMPLEX) == S_OK);

    PRInt32 length = 0;
    HRESULT rv;
    int numItems, maxItems = 2;

    SCRIPT_CACHE sc = NULL;
    SCRIPT_CONTROL *control = nsnull;
    SCRIPT_STATE *state = nsnull;
    PRBool rtl = ((::GetTextAlign(aDC) & TA_RTLREADING) == TA_RTLREADING);
    if (rtl) {
        control = (SCRIPT_CONTROL *)malloc(sizeof(SCRIPT_CONTROL));
        state = (SCRIPT_STATE *)malloc(sizeof(SCRIPT_STATE));
        memset(control, 0, sizeof(control));
        memset(state, 0, sizeof(state));
        control->fNeutralOverride = 1;
        state->uBidiLevel = 1;
    }
  
    SCRIPT_ITEM *items = (SCRIPT_ITEM *)malloc(maxItems*sizeof(SCRIPT_ITEM));
    while ((rv = ScriptItemize(aString, aLength, maxItems, control, state,
                               items, &numItems)) == E_OUTOFMEMORY) {
        maxItems *= 2;
        items = (SCRIPT_ITEM *)realloc(items, maxItems*sizeof(SCRIPT_ITEM));
    }

    for (int h=0; h<numItems; h++) {
        int i = (rtl) ? numItems - h - 1 : h;
        const PRUnichar *itemChars = aString + items[i].iCharPos;
        PRUint32 itemLength = items[i+1].iCharPos - items[i].iCharPos;
        int numGlyphs = 0, maxGlyphs = 1.5 * itemLength + 16;
        WORD *glyphs = (WORD *)malloc(maxGlyphs*sizeof(WORD));
        WORD *clusters = (WORD *)malloc(itemLength*sizeof(WORD));
        SCRIPT_VISATTR *attr = (SCRIPT_VISATTR *)malloc(maxGlyphs*sizeof(SCRIPT_VISATTR));
        items[i].a.fLogicalOrder = 1;

        cairo_font_face_t *fontFace = nsnull;
        cairo_scaled_font_t *scaledFont = nsnull;

TRY_AGAIN:
        int fontIndex = 0;

TRY_AGAIN_SAME_SCRIPT:
        loops++;
        SaveDC(aDC);

        if (fontFace && scaledFont) {
            cairo_font_face_destroy(fontFace);
            cairo_scaled_font_destroy(scaledFont);
        }

        fontFace = MakeCairoFontFace(mFonts[fontIndex]);
        scaledFont = MakeCairoScaledFont(cr, fontFace);

        cairo_set_font_face(cr, fontFace);
        cairo_set_font_size(cr, NSToIntRound(mFont.size * app2dev));
        cairo_win32_scaled_font_select_font(scaledFont, aDC);
        double cairofontfactor = cairo_win32_scaled_font_get_metrics_factor(scaledFont);

        if (!isComplex)
            items[i].a.eScript = SCRIPT_UNDEFINED;

        while ((rv = ScriptShape(aDC, &sc, itemChars, itemLength,
                                 maxGlyphs, &items[i].a,
                                 glyphs, clusters,
                                 attr, &numGlyphs)) == E_OUTOFMEMORY) {
            maxGlyphs *= 2;
            glyphs = (WORD *)realloc(glyphs, maxGlyphs*sizeof(WORD));
            attr = (SCRIPT_VISATTR *)realloc(attr, maxGlyphs*sizeof(SCRIPT_VISATTR));
        }

        if (rv == USP_E_SCRIPT_NOT_IN_FONT) {
            if (fontIndex < mFonts.Count() - 1) {
                fontIndex++;
                cairo_win32_scaled_font_done_font(scaledFont);
                RestoreDC(aDC, -1);
                goto TRY_AGAIN_SAME_SCRIPT;
            } else {
                // try again with SCRIPT_UNDEFINED
                items[i].a.eScript = SCRIPT_UNDEFINED;
                cairo_win32_scaled_font_done_font(scaledFont);
                RestoreDC(aDC, -1);
                goto TRY_AGAIN;
            }
        }

        if (numGlyphs > 0 && glyphs[0] == 0) {
            if (fontIndex < mFonts.Count() - 1) {
                fontIndex++;
                cairo_win32_scaled_font_done_font(scaledFont);
                RestoreDC(aDC, -1);
                goto TRY_AGAIN_SAME_SCRIPT;
            }
            // otherwise we fail to draw the characters so give up and continue on.
        }

        if (rv == 0) {
      
            ABC abc;
            GOFFSET *offsets = (GOFFSET *)malloc(numGlyphs*sizeof(GOFFSET));
            int *advance = (int *)malloc(numGlyphs*sizeof(int));
            rv = ScriptPlace(aDC, &sc, glyphs, numGlyphs, attr, &items[i].a,
                             advance, offsets, &abc);

            if (rv != S_OK)
                printf("error ScriptPlacing\n");
#ifdef DEBUG_tor
            fprintf(stderr, "ABC[%d]: %d %d %d\n", i, abc.abcA, abc.abcB, abc.abcC);
#endif

            if (!aDraw) {
                length += NSToCoordRound((abc.abcA + abc.abcB + abc.abcC) * cairofontfactor * (mFont.size * app2dev));
            } else {
                PRInt32 *spacing = 0;
                PRInt32 justTotal = 0;
                if (aSpacing) {
                    /* need to correct for layout/gfx spacing mismatch */
#ifdef DEBUG_tor
                    fprintf(stderr, "advn: ");
                    for (int j=0; j<numGlyphs; j++) {
                        fprintf(stderr, "%4d,", advance[j]);
                    }
                    fprintf(stderr, "\n");

                    fprintf(stderr, "clus: ");
                    for (j=0; j<itemLength; j++)
                        fprintf(stderr, "%4d,", clusters[j]);
                    fprintf(stderr, "\n");

                    fprintf(stderr, "attr: ");
                    for (j=0; j<numGlyphs; j++)
                        fprintf(stderr, "0x%2x,", attr[j]);
                    fprintf(stderr, "\n");
#endif
                    // hacky inefficient justification: take the excess of what layout
                    // thinks the width is over what uniscribe thinks the width is and
                    // share it evenly between the justification opportunities
                    PRInt32 layoutTotal = 0;
                    for (PRUint32 j = items[i].iCharPos; j < items[i+1].iCharPos; j++) {
                        layoutTotal += NSToIntRound(aSpacing[j] * app2dev);
                    }
                    PRInt32 gfxTotal = abc.abcA + abc.abcB + abc.abcC;
                    if (layoutTotal > gfxTotal) {
                        spacing = (PRInt32 *)malloc(numGlyphs*sizeof(PRInt32));
                        justTotal = layoutTotal - gfxTotal;
#if 0
                        ScriptJustify(attr, advance, numGlyphs, justTotal, 1, spacing);
#else
                        memcpy(spacing, advance, sizeof(PRInt32)*numGlyphs);

                        int justOpps = 0;
                        int lookForOpp = 0;
                        for (j = 0; j < numGlyphs-1; j++) {
                            if (attr[j+1].uJustification > 1) {
                                ++justOpps;
                            }
                        }
                        if (justOpps > 0) {
                            int eachJust = justTotal / justOpps;

                            for (j=0; j<numGlyphs-1; j++) {
                                if (attr[j+1].uJustification > 1) {
                                    --justOpps;
                                    if (justOpps == 0) {
                                        spacing[j] += justTotal;
                                    } else {
                                        spacing[j] += eachJust;
                                        justTotal -= eachJust;
                                    }
                                }
                            }
                        }
#endif

#ifdef DEBUG_tor
                        fprintf(stderr, "hand: ");
                        for (j=0; j<numGlyphs; j++) {
                            fprintf(stderr, "%4d,", spacing[j]);
                        }
                        fprintf(stderr, "\n");
#endif

                    }
                }

                double cairoToPixels = cairofontfactor * (mFont.size * app2dev);

                cairo_glyph_t *cglyphs = (cairo_glyph_t*)malloc(numGlyphs*sizeof(cairo_glyph_t));
                PRInt32 offset = 0;
                for (PRInt32 k = 0; k < numGlyphs; k++) {
                    cglyphs[k].index = glyphs[k];
                    cglyphs[k].x = aX + offset + NSToCoordRound(offsets[k].du * cairoToPixels);
                    cglyphs[k].y = aY + NSToCoordRound(offsets[k].dv * cairoToPixels);
                    offset += NSToCoordRound(advance[k] * cairoToPixels);
#if 0
                    if (aSpacing)
                        offset += NSToIntRound(aSpacing[k] * app2dev) * cairoToPixels;
#endif
                }

                cairo_show_glyphs(cr, cglyphs, numGlyphs);

                free(cglyphs);


                aX += NSToCoordRound((abs(abc.abcA) + abc.abcB + abs(abc.abcC) + justTotal) * cairoToPixels);
                free(spacing);
            }
            free(offsets);
            free(advance);

           cairo_font_face_destroy(fontFace);
           cairo_scaled_font_destroy(scaledFont);

           cairo_win32_scaled_font_done_font(scaledFont);

           RestoreDC(aDC, -1);

        }
        free(glyphs);
        free(clusters);
        free(attr);

    }
  
    free(items);

    //    printf("loops: %d\n", loops - numItems);
    return length;
}


nsresult 
nsFontMetricsWin::GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth,
                           nsThebesRenderingContext *aContext)
{
    PRInt32 aFontID = 0;
    return GetWidth(PromiseFlatString(NS_ConvertUTF8toUTF16(aString, aLength)).get(),
                    aLength, aWidth, &aFontID, aContext);
}

nsresult
nsFontMetricsWin::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                           nscoord& aWidth, PRInt32 *aFontID,
                           nsThebesRenderingContext *aContext)
{
    //    if (::ScriptIsComplex(aString, aLength, SIC_COMPLEX) == S_OK) {
    aWidth = MeasureOrDrawUniscribe(aContext, aString, aLength, PR_FALSE, 0, 0, NULL);

    float f;
    f = mDeviceContext->DevUnitsToAppUnits();
    aWidth = NSToCoordRound(aWidth * f);

#if 0
} else {
    ::GetTextExtentPoint32W(aDC, aString, aLength, &size);
    size.cx -= mOverhangCorrection;
    return size.cx;
 }
#endif
    return NS_OK;

}

// Get the text dimensions for this string
nsresult
nsFontMetricsWin::GetTextDimensions(const PRUnichar* aString,
                                    PRUint32 aLength,
                                    nsTextDimensions& aDimensions, 
                                    PRInt32* aFontID)
{
    return NS_OK;
}

nsresult
nsFontMetricsWin::GetTextDimensions(const char*         aString,
                                   PRInt32             aLength,
                                   PRInt32             aAvailWidth,
                                   PRInt32*            aBreaks,
                                   PRInt32             aNumBreaks,
                                   nsTextDimensions&   aDimensions,
                                   PRInt32&            aNumCharsFit,
                                   nsTextDimensions&   aLastWordDimensions,
                                   PRInt32*            aFontID)
{
    return NS_OK;
}
nsresult
nsFontMetricsWin::GetTextDimensions(const PRUnichar*    aString,
                                   PRInt32             aLength,
                                   PRInt32             aAvailWidth,
                                   PRInt32*            aBreaks,
                                   PRInt32             aNumBreaks,
                                   nsTextDimensions&   aDimensions,
                                   PRInt32&            aNumCharsFit,
                                   nsTextDimensions&   aLastWordDimensions,
                                   PRInt32*            aFontID)
{
    return NS_OK;
}

// Draw a string using this font handle on the surface passed in.  
nsresult
nsFontMetricsWin::DrawString(const char *aString, PRUint32 aLength,
                            nscoord aX, nscoord aY,
                            const nscoord* aSpacing,
                            nsThebesRenderingContext *aContext)
{
    return DrawString(PromiseFlatString(NS_ConvertUTF8toUTF16(aString, aLength)).get(),
                      aLength, aX, aY, 0, aSpacing, aContext);
}

// aCachedOffset will be updated with a new offset.
nsresult
nsFontMetricsWin::DrawString(const PRUnichar* aString, PRUint32 aLength,
                            nscoord aX, nscoord aY,
                            PRInt32 aFontID,
                            const nscoord* aSpacing,
                            nsThebesRenderingContext *aContext)
{
    float app2dev = mDeviceContext->AppUnitsToDevUnits();
    MeasureOrDrawUniscribe(aContext, aString, aLength, PR_TRUE,
                           NSToIntRound(aX * app2dev), NSToIntRound(aY * app2dev), aSpacing);

    return NS_OK;
}


#ifdef MOZ_MATHML
// These two functions get the bounding metrics for this handle,
// updating the aBoundingMetrics in Points.  This means that the
// caller will have to update them to twips before passing it
// back.
nsresult
nsFontMetricsWin::GetBoundingMetrics(const char *aString, PRUint32 aLength,
                                    nsBoundingMetrics &aBoundingMetrics)
{
    return NS_OK;
}

// aCachedOffset will be updated with a new offset.
nsresult
nsFontMetricsWin::GetBoundingMetrics(const PRUnichar *aString,
                                    PRUint32 aLength,
                                    nsBoundingMetrics &aBoundingMetrics,
                                    PRInt32 *aFontID)
{
    return NS_OK;
}

#endif /* MOZ_MATHML */

// Set the direction of the text rendering
nsresult
nsFontMetricsWin::SetRightToLeftText(PRBool aIsRTL)
{
    return NS_OK;
}
















void
nsFontMetricsWin::RealizeFont(HDC dc)
{
    float app2dev = mDeviceContext->AppUnitsToDevUnits();

    SaveDC(dc);

    cairo_win32_scaled_font_select_font(mCairoFont, dc);

    double cairofontfactor = cairo_win32_scaled_font_get_metrics_factor(mCairoFont);
    double multiplier = mDev2App * cairofontfactor * NSToIntRound(mFont.size * app2dev);

    // Get font metrics
    OUTLINETEXTMETRIC oMetrics;
    TEXTMETRIC& metrics = oMetrics.otmTextMetrics;
    nscoord onePixel = NSToCoordRound(1 * mDev2App);
    nscoord descentPos = 0;

    if (0 < ::GetOutlineTextMetrics(dc, sizeof(oMetrics), &oMetrics)) {
        //    mXHeight = NSToCoordRound(oMetrics.otmsXHeight * mDev2App);  XXX not really supported on windows
        mXHeight = NSToCoordRound((float)metrics.tmAscent * multiplier * 0.56f); // 50% of ascent, best guess for true type
        mSuperscriptOffset = NSToCoordRound(oMetrics.otmptSuperscriptOffset.y * multiplier);
        mSubscriptOffset = NSToCoordRound(oMetrics.otmptSubscriptOffset.y * multiplier);
        
        mStrikeoutSize = PR_MAX(onePixel, NSToCoordRound(oMetrics.otmsStrikeoutSize * multiplier));
        mStrikeoutOffset = NSToCoordRound(oMetrics.otmsStrikeoutPosition * multiplier);
        mUnderlineSize = PR_MAX(onePixel, NSToCoordRound(oMetrics.otmsUnderscoreSize * multiplier));

        mUnderlineOffset = NSToCoordRound(oMetrics.otmsUnderscorePosition * multiplier);
        
        // Begin -- section of code to get the real x-height with GetGlyphOutline()
        GLYPHMETRICS gm;
        MAT2 mMat = { 1, 0, 0, 1 };    // glyph transform matrix (always identity in our context)
        DWORD len = GetGlyphOutlineW(dc, PRUnichar('x'), GGO_METRICS, &gm, 0, nsnull, &mMat);

        if (GDI_ERROR != len && gm.gmptGlyphOrigin.y > 0) {
            mXHeight = NSToCoordRound(gm.gmptGlyphOrigin.y * multiplier);
        }
        // End -- getting x-height
    }
    else {
        // Make a best-effort guess at extended metrics
        // this is based on general typographic guidelines
        ::GetTextMetrics(dc, &metrics);
        mXHeight = NSToCoordRound((float)metrics.tmAscent * mDev2App * 0.56f); // 56% of ascent, best guess for non-true type
        mSuperscriptOffset = mXHeight;     // XXX temporary code!
        mSubscriptOffset = mXHeight;     // XXX temporary code!
        
        mStrikeoutSize = onePixel; // XXX this is a guess
        mStrikeoutOffset = NSToCoordRound(mXHeight / 2.0f); // 50% of xHeight
        mUnderlineSize = onePixel; // XXX this is a guess
        mUnderlineOffset = -NSToCoordRound((float)metrics.tmDescent * mDev2App * 0.30f); // 30% of descent
    }
    

    mInternalLeading = NSToCoordRound(metrics.tmInternalLeading * multiplier);
    mExternalLeading = NSToCoordRound(metrics.tmExternalLeading * multiplier);
    mEmHeight = NSToCoordRound((metrics.tmHeight - metrics.tmInternalLeading) *
                               multiplier);
    mEmAscent = NSToCoordRound((metrics.tmAscent - metrics.tmInternalLeading) *
                               multiplier);
    mEmDescent = NSToCoordRound(metrics.tmDescent * multiplier);
    mMaxHeight = NSToCoordRound(metrics.tmHeight * multiplier);
    mMaxAscent = NSToCoordRound(metrics.tmAscent * multiplier);
    mMaxDescent = NSToCoordRound(metrics.tmDescent * multiplier);
    mMaxAdvance = NSToCoordRound(metrics.tmMaxCharWidth * multiplier);
    mAveCharWidth = PR_MAX(1, NSToCoordRound(metrics.tmAveCharWidth * multiplier));
    
#if 0
    // descent position is preferred, but we need to make sure there is enough 
    // space available. Baseline need to be raised so that underline will stay 
    // within boundary.
    // only do this for CJK to minimize possible risk
    if (IsCJKLangGroupAtom(mLangGroup.get())) {
        if (gDoingLineheightFixup && 
            mInternalLeading+mExternalLeading > mUnderlineSize &&
            descentPos < mUnderlineOffset) {
            mEmAscent -= mUnderlineSize;
            mEmDescent += mUnderlineSize;
            mMaxAscent -= mUnderlineSize;
            mMaxDescent += mUnderlineSize;
            mUnderlineOffset = descentPos;
        }
    }
#endif
    // Cache the width of a single space.
    SIZE  size;
    ::GetTextExtentPoint32(dc, " ", 1, &size);
    //size.cx -= font->mOverhangCorrection;
    mSpaceWidth = NSToCoordRound(size.cx * multiplier);

    cairo_win32_scaled_font_done_font(mCairoFont);

    RestoreDC(dc, -1);
}

