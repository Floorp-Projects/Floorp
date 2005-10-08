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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
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
#include "gfxTypes.h"

#include "nsPromiseFlatString.h"

#include "gfxContext.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsFonts.h"
#include <math.h>
#include <Usp10.h>


#define NSToCoordRound(x) (floor((x) + 0.5))


gfxWindowsFont::gfxWindowsFont(const nsAString &aName, const gfxFontGroup *aFontGroup, HWND aHWnd)
{
    mName = aName;
    mGroup = aFontGroup;
    mStyle = mGroup->GetStyle();

    mHWnd = (HWND)aHWnd;

    mFontFace = MakeCairoFontFace();
    mScaledFont = MakeCairoScaledFont(nsnull);

    ComputeMetrics();
}

gfxWindowsFont::~gfxWindowsFont()
{
    cairo_font_face_destroy(mFontFace);
    cairo_scaled_font_destroy(mScaledFont);
}


cairo_font_face_t *
gfxWindowsFont::MakeCairoFontFace()
{
    cairo_font_face_t *fontFace = nsnull;

    FillLogFont();

    fontFace = cairo_win32_font_face_create_for_logfontw(&mLogFont);

    return fontFace;
}

cairo_scaled_font_t *
gfxWindowsFont::MakeCairoScaledFont(cairo_t *cr)
{
    cairo_scaled_font_t *font = nsnull;

    cairo_matrix_t sizeMatrix, ctm;

    if (cr)
        cairo_get_matrix(cr, &ctm);
    else
        cairo_matrix_init_identity(&ctm);

    cairo_matrix_init_scale(&sizeMatrix, mStyle->size, mStyle->size);

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    font = cairo_scaled_font_create(mFontFace, &sizeMatrix, &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);

    return font;
}

void
gfxWindowsFont::ComputeMetrics()
{
    HDC dc = GetDC(mHWnd);

    SaveDC(dc);

    cairo_win32_scaled_font_select_font(mScaledFont, dc);

    double cairofontfactor = cairo_win32_scaled_font_get_metrics_factor(mScaledFont);
    double multiplier = cairofontfactor * mStyle->size;


    // Get font metrics
    OUTLINETEXTMETRIC oMetrics;
    TEXTMETRIC& metrics = oMetrics.otmTextMetrics;
    gfxFloat descentPos = 0;

    if (0 < ::GetOutlineTextMetrics(dc, sizeof(oMetrics), &oMetrics)) {
        //    mXHeight = NSToCoordRound(oMetrics.otmsXHeight * mDev2App);  XXX not really supported on windows
        mMetrics.xHeight = NSToCoordRound((float)metrics.tmAscent * multiplier * 0.56f); // 50% of ascent, best guess for true type
        mMetrics.superscriptOffset = NSToCoordRound(oMetrics.otmptSuperscriptOffset.y * multiplier);
        mMetrics.subscriptOffset = NSToCoordRound(oMetrics.otmptSubscriptOffset.y * multiplier);
        
        mMetrics.strikeoutSize = PR_MAX(1, NSToCoordRound(oMetrics.otmsStrikeoutSize * multiplier));
        mMetrics.strikeoutOffset = NSToCoordRound(oMetrics.otmsStrikeoutPosition * multiplier);
        mMetrics.underlineSize = PR_MAX(1, NSToCoordRound(oMetrics.otmsUnderscoreSize * multiplier));

        mMetrics.underlineOffset = NSToCoordRound(oMetrics.otmsUnderscorePosition * multiplier);
        
        // Begin -- section of code to get the real x-height with GetGlyphOutline()
        GLYPHMETRICS gm;
        MAT2 mMat = { 1, 0, 0, 1 };    // glyph transform matrix (always identity in our context)
        DWORD len = GetGlyphOutlineW(dc, PRUnichar('x'), GGO_METRICS, &gm, 0, nsnull, &mMat);

        if (GDI_ERROR != len && gm.gmptGlyphOrigin.y > 0) {
            mMetrics.xHeight = NSToCoordRound(gm.gmptGlyphOrigin.y * multiplier);
        }
        // End -- getting x-height
    }
    else {
        // Make a best-effort guess at extended metrics
        // this is based on general typographic guidelines
        ::GetTextMetrics(dc, &metrics);
        mMetrics.xHeight = NSToCoordRound((float)metrics.tmAscent * 0.56f); // 56% of ascent, best guess for non-true type
        mMetrics.superscriptOffset = mMetrics.xHeight;     // XXX temporary code!
        mMetrics.subscriptOffset = mMetrics.xHeight;     // XXX temporary code!
        
        mMetrics.strikeoutSize = 1; // XXX this is a guess
        mMetrics.strikeoutOffset = NSToCoordRound(mMetrics.xHeight / 2.0f); // 50% of xHeight
        mMetrics.underlineSize = 1; // XXX this is a guess
        mMetrics.underlineOffset = -NSToCoordRound((float)metrics.tmDescent * 0.30f); // 30% of descent
    }
    

    mMetrics.internalLeading = NSToCoordRound(metrics.tmInternalLeading * multiplier);
    mMetrics.externalLeading = NSToCoordRound(metrics.tmExternalLeading * multiplier);
    mMetrics.emHeight = NSToCoordRound((metrics.tmHeight - metrics.tmInternalLeading) * multiplier);
    mMetrics.emAscent = NSToCoordRound((metrics.tmAscent - metrics.tmInternalLeading) * multiplier);
    mMetrics.emDescent = NSToCoordRound(metrics.tmDescent * multiplier);
    mMetrics.maxHeight = NSToCoordRound(metrics.tmHeight * multiplier);
    mMetrics.maxAscent = NSToCoordRound(metrics.tmAscent * multiplier);
    mMetrics.maxDescent = NSToCoordRound(metrics.tmDescent * multiplier);
    mMetrics.maxAdvance = NSToCoordRound(metrics.tmMaxCharWidth * multiplier);
    mMetrics.aveCharWidth = PR_MAX(1, NSToCoordRound(metrics.tmAveCharWidth * multiplier));
    
    // Cache the width of a single space.
    SIZE  size;
    ::GetTextExtentPoint32(dc, " ", 1, &size);
    //size.cx -= font->mOverhangCorrection;
    mMetrics.spaceWidth = NSToCoordRound(size.cx * multiplier);

    cairo_win32_scaled_font_done_font(mScaledFont);

    RestoreDC(dc, -1);

    ReleaseDC(mHWnd, dc);
}


