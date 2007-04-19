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
 *   Masayuki Nakano <masayuki@d-toybox.com>
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
#ifdef DEBUG_smontagu
#define DEBUG_pavlov
#endif

// #define FORCE_UNISCRIBE 1
#define FORCE_PR_LOG

#include "prtypes.h"
#include "gfxTypes.h"

#include "gfxContext.h"
#include "gfxWindowsFonts.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"

#ifdef MOZ_ENABLE_GLITZ
#include "gfxGlitzSurface.h"
#endif

#include "gfxFontTest.h"

#include "cairo.h"
#include "cairo-win32.h"

#include <windows.h>

#include "nsUnicodeRange.h"
#include "nsUnicharUtils.h"

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"

#include "nsCRT.h"

#include <math.h>

#include "prlog.h"
static PRLogModuleInfo *gFontLog = PR_NewLogModule("winfonts");

#define ROUND(x) floor((x) + 0.5)

inline HDC GetDCFromSurface(gfxASurface *aSurface) {
    if (aSurface->GetType() != gfxASurface::SurfaceTypeWin32) {
        NS_ERROR("GetDCFromSurface: Context target is not win32!");
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
      mFont(nsnull), mAdjustedSize(0), mScriptCache(nsnull),
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

HFONT
gfxWindowsFont::GetHFONT()
{
    if (!mFont)
        mFont = MakeHFONT();

    NS_ASSERTION(mFont, "Failed to make HFONT");

    return mFont;
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
    if (aMatrix.yy == mCTM.yy &&
        aMatrix.xx == mCTM.xx &&
        aMatrix.xy == mCTM.xy &&
        aMatrix.yx == mCTM.yx)
        return;

    Destroy();

    mCTM = aMatrix;
}

HFONT
gfxWindowsFont::MakeHFONT()
{
    if (mFont)
        return mFont;

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
    GetStyle()->ComputeWeightAndOffset(&baseWeight, &weightDistance);

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

                FillLogFont(GetStyle()->size, tryWeight);
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

    if (chosenWeight == 0)
        chosenWeight = baseWeight * 100;

    mAdjustedSize = GetStyle()->size;
    if (GetStyle()->sizeAdjust > 0) {
        if (!mFont) {
            FillLogFont(mAdjustedSize, chosenWeight);
            mFont = CreateFontIndirectW(&mLogFont);
        }

        Metrics *oldMetrics = mMetrics;
        ComputeMetrics();
        gfxFloat aspect = mMetrics->xHeight / mMetrics->emHeight;
        mAdjustedSize =
            PR_MAX(ROUND(GetStyle()->size * (GetStyle()->sizeAdjust / aspect)), 1.0f);

        if (mMetrics != oldMetrics) {
            delete mMetrics;
            mMetrics = oldMetrics;
        }
        DeleteObject(mFont);
        mFont = nsnull;
    }

    if (!mFont) {
        FillLogFont(mAdjustedSize, chosenWeight);
        mFont = CreateFontIndirectW(&mLogFont);
    }

    if (dc)
        ReleaseDC((HWND)nsnull, dc);

    return mFont;
}

cairo_font_face_t *
gfxWindowsFont::MakeCairoFontFace()
{
    // ensure mFont is around
    MakeHFONT();

    return cairo_win32_font_face_create_for_hfont(mFont);
}

cairo_scaled_font_t *
gfxWindowsFont::MakeCairoScaledFont()
{
    cairo_scaled_font_t *font = nsnull;

    cairo_matrix_t sizeMatrix;

    MakeHFONT(); // Ensure mAdjustedSize being initialized.
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    font = cairo_scaled_font_create(CairoFontFace(), &sizeMatrix,
                                    reinterpret_cast<cairo_matrix_t*>(&mCTM),
                                    fontOptions);
    cairo_font_options_destroy(fontOptions);

    return font;
}

void
gfxWindowsFont::ComputeMetrics()
{
    if (!mMetrics)
        mMetrics = new gfxFont::Metrics;

    HDC dc = GetDC((HWND)nsnull);

    HFONT font = GetHFONT();

    HGDIOBJ oldFont = SelectObject(dc, font);

    // Get font metrics
    OUTLINETEXTMETRIC oMetrics;
    TEXTMETRIC& metrics = oMetrics.otmTextMetrics;

    if (0 < GetOutlineTextMetrics(dc, sizeof(oMetrics), &oMetrics)) {
        mMetrics->superscriptOffset = (double)oMetrics.otmptSuperscriptOffset.y;
        mMetrics->subscriptOffset = (double)oMetrics.otmptSubscriptOffset.y;
        mMetrics->strikeoutSize = PR_MAX(1, (double)oMetrics.otmsStrikeoutSize);
        mMetrics->strikeoutOffset = (double)oMetrics.otmsStrikeoutPosition;
        mMetrics->underlineSize = PR_MAX(1, (double)oMetrics.otmsUnderscoreSize);
        mMetrics->underlineOffset = (double)oMetrics.otmsUnderscorePosition;
        
        const MAT2 kIdentityMatrix = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
        GLYPHMETRICS gm;
        DWORD len = GetGlyphOutlineW(dc, PRUnichar('x'), GGO_METRICS, &gm, 0, nsnull, &kIdentityMatrix);
        if (len == GDI_ERROR || gm.gmptGlyphOrigin.y <= 0) {
            // 56% of ascent, best guess for true type
            mMetrics->xHeight = ROUND((double)metrics.tmAscent * 0.56);
        } else {
            mMetrics->xHeight = gm.gmptGlyphOrigin.y;
        }
        // The MS (P)Gothic and MS (P)Mincho are not having suitable values
        // in them super script offset. If the values are not suitable,
        // we should use x-height instead of them.
        // See https://bugzilla.mozilla.org/show_bug.cgi?id=353632
        if (mMetrics->superscriptOffset == 0 ||
            mMetrics->superscriptOffset >= metrics.tmAscent) {
            mMetrics->superscriptOffset = mMetrics->xHeight;
        }
        // And also checking the case of sub script offset.
        // The old gfx has checked this too.
        if (mMetrics->subscriptOffset == 0 ||
            mMetrics->subscriptOffset >= metrics.tmAscent) {
            mMetrics->subscriptOffset = mMetrics->xHeight;
        }
    } else {
        // Make a best-effort guess at extended metrics
        // this is based on general typographic guidelines
        GetTextMetrics(dc, &metrics);

        mMetrics->xHeight = ROUND((float)metrics.tmAscent * 0.56f); // 56% of ascent, best guess for non-true type
        mMetrics->superscriptOffset = mMetrics->xHeight;
        mMetrics->subscriptOffset = mMetrics->xHeight;
        mMetrics->strikeoutSize = 1;
        mMetrics->strikeoutOffset = ROUND(mMetrics->xHeight / 2.0f); // 50% of xHeight
        mMetrics->underlineSize = 1;
        mMetrics->underlineOffset = -ROUND((float)metrics.tmDescent * 0.30f); // 30% of descent
    }

    mMetrics->internalLeading = metrics.tmInternalLeading;
    mMetrics->externalLeading = metrics.tmExternalLeading;
    mMetrics->emHeight = (metrics.tmHeight - metrics.tmInternalLeading);
    mMetrics->emAscent = (metrics.tmAscent - metrics.tmInternalLeading);
    mMetrics->emDescent = metrics.tmDescent;
    mMetrics->maxHeight = metrics.tmHeight;
    mMetrics->maxAscent = metrics.tmAscent;
    mMetrics->maxDescent = metrics.tmDescent;
    mMetrics->maxAdvance = metrics.tmMaxCharWidth;
    mMetrics->aveCharWidth = PR_MAX(1, metrics.tmAveCharWidth);

    // Cache the width of a single space.
    SIZE size;
    GetTextExtentPoint32(dc, " ", 1, &size);
    mMetrics->spaceWidth = ROUND(size.cx);

    SelectObject(dc, oldFont);

    ReleaseDC((HWND)nsnull, dc);
}

void
gfxWindowsFont::FillLogFont(gfxFloat aSize, PRInt16 aWeight)
{
#define CLIP_TURNOFF_FONTASSOCIATION 0x40
    
    const double yScale = mCTM.yy;

    mLogFont.lfHeight = (LONG)-ROUND(aSize * yScale);

    if (mLogFont.lfHeight == 0)
        mLogFont.lfHeight = -1;

    // Fill in logFont structure
    mLogFont.lfWidth          = 0; 
    mLogFont.lfEscapement     = 0;
    mLogFont.lfOrientation    = 0;
    mLogFont.lfUnderline      = (GetStyle()->decorations & FONT_DECORATION_UNDERLINE) ? TRUE : FALSE;
    mLogFont.lfStrikeOut      = (GetStyle()->decorations & FONT_DECORATION_STRIKEOUT) ? TRUE : FALSE;
    mLogFont.lfCharSet        = DEFAULT_CHARSET;
#ifndef WINCE
    mLogFont.lfOutPrecision   = OUT_TT_PRECIS;
#else
    mLogFont.lfOutPrecision   = OUT_DEFAULT_PRECIS;
#endif
    mLogFont.lfClipPrecision  = CLIP_TURNOFF_FONTASSOCIATION;
    mLogFont.lfQuality        = DEFAULT_QUALITY;
    mLogFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    mLogFont.lfItalic         = (GetStyle()->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) ? TRUE : FALSE;
    mLogFont.lfWeight         = aWeight;

    int len = PR_MIN(mName.Length(), LF_FACESIZE - 1);
    memcpy(mLogFont.lfFaceName, nsPromiseFlatString(mName).get(), len * 2);
    mLogFont.lfFaceName[len] = '\0';
}


nsString
gfxWindowsFont::GetUniqueName()
{
    nsString uniqueName;

    // make sure this exists, because we're going to read its fields
    MakeHFONT();

    // start with the family name
    uniqueName.Assign(mName);

    // append the weight code
    if (mLogFont.lfWeight != 400) {
        uniqueName.AppendLiteral(":");
        uniqueName.AppendInt((PRInt32)mLogFont.lfWeight);
    }

    // append italic?
    if (mLogFont.lfItalic)
        uniqueName.AppendLiteral(":Italic");

    if (mLogFont.lfUnderline)
        uniqueName.AppendLiteral(":Underline");

    if (mLogFont.lfStrikeOut)
        uniqueName.AppendLiteral(":StrikeOut");

    return uniqueName;
}

void
gfxWindowsFont::Draw(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd,
                     gfxContext *aContext, PRBool aDrawToPath, gfxPoint *aBaselineOrigin,
                     Spacing *aSpacing)
{
    // XXX stuart may want us to do something faster here
    gfxFont::Draw(aTextRun, aStart, aEnd, aContext, aDrawToPath, aBaselineOrigin,
                  aSpacing);
}

void
gfxWindowsFont::SetupCairoFont(cairo_t *aCR)
{
    cairo_set_scaled_font(aCR, CairoScaledFont());
}

/**********************************************************************
 *
 * class gfxWindowsFontGroup
 *
 **********************************************************************/

/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref, append it to the aFonts array, and return it ---
 * except for OOM in which case we do nothing and return null.
 */
static already_AddRefed<gfxWindowsFont>
GetOrMakeFont(const nsAString& aName, const gfxFontStyle *aStyle)
{
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(aName, aStyle);
    if (!font) {
        font = new gfxWindowsFont(aName, aStyle);
        if (!font)
            return nsnull;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nsnull;
    font.swap(f);
    return static_cast<gfxWindowsFont *>(f);
}

PRBool
gfxWindowsFontGroup::MakeFont(const nsAString& aName,
                              const nsACString& aGenericName,
                              void *closure)
{
    if (!aName.IsEmpty()) {
        gfxWindowsFontGroup *fg = NS_STATIC_CAST(gfxWindowsFontGroup*, closure);

        if (fg->HasFontNamed(aName))
            return PR_TRUE;

        nsRefPtr<gfxWindowsFont> font = GetOrMakeFont(aName, fg->GetStyle());
        if (font) {
            fg->AppendFont(font);
        }

        if (!aGenericName.IsEmpty() && fg->GetGenericFamily().IsEmpty())
            fg->mGenericFamily = aGenericName;
    }

    return PR_TRUE;
}


gfxWindowsFontGroup::gfxWindowsFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle)
    : gfxFontGroup(aFamilies, aStyle)
{
    ForEachFont(MakeFont, this);

    if (mGenericFamily.IsEmpty())
        FindGenericFontFromStyle(MakeFont, this);

    if (mFonts.Length() == 0) {
        // Should append default GUI font if there are no available fonts.
        HGDIOBJ hGDI = ::GetStockObject(DEFAULT_GUI_FONT);
        LOGFONTW logFont;
        if (!hGDI ||
            !::GetObjectW(hGDI, sizeof(logFont), &logFont)) {
            NS_ERROR("Failed to create font group");
            return;
        }
        nsAutoString defaultFont(logFont.lfFaceName);
        MakeFont(defaultFont, mGenericFamily, this);
    }
}

gfxWindowsFontGroup::~gfxWindowsFontGroup()
{
}

gfxFontGroup *
gfxWindowsFontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxWindowsFontGroup(mFamilies, aStyle);
}

gfxTextRun *
gfxWindowsFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                                 Parameters *aParams)
{
    // XXX comment out the assertion for now since it fires too much
    //    NS_ASSERTION(!(aParams->mFlags & TEXT_NEED_BOUNDING_BOX),
    //                 "Glyph extents not yet supported");

    gfxTextRun *textRun = new gfxTextRun(aParams, aLength);
    if (!textRun)
        return nsnull;

    textRun->RecordSurrogates(aString);
    
#ifdef FORCE_UNISCRIBE
    const PRBool isComplex = PR_TRUE;
#else
    const PRBool isComplex = ScriptIsComplex(aString, aLength, SIC_COMPLEX) == S_OK ||
                             textRun->IsRightToLeft();
#endif
    if (isComplex)
        InitTextRunUniscribe(aParams->mContext, textRun, aString, aLength);
    else
        InitTextRunGDI(aParams->mContext, textRun, aString, aLength);

    return textRun;
}

