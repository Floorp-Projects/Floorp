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

#define FORCE_PR_LOG

#include "prtypes.h"
#include "gfxTypes.h"

#include "gfxContext.h"
#include "gfxWindowsFonts.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"

#include "nsUnicodeRange.h"
#include "nsUnicharUtils.h"

#include "nsIPref.h"
#include "nsServiceManagerUtils.h"

#include <math.h>

#include "prlog.h"
static PRLogModuleInfo *gFontLog = PR_NewLogModule("winfonts");

#define ROUND(x) floor((x) + 0.5)

inline HDC GetDCFromSurface(gfxASurface *aSurface) {
    if (aSurface->GetType() != gfxASurface::SurfaceTypeWin32) {
        NS_ERROR("gfxWindowsTextRun::MeasureOrDrawFast: Context target is not win32!");
        return nsnull;
    }
    return NS_STATIC_CAST(gfxWindowsSurface*, aSurface)->GetDC();
}

/**********************************************************************
 *
 * class gfxWindowsFont
 *
 **********************************************************************/

gfxWindowsFont::gfxWindowsFont(const nsAString& aName, const gfxFontStyle *aFontStyle)
    : gfxFont(aName, aFontStyle),
      mFont(nsnull), mScriptCache(nsnull),
      mFontFace(nsnull), mScaledFont(nsnull),
      mMetrics(nsnull)
{
}

gfxWindowsFont::~gfxWindowsFont()
{
    Destroy();
}

void
gfxWindowsFont::Destroy()
{
    if (mFontFace)
        cairo_font_face_destroy(mFontFace);

    if (mScaledFont)
        cairo_scaled_font_destroy(mScaledFont);

    if (mFont)
        DeleteObject(mFont);

    ScriptFreeCache(&mScriptCache);

    delete mMetrics;

    mFont = nsnull;
    mScriptCache = nsnull;
    mFontFace = nsnull;
    mScaledFont = nsnull;
    mMetrics = nsnull;
}

const gfxFont::Metrics&
gfxWindowsFont::GetMetrics()
{
    if (!mMetrics)
        ComputeMetrics();

    return *mMetrics;
}

cairo_font_face_t *
gfxWindowsFont::CairoFontFace()
{
    if (!mFontFace)
        mFontFace = MakeCairoFontFace();

    NS_ASSERTION(mFontFace, "Failed to make font face");

    return mFontFace;
}

cairo_scaled_font_t *
gfxWindowsFont::CairoScaledFont()
{
    if (!mScaledFont)
        mScaledFont = MakeCairoScaledFont();

    NS_ASSERTION(mScaledFont, "Failed to make scaled font");

    return mScaledFont;
}

void
gfxWindowsFont::UpdateCTM(const gfxMatrix& aMatrix)
{
    if (aMatrix.ToCairoMatrix().yy == mCTM.ToCairoMatrix().yy)
        return;

    Destroy();

    mCTM = aMatrix;
}

cairo_font_face_t *
gfxWindowsFont::MakeCairoFontFace()
{
    if (mFont)
        return cairo_win32_font_face_create_for_hfont(mFont);

    if (!mWeightTable) {
        nsString name(mName);
        ToLowerCase(name);

        gfxWindowsPlatform *platform = gfxWindowsPlatform::GetPlatform();

        mWeightTable = platform->GetFontWeightTable(name);
        if (!mWeightTable) {
            mWeightTable = new WeightTable();
            platform->PutFontWeightTable(name, mWeightTable);
        }
    }

    PRInt8 baseWeight, weightDistance;
    mStyle->ComputeWeightAndOffset(&baseWeight, &weightDistance);

    HDC dc = nsnull;

    PRUint32 chosenWeight = 0;

    if (weightDistance >= 0) {

        for (PRUint8 i = baseWeight, k = 0; i < 10; i++) {
            if (mWeightTable->HasWeight(i)) {
                k++;
                chosenWeight = i * 100;
            } else if (mWeightTable->TriedWeight(i)) {
                continue;
            } else {
                const PRUint32 tryWeight = i * 100;

                if (!dc)
                    dc = GetDC((HWND)nsnull);

                FillLogFont(tryWeight);
                mFont = CreateFontIndirectW(&mLogFont);
                HGDIOBJ oldFont = SelectObject(dc, mFont);
                TEXTMETRIC metrics;
                GetTextMetrics(dc, &metrics);

                PRBool hasWeight = (metrics.tmWeight == tryWeight);
                mWeightTable->SetWeight(i, hasWeight);
                if (hasWeight) {
                    chosenWeight = i * 100;
                    k++;
                }

                SelectObject(dc, oldFont);
                if (k <= weightDistance) {
                    DeleteObject(mFont);
                    mFont = nsnull;
                }
            }

            if (k > weightDistance) {
                chosenWeight = i * 100;
                break;
            }
        }

    } else if (weightDistance < 0) {
#ifdef DEBUG_pavlov
        printf("dont' support light/lighter yet\n");
#endif
        chosenWeight = baseWeight * 100;
    }

    if (!mFont) {
        FillLogFont(chosenWeight);
        mFont = CreateFontIndirectW(&mLogFont);
    }
    
    if (dc)
        ReleaseDC((HWND)nsnull, dc);

    return cairo_win32_font_face_create_for_hfont(mFont);
}

cairo_scaled_font_t *
gfxWindowsFont::MakeCairoScaledFont()
{
    cairo_scaled_font_t *font = nsnull;

    cairo_matrix_t sizeMatrix;

    cairo_matrix_init_scale(&sizeMatrix, mStyle->size, mStyle->size);

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    font = cairo_scaled_font_create(CairoFontFace(), &sizeMatrix, &mCTM.ToCairoMatrix(), fontOptions);
    cairo_font_options_destroy(fontOptions);

    return font;
}