#define CLIP_TURNOFF_FONTASSOCIATION 0x40
void
gfxWindowsFont::FillLogFont()
{
    mLogFont.lfHeight = -NSToCoordRound(mStyle->size);

    if (mLogFont.lfHeight == 0)
        mLogFont.lfHeight = -1;

    // Fill in logFont structure
    mLogFont.lfWidth          = 0; 
    mLogFont.lfEscapement     = 0;
    mLogFont.lfOrientation    = 0;
    mLogFont.lfUnderline      = (mStyle->decorations & FONT_DECORATION_UNDERLINE) ? TRUE : FALSE;
    mLogFont.lfStrikeOut      = (mStyle->decorations & FONT_DECORATION_STRIKEOUT) ? TRUE : FALSE;
    mLogFont.lfCharSet        = DEFAULT_CHARSET;
#ifndef WINCE
    mLogFont.lfOutPrecision   = OUT_TT_PRECIS;
#else
    mLogFont.lfOutPrecision   = OUT_DEFAULT_PRECIS;
#endif
    mLogFont.lfClipPrecision  = CLIP_TURNOFF_FONTASSOCIATION;
    mLogFont.lfQuality        = DEFAULT_QUALITY;
    mLogFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    mLogFont.lfWeight = mStyle->weight;
    mLogFont.lfItalic = (mStyle->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) ? TRUE : FALSE;   // XXX need better oblique support

    int len = PR_MIN(mName.Length(), LF_FACESIZE);
    memcpy(mLogFont.lfFaceName, mName.get(), len * 2);
    mLogFont.lfFaceName[len] = '\0';
}


















gfxFont *
gfxWindowsFontGroup::MakeFont(const nsAString& aName)
{
    return (new gfxWindowsFont(aName, this, mWnd));
}


gfxWindowsFontGroup::gfxWindowsFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle, HWND hwnd)
    : gfxFontGroup(aFamilies, aStyle), mWnd(hwnd)
{
    Init();
}

gfxWindowsFontGroup::~gfxWindowsFontGroup()
{

}

void
gfxWindowsFontGroup::DrawString(gfxContext *aContext, const nsAString& aString, gfxPoint pt)
{
    MeasureOrDrawUniscribe(aContext, PromiseFlatString(aString).get(), aString.Length(), PR_TRUE, pt.x, pt.y, nsnull);
}

gfxFloat
gfxWindowsFontGroup::MeasureText(gfxContext *aContext, const nsAString& aString)
{
    return MeasureOrDrawUniscribe(aContext, PromiseFlatString(aString).get(), aString.Length(), PR_FALSE, 0, 0, nsnull);
}


PRInt32
gfxWindowsFontGroup::MeasureOrDrawUniscribe(gfxContext *aContext,
                                            const PRUnichar *aString, PRUint32 aLength,
                                            PRBool aDraw, PRInt32 aX, PRInt32 aY, const PRInt32 *aSpacing)
{
    HDC aDC = static_cast<gfxWindowsSurface*>(aContext->CurrentSurface())->GetDC();

    int loops = 0;

    cairo_t *cr = aContext->GetCairo();

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

        gfxWindowsFont *currentFont = static_cast<gfxWindowsFont*>(mFonts[fontIndex]);
        fontFace = currentFont->CairoFontFace();
        scaledFont = currentFont->CairoScaledFont();

        cairo_set_font_face(cr, fontFace);
        cairo_set_font_size(cr, mStyle.size);
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
            if (fontIndex < mFonts.size() - 1) {
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
            if (fontIndex < mFonts.size() - 1) {
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
                length += NSToCoordRound((abc.abcA + abc.abcB + abc.abcC) * cairofontfactor * mStyle.size);
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
                        layoutTotal += aSpacing[j];
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

                double cairoToPixels = cairofontfactor * mStyle.size;

                cairo_glyph_t *cglyphs = (cairo_glyph_t*)malloc(numGlyphs*sizeof(cairo_glyph_t));
                PRInt32 offset = 0;
                for (PRInt32 k = 0; k < numGlyphs; k++) {
                    cglyphs[k].index = glyphs[k];
                    cglyphs[k].x = aX + offset + NSToCoordRound(offsets[k].du * cairoToPixels);
                    cglyphs[k].y = aY + NSToCoordRound(offsets[k].dv * cairoToPixels);
                    offset += NSToCoordRound(advance[k] * cairoToPixels);
#if 0
                    if (aSpacing)
                        offset += aSpacing[k] * cairoToPixels;
#endif
                }

                cairo_show_glyphs(cr, cglyphs, numGlyphs);

                free(cglyphs);


                aX += NSToCoordRound((abs(abc.abcA) + abc.abcB + abs(abc.abcC) + justTotal) * cairoToPixels);
                free(spacing);
            }
            free(offsets);
            free(advance);

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