gfxTextRun *
gfxWindowsFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                                 Parameters *aParams)
{
    aParams->mFlags |= TEXT_IS_8BIT;
    gfxTextRun *textRun = new gfxTextRun(aParams, aLength);
    if (!textRun)
        return nsnull;

#ifdef FORCE_UNISCRIBE
    const PRBool isComplex = PR_TRUE;
#else
    const PRBool isComplex = textRun->IsRightToLeft();
#endif

    if (isComplex) {
        nsDependentCSubstring cString(reinterpret_cast<const char*>(aString),
                                  reinterpret_cast<const char*>(aString + aLength));
        nsAutoString utf16;
        AppendASCIItoUTF16(cString, utf16);
        InitTextRunUniscribe(aParams->mContext, textRun, utf16.get(), aLength);
    } else {
        InitTextRunGDI(aParams->mContext, textRun,
                       reinterpret_cast<const char*>(aString), aLength);
    }

    return textRun;
}

/**
 * Set the font in the DC for aContext's surface. Return the DC if we're
 * successful. If it's not a win32 surface or something goes wrong or if
 * the font is not a Truetype font (hence GetGlyphIndices may be buggy) then
 * we're not successful and return 0.
 */
static HDC
SetupContextFont(gfxContext *aContext, gfxWindowsFont *aFont)
{
    nsRefPtr<gfxASurface> surf = aContext->CurrentSurface();
    HDC dc = GetDCFromSurface(surf);
    if (!dc)
        return 0;

    // Set the font to have the identity matrix
    aFont->UpdateCTM(gfxMatrix());
    HFONT hfont = aFont->GetHFONT();
    if (!hfont)
        return 0;
    SelectObject(dc, hfont);

    /* GetGlyphIndices is buggy for bitmap and vector fonts,
       so send them to uniscribe */
    TEXTMETRIC metrics;
    GetTextMetrics(dc, &metrics);
    if ((metrics.tmPitchAndFamily & (TMPF_TRUETYPE)) == 0)
        return 0;

    return dc;
}