void
gfxWindowsFont::ComputeMetrics()
{
    if (!mMetrics)
        mMetrics = new gfxFont::Metrics;

    HDC dc = GetDC((HWND)nsnull);

    SaveDC(dc);

    cairo_scaled_font_t *scaledFont = CairoScaledFont();

    cairo_win32_scaled_font_select_font(scaledFont, dc);

    const double cairofontfactor = cairo_win32_scaled_font_get_metrics_factor(scaledFont);
    const double multiplier = cairofontfactor * mStyle->size;

    // Get font metrics
    OUTLINETEXTMETRIC oMetrics;
    TEXTMETRIC& metrics = oMetrics.otmTextMetrics;
    gfxFloat descentPos = 0;

    if (0 < ::GetOutlineTextMetrics(dc, sizeof(oMetrics), &oMetrics)) {
        //    mXHeight = ROUND(oMetrics.otmsXHeight * mDev2App);  XXX not really supported on windows
        mMetrics->xHeight = ROUND((double)metrics.tmAscent * multiplier * 0.56); // 50% of ascent, best guess for true type
        mMetrics->superscriptOffset = ROUND((double)oMetrics.otmptSuperscriptOffset.y * multiplier);
        mMetrics->subscriptOffset = ROUND((double)oMetrics.otmptSubscriptOffset.y * multiplier);
        
        mMetrics->strikeoutSize = PR_MAX(1, ROUND((double)oMetrics.otmsStrikeoutSize * multiplier));
        mMetrics->strikeoutOffset = ROUND((double)oMetrics.otmsStrikeoutPosition * multiplier);
        mMetrics->underlineSize = PR_MAX(1, ROUND((double)oMetrics.otmsUnderscoreSize * multiplier));

        mMetrics->underlineOffset = ROUND((double)oMetrics.otmsUnderscorePosition * multiplier);
        
        // Begin -- section of code to get the real x-height with GetGlyphOutline()
        GLYPHMETRICS gm;
        MAT2 mMat = { 1, 0, 0, 1 };    // glyph transform matrix (always identity in our context)
        DWORD len = GetGlyphOutlineW(dc, PRUnichar('x'), GGO_METRICS, &gm, 0, nsnull, &mMat);

        if (GDI_ERROR != len && gm.gmptGlyphOrigin.y > 0) {
            mMetrics->xHeight = ROUND(gm.gmptGlyphOrigin.y * multiplier);
        }
        // End -- getting x-height
    }
    else {
        // Make a best-effort guess at extended metrics
        // this is based on general typographic guidelines
        ::GetTextMetrics(dc, &metrics);

        mMetrics->xHeight = ROUND((float)metrics.tmAscent * 0.56f); // 56% of ascent, best guess for non-true type
        mMetrics->superscriptOffset = mMetrics->xHeight;     // XXX temporary code!
        mMetrics->subscriptOffset = mMetrics->xHeight;     // XXX temporary code!
        
        mMetrics->strikeoutSize = 1; // XXX this is a guess
        mMetrics->strikeoutOffset = ROUND(mMetrics->xHeight / 2.0f); // 50% of xHeight
        mMetrics->underlineSize = 1; // XXX this is a guess
        mMetrics->underlineOffset = -ROUND((float)metrics.tmDescent * 0.30f); // 30% of descent
    }
    
    mMetrics->internalLeading = ROUND(metrics.tmInternalLeading * multiplier);
    mMetrics->externalLeading = ROUND(metrics.tmExternalLeading * multiplier);
    mMetrics->emHeight = ROUND((metrics.tmHeight - metrics.tmInternalLeading) * multiplier);
    mMetrics->emAscent = ROUND((metrics.tmAscent - metrics.tmInternalLeading) * multiplier);
    mMetrics->emDescent = ROUND(metrics.tmDescent * multiplier);
    mMetrics->maxHeight = ROUND(metrics.tmHeight * multiplier);
    mMetrics->maxAscent = ROUND(metrics.tmAscent * multiplier);
    mMetrics->maxDescent = ROUND(metrics.tmDescent * multiplier);
    mMetrics->maxAdvance = ROUND(metrics.tmMaxCharWidth * multiplier);
    mMetrics->aveCharWidth = PR_MAX(1, ROUND(metrics.tmAveCharWidth * multiplier));

    // Cache the width of a single space.
    SIZE size;
    ::GetTextExtentPoint32(dc, " ", 1, &size);
    //size.cx -= font->mOverhangCorrection;
    mMetrics->spaceWidth = ROUND(size.cx * multiplier);

    cairo_win32_scaled_font_done_font(scaledFont);

    RestoreDC(dc, -1);

    ReleaseDC((HWND)nsnull, dc);
}

void
gfxWindowsFont::FillLogFont(PRInt16 currentWeight)
{
#define CLIP_TURNOFF_FONTASSOCIATION 0x40
    
    const double yScale = mCTM.ToCairoMatrix().yy;

    mLogFont.lfHeight = (LONG)-ROUND(mStyle->size * yScale);

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

    int len = PR_MIN(mName.Length(), LF_FACESIZE - 1);
    memcpy(mLogFont.lfFaceName, nsPromiseFlatString(mName).get(), len * 2);
    mLogFont.lfFaceName[len] = '\0';
}


/**********************************************************************
 *
 * class gfxWindowsFontGroup
 *
 **********************************************************************/

PRBool
gfxWindowsFontGroup::MakeFont(const nsAString& aName, const nsACString& aGenericName, void *closure)
{
    if (!aName.IsEmpty()) {
        gfxWindowsFontGroup *fg = NS_STATIC_CAST(gfxWindowsFontGroup*, closure);

        if (fg->HasFontNamed(aName))
            return PR_TRUE;

        gfxWindowsFont *font = new gfxWindowsFont(aName, fg->GetStyle());
        fg->AppendFont(font);

        if (!aGenericName.IsEmpty() && fg->GetGenericFamily().IsEmpty())
            fg->mGenericFamily = aGenericName;
    }

    return PR_TRUE;
}


