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

#include "gfxContext.h"
#include "gfxWindowsFonts.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"

#include <math.h>
#include <mlang.h>

#define NSToCoordRound(x) (floor((x) + 0.5))


/**********************************************************************
 *
 * class gfxWindowsFont
 *
 **********************************************************************/

THEBES_IMPL_REFCOUNTING(gfxWindowsFont)

gfxWindowsFont::gfxWindowsFont(const nsAString &aName, const gfxFontGroup *aFontGroup, HDC aDC)
    : mFont(nsnull), mScriptCache(nsnull), mIsMLangFont(PR_FALSE)
{
    mName = aName;
    mGroup = aFontGroup;
    mStyle = mGroup->GetStyle();

    mFontFace = MakeCairoFontFace();
    NS_ASSERTION(mFontFace, "Failed to make font face");

    mScaledFont = MakeCairoScaledFont(nsnull);
    NS_ASSERTION(mScaledFont, "Failed to make scaled font");

    HDC dc = aDC ? aDC : GetDC((HWND)nsnull);

    ComputeMetrics(dc);

    if (dc != aDC)
        ReleaseDC((HWND)nsnull, dc);
}

gfxWindowsFont::gfxWindowsFont(HFONT aFont, const gfxFontGroup *aFontGroup, PRBool aIsMLangFont)
    : mScriptCache(nsnull), mIsMLangFont(aIsMLangFont)
{
    mFont = aFont;
    mGroup = aFontGroup;
    mStyle = mGroup->GetStyle();

    mFontFace = MakeCairoFontFace();
    NS_ASSERTION(mFontFace, "Failed to make font face");

    mScaledFont = MakeCairoScaledFont(nsnull);
    NS_ASSERTION(mScaledFont, "Failed to make scaled font");

    // don't bother creating the metrics.
}

gfxWindowsFont::~gfxWindowsFont()
{
    cairo_font_face_destroy(mFontFace);
    cairo_scaled_font_destroy(mScaledFont);

    if (mIsMLangFont) {
        IMultiLanguage *ml = gfxWindowsPlatform::GetPlatform()->GetMLangService();
        if (ml) {
            nsRefPtr<IMLangFontLink2> fl;
            HRESULT rv = ml->QueryInterface(IID_IMLangFontLink2, getter_AddRefs(fl));
            if (SUCCEEDED(rv))
                fl->ReleaseFont(mFont);
        }
        mFont = nsnull;
    }

    if (mFont)
        DeleteObject(mFont);

    ScriptFreeCache(&mScriptCache);
}

void
gfxWindowsFont::UpdateFonts(cairo_t *cr)
{
    cairo_font_face_destroy(mFontFace);
    cairo_scaled_font_destroy(mScaledFont);
    
    mFontFace = MakeCairoFontFace();
    NS_ASSERTION(mFontFace, "Failed to make font face");
    mScaledFont = MakeCairoScaledFont(nsnull);
    NS_ASSERTION(mScaledFont, "Failed to make scaled font");
}