static PRBool
IsAnyGlyphMissing(WCHAR *aGlyphs, PRUint32 aLength)
{
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
        if (aGlyphs[i] == 0xFFFF)
            return PR_TRUE;
    }
    return PR_FALSE;
}

static PRBool
SetupTextRunFromGlyphs(gfxTextRun *aRun, WCHAR *aGlyphs, HDC aDC,
                       gfxWindowsFont *aFont)
{
    PRUint32 length = aRun->GetLength();
    if (IsAnyGlyphMissing(aGlyphs, length))
        return PR_FALSE;
       
    SIZE size;
    nsAutoTArray<int,500> partialWidthArray;
    if (!partialWidthArray.AppendElements(length))
        return PR_FALSE;
    BOOL success = GetTextExtentExPointI(aDC,
                                         (WORD*) aGlyphs,
                                         length,
                                         INT_MAX,
                                         NULL,
                                         partialWidthArray.Elements(),
                                         &size);
    if (!success)
        return PR_FALSE;

    aRun->AddGlyphRun(aFont, 0);

    gfxTextRun::CompressedGlyph g;
    PRUint32 i;
    PRInt32 lastWidth = 0;
    PRUint32 appUnitsPerDevPixel = aRun->GetAppUnitsPerDevUnit();
    for (i = 0; i < length; ++i) {
        PRInt32 advancePixels = partialWidthArray[i] - lastWidth;
        lastWidth = partialWidthArray[i];
        PRInt32 advanceAppUnits = advancePixels*appUnitsPerDevPixel;
        WCHAR glyph = aGlyphs[i];
        if (advanceAppUnits >= 0 &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advanceAppUnits) &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyph)) {
            aRun->SetCharacterGlyph(i, g.SetSimpleGlyph(advanceAppUnits, glyph));
        } else {
            gfxTextRun::DetailedGlyph details;
            details.mIsLastGlyph = PR_TRUE;
            details.mGlyphID = glyph;
            details.mAdvance = advanceAppUnits;
            details.mXOffset = 0;
            details.mYOffset = 0;
            aRun->SetDetailedGlyphs(i, &details, 1);
        }
    }
    return PR_TRUE;
}