gfxWindowsFontGroup::gfxWindowsFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle)
    : gfxFontGroup(aFamilies, aStyle)
{
    mFontCache.Init(25);

    ForEachFont(MakeFont, this);

    if (mGenericFamily.IsEmpty())
        FindGenericFontFromStyle(MakeFont, this);
}

gfxWindowsFontGroup::~gfxWindowsFontGroup()
{

}

gfxTextRun *
gfxWindowsFontGroup::MakeTextRun(const nsAString& aString)
{
    if (aString.IsEmpty()) {
        NS_WARNING("It is illegal to create a gfxTextRun with empty strings");
        return nsnull;
    }
    return new gfxWindowsTextRun(aString, this);
}

gfxTextRun *
gfxWindowsFontGroup::MakeTextRun(const nsACString& aString)
{
    if (aString.IsEmpty()) {
        NS_WARNING("It is illegal to create a gfxTextRun with empty strings");
        return nsnull;
    }
    return new gfxWindowsTextRun(aString, this);
}

/**********************************************************************
 *
 * class gfxWindowsTextRun
 *
 **********************************************************************/

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
gfxWindowsTextRun::Draw(gfxContext *aContext, gfxPoint pt)
{
    double ret = MeasureOrDrawFast(aContext, PR_TRUE, pt);
    if (ret >= 0) {
        return;
    }

    MeasureOrDrawUniscribe(aContext, PR_TRUE, pt);
}

gfxFloat
gfxWindowsTextRun::Measure(gfxContext *aContext)
{
    double ret = MeasureOrDrawFast(aContext, PR_FALSE, gfxPoint(0,0));
    if (ret >= 0.0) {
        return ROUND(ret);
    }
    ret = MeasureOrDrawUniscribe(aContext, PR_FALSE, gfxPoint(0,0));
    return ROUND(ret);
}

void
gfxWindowsTextRun::SetSpacing(const nsTArray<gfxFloat>& spacingArray)
{
    mSpacing = spacingArray;
}

const nsTArray<gfxFloat> *const
gfxWindowsTextRun::GetSpacing() const
{
    return &mSpacing;
}