cairo_font_face_t *
gfxWindowsFont::MakeCairoFontFace()
{
    if (!mFont) {
        /* XXX split this code out some, make it a bit more CSS2 15.5.1 compliant */
        PRInt16 baseWeight, weightDistance;
        mStyle->ComputeWeightAndOffset(&baseWeight, &weightDistance);

        HDC dc = GetDC((HWND)nsnull);

        int i = 0;
        do {
            const PRInt16 targetWeight = (baseWeight * 100) + (weightDistance ? i + 1 : 0) * 100;

            FillLogFont(targetWeight);
            mFont = CreateFontIndirectW(&mLogFont);

            HGDIOBJ oldFont = SelectObject(dc, mFont);
            TEXTMETRIC metrics;
            GetTextMetrics(dc, &metrics);
            if (metrics.tmWeight == targetWeight) {
                SelectObject(dc, oldFont);
                break;
            }

            SelectObject(dc, oldFont);
            DeleteObject(mFont);
            mFont = nsnull;

            i++;
        } while (i < (10 - baseWeight));

        if (!mFont) {
            FillLogFont(baseWeight * 100);
            mFont = CreateFontIndirectW(&mLogFont);
        }

        ReleaseDC((HWND)nsnull, dc);
    }

    return cairo_win32_font_face_create_for_hfont(mFont);
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
gfxWindowsFont::ComputeMetrics(HDC aDC)
{
    SaveDC(aDC);

    cairo_win32_scaled_font_select_font(mScaledFont, aDC);

    const double cairofontfactor = cairo_win32_scaled_font_get_metrics_factor(mScaledFont);
    const double multiplier = cairofontfactor * mStyle->size;

    // Get font metrics
    OUTLINETEXTMETRIC oMetrics;
    TEXTMETRIC& metrics = oMetrics.otmTextMetrics;
    gfxFloat descentPos = 0;

    if (0 < ::GetOutlineTextMetrics(aDC, sizeof(oMetrics), &oMetrics)) {
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
        DWORD len = GetGlyphOutlineW(aDC, PRUnichar('x'), GGO_METRICS, &gm, 0, nsnull, &mMat);

        if (GDI_ERROR != len && gm.gmptGlyphOrigin.y > 0) {
            mMetrics.xHeight = NSToCoordRound(gm.gmptGlyphOrigin.y * multiplier);
        }
        // End -- getting x-height
    }
    else {
        // Make a best-effort guess at extended metrics
        // this is based on general typographic guidelines
        ::GetTextMetrics(aDC, &metrics);
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
    SIZE size;
    ::GetTextExtentPoint32(aDC, " ", 1, &size);
    //size.cx -= font->mOverhangCorrection;
    mMetrics.spaceWidth = NSToCoordRound(size.cx * multiplier);

    cairo_win32_scaled_font_done_font(mScaledFont);

    RestoreDC(aDC, -1);
}

void
gfxWindowsFont::FillLogFont(PRInt16 currentWeight)
{
#define CLIP_TURNOFF_FONTASSOCIATION 0x40

    mLogFont.lfHeight = -NSToCoordRound(mStyle->size * 32);

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
    mLogFont.lfItalic         = (mStyle->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) ? TRUE : FALSE;
    mLogFont.lfWeight         = currentWeight;

    int len = PR_MIN(mName.Length(), LF_FACESIZE);
    memcpy(mLogFont.lfFaceName, mName.get(), len * 2);
    mLogFont.lfFaceName[len] = '\0';
}


/**********************************************************************
 *
 * class gfxWindowsFontGroup
 *
 **********************************************************************/

PRBool
gfxWindowsFontGroup::MakeFont(const nsAString& aName, const nsAString& aGenericName, void *closure)
{
    gfxWindowsFontGroup *fg = NS_STATIC_CAST(gfxWindowsFontGroup*, closure);
    gfxFont *font = new gfxWindowsFont(aName, fg, fg->mDC);
    NS_ADDREF(font);
    fg->mFonts.AppendElement(font);
    return PR_TRUE;
}


gfxWindowsFontGroup::gfxWindowsFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle, HDC dc)
    : gfxFontGroup(aFamilies, aStyle), mDC(dc)
{
    ForEachFont(MakeFont, this);
}

gfxWindowsFontGroup::~gfxWindowsFontGroup()
{

}

gfxTextRun *
gfxWindowsFontGroup::MakeTextRun(const nsAString& aString)
{
    return new gfxWindowsTextRun(aString, this);
}

gfxTextRun *
gfxWindowsFontGroup::MakeTextRun(const nsACString& aString)
{
    return new gfxWindowsTextRun(aString, this);
}

/**********************************************************************
 *
 * class gfxWindowsTextRun
 *
 **********************************************************************/

THEBES_IMPL_REFCOUNTING(gfxWindowsTextRun)

gfxWindowsTextRun::gfxWindowsTextRun(const nsAString& aString, gfxWindowsFontGroup *aFontGroup)
    : mGroup(aFontGroup), mString(aString), mCString(EmptyCString()), mIsASCII(PR_FALSE)
{
}

gfxWindowsTextRun::gfxWindowsTextRun(const nsACString& aString, gfxWindowsFontGroup *aFontGroup)
    : mGroup(aFontGroup), mString(EmptyString()), mCString(aString), mIsASCII(PR_TRUE)
{
}

gfxWindowsTextRun::~gfxWindowsTextRun()
{
}

void
gfxWindowsTextRun::DrawString(gfxContext *aContext, gfxPoint pt)
{
    if (mIsASCII) {
        PRInt32 ret = MeasureOrDrawAscii(aContext, PR_TRUE, pt.x, pt.y, nsnull);
        if (ret != -1) {
            return;
        }
    }

    MeasureOrDrawUniscribe(aContext, PR_TRUE, pt.x, pt.y, nsnull);
}

//#define TIME_MEASURE 1
gfxFloat
gfxWindowsTextRun::MeasureString(gfxContext *aContext)
{
#ifdef TIME_MEASURE
    if (mIsASCII) {
        PRInt32 ret;
        LARGE_INTEGER start, end;

        nsCString foo(mCString);
        printf("%s\n", foo.get());

        QueryPerformanceCounter(&start);
        ret = MeasureOrDrawAscii(aContext, PR_FALSE, 0, 0, nsnull);
        QueryPerformanceCounter(&end);
        printf("ascii: %d\n", end.QuadPart - start.QuadPart);

        QueryPerformanceCounter(&start);
        ret = MeasureOrDrawUniscribe(aContext, PR_FALSE, 0, 0, nsnull);
        QueryPerformanceCounter(&end);
        printf("utf16: %d\n\n", end.QuadPart - start.QuadPart);

        return ret;
    }
#else
    if (mIsASCII) {
        PRInt32 ret = MeasureOrDrawAscii(aContext, PR_FALSE, 0, 0, nsnull);
        if (ret != -1) {
            return ret;
        }
    }
#endif
    return MeasureOrDrawUniscribe(aContext, PR_FALSE, 0, 0, nsnull);
}

gfxWindowsFont *
gfxWindowsTextRun::FindFallbackFont(HDC aDC, const PRUnichar *aString, PRUint32 aLength, gfxWindowsFont *aFont)
{
    HRESULT rv;
    IMultiLanguage *multiLanguage = gfxWindowsPlatform::GetPlatform()->GetMLangService();
    if (!multiLanguage)
        return nsnull;

    // yes, that is right. we're using nsRefPtr's on MSCOM objects.
    nsRefPtr<IMLangFontLink2> langFontLink;
    rv = multiLanguage->QueryInterface(IID_IMLangFontLink2, getter_AddRefs(langFontLink));
    if (FAILED(rv))
        return nsnull;

    DWORD fontCodePages;
    langFontLink->GetFontCodePages(aDC, aFont->GetHFONT(), &fontCodePages);

    DWORD actualCodePages;
    long cchActual;
    rv = langFontLink->GetStrCodePages(aString, aLength, fontCodePages, &actualCodePages, &cchActual);

    HFONT retFont;
    rv = langFontLink->MapFont(aDC, actualCodePages, aString[0], &retFont);
    if (FAILED(rv))
        return nsnull;

    return new gfxWindowsFont(retFont, mGroup, PR_TRUE);
}

PRInt32
gfxWindowsTextRun::MeasureOrDrawAscii(gfxContext *aContext,
                                      PRBool aDraw,
                                      PRInt32 aX, PRInt32 aY,
                                      const PRInt32 *aSpacing)
{
    PRInt32 length = 0;

    if (IsRightToLeft()) {
        NS_WARNING("Trying to draw ASCII text with RTL set.");
        return -1;
    }

    nsRefPtr<gfxASurface> surf = aContext->CurrentGroupSurface();
    if (!surf)
        surf = aContext->CurrentSurface();

    HDC aDC = cairo_win32_surface_get_dc(surf->CairoSurface());
    NS_ASSERTION(aDC, "No DC");

    const char *aString = mCString.BeginReading();
    const PRUint32 aLength = mCString.Length();

    // cairo munges it underneath us
    XFORM savedxform;
    GetWorldTransform(aDC, &savedxform);

    cairo_t *cr = aContext->GetCairo();

    nsRefPtr<gfxWindowsFont> currentFont = NS_STATIC_CAST(gfxWindowsFont*, mGroup->mFonts[0]);
    cairo_font_face_t *fontFace = currentFont->CairoFontFace();
    cairo_scaled_font_t *scaledFont = currentFont->CairoScaledFont();

    cairo_set_font_face(cr, fontFace);
    cairo_set_font_size(cr, mGroup->mStyle.size);

    SaveDC(aDC);

    cairo_win32_scaled_font_select_font(scaledFont, aDC);

    const double cairofontfactor = cairo_win32_scaled_font_get_metrics_factor(scaledFont);
    const double cairoToPixels = cairofontfactor * mGroup->mStyle.size;

    LPWORD glyphs = (LPWORD)malloc(aLength * sizeof(WORD));
    DWORD ret = GetGlyphIndicesA(aDC, aString, aLength, glyphs, GGI_MARK_NONEXISTING_GLYPHS);
    for (PRInt32 i = 0; i < ret; ++i) {
        if (glyphs[i] == 0xffff) {
            free(glyphs);
            cairo_win32_scaled_font_done_font(scaledFont);
            RestoreDC(aDC, -1);
            return -1;
        }
    }

    if (!aDraw) {
        SIZE len;
        GetTextExtentPoint32A(aDC, aString, aLength, &len);
        length = NSToCoordRound(len.cx * cairoToPixels);
    } else {
        GCP_RESULTSA results;
        memset(&results, 0, sizeof(GCP_RESULTS));
        results.lStructSize = sizeof(GCP_RESULTS);
        int stackBuf[1024];
        int *dxBuf = stackBuf;
        if (aLength > sizeof(stackBuf))
            dxBuf = (int *)malloc(aLength * sizeof(int));

        results.nGlyphs = aLength;
        results.lpDx = dxBuf;

        length = GetCharacterPlacementA(aDC, aString, aLength, 0, &results, GCP_USEKERNING);

        const PRInt32 numGlyphs = results.nGlyphs;
        cairo_glyph_t *cglyphs = (cairo_glyph_t*)malloc(numGlyphs*sizeof(cairo_glyph_t));
        double offset = 0;
        for (PRInt32 k = 0; k < numGlyphs; k++) {
            cglyphs[k].index = glyphs[k];
            cglyphs[k].x = aX + offset;
            cglyphs[k].y = aY;
            offset += NSToCoordRound(results.lpDx[k] * cairoToPixels);
        }

        SetWorldTransform(aDC, &savedxform);

        cairo_show_glyphs(cr, cglyphs, numGlyphs);

        if (dxBuf != stackBuf)
            free(dxBuf);
    }

    free(glyphs);

    cairo_win32_scaled_font_done_font(scaledFont);

    RestoreDC(aDC, -1);

    if (aDraw)
        surf->MarkDirty();

    return length;
}


PRInt32
gfxWindowsTextRun::MeasureOrDrawUniscribe(gfxContext *aContext,
                                          PRBool aDraw,
                                          PRInt32 aX, PRInt32 aY,
                                          const PRInt32 *aSpacing)
{
    nsRefPtr<gfxASurface> surf = aContext->CurrentGroupSurface();
    if (!surf)
        surf = aContext->CurrentSurface();

    HDC aDC = cairo_win32_surface_get_dc(surf->CairoSurface());
    NS_ASSERTION(aDC, "No DC");

    const nsAString& theString = (mIsASCII) ? NS_STATIC_CAST(nsAString&, NS_ConvertASCIItoUTF16(mCString)) : mString;
    const PRUnichar *aString = theString.BeginReading();
    const PRUint32 aLength = theString.Length();
    const PRBool isComplex = (::ScriptIsComplex(aString, aLength, SIC_COMPLEX) == S_OK);

    // save the xform so that we can restore to it while cairo
    // munges it underneath us
    XFORM savedxform;
    GetWorldTransform(aDC, &savedxform);

    cairo_t *cr = aContext->GetCairo();

    PRInt32 length = 0;
    HRESULT rv;
    int numItems, maxItems = 5;

    SCRIPT_CONTROL *control = nsnull;
    SCRIPT_STATE *state = nsnull;

    const PRBool rtl = IsRightToLeft();
    if (rtl) {
        control = (SCRIPT_CONTROL *)malloc(sizeof(SCRIPT_CONTROL));
        state = (SCRIPT_STATE *)malloc(sizeof(SCRIPT_STATE));
        memset(control, 0, sizeof(SCRIPT_CONTROL));
        memset(state, 0, sizeof(SCRIPT_STATE));
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
        // don't set this, we want things in visual order (default)
        // items[i].a.fLogicalOrder = 1;

        cairo_font_face_t *fontFace = nsnull;
        cairo_scaled_font_t *scaledFont = nsnull;

        PRBool fontSelected = PR_FALSE;

        nsRefPtr<gfxWindowsFont> currentFont;
        nsRefPtr<gfxWindowsFont> fallbackFont;

DO_TRY_AGAIN:
        int fontIndex = 0;

TRY_AGAIN_SAME_SCRIPT:
        currentFont = NS_STATIC_CAST(gfxWindowsFont*, mGroup->mFonts[fontIndex]);

TRY_AGAIN_HOPE_FOR_THE_BEST:
        if (fallbackFont)
            currentFont = fallbackFont;

        SaveDC(aDC);

        //currentFont->UpdateFonts(cr);
        fontFace = currentFont->CairoFontFace();
        scaledFont = currentFont->CairoScaledFont();

        cairo_set_font_face(cr, fontFace);
        cairo_set_font_size(cr, mGroup->mStyle.size);
        fontSelected = PR_TRUE;

        cairo_win32_scaled_font_select_font(scaledFont, aDC);

        const double cairofontfactor = cairo_win32_scaled_font_get_metrics_factor(scaledFont);
        const double cairoToPixels = cairofontfactor * mGroup->mStyle.size;

        if (!isComplex) {
            // disable shaping for non-complex scripts
            items[i].a.eScript = SCRIPT_UNDEFINED;
        } else {
            rv = ScriptGetCMap(aDC, currentFont->ScriptCache(),
                               itemChars, itemLength,
                               0, glyphs);

            if (rv == S_FALSE) {
                // Some of the Unicode code points were mapped to the default glyph.
                SCRIPT_FONTPROPERTIES scriptProperties;
                ScriptGetFontProperties(aDC, currentFont->ScriptCache(), &scriptProperties);
                

                if (glyphs[0] == scriptProperties.wgDefault) {
                    // ...
                }
            }
        }

        HDC scriptShapeDC = nsnull;
TRY_AGAIN_JUST_SHAPE:
        while ((rv = ScriptShape(scriptShapeDC, currentFont->ScriptCache(),
                                 itemChars, itemLength,
                                 maxGlyphs, &items[i].a,
                                 glyphs, clusters,
                                 attr, &numGlyphs)) == E_OUTOFMEMORY) {

            maxGlyphs *= 2;
            glyphs = (WORD *)realloc(glyphs, maxGlyphs*sizeof(WORD));
            attr = (SCRIPT_VISATTR *)realloc(attr, maxGlyphs*sizeof(SCRIPT_VISATTR));
        }

        if (rv == E_PENDING) {
            scriptShapeDC = aDC;
            if (!fontSelected) {
                cairo_win32_scaled_font_select_font(scaledFont, aDC);
                fontSelected = PR_TRUE;
            }
            goto TRY_AGAIN_JUST_SHAPE;
        }

        if (rv == USP_E_SCRIPT_NOT_IN_FONT) {
            if (fontIndex < mGroup->mFonts.Length() - 1) {
                fontIndex++;
                if (fontSelected)
                    cairo_win32_scaled_font_done_font(scaledFont);
                fontSelected = PR_FALSE;
                RestoreDC(aDC, -1);
                goto TRY_AGAIN_SAME_SCRIPT;
            } else {
                // try again with SCRIPT_UNDEFINED
                items[i].a.eScript = SCRIPT_UNDEFINED;
                if (fontSelected)
                    cairo_win32_scaled_font_done_font(scaledFont);
                fontSelected = PR_FALSE;
                RestoreDC(aDC, -1);
                goto DO_TRY_AGAIN;
            }
        }

        if (numGlyphs > 0 && glyphs[0] == 0) {
            if (fontIndex < mGroup->mFonts.Length() - 1) {
                fontIndex++;
                cairo_win32_scaled_font_done_font(scaledFont);
                RestoreDC(aDC, -1);
                goto TRY_AGAIN_SAME_SCRIPT;
            }

            if (!fallbackFont) {
                // only look for a fallback font once
                fallbackFont = FindFallbackFont(aDC, itemChars, itemLength,
                                                NS_STATIC_CAST(gfxWindowsFont*, mGroup->mFonts[0]));

                if (fallbackFont) {
                    RestoreDC(aDC, -1);
                    goto TRY_AGAIN_HOPE_FOR_THE_BEST;
                }
            }

            // otherwise we fail to draw the characters so give up and continue on.
#ifdef DEBUG_pavlov
            printf("failed to render glyphs :(\n");
#endif
        }

        if (FAILED(rv)) {
            printf("you lose!\n");
        }

        if (SUCCEEDED(rv)) {
            ABC abc;
            GOFFSET *offsets = (GOFFSET *)malloc(numGlyphs*sizeof(GOFFSET));
            int *advance = (int *)malloc(numGlyphs*sizeof(int));

            HDC scriptPlaceDC = scriptShapeDC;
TRY_AGAIN_JUST_PLACE:
            rv = ScriptPlace(scriptPlaceDC, currentFont->ScriptCache(),
                             glyphs, numGlyphs,
                             attr, &items[i].a,
                             advance, offsets, &abc);

            if (rv == E_PENDING) {
                scriptPlaceDC = aDC;
                goto TRY_AGAIN_JUST_PLACE;
            }

            if (FAILED(rv))
                printf("error ScriptPlacing\n");

            if (!aDraw) {
                length += NSToCoordRound((abc.abcA + abc.abcB + abc.abcC) * cairoToPixels);
            } else {
                PRInt32 *spacing = 0;
                PRInt32 justTotal = 0;
                if (aSpacing) {
                    PRUint32 j;
                    /* need to correct for layout/gfx spacing mismatch */
                    // hacky inefficient justification: take the excess of what layout
                    // thinks the width is over what uniscribe thinks the width is and
                    // share it evenly between the justification opportunities
                    PRInt32 layoutTotal = 0;
                    for (j = items[i].iCharPos; j < items[i+1].iCharPos; j++) {
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

                            for (PRUint32 m=0; m<numGlyphs-1; m++) {
                                if (attr[m+1].uJustification > 1) {
                                    --justOpps;
                                    if (justOpps == 0) {
                                        spacing[m] += justTotal;
                                    } else {
                                        spacing[m] += eachJust;
                                        justTotal -= eachJust;
                                    }
                                }
                            }
                        }
#endif
                    }
                }

                cairo_glyph_t *cglyphs = (cairo_glyph_t*)malloc(numGlyphs*sizeof(cairo_glyph_t));
                PRInt32 offset = 0;
                for (PRInt32 k = 0; k < numGlyphs; k++) {
                    cglyphs[k].index = glyphs[k];
                    cglyphs[k].x = aX + offset + NSToCoordRound(offsets[k].du * cairoToPixels);
                    cglyphs[k].y = aY + NSToCoordRound(offsets[k].dv * cairoToPixels);
                    offset += NSToCoordRound(advance[k] * cairoToPixels);

                    /* XXX this needs some testing */
                    if (aSpacing)
                        offset += spacing[k] * cairoToPixels;
                }
#if 0
                /* Draw using ScriptTextOut() */
                SaveDC(aDC);

                gfxMatrix m = aContext->CurrentMatrix();
                XFORM xform;
                double dm[6];
                m.ToValues(&dm[0], &dm[1], &dm[2], &dm[3], &dm[4], &dm[5]);
                xform.eM11 = dm[0];
                xform.eM21 = dm[1];
                xform.eM12 = dm[2];
                xform.eM22 = dm[3];
                xform.eDx  = dm[4];
                xform.eDy  = dm[5];
                SetWorldTransform (aDC, &xform);

                ScriptTextOut(aDC, currentFont->ScriptCache(), aX, aY, 0, NULL, &items->a,
                              NULL, 0, glyphs, numGlyphs,
                              advance, NULL, offsets);

                RestoreDC(aDC, -1);
#else
                /* Draw using cairo */

                /* XXX cairo sets a world transform in order to get subpixel accuracy in some cases;
                 * this breaks it there's a surface fallback that happens with clipping, because
                 * the clip gets applied with the world transform and the world breaks.
                 * We restore the transform to the start of this function while we're drawing.
                 * Need to investigate this further.
                 */

                if (!fontSelected) {
                    cairo_win32_scaled_font_select_font(scaledFont, aDC);
                    fontSelected = PR_TRUE;
                }

                SaveDC(aDC);
                SetWorldTransform(aDC, &savedxform);

                cairo_show_glyphs(cr, cglyphs, numGlyphs);

                RestoreDC(aDC, -1);
#endif
                free(cglyphs);

                aX += NSToCoordRound((abc.abcA + abc.abcB + abc.abcC + justTotal) * cairoToPixels);
                free(spacing);
            }
            free(offsets);
            free(advance);

            cairo_win32_scaled_font_done_font(scaledFont);

            RestoreDC(aDC, -1);

            /* There's a (good) chance that something set a new clip
             * region while inside the SaveDC/RestoreDC; cairo will get
             * very confused, because its clip caching will tell it
             * that the clip is up to date, when in fact it will have
             * been reset.  MarkDirty resets a surface's clip serial,
             * such that it will be reset the next time clipping is
             * necessary. */
            if (aDraw)
                surf->MarkDirty();

        }
        free(glyphs);
        free(clusters);
        free(attr);

    }
  
    free(items);
    if (control)
        free(control);
    if (state)
        free(state);

    return length;
}