void
gfxWindowsFontGroup::InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun,
                                    const char *aString, PRUint32 aLength)
{
    gfxWindowsFont *font = GetFontAt(0);
    HDC dc = SetupContextFont(aContext, font);
    if (dc) {
        nsAutoTArray<WCHAR,500> glyphArray;
        if (!glyphArray.AppendElements(aLength))
            return;

        DWORD ret = GetGlyphIndicesA(dc, aString, aLength, (WORD*) glyphArray.Elements(),
                                     GGI_MARK_NONEXISTING_GLYPHS);
        if (ret != GDI_ERROR &&
            SetupTextRunFromGlyphs(aRun, glyphArray.Elements(), dc, font))
            return;
    }

    nsDependentCSubstring cString(aString, aString + aLength);
    nsAutoString utf16;
    AppendASCIItoUTF16(cString, utf16);
    InitTextRunUniscribe(aContext, aRun, utf16.get(), aLength);
}

void
gfxWindowsFontGroup::InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun,
                                    const PRUnichar *aString, PRUint32 aLength)
{
    gfxWindowsFont *font = GetFontAt(0);
    HDC dc = SetupContextFont(aContext, font);
    if (dc) {
        nsAutoTArray<WCHAR,500> glyphArray;
        if (!glyphArray.AppendElements(aLength))
            return;

        DWORD ret = GetGlyphIndicesW(dc, aString, aLength, (WORD*) glyphArray.Elements(),
                                     GGI_MARK_NONEXISTING_GLYPHS);
        if (ret != GDI_ERROR &&
            SetupTextRunFromGlyphs(aRun, glyphArray.Elements(), dc, font))
            return;
    }

    InitTextRunUniscribe(aContext, aRun, aString, aLength);
}

/*******************
 * Uniscribe
 *******************/

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

static const char *sCJKLangGroup[] = {
    "ja",
    "ko",
    "zh-CN",
    "zh-HK",
    "zh-TW"
};

#define COUNT_OF_CJK_LANG_GROUP 5
#define CJK_LANG_JA    sCJKLangGroup[0]
#define CJK_LANG_KO    sCJKLangGroup[1]
#define CJK_LANG_ZH_CN sCJKLangGroup[2]
#define CJK_LANG_ZH_HK sCJKLangGroup[3]
#define CJK_LANG_ZH_TW sCJKLangGroup[4]

#define STATIC_STRING_LENGTH 100

/**
 * XXX We could use a bunch of nsAutoTArrays to avoid memory allocation
 * for non-huge strings.
 */
class UniscribeItem
{
public:
    UniscribeItem(gfxContext *aContext, HDC aDC,
                  const PRUnichar *aString, PRUint32 aLength,
                  SCRIPT_ITEM *aItem,
                  gfxWindowsFontGroup *aGroup) :
        mContext(aContext), mDC(aDC), mString(aString),
        mLength(aLength), mAlternativeString(nsnull), mScriptItem(aItem),
        mScript(aItem->a.eScript), mGroup(aGroup),
        mGlyphs(nsnull), mClusters(nsnull), mAttr(nsnull),
        mNumGlyphs(0), mMaxGlyphs((int)(1.5 * aLength) + 16),
        mOffsets(nsnull), mAdvances(nsnull),
        mFontIndex(0), mTriedPrefFonts(PR_FALSE),
        mTriedOtherFonts(PR_FALSE), mAppendedCJKFonts(PR_FALSE),
        mFontSelected(PR_FALSE)
    {
        mGlyphs = (WORD *)malloc(mMaxGlyphs * sizeof(WORD));
        mClusters = (WORD *)malloc((mLength + 1) * sizeof(WORD));
        mAttr = (SCRIPT_VISATTR *)malloc(mMaxGlyphs * sizeof(SCRIPT_VISATTR));

        /* copy in the fonts from the group in to the item */
        for (PRUint32 i = 0; i < mGroup->FontListLength(); ++i)
            mFonts.AppendElement(mGroup->GetFontAt(i));
    }