double
gfxWindowsTextRun::MeasureOrDrawFast(gfxContext *aContext,
                                     PRBool aDraw,
                                     gfxPoint pt)
{
    double length = 0;

    /* this function doesn't handle RTL text. */
    if (IsRightToLeft()) {
        return -1;
    }

    const char *aCString;
    const PRUnichar *aWString;
    PRUint32 aLength;

    if (mIsASCII) {
        aCString = mCString.BeginReading();
        aLength = mCString.Length();
    } else {
        aWString = mString.BeginReading();
        aLength = mString.Length();
        if (ScriptIsComplex(aWString, aLength, SIC_COMPLEX) == S_OK)
            return -1; // try uniscribe instead
    }

    nsRefPtr<gfxASurface> surf = aContext->CurrentSurface();
    HDC aDC = GetDCFromSurface(surf);
    NS_ASSERTION(aDC, "No DC");

    // cairo munges it underneath us
    XFORM savedxform;
    GetWorldTransform(aDC, &savedxform);

    cairo_t *cr = aContext->GetCairo();

    nsRefPtr<gfxWindowsFont> currentFont = mGroup->GetFontAt(0);
    currentFont->UpdateCTM(aContext->CurrentMatrix());
    cairo_font_face_t *fontFace = currentFont->CairoFontFace();
    cairo_scaled_font_t *scaledFont = currentFont->CairoScaledFont();

    cairo_set_font_face(cr, fontFace);
    cairo_set_font_size(cr, mGroup->mStyle.size);

    SaveDC(aDC);

    cairo_win32_scaled_font_select_font(scaledFont, aDC);

    const double cairofontfactor = cairo_win32_scaled_font_get_metrics_factor(scaledFont);
    const double cairoToPixels = cairofontfactor * mGroup->mStyle.size;

    LPWORD glyphs = (LPWORD)malloc(aLength * sizeof(WORD));
    DWORD ret;
    if (mIsASCII)
        ret = GetGlyphIndicesA(aDC, aCString, aLength, glyphs, GGI_MARK_NONEXISTING_GLYPHS);
    else
        ret = GetGlyphIndicesW(aDC, aWString, aLength, glyphs, GGI_MARK_NONEXISTING_GLYPHS);

    if (ret == GDI_ERROR) {
        NS_WARNING("GetGlyphIndicies failed\n");
        free(glyphs);
        cairo_win32_scaled_font_done_font(scaledFont);
        RestoreDC(aDC, -1);
        return 0; // return 0 length
    }

    for (DWORD i = 0; i < ret; ++i) {
        if (glyphs[i] == 0xffff) {
            free(glyphs);
            cairo_win32_scaled_font_done_font(scaledFont);
            RestoreDC(aDC, -1);
            return -1; // try uniscribe instead
        }
    }

    if (!aDraw) {
        SIZE len;
        if (mIsASCII)
            GetTextExtentPoint32A(aDC, aCString, aLength, &len);
        else
            GetTextExtentPoint32W(aDC, aWString, aLength, &len);
        length = ((double)len.cx * cairoToPixels);
    } else {
        PRInt32 numGlyphs;
        int stackBuf[1024];
        int *dxBuf = stackBuf;
        if (aLength > sizeof(stackBuf) / sizeof(int))
            dxBuf = (int *)malloc(aLength * sizeof(int));

        if (mIsASCII) {
            GCP_RESULTSA results;
            memset(&results, 0, sizeof(GCP_RESULTSA));
            results.lStructSize = sizeof(GCP_RESULTSA);

            results.nGlyphs = aLength;
            results.lpDx = dxBuf;

            length = GetCharacterPlacementA(aDC, aCString, aLength, 0, &results, GCP_USEKERNING);

            numGlyphs = results.nGlyphs;
        } else {
            GCP_RESULTSW results;
            memset(&results, 0, sizeof(GCP_RESULTSW));
            results.lStructSize = sizeof(GCP_RESULTSW);

            results.nGlyphs = aLength;
            results.lpDx = dxBuf;

            length = GetCharacterPlacementW(aDC, aWString, aLength, 0, &results, GCP_USEKERNING);

            numGlyphs = results.nGlyphs;
        }

        cairo_glyph_t *cglyphs = (cairo_glyph_t*)malloc(numGlyphs*sizeof(cairo_glyph_t));
        double offset = 0;
#ifdef DEBUG_pavlov
        if (!mSpacing.IsEmpty() && mSpacing.Length() != numGlyphs) {
            printf("different number of glyphs/spacings\n");
        }
#endif

        for (PRInt32 k = 0; k < numGlyphs; k++) {
            cglyphs[k].index = glyphs[k];
            cglyphs[k].x = pt.x + offset;
            cglyphs[k].y = pt.y;

            if (!mSpacing.IsEmpty()) {
                offset += mSpacing[k];
            } else {
                offset += dxBuf[k] * cairoToPixels;
            }
        }

        SetWorldTransform(aDC, &savedxform);

        cairo_show_glyphs(cr, cglyphs, numGlyphs);

        free(cglyphs);

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


/* we map primary language id's to this to look up language codes */
struct ScriptPropertyEntry {
    const char *value;
    const char *langCode;
};

static const struct ScriptPropertyEntry gScriptToText[] =
{
    { nsnull, nsnull },
    { "LANG_ARABIC",     "ara" },
    { "LANG_BULGARIAN",  "bul" },
    { "LANG_CATALAN",    "cat" },
    { "LANG_CHINESE",    "zh-CN" }, //XXX right lang code?
    { "LANG_CZECH",      "cze" }, // cze/ces
    { "LANG_DANISH",     "dan" },
    { "LANG_GERMAN",     "ger" }, // ger/deu
    { "LANG_GREEK",      "el" }, // gre/ell
    { "LANG_ENGLISH",    "x-western" },
    { "LANG_SPANISH",    "spa" },
    { "LANG_FINNISH",    "fin" },
    { "LANG_FRENCH",     "fre" }, // fre/fra
    { "LANG_HEBREW",     "he" }, // heb
    { "LANG_HUNGARIAN",  "hun" },
    { "LANG_ICELANDIC",  "ice" }, // ice/isl
    { "LANG_ITALIAN",    "ita" },
    { "LANG_JAPANESE",   "ja" }, // jpn
    { "LANG_KOREAN",     "ko" }, // kor
    { "LANG_DUTCH",      "dut" }, // dut/nld
    { "LANG_NORWEGIAN",  "nor" },
    { "LANG_POLISH",     "pol" },
    { "LANG_PORTUGUESE", "por" },
    { nsnull, nsnull },
    { "LANG_ROMANIAN",   "rum" }, // rum/ron
    { "LANG_RUSSIAN",    "rus" },
    { "LANG_SERBIAN",    "scc" }, // scc/srp
    { "LANG_SLOVAK",     "slo" }, // slo/slk
    { "LANG_ALBANIAN",   "alb" }, // alb/sqi
    { "LANG_SWEDISH",    "swe" },
    { "LANG_THAI",       "th" }, // tha
    { "LANG_TURKISH",    "tr" }, // tur
    { "LANG_URDU",       "urd" },
    { "LANG_INDONESIAN", "ind" },
    { "LANG_UKRAINIAN",  "ukr" },
    { "LANG_BELARUSIAN", "bel" },
    { "LANG_SLOVENIAN",  "slv" },
    { "LANG_ESTONIAN",   "est" },
    { "LANG_LATVIAN",    "lav" },
    { "LANG_LITHUANIAN", "lit" },
    { nsnull, nsnull },
    { "LANG_FARSI",      "per" }, // per/fas
    { "LANG_VIETNAMESE", "vie" },
    { "LANG_ARMENIAN",   "x-armn" }, // arm/hye
    { "LANG_AZERI",      "aze" },
    { "LANG_BASQUE",     "baq" }, // baq/eus
    { nsnull, nsnull },
    { "LANG_MACEDONIAN", "mac" }, // mac/mkd
    { nsnull, nsnull },
    { nsnull, nsnull },
    { nsnull, nsnull },
    { nsnull, nsnull },
    { nsnull, nsnull },
    { nsnull, nsnull },
    { "LANG_AFRIKAANS",  "afr" },
    { "LANG_GEORGIAN",   "x-geor" }, // geo
    { "LANG_FAEROESE",   "fao" },
    { "LANG_HINDI",      "x-devanagari" }, // hin
    { nsnull, nsnull },
    { nsnull, nsnull },
    { nsnull, nsnull },
    { nsnull, nsnull },
    { "LANG_MALAY",      "may" }, // may/msa
    { "LANG_KAZAK",      "kaz" }, // listed as kazakh?
    { "LANG_KYRGYZ",     "kis" },
    { "LANG_SWAHILI",    "swa" },
    { nsnull, nsnull },
    { "LANG_UZBEK",      "uzb" },
    { "LANG_TATAR",      "tat" },
    { "LANG_BENGALI",    "x-beng" }, // ben
    { "LANG_PUNJABI",    "x-guru" }, // pan -- XXX x-guru is for Gurmukhi which isn't just Punjabi
    { "LANG_GUJARATI",   "x-gujr" }, // guj
    { "LANG_ORIYA",      "ori" },
    { "LANG_TAMIL",      "x-tamil" }, // tam
    { "LANG_TELUGU",     "tel" },
    { "LANG_KANNADA",    "kan" },
    { "LANG_MALAYALAM",  "x-mlym" }, // mal
    { "LANG_ASSAMESE",   "asm" },
    { "LANG_MARATHI",    "mar" },
    { "LANG_SANSKRIT",   "san" },
    { "LANG_MONGOLIAN",  "mon" },
    { "TIBETAN",         "tib" }, // tib/bod
    { nsnull, nsnull },
    { "KHMER",           "x-khmr" }, // khm
    { "LAO",             "lao" },
    { "MYANMAR",         "bur" }, // bur/mya
    { "LANG_GALICIAN",   "glg" },
    { "LANG_KONKANI",    "kok" },
    { "LANG_MANIPURI",   "mni" },
    { "LANG_SINDHI",     "x-devanagari" }, // snd
    { "LANG_SYRIAC",     "syr" },
    { "SINHALESE",       "sin" },
    { "CHEROKEE",        "chr" },
    { "INUKTITUT",       "x-cans" }, // iku
    { "ETHIOPIC",        "x-ethi" }, // amh -- this is both Amharic and Tigrinya
    { nsnull, nsnull },
    { "LANG_KASHMIRI",   "x-devanagari" }, // kas
    { "LANG_NEPALI",     "x-devanagari" }, // nep
    { nsnull, nsnull },
    { nsnull, nsnull },
    { nsnull, nsnull },
    { "LANG_DIVEHI",     "div" }
};

class UniscribeItem
{
public:
    UniscribeItem(gfxContext *aContext, HDC aDC,
                  const PRUnichar *aString, PRUint32 aLength,
                  SCRIPT_ITEM *aItem,
                  gfxWindowsFontGroup *aGroup) :
        mContext(aContext), mDC(aDC),
        mString(aString), mLength(aLength), mScriptItem(aItem), mGroup(aGroup),
        mGlyphs(nsnull), mClusters(nsnull), mAttr(nsnull),
        mNumGlyphs(0), mMaxGlyphs((1.5 * aLength) + 16),
        mOffsets(nsnull), mAdvances(nsnull),
        mFontIndex(0), mTriedPrefFonts(0), mTriedOtherFonts(0), mFontSelected(PR_FALSE)
    {
        mGlyphs = (WORD *)malloc(mMaxGlyphs * sizeof(WORD));
        mClusters = (WORD *)malloc((mLength + 1) * sizeof(WORD));
        mAttr = (SCRIPT_VISATTR *)malloc(mMaxGlyphs * sizeof(SCRIPT_VISATTR));

        /* copy in the fonts from the group in to the item */
        for (PRUint32 i = 0; i < mGroup->FontListLength(); ++i)
            mFonts.AppendElement(mGroup->GetFontAt(i));

        memset(&mABC, 0, sizeof(ABC));
    }

    ~UniscribeItem() {
        free(mGlyphs);
        free(mClusters);
        free(mAttr);
        free(mOffsets);
        free(mAdvances);
    }

    const PRUnichar *GetString() const { return mString; }
    const PRUint32 GetStringLength() const { return mLength; }



    static PRBool AddFontCallback(const nsAString& aName,
                                  const nsACString& aGenericName,
                                  void *closure) {
        if (aName.IsEmpty())
            return PR_TRUE;

        UniscribeItem *item = NS_STATIC_CAST(UniscribeItem*, closure);

        // XXX do something better than this to remove dups
        PRUint32 len = item->mFonts.Length();
        for (PRUint32 i = 0; i < len; ++i)
            if (aName.Equals(item->mFonts[i]->GetName()))
                return PR_TRUE;


        nsRefPtr<gfxWindowsFont> font = item->mGroup->GetCachedFont(aName);
        if (!font) {
            font = new gfxWindowsFont(aName, item->mGroup->GetStyle());
            item->mGroup->PutCachedFont(aName, font);
        }
        item->mFonts.AppendElement(font);

        return PR_TRUE;
    }


    /* possible return values:
       E_PENDING -- means script cache lookup failed, DC needs to be set and font selected in to it.
       USP_E_SCRIPT_NOT_IN_FONT -- this font doesn't support this text. keep trying new fonts.
       if you try all possible fonts, then go back to font 0 after calling DisableShaping and try again
       through all the fonts
    */
    HRESULT Shape() {
        HRESULT rv;

        HDC shapeDC = nsnull;

        while (PR_TRUE) {
            rv = ScriptShape(shapeDC, mCurrentFont->ScriptCache(),
                             mString, mLength,
                             mMaxGlyphs, &mScriptItem->a,
                             mGlyphs, mClusters,
                             mAttr, &mNumGlyphs);

            if (rv == E_OUTOFMEMORY) {
                mMaxGlyphs *= 2;
                mGlyphs = (WORD *)realloc(mGlyphs, mMaxGlyphs * sizeof(WORD));
                mAttr = (SCRIPT_VISATTR *)realloc(mAttr, mMaxGlyphs * sizeof(SCRIPT_VISATTR));
                continue;
            }

            if (rv == E_PENDING) {
                SelectFont();

                shapeDC = mDC;
                continue;
            }

#if 0 // debugging only
            if (rv != USP_E_SCRIPT_NOT_IN_FONT && !shapeDC)
                printf("skipped select-shape %d\n", rv);
#endif
            return rv;
        }
    }

    PRBool ShapingEnabled() {
        return (mScriptItem->a.eScript != SCRIPT_UNDEFINED);
    }
    void DisableShaping() {
        mScriptItem->a.eScript = SCRIPT_UNDEFINED;
    }

    PRBool IsMissingGlyphs() {
        for (int i = 0; i < mNumGlyphs; ++i) {
            if (mGlyphs[i] == 0)
                return PR_TRUE;
        }
        return PR_FALSE;
    }

    HRESULT Place() {
        HRESULT rv;
        SCRIPT_CACHE sc = nsnull;

        mOffsets = (GOFFSET *)malloc(mNumGlyphs * sizeof(GOFFSET));
        mAdvances = (int *)malloc(mNumGlyphs * sizeof(int));

        HDC placeDC = nsnull;

        while (PR_TRUE) {
            rv = ScriptPlace(placeDC, mCurrentFont->ScriptCache(),
                             mGlyphs, mNumGlyphs,
                             mAttr, &mScriptItem->a,
                             mAdvances, mOffsets, &mABC);

            if (rv == E_PENDING) {
                SelectFont();
                placeDC = mDC;
                continue;
            }

            break;
        }

        return rv;
    }

    int ABCLength() {
        return (mABC.abcA + mABC.abcB + mABC.abcC);
    }

    void SetSpacing(nsTArray<gfxFloat> *aSpacing) {
        mSpacing = aSpacing;
    }

    const SCRIPT_PROPERTIES *ScriptProperties() {
        /* we can use this to figure out in some cases the language of the item */
        static const SCRIPT_PROPERTIES **gScriptProperties;
        static int gMaxScript = -1;

        if (gMaxScript == -1) {
            ScriptGetProperties(&gScriptProperties, &gMaxScript);
        }
        return gScriptProperties[mScriptItem->a.eScript];
    }

    cairo_glyph_t *GetCairoGlyphs(const gfxPoint& pt, gfxFloat &offset, PRUint32 *nglyphs) {
        const double cairoToPixels = 1.0;

        cairo_glyph_t *cglyphs = (cairo_glyph_t*)malloc(mNumGlyphs * sizeof(cairo_glyph_t));

        offset = 0;

        const int isRTL = mScriptItem->a.s.uBidiLevel == 1;

#ifdef DEBUG_pavlov
        if (!mSpacing->IsEmpty() && mNumGlyphs != mLength)
            printf("glyph/spacing mismatch\n");
#endif
        PRInt32 m = mScriptItem->iCharPos;
        if (isRTL)
            m += mNumGlyphs - 1;
        for (PRInt32 k = 0; k < mNumGlyphs; k++) {
            cglyphs[k].index = mGlyphs[k];
            cglyphs[k].x = pt.x + offset + (mOffsets[k].du * cairoToPixels);
            cglyphs[k].y = pt.y + (mOffsets[k].dv * cairoToPixels);

            if (!mSpacing->IsEmpty()) {
                // XXX We need to convert char index to cluster index.
                // But we cannot do it until nsTextFrame is refactored.
                offset += mSpacing->ElementAt(isRTL ? m-- : m++);
            } else {
                offset += mAdvances[k] * cairoToPixels;
            }
        }

        *nglyphs = mNumGlyphs;

        return cglyphs;
    }

    gfxWindowsFont *GetNextFont() {
        // for legacy reasons, we must keep this goto label.
TRY_AGAIN_HOPE_FOR_THE_BEST_2:
        if (mFontIndex < mFonts.Length()) {
            nsRefPtr<gfxWindowsFont> font = mFonts[mFontIndex];
            mFontIndex++;
            return font;
        } else if (!mTriedPrefFonts) {
            mTriedPrefFonts = PR_TRUE;

            gfxWindowsPlatform *platform = gfxWindowsPlatform::GetPlatform();

            nsString fonts;

            /* first check with the script properties to see what they think */
            const SCRIPT_PROPERTIES *sp = ScriptProperties();
            WORD primaryId = PRIMARYLANGID(sp->langid);
            WORD subId = SUBLANGID(sp->langid);
            if (!sp->fAmbiguousCharSet) {
                const char *langGroup = gScriptToText[primaryId].langCode;
                if (langGroup) {
                    if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                        PR_LOG(gFontLog, PR_LOG_DEBUG, ("Trying to find fonts for: %s (%s)", langGroup, gScriptToText[primaryId].value));

                    platform->GetPrefFonts(langGroup, fonts);
                    if (!fonts.IsEmpty()) {
                        const nsACString& lg = (langGroup) ? nsDependentCString(langGroup) : EmptyCString();
                        gfxFontGroup::ForEachFont(fonts, lg, UniscribeItem::AddFontCallback, this);
                    }
                } else if (primaryId != 0) {
#ifdef DEBUG_pavlov
                    printf("Couldn't find anything about %d\n", primaryId);
#endif
                }
            } else {
                for (PRUint32 i = 0; i < mLength; ++i) {
                    const PRUnichar ch = mString[i];
                    const char *langGroup = LangGroupFromUnicodeRange(FindCharUnicodeRange(ch));
                    if (langGroup) {
                        if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                            PR_LOG(gFontLog, PR_LOG_DEBUG, ("Trying to find fonts for: %s", langGroup));
                        platform->GetPrefFonts(langGroup, fonts);
                        if (!fonts.IsEmpty()) {
                            const nsACString& lg = (langGroup) ? nsDependentCString(langGroup) : EmptyCString();
                            gfxFontGroup::ForEachFont(fonts, lg, UniscribeItem::AddFontCallback, this);
                        }
                    }
#if 0 // the old code does this -- i think it is sort of a hack
                    else {
                        platform->GetPrefFonts("ja", fonts);
                        if (!fonts.IsEmpty()) {
                            const nsACString& lg = (langGroup) ? nsDependentCString(langGroup) : EmptyCString();
                            gfxFontGroup::ForEachFont(fonts, lg, UniscribeItem::AddFontCallback, this);
                        }
                        platform->GetPrefFonts("zh-CN", fonts);
                        if (!fonts.IsEmpty()) {
                            const nsACString& lg = (langGroup) ? nsDependentCString(langGroup) : EmptyCString();
                            gfxFontGroup::ForEachFont(fonts, lg, UniscribeItem::AddFontCallback, this);
                        }
                        platform->GetPrefFonts("zh-TW", fonts);
                        if (!fonts.IsEmpty()) {
                            const nsACString& lg = (langGroup) ? nsDependentCString(langGroup) : EmptyCString();
                            gfxFontGroup::ForEachFont(fonts, lg, UniscribeItem::AddFontCallback, this);
                        }
                        platform->GetPrefFonts("zh-HK", fonts);
                        if (!fonts.IsEmpty()) {
                            const nsACString& lg = (langGroup) ? nsDependentCString(langGroup) : EmptyCString();
                            gfxFontGroup::ForEachFont(fonts, lg, UniscribeItem::AddFontCallback, this);
                        }
                        platform->GetPrefFonts("ko", fonts);
                        if (!fonts.IsEmpty()) {
                            const nsACString& lg = (langGroup) ? nsDependentCString(langGroup) : EmptyCString();
                            gfxFontGroup::ForEachFont(fonts, lg, UniscribeItem::AddFontCallback, this);
                        }
                    }
#endif
                }
            }
            goto TRY_AGAIN_HOPE_FOR_THE_BEST_2;
        } else if (!mTriedOtherFonts) {
            mTriedOtherFonts = PR_TRUE;
            nsString fonts;
            gfxWindowsPlatform *platform = gfxWindowsPlatform::GetPlatform();

            if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG)) {
                PR_LOG(gFontLog, PR_LOG_DEBUG, ("Looking for other fonts to support the string:"));
                for (PRUint32 la = 0; la < mLength; la++)
                    PR_LOG(gFontLog, PR_LOG_DEBUG, (" - 0x%04x", mString[la]));
            }
            
            platform->FindOtherFonts(mString, mLength,
                                     nsPromiseFlatCString(mGroup->GetStyle()->langGroup).get(),
                                     nsPromiseFlatCString(mGroup->GetGenericFamily()).get(),
                                     fonts);
            if (!fonts.IsEmpty()) {
                if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                    PR_LOG(gFontLog, PR_LOG_DEBUG, ("Got back: %s", NS_LossyConvertUTF16toASCII(fonts).get()));
                gfxFontGroup::ForEachFont(fonts, EmptyCString(), UniscribeItem::AddFontCallback, this);
            }
            goto TRY_AGAIN_HOPE_FOR_THE_BEST_2;
        } else {
            // const SCRIPT_PROPERTIES *sp = item->ScriptProperties();
            // We should try to look up the font based on sp.langgroup
            // if it isn't ambiguious.  Would save us time over doing
            // character by character lookups at least for pref fonts
        }

        return nsnull;
    }

    void ResetFontIndex() {
        mFontIndex = 0;
    }

    void SetCurrentFont(gfxWindowsFont *aFont) {
        if (mCurrentFont != aFont) {
            mCurrentFont = aFont;
            cairo_win32_scaled_font_done_font(mCurrentFont->CairoScaledFont());
            mFontSelected = PR_FALSE;
        }
    }

    void SelectFont() {
        if (mFontSelected)
            return;

        cairo_t *cr = mContext->GetCairo();

        cairo_set_font_face(cr, mCurrentFont->CairoFontFace());
        cairo_set_font_size(cr, mCurrentFont->GetStyle()->size);

        cairo_win32_scaled_font_select_font(mCurrentFont->CairoScaledFont(), mDC);

        mFontSelected = PR_TRUE;
    }

private:
    nsRefPtr<gfxContext> mContext;
    HDC mDC;

    SCRIPT_ITEM *mScriptItem;

    const PRUnichar *mString;
    const PRUint32 mLength;

    gfxWindowsFontGroup *mGroup;

    WORD *mGlyphs;
    WORD *mClusters;
    SCRIPT_VISATTR *mAttr;

    int mMaxGlyphs;
    int mNumGlyphs;

    ABC mABC;
    GOFFSET *mOffsets;
    int *mAdvances;

    nsTArray<gfxFloat> *mSpacing;

    nsTArray< nsRefPtr<gfxWindowsFont> > mFonts;

    PRUint32 mFontIndex;
    PRPackedBool mTriedPrefFonts;
    PRPackedBool mTriedOtherFonts;

    nsRefPtr<gfxWindowsFont> mCurrentFont;
    PRBool mFontSelected;
};

class Uniscribe
{
public:
    Uniscribe(gfxContext *aContext, HDC aDC, const PRUnichar *aString, PRUint32 aLength, PRBool aIsRTL) :
        mContext(aContext), mDC(aDC), mString(aString), mLength(aLength), mIsRTL(aIsRTL),
        mIsComplex(ScriptIsComplex(aString, aLength, SIC_COMPLEX) == S_OK),
        mControl(nsnull), mState(nsnull), mItems(nsnull) {
    }

    void Init() {
        if (mIsRTL) {
            mControl = (SCRIPT_CONTROL *)malloc(sizeof(SCRIPT_CONTROL));
            mState = (SCRIPT_STATE *)malloc(sizeof(SCRIPT_STATE));
            memset(mControl, 0, sizeof(SCRIPT_CONTROL));
            memset(mState, 0, sizeof(SCRIPT_STATE));
            mControl->fNeutralOverride = 1;
            mState->uBidiLevel = 1;
        }
    }

    int Itemize() {
        HRESULT rv;

        int maxItems = 5;

        mItems = (SCRIPT_ITEM *)malloc(maxItems * sizeof(SCRIPT_ITEM));
        while ((rv = ScriptItemize(mString, mLength, maxItems, mControl, mState,
                                   mItems, &mNumItems)) == E_OUTOFMEMORY) {
            maxItems *= 2;
            mItems = (SCRIPT_ITEM *)realloc(mItems, maxItems * sizeof(SCRIPT_ITEM));
        }

        return mNumItems;
    }