    ~UniscribeItem() {
        free(mGlyphs);
        free(mClusters);
        free(mAttr);
        free(mOffsets);
        free(mAdvances);
        free(mAlternativeString);
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

        nsRefPtr<gfxWindowsFont> font =
            GetOrMakeFont(aName, item->mGroup->GetStyle());
        if (font) {
            item->mFonts.AppendElement(font);
        }
        return PR_TRUE;
    }

#ifdef DEBUG_pavlov
    HRESULT Break() {
        HRESULT rv;

        SCRIPT_LOGATTR *logAttrs = (SCRIPT_LOGATTR*)malloc(sizeof(SCRIPT_LOGATTR) * mLength);

        rv = ScriptBreak(mString, mLength, &mScriptItem->a, logAttrs);

        for (PRUint32 i = 0; i < mLength; ++i) {
            PR_LOG(gFontLog, PR_LOG_DEBUG, ("0x%04x - %d %d %d %d %d",
                                            mString[i],
                                            logAttrs[i].fSoftBreak,
                                            logAttrs[i].fWhiteSpace,
                                            logAttrs[i].fCharStop,
                                            logAttrs[i].fWordStop,
                                            logAttrs[i].fInvalid));
        }

        free(logAttrs);
        return rv;
    }
#endif

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
            const PRUnichar *str =
                mAlternativeString ? mAlternativeString : mString;
            mScriptItem->a.fLogicalOrder = PR_TRUE;
            mScriptItem->a.s.fDisplayZWG = PR_TRUE;

            rv = ScriptShape(shapeDC, mCurrentFont->ScriptCache(),
                             str, mLength,
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
        // Note: If we disable the shaping by using SCRIPT_UNDEFINED and
        // the string has the surrogate pair, ScriptShape API is
        // *sometimes* crashed. Therefore, we should replace the surrogate
        // pair to U+FFFD. See bug 341500.
        GenerateAlternativeString();
    }

    /* this should only be called if there are no surrogates
     * in the string */
    PRBool IsMissingGlyphsCMap() {
        HRESULT rv;
        HDC cmapDC = nsnull;

        while (PR_TRUE) {
            rv = ScriptGetCMap(cmapDC, mCurrentFont->ScriptCache(),
                               mString, mLength, 0, mGlyphs);

            if (rv == E_PENDING) {
                SelectFont();
                cmapDC = mDC;
                continue;
            }

            if (rv == S_OK)
                return PR_FALSE;

            PR_LOG(gFontLog, PR_LOG_WARNING, ("cmap is missing a glyph"));
            for (PRUint32 i = 0; i < mLength; i++)
                PR_LOG(gFontLog, PR_LOG_WARNING, (" - %d", mGlyphs[i]));
            return PR_TRUE;
        }
    }

    PRBool IsGlyphMissing(SCRIPT_FONTPROPERTIES *aSFP, PRUint32 aGlyphIndex) {
        if (mGlyphs[aGlyphIndex] == aSFP->wgDefault)
            return PR_TRUE;
        return PR_FALSE;
    }

    PRBool IsMissingGlyphs() {
        SCRIPT_FONTPROPERTIES sfp;
        ScriptFontProperties(&sfp);
        PRUint32 charIndex = 0;
        for (int i = 0; i < mNumGlyphs; ++i) {
            if (IsGlyphMissing(&sfp, i))
                return PR_TRUE;
#ifdef DEBUG_pavlov // excess debugging code
            PR_LOG(gFontLog, PR_LOG_DEBUG, ("%04x %04x %04x", sfp.wgBlank, sfp.wgDefault, sfp.wgInvalid));
            PR_LOG(gFontLog, PR_LOG_DEBUG, ("glyph%d - 0x%04x", i, mGlyphs[i]));
            PR_LOG(gFontLog, PR_LOG_DEBUG, ("%04x  --  %04x -- %04x", ScriptProperties()->fInvalidGlyph, mScriptItem->a.fNoGlyphIndex, mAttr[i].fZeroWidth));
#endif
        }
        return PR_FALSE;
    }

    HRESULT Place() {
        HRESULT rv;

        mOffsets = (GOFFSET *)malloc(mNumGlyphs * sizeof(GOFFSET));
        mAdvances = (int *)malloc(mNumGlyphs * sizeof(int));

        HDC placeDC = nsnull;

        while (PR_TRUE) {
            rv = ScriptPlace(placeDC, mCurrentFont->ScriptCache(),
                             mGlyphs, mNumGlyphs,
                             mAttr, &mScriptItem->a,
                             mAdvances, mOffsets, NULL);

            if (rv == E_PENDING) {
                SelectFont();
                placeDC = mDC;
                continue;
            }

            break;
        }

        return rv;
    }

    const SCRIPT_PROPERTIES *ScriptProperties() {
        /* we can use this to figure out in some cases the language of the item */
        static const SCRIPT_PROPERTIES **gScriptProperties;
        static int gMaxScript = -1;

        if (gMaxScript == -1) {
            ScriptGetProperties(&gScriptProperties, &gMaxScript);
        }
        return gScriptProperties[mScript];
    }

    void ScriptFontProperties(SCRIPT_FONTPROPERTIES *sfp) {
        HRESULT rv;

        memset(sfp, 0, sizeof(SCRIPT_FONTPROPERTIES));
        sfp->cBytes = sizeof(SCRIPT_FONTPROPERTIES);
        rv = ScriptGetFontProperties(NULL, mCurrentFont->ScriptCache(),
                                     sfp);
        if (rv == E_PENDING) {
            SelectFont();
            rv = ScriptGetFontProperties(mDC, mCurrentFont->ScriptCache(),
                                         sfp);
        }
    }

    void SetupClusterBoundaries(gfxTextRun *aRun, PRUint32 aOffsetInRun) {
        if (aRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT)
            return;

        nsAutoTArray<SCRIPT_LOGATTR,STATIC_STRING_LENGTH> logAttr;
        if (!logAttr.AppendElements(mLength))
            return;
        HRESULT rv = ScriptBreak(mString, mLength, &mScriptItem->a, logAttr.Elements());
        if (FAILED(rv))
            return;
        gfxTextRun::CompressedGlyph g;
        // The first character is never inside a cluster. Windows might tell us
        // that it should be, but we have no before-character to cluster
        // it with so we just can't cluster it. So skip it here.
        for (PRUint32 i = 1; i < mLength; ++i) {
            if (!logAttr[i].fCharStop) {
                aRun->SetCharacterGlyph(i + aOffsetInRun, g.SetClusterContinuation());
            }
        }
    }

    void SaveGlyphs(gfxTextRun *aRun) {
        PRUint32 offsetInRun = mScriptItem->iCharPos;
        SetupClusterBoundaries(aRun, offsetInRun);

        aRun->AddGlyphRun(GetCurrentFont(), offsetInRun);

        // XXX We should store this in the item and only fetch it once
        SCRIPT_FONTPROPERTIES sfp;
        ScriptFontProperties(&sfp);

        PRUint32 offset = 0;
        nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;
        gfxTextRun::CompressedGlyph g;
        const PRUint32 appUnitsPerDevUnit = aRun->GetAppUnitsPerDevUnit();
        while (offset < mLength) {
            PRUint32 runOffset = offsetInRun + offset;
            if (offset > 0 && mClusters[offset] == mClusters[offset - 1]) {
                if (!aRun->GetCharacterGlyphs()[runOffset].IsClusterContinuation()) {
                    // No glyphs for character 'index', it must be a ligature continuation
                    aRun->SetCharacterGlyph(runOffset, g.SetLigatureContinuation());
                }
            } else {
                // Count glyphs for this character
                PRUint32 k = mClusters[offset];
                PRUint32 glyphCount = mNumGlyphs - k;
                PRUint32 nextClusterOffset;
                PRBool missing = IsGlyphMissing(&sfp, k);
                for (nextClusterOffset = offset + 1; nextClusterOffset < mLength; ++nextClusterOffset) {
                    if (mClusters[nextClusterOffset] > k) {
                        glyphCount = mClusters[nextClusterOffset] - k;
                        break;
                    }
                }
                PRUint32 j;
                for (j = 1; j < glyphCount; ++j) {
                    if (IsGlyphMissing(&sfp, k + j)) {
                        missing = PR_TRUE;
                    }
                }
                PRInt32 advance = mAdvances[k]*appUnitsPerDevUnit;
                WORD glyph = mGlyphs[k];
                if (missing) {
                    aRun->SetMissingGlyph(runOffset, mString[offset]);
                } else if (glyphCount == 1 && advance >= 0 &&
                    mOffsets[k].dv == 0 && mOffsets[k].du == 0 &&
                    gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
                    gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyph)) {
                    aRun->SetCharacterGlyph(runOffset, g.SetSimpleGlyph(advance, glyph));
                } else {
                    if (detailedGlyphs.Length() < glyphCount) {
                        if (!detailedGlyphs.AppendElements(glyphCount - detailedGlyphs.Length()))
                            return;
                    }
                    PRUint32 i;
                    for (i = 0; i < glyphCount; ++i) {
                        gfxTextRun::DetailedGlyph *details = &detailedGlyphs[i];
                        details->mIsLastGlyph = i == glyphCount - 1;
                        details->mGlyphID = mGlyphs[k + i];
                        details->mAdvance = mAdvances[k + i]*appUnitsPerDevUnit;
                        details->mXOffset = float(mOffsets[k + i].du)*appUnitsPerDevUnit;
                        details->mYOffset = float(mOffsets[k + i].dv)*appUnitsPerDevUnit;
                    }
                    aRun->SetDetailedGlyphs(runOffset, detailedGlyphs.Elements(), glyphCount);
                }
            }
            ++offset;
        }
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