    PRUint32 ItemsLength() {
        return mNumItems;
    }

    UniscribeItem *GetItem(PRUint32 i, gfxWindowsFontGroup *aGroup) {
        NS_ASSERTION(i < mNumItems, "Trying to get out of bounds item");

        UniscribeItem *item = new UniscribeItem(mContext, mDC,
                                                mString + mItems[i].iCharPos,
                                                mItems[i+1].iCharPos - mItems[i].iCharPos,
                                                &mItems[i],
                                                aGroup);
        if (!mIsComplex)
            item->DisableShaping();

        return item;
    }

private:
    nsRefPtr<gfxContext> mContext;
    HDC mDC;
    const PRUnichar *mString;
    const PRUint32 mLength;
    const PRBool mIsRTL;
    const PRBool mIsComplex;

    SCRIPT_CONTROL *mControl;
    SCRIPT_STATE *mState;
    SCRIPT_ITEM *mItems;
    int mNumItems;
};


double
gfxWindowsTextRun::MeasureOrDrawUniscribe(gfxContext *aContext,
                                          PRBool aDraw,
                                          gfxPoint pt)
{
    nsRefPtr<gfxASurface> surf = aContext->CurrentSurface();
    HDC aDC = GetDCFromSurface(surf);
    NS_ASSERTION(aDC, "No DC");

    const nsAString& theString = (mIsASCII) ? NS_STATIC_CAST(const nsAString&, NS_ConvertASCIItoUTF16(mCString)) : mString;
    const PRUnichar *aString = theString.BeginReading();
    const PRUint32 aLength = theString.Length();

    const PRBool isRTL = IsRightToLeft();

    // save the xform so that we can restore to it while cairo
    // munges it underneath us
    XFORM savedxform;
    GetWorldTransform(aDC, &savedxform);

    cairo_t *cr = aContext->GetCairo();

    double length = 0.0;
    HRESULT rv;

    Uniscribe us(aContext, aDC, aString, aLength, isRTL);
    us.Init();

    /* itemize the string */
    int numItems = us.Itemize();

    for (int h=0; h < numItems; h++) {
        int i = (isRTL) ? numItems - h - 1 : h;

        PRUint32 fontIndex = 0;

        SaveDC(aDC);

        UniscribeItem *item = us.GetItem(i, mGroup);

        int giveUp = PR_FALSE;

        while (PR_TRUE) {
            nsRefPtr<gfxWindowsFont> font = item->GetNextFont();

            if (font) {
                /* make sure the font is set for the current matrix */
                font->UpdateCTM(aContext->CurrentMatrix());

                /* set the curret font on the item */
                item->SetCurrentFont(font);

                if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                    PR_LOG(gFontLog, PR_LOG_DEBUG, ("trying: %s", NS_LossyConvertUTF16toASCII(font->GetName()).get()));

                rv = item->Shape();

                if (giveUp) {
                    if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                        PR_LOG(gFontLog, PR_LOG_DEBUG, ("%s - gave up", NS_LossyConvertUTF16toASCII(font->GetName()).get()));
                    goto SCRIPT_PLACE;
                }
                if (FAILED(rv) || item->IsMissingGlyphs())
                    continue;
            } else {
#if 0
                /* code to try all the fonts again without shaping on.
                   in general, if we couldn't shape we should probably just give up */
                if (item->ShapingEnabled()) {
                    item->DisableShaping();
                    item->ResetFontIndex();
                    continue;
                }
#endif
                giveUp = PR_TRUE;
                item->DisableShaping();
                item->ResetFontIndex();
                continue;
            }

            if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                PR_LOG(gFontLog, PR_LOG_DEBUG, ("%s - worked", NS_LossyConvertUTF16toASCII(font->GetName()).get()));

SCRIPT_PLACE:
            NS_ASSERTION(SUCCEEDED(rv), "Failed to shape -- we should never hit this");

            rv = item->Place();
            if (FAILED(rv)) {
                if (PR_LOG_TEST(gFontLog, PR_LOG_WARNING))
                    PR_LOG(gFontLog, PR_LOG_WARNING, ("Failed to place with %s", NS_LossyConvertUTF16toASCII(font->GetName()).get()));
                continue;
            }

            break;
        }


        if (!aDraw) {
            length += item->ABCLength();
        } else {
            /* we have to select the font when we draw */
            item->SelectFont();

            item->SetSpacing(&mSpacing);
            gfxFloat offset = 0;
            PRUint32 nglyphs = 0;
            cairo_glyph_t *cglyphs = item->GetCairoGlyphs(pt, offset, &nglyphs);

            /* Draw using cairo */

            /* XXX cairo sets a world transform in order to get subpixel accuracy in some cases;
             * this breaks it there's a surface fallback that happens with clipping, because
             * the clip gets applied with the world transform and the world breaks.
             * We restore the transform to the start of this function while we're drawing.
             * Need to investigate this further.
             */

            XFORM currentxform;
            GetWorldTransform(aDC, &currentxform);
            SetWorldTransform(aDC, &savedxform);

            cairo_show_glyphs(cr, cglyphs, nglyphs);

            SetWorldTransform(aDC, &currentxform);

            free(cglyphs);

            pt.x += offset;
        }

        delete item;

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

    return length;
}