            /* first check with the script properties to see what they think */
            const SCRIPT_PROPERTIES *sp = ScriptProperties();
            if (!sp->fAmbiguousCharSet) {
                WORD primaryId = PRIMARYLANGID(sp->langid);
                const char *langGroup = gScriptToText[primaryId].langCode;
                if (langGroup) {
                    if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                        PR_LOG(gFontLog, PR_LOG_DEBUG, ("Trying to find fonts for: %s (%s)", langGroup, gScriptToText[primaryId].value));
                    AppendPrefFonts(langGroup);
                } else if (primaryId != 0) {
#ifdef DEBUG_pavlov
                    printf("Couldn't find anything about %d\n", primaryId);
#endif
                }
            } else {
                for (PRUint32 i = 0; i < mLength; ++i) {
                    const PRUnichar ch = mString[i];
                    PRUint32 unicodeRange = FindCharUnicodeRange(ch);

                    /* special case CJK */
                    if (unicodeRange == kRangeSetCJK) {
                        if (!mAppendedCJKFonts) {
                            mAppendedCJKFonts = PR_TRUE;

                            if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                                PR_LOG(gFontLog, PR_LOG_DEBUG, ("Trying to find fonts for: CJK"));

                            AppendCJKPrefFonts();
                        }
                    } else {
                        const char *langGroup = LangGroupFromUnicodeRange(unicodeRange);
                        if (langGroup) {
                            if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                                PR_LOG(gFontLog, PR_LOG_DEBUG, ("Trying to find fonts for: %s", langGroup));
                            AppendPrefFonts(langGroup);
                        }
                    }
                }
            }
            goto TRY_AGAIN_HOPE_FOR_THE_BEST_2;
        } else if (!mTriedOtherFonts) {
            mTriedOtherFonts = PR_TRUE;
            nsString fonts;
            gfxWindowsPlatform *platform = gfxWindowsPlatform::GetPlatform();

            if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG)) {
                PR_LOG(gFontLog, PR_LOG_DEBUG, ("Looking for other fonts to support the string:"));
                for (PRUint32 la = 0; la < mLength; la++) {
                    PRUint32 ch = mString[la];

                    if ((la+1 < mLength) && NS_IS_HIGH_SURROGATE(ch) && NS_IS_LOW_SURROGATE(mString[la+1])) {
                        la++;
                        ch = SURROGATE_TO_UCS4(ch, mString[la]);
                    }

                    PR_LOG(gFontLog, PR_LOG_DEBUG, (" - 0x%04x", ch));
                }
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

    gfxWindowsFont *GetCurrentFont() {
        return mCurrentFont;
    }

    void SelectFont() {
        if (mFontSelected)
            return;

        cairo_t *cr = mContext->GetCairo();

        cairo_set_font_face(cr, mCurrentFont->CairoFontFace());
        cairo_set_font_size(cr, mCurrentFont->GetAdjustedSize());

        cairo_win32_scaled_font_select_font(mCurrentFont->CairoScaledFont(), mDC);

        mFontSelected = PR_TRUE;
    }


private:
    static PRInt32 GetCJKLangGroupIndex(const char *aLangGroup) {
        PRInt32 i;
        for (i = 0; i < COUNT_OF_CJK_LANG_GROUP; i++) {
            if (!PL_strcasecmp(aLangGroup, sCJKLangGroup[i]))
                return i;
        }
        return -1;
    }

    void AppendPrefFonts(const char *aLangGroup) {
        NS_ASSERTION(aLangGroup, "aLangGroup is null");
        gfxPlatform *platform = gfxPlatform::GetPlatform();
        nsString fonts;
        platform->GetPrefFonts(aLangGroup, fonts);
        if (fonts.IsEmpty())
            return;
        gfxFontGroup::ForEachFont(fonts, nsDependentCString(aLangGroup),
                                  UniscribeItem::AddFontCallback, this);
        return;
   }

   void AppendCJKPrefFonts() {
       nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
       if (!prefs)
           return;

       nsCOMPtr<nsIPrefBranch> prefBranch;
       prefs->GetBranch(0, getter_AddRefs(prefBranch));
       if (!prefBranch)
           return;

       // Add by the order of accept languages.
       nsXPIDLCString list;
       nsresult rv = prefBranch->GetCharPref("intl.accept_languages", getter_Copies(list));
       if (NS_SUCCEEDED(rv) && !list.IsEmpty()) {
           const char kComma = ',';
           const char *p, *p_end;
           list.BeginReading(p);
           list.EndReading(p_end);
           while (p < p_end) {
               while (nsCRT::IsAsciiSpace(*p)) {
                   if (++p == p_end)
                       break;
               }
               if (p == p_end)
                   break;
               const char *start = p;
               while (++p != p_end && *p != kComma)
                   /* nothing */ ;
               nsCAutoString lang(Substring(start, p));
               lang.CompressWhitespace(PR_FALSE, PR_TRUE);
               PRInt32 index = GetCJKLangGroupIndex(lang.get());
               if (index >= 0)
                   AppendPrefFonts(sCJKLangGroup[index]);
               p++;
           }
       }

       // Add the system locale
       switch (::GetACP()) {
           case 932: AppendPrefFonts(CJK_LANG_JA);    break;
           case 936: AppendPrefFonts(CJK_LANG_ZH_CN); break;
           case 949: AppendPrefFonts(CJK_LANG_KO);    break;
           // XXX Don't we need to append CJK_LANG_ZH_HK if the codepage is 950?
           case 950: AppendPrefFonts(CJK_LANG_ZH_TW); break;
       }

       // last resort...
       AppendPrefFonts(CJK_LANG_JA);
       AppendPrefFonts(CJK_LANG_KO);
       AppendPrefFonts(CJK_LANG_ZH_CN);
       AppendPrefFonts(CJK_LANG_ZH_HK);
       AppendPrefFonts(CJK_LANG_ZH_TW);
    }

    void GenerateAlternativeString() {
        if (mAlternativeString)
            free(mAlternativeString);
        mAlternativeString = (PRUnichar *)malloc(mLength * sizeof(PRUnichar));
        memcpy((void *)mAlternativeString, (const void *)mString,
               mLength * sizeof(PRUnichar));
        for (PRUint32 i = 0; i < mLength; i++) {
            if (NS_IS_HIGH_SURROGATE(mString[i]) || NS_IS_LOW_SURROGATE(mString[i]))
                mAlternativeString[i] = PRUnichar(0xFFFD);
        }
    }
private:
    nsRefPtr<gfxContext> mContext;
    HDC mDC;

    SCRIPT_ITEM *mScriptItem;
    WORD mScript;

    const PRUnichar *mString;
    const PRUint32 mLength;

    PRUnichar *mAlternativeString;

    gfxWindowsFontGroup *mGroup;

    WORD *mGlyphs;
    WORD *mClusters;
    SCRIPT_VISATTR *mAttr;

    int mMaxGlyphs;
    int mNumGlyphs;

    GOFFSET *mOffsets;
    int *mAdvances;

    nsTArray< nsRefPtr<gfxWindowsFont> > mFonts;

    nsRefPtr<gfxWindowsFont> mCurrentFont;

    PRUint32 mFontIndex;
    PRPackedBool mTriedPrefFonts;
    PRPackedBool mTriedOtherFonts;
    PRPackedBool mAppendedCJKFonts;
    PRPackedBool mFontSelected;
};

class Uniscribe
{
public:
    Uniscribe(gfxContext *aContext, HDC aDC, const PRUnichar *aString, PRUint32 aLength, PRBool aIsRTL) :
        mContext(aContext), mDC(aDC), mString(aString), mLength(aLength), mIsRTL(aIsRTL),
        mIsComplex(ScriptIsComplex(aString, aLength, SIC_COMPLEX) == S_OK),
        mItems(nsnull) {
    }
    ~Uniscribe() {
        if (mItems)
            free(mItems);
    }

    void Init() {
        memset(&mControl, 0, sizeof(SCRIPT_CONTROL));
        memset(&mState, 0, sizeof(SCRIPT_STATE));
        // Lock the direction. Don't allow the itemizer to change directions
        // based on character type.
        mState.uBidiLevel = mIsRTL;
        mState.fOverrideDirection = PR_TRUE;
    }

    int Itemize() {
        HRESULT rv;

        int maxItems = 5;

        Init();
        mItems = (SCRIPT_ITEM *)malloc(maxItems * sizeof(SCRIPT_ITEM));
        while ((rv = ScriptItemize(mString, mLength, maxItems, &mControl, &mState,
                                   mItems, &mNumItems)) == E_OUTOFMEMORY) {
            maxItems *= 2;
            mItems = (SCRIPT_ITEM *)realloc(mItems, maxItems * sizeof(SCRIPT_ITEM));
            Init();
        }

        return mNumItems;
    }

    PRUint32 ItemsLength() {
        return mNumItems;
    }

    // XXX Why do we dynamically allocate this? We could just fill in an object
    // on the stack.
    UniscribeItem *GetItem(PRUint32 i, gfxWindowsFontGroup *aGroup) {
        NS_ASSERTION(i < (PRUint32)mNumItems, "Trying to get out of bounds item");

        UniscribeItem *item = new UniscribeItem(mContext, mDC,
                                                mString + mItems[i].iCharPos,
                                                mItems[i+1].iCharPos - mItems[i].iCharPos,
                                                &mItems[i],
                                                aGroup);

        return item;
    }

private:
    nsRefPtr<gfxContext> mContext;
    HDC mDC;
    const PRUnichar *mString;
    const PRUint32 mLength;
    const PRBool mIsRTL;
    // XXX not used
    const PRBool mIsComplex;

    SCRIPT_CONTROL mControl;
    SCRIPT_STATE   mState;
    SCRIPT_ITEM   *mItems;
    int mNumItems;
};


void
gfxWindowsFontGroup::InitTextRunUniscribe(gfxContext *aContext, gfxTextRun *aRun, const PRUnichar *aString,
                                         PRUint32 aLength)
{
    nsRefPtr<gfxASurface> surf = aContext->CurrentSurface();
    HDC aDC = GetDCFromSurface(surf);
    NS_ASSERTION(aDC, "No DC");
 
    const PRBool isRTL = aRun->IsRightToLeft();

    HRESULT rv;

    Uniscribe us(aContext, aDC, aString, aLength, isRTL);

    /* itemize the string */
    int numItems = us.Itemize();

    for (int i=0; i < numItems; ++i) {
        PRUint32 fontIndex = 0;

        SaveDC(aDC);

        UniscribeItem *item = us.GetItem(i, this);

        int giveUp = PR_FALSE;

        // Perform font substitution
        while (PR_TRUE) {
            nsRefPtr<gfxWindowsFont> font = item->GetNextFont();

            if (font) {
                /* make sure the font is set for the current matrix */
                font->UpdateCTM(aContext->CurrentMatrix());

                /* set the curret font on the item */
                item->SetCurrentFont(font);

                if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                    PR_LOG(gFontLog, PR_LOG_DEBUG, ("trying: %s", NS_LossyConvertUTF16toASCII(font->GetName()).get()));

                PRBool cmapHasGlyphs = PR_FALSE; // false means "maybe" here

                if (!giveUp && !(aRun->GetFlags() & TEXT_HAS_SURROGATES)) {
                    if (item->IsMissingGlyphsCMap())
                        continue;
                    else
                        cmapHasGlyphs = PR_TRUE;
                }

                rv = item->Shape();

                if (giveUp) {
                    if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                        PR_LOG(gFontLog, PR_LOG_DEBUG, ("%s - gave up", NS_LossyConvertUTF16toASCII(font->GetName()).get()));
                    goto SCRIPT_PLACE;
                }

                if (FAILED(rv))
                    continue;

                if (!cmapHasGlyphs && item->IsMissingGlyphs())
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

#ifdef DEBUG_pavlov
            item->Break();
#endif

            rv = item->Place();
            if (FAILED(rv)) {
                if (PR_LOG_TEST(gFontLog, PR_LOG_WARNING))
                    PR_LOG(gFontLog, PR_LOG_WARNING, ("Failed to place with %s", NS_LossyConvertUTF16toASCII(font->GetName()).get()));
                continue;
            }

            break;
        }

        item->SaveGlyphs(aRun);
        delete item;

        RestoreDC(aDC, -1);
    }
}
