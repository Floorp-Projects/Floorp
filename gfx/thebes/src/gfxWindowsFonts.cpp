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
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   John Daggett <jdaggett@mozilla.com>
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

//#define FORCE_UNISCRIBE 1
#define FORCE_PR_LOG

#include "prtypes.h"
#include "gfxTypes.h"

#include "gfxContext.h"
#include "gfxWindowsFonts.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"

#include "gfxFontTest.h"

#include "cairo.h"
#include "cairo-win32.h"

#include <windows.h>

#include "nsTArray.h"
#include "nsUnicodeRange.h"
#include "nsUnicharUtils.h"

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPrefLocalizedString.h"
#include "nsServiceManagerUtils.h"

#include "nsCRT.h"

#include <math.h>

#include "prlog.h"
#include "prinit.h"
static PRLogModuleInfo *gFontLog = PR_NewLogModule("winfonts");

#define ROUND(x) floor((x) + 0.5)

BYTE 
FontTypeToOutPrecision(PRUint8 fontType)
{
#ifdef WINCE
    return OUT_DEFAULT_PRECIS;
#else
    BYTE ret;
    switch (fontType) {
    case GFX_FONT_TYPE_TT_OPENTYPE:
    case GFX_FONT_TYPE_TRUETYPE:
        ret = OUT_TT_ONLY_PRECIS;
        break;
    case GFX_FONT_TYPE_PS_OPENTYPE:
        ret = OUT_PS_ONLY_PRECIS;
        break;
    case GFX_FONT_TYPE_TYPE1:
        ret = OUT_OUTLINE_PRECIS;
        break;
    case GFX_FONT_TYPE_RASTER:
        ret = OUT_RASTER_PRECIS;
        break;
    case GFX_FONT_TYPE_DEVICE:
        ret = OUT_DEVICE_PRECIS;
        break;
    default:
        ret = OUT_DEFAULT_PRECIS;
    }
    return ret;
#endif
}

struct DCFromContext {
    DCFromContext(gfxContext *aContext) {
        dc = NULL;
        nsRefPtr<gfxASurface> aSurface = aContext->CurrentSurface();
        NS_ASSERTION(aSurface, "DCFromContext: null surface");
        if (aSurface &&
            (aSurface->GetType() == gfxASurface::SurfaceTypeWin32 ||
             aSurface->GetType() == gfxASurface::SurfaceTypeWin32Printing))
        {
            dc = static_cast<gfxWindowsSurface*>(aSurface.get())->GetDC();
            needsRelease = PR_FALSE;
        }
        if (!dc) {
            dc = GetDC(NULL);
            SetGraphicsMode(dc, GM_ADVANCED);
            needsRelease = PR_TRUE;
        }
    }

    ~DCFromContext() {
        if (needsRelease)
            ReleaseDC(NULL, dc);
    }

    operator HDC () {
        return dc;
    }

    HDC dc;
    PRBool needsRelease;
};


/**********************************************************************
 *
 * class FontFamily
 *
 **********************************************************************/
static nsresult
ReadCMAP(HDC hdc, FontEntry *aFontEntry)
{
    const PRUint32 kCMAP = (('c') | ('m' << 8) | ('a' << 16) | ('p' << 24));

    DWORD len = GetFontData(hdc, kCMAP, 0, nsnull, 0);
    if (len == GDI_ERROR || len == 0) // not a truetype font --
        return NS_ERROR_FAILURE;      // we'll treat it as a symbol font

    nsAutoTArray<PRUint8,16384> buffer;
    if (!buffer.AppendElements(len))
        return NS_ERROR_OUT_OF_MEMORY;
    PRUint8 *buf = buffer.Elements();

    DWORD newLen = GetFontData(hdc, kCMAP, 0, buf, len);
    NS_ENSURE_TRUE(newLen == len, NS_ERROR_FAILURE);

    // can't pass bits as references...
    PRPackedBool unicodeFont = aFontEntry->mUnicodeFont;
    PRPackedBool symbolFont = aFontEntry->mSymbolFont;
    nsresult rv = gfxFontUtils::ReadCMAP(buf, len, aFontEntry->mCharacterMap,
                                         unicodeFont, symbolFont);
    aFontEntry->mUnicodeFont = unicodeFont;
    aFontEntry->mSymbolFont = symbolFont;

    return rv;
}

struct FamilyAddStyleProcData {
    HDC dc;
    FontFamily *ff;
};

int CALLBACK 
FontFamily::FamilyAddStylesProc(const ENUMLOGFONTEXW *lpelfe,
                                const NEWTEXTMETRICEXW *nmetrics,
                                DWORD fontType, LPARAM data)
{
    const NEWTEXTMETRICW& metrics = nmetrics->ntmTm;
    LOGFONTW logFont = lpelfe->elfLogFont;

    FamilyAddStyleProcData *faspd = reinterpret_cast<FamilyAddStyleProcData*>(data);
    FontFamily *ff = faspd->ff;
    HDC hdc = faspd->dc;

    // Some fonts claim to support things > 900, but we don't so clamp the sizes
    logFont.lfWeight = PR_MAX(PR_MIN(logFont.lfWeight, 900), 100);


    gfxWindowsFontType feType = FontEntry::DetermineFontType(metrics, fontType);

    FontEntry *fe = nsnull;
    for (PRUint32 i = 0; i < ff->mVariations.Length(); ++i) {
        fe = ff->mVariations[i];
        if (feType > fe->mFontType) {
            // if the new type is better than the old one, remove the old entries
            ff->mVariations.RemoveElementAt(i);
            --i;
        } else if (feType < fe->mFontType) {
            // otherwise if the new type is worse, skip it
            return 1;
        }
    }

    for (PRUint32 i = 0; i < ff->mVariations.Length(); ++i) {
        fe = ff->mVariations[i];
        // check if we already know about this face
        if (fe->mWeight == logFont.lfWeight &&
            fe->mItalic == (logFont.lfItalic == 0xFF)) {
            // update the charset bit here since this could be different
            fe->mCharset[metrics.tmCharSet] = 1;
            return 1; 
        }
    }

    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfOutPrecision = FontTypeToOutPrecision(feType);
    fe = FontEntry::CreateFontEntry(ff->mName, feType, (logFont.lfItalic == 0xFF), (PRUint16) (logFont.lfWeight), nsnull, hdc, &logFont);

    if (!fe)
        return 1;

    ff->mVariations.AppendElement(fe);

    // mark the charset bit
    fe->mCharset[metrics.tmCharSet] = 1;

    fe->mWindowsFamily = logFont.lfPitchAndFamily & 0xF0;
    fe->mWindowsPitch = logFont.lfPitchAndFamily & 0x0F;

    if (nmetrics->ntmFontSig.fsUsb[0] != 0x00000000 &&
        nmetrics->ntmFontSig.fsUsb[1] != 0x00000000 &&
        nmetrics->ntmFontSig.fsUsb[2] != 0x00000000 &&
        nmetrics->ntmFontSig.fsUsb[3] != 0x00000000) {

        // set the unicode ranges
        PRUint32 x = 0;
        for (PRUint32 i = 0; i < 4; ++i) {
            DWORD range = nmetrics->ntmFontSig.fsUsb[i];
            for (PRUint32 k = 0; k < 32; ++k) {
                fe->mUnicodeRanges[x++] = (range & (1 << k)) != 0;
            }
        }
    }

    fe->mIsBadUnderlineFont = ff->mIsBadUnderlineFontFamily;

    return 1;
}

// general cmap reading routines moved to gfxFontUtils.cpp
void
FontFamily::FindStyleVariations()
{
    if (mHasStyles)
        return;
    mHasStyles = PR_TRUE;

    HDC hdc = GetDC(nsnull);
    SetGraphicsMode(hdc, GM_ADVANCED);

    LOGFONTW logFont;
    memset(&logFont, 0, sizeof(LOGFONTW));
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfPitchAndFamily = 0;
    PRUint32 l = PR_MIN(mName.Length(), LF_FACESIZE - 1);
    memcpy(logFont.lfFaceName,
           nsPromiseFlatString(mName).get(),
           l * sizeof(PRUnichar));
    logFont.lfFaceName[l] = 0;

    FamilyAddStyleProcData faspd;
    faspd.dc = hdc;
    faspd.ff = this;

    EnumFontFamiliesExW(hdc, &logFont, (FONTENUMPROCW)FontFamily::FamilyAddStylesProc, (LPARAM)&faspd, 0);

    ReleaseDC(nsnull, hdc);

    // Look for font families without bold variations and add a FontEntry
    // with synthetic bold (weight 600) for them.
    FontEntry *darkestItalic = nsnull;
    FontEntry *darkestNonItalic = nsnull;
    PRUint8 highestItalic = 0, highestNonItalic = 0;
    for (PRUint32 i = 0; i < mVariations.Length(); i++) {
        FontEntry *fe = mVariations[i];
        if (fe->mItalic) {
            if (!darkestItalic || fe->mWeight > darkestItalic->mWeight)
                darkestItalic = fe;
        } else {
            if (!darkestNonItalic || fe->mWeight > darkestNonItalic->mWeight)
                darkestNonItalic = fe;
        }
    }

    if (darkestItalic && darkestItalic->mWeight < 600) {
        FontEntry *newEntry = new FontEntry(*darkestItalic);
        newEntry->mWeight = 600;
        mVariations.AppendElement(newEntry);
    }
    if (darkestNonItalic && darkestNonItalic->mWeight < 600) {
        FontEntry *newEntry = new FontEntry(*darkestNonItalic);
        newEntry->mWeight = 600;
        mVariations.AppendElement(newEntry);
    }
}


FontEntry *
FontFamily::FindFontEntry(const gfxFontStyle& aFontStyle)
{
    PRBool needsBold;
    return static_cast<FontEntry*> (FindFontForStyle(aFontStyle, needsBold));
}

PRBool
FontFamily::FindWeightsForStyle(gfxFontEntry* aFontsForWeights[], const gfxFontStyle& aFontStyle)
{
    if (!mHasStyles)
        FindStyleVariations();

    PRBool italic = (aFontStyle.style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) != 0;
    PRBool matchesSomething;

    for (PRUint32 j = 0; j < 2; j++) {
        matchesSomething = PR_FALSE;
        // build up an array of weights that match the italicness we're looking for
        for (PRUint32 i = 0; i < mVariations.Length(); i++) {
            FontEntry *fe = mVariations[i];
            const PRUint8 weight = (fe->mWeight / 100);
            if (fe->mItalic == italic) {
                aFontsForWeights[weight] = fe;
                matchesSomething = PR_TRUE;
            }
        }
        if (matchesSomething)
            break;
        italic = !italic;
    }

    return matchesSomething;
}

FontEntry* 
FontEntry::CreateFontEntry(const nsAString& aName, gfxWindowsFontType aFontType, PRBool aItalic, PRUint16 aWeight, gfxUserFontData* aUserFontData, HDC hdc, LOGFONTW *aLogFont)
{
    LOGFONTW logFont;
    PRBool needRelease = PR_FALSE;

    // jtdfix - need to set charset, unicode ranges, pitch/family

    FontEntry *fe;

    fe = new FontEntry(aName);
    fe->mFontType = aFontType;
    fe->mUserFontData = aUserFontData;

    fe->mItalic = aItalic;
    fe->mWeight = aWeight;

    if (fe->IsType1())
        fe->mForceGDI = PR_TRUE;

    if (!aLogFont) {
        aLogFont = &logFont;
        FontEntry::FillLogFont(aLogFont, fe, 0, aItalic);
    }

    if (!hdc) {
        hdc = GetDC(nsnull);
        SetGraphicsMode(hdc, GM_ADVANCED);
        needRelease = PR_TRUE;
    }

    HFONT font = CreateFontIndirectW(aLogFont);

    if (font) {
        HFONT oldFont = (HFONT)SelectObject(hdc, font);

        // ReadCMAP may change the values of mUnicodeFont and mSymbolFont
        if (NS_FAILED(::ReadCMAP(hdc, fe))) {
            // Type1 fonts aren't necessarily Unicode but
            // this is the best guess we can make here
            if (fe->IsType1())
                fe->mUnicodeFont = PR_TRUE;
            else
                fe->mUnicodeFont = PR_FALSE;

            // For fonts where we failed to read the character map,
            // we can take a slow path to look up glyphs character by character
            fe->mUnknownCMAP = PR_TRUE;

        } 

        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }

    if (needRelease)
        ReleaseDC(nsnull, hdc);

    return fe;
}


void
FontEntry::FillLogFont(LOGFONTW *aLogFont, FontEntry *aFontEntry, gfxFloat aSize, PRBool aItalic)
{
#define CLIP_TURNOFF_FONTASSOCIATION 0x40
    
    aLogFont->lfHeight = (LONG)-ROUND(aSize);

    if (aLogFont->lfHeight == 0)
        aLogFont->lfHeight = -1;

    // Fill in logFont structure
    aLogFont->lfWidth          = 0;
    aLogFont->lfEscapement     = 0;
    aLogFont->lfOrientation    = 0;
    aLogFont->lfUnderline      = FALSE;
    aLogFont->lfStrikeOut      = FALSE;
    aLogFont->lfCharSet        = DEFAULT_CHARSET;
    aLogFont->lfOutPrecision   = FontTypeToOutPrecision(aFontEntry->mFontType);
    aLogFont->lfClipPrecision  = CLIP_TURNOFF_FONTASSOCIATION;
    aLogFont->lfQuality        = DEFAULT_QUALITY;
    aLogFont->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    // always force lfItalic if we want it.  Font selection code will
    // do its best to give us an italic font entry, but if no face exists
    // it may give us a regular one based on weight.  Windows should
    // do fake italic for us in that case.
    aLogFont->lfItalic         = aItalic;
    aLogFont->lfWeight         = aFontEntry->mWeight;

    int len = PR_MIN(aFontEntry->Name().Length(), LF_FACESIZE - 1);
    memcpy(aLogFont->lfFaceName, nsPromiseFlatString(aFontEntry->Name()).get(), len * 2);
    aLogFont->lfFaceName[len] = '\0';
}


PRBool 
FontEntry::TestCharacterMap(PRUint32 aCh)
{
    if (mUnknownCMAP) {
        if (aCh > 0xFFFF)
            return PR_FALSE;

        // previous code was using the group style
        gfxFontStyle fakeStyle;  
        if (mItalic)
            fakeStyle.style = FONT_STYLE_ITALIC;
        fakeStyle.weight = mWeight * 100;

        nsRefPtr<gfxWindowsFont> font =
            gfxWindowsFont::GetOrMakeFont(this, &fakeStyle);
        if (!font->IsValid())
            return PR_FALSE;

        HDC dc = GetDC((HWND)nsnull);
        SetGraphicsMode(dc, GM_ADVANCED);
        HFONT hfont = font->GetHFONT();
        HFONT oldFont = (HFONT)SelectObject(dc, hfont);

        PRUnichar str[1] = { (PRUnichar)aCh };
        WORD glyph[1];

        PRBool hasGlyph = PR_FALSE;
        if (IsType1()) {
            // Type1 fonts and uniscribe APIs don't get along.  ScriptGetCMap will return E_HANDLE
            DWORD ret = GetGlyphIndicesW(dc, str, 1, glyph, GGI_MARK_NONEXISTING_GLYPHS);
            if (ret != GDI_ERROR && glyph[0] != 0xFFFF)
                hasGlyph = PR_TRUE;
        } else {
            // ScriptGetCMap works better than GetGlyphIndicesW for things like bitmap/vector fonts
            HRESULT rv = ScriptGetCMap(dc, font->ScriptCache(), str, 1, 0, glyph);
            if (rv == S_OK)
                hasGlyph = PR_TRUE;
        }

        if (hasGlyph) {
            mCharacterMap.set(aCh);
            return PR_TRUE;         // jtdcheck -- don't we need to do a ReleaseDC??
        }

        SelectObject(dc, oldFont);
        ReleaseDC(NULL, dc);
    }

    return PR_FALSE;
}

/**********************************************************************
 *
 * class gfxWindowsFont
 *
 **********************************************************************/

gfxWindowsFont::gfxWindowsFont(FontEntry *aFontEntry, const gfxFontStyle *aFontStyle)
    : gfxFont(aFontEntry, aFontStyle),
      mFont(nsnull), mAdjustedSize(0.0), mScriptCache(nsnull),
      mFontFace(nsnull), mScaledFont(nsnull),
      mMetrics(nsnull)
{
    mFontEntry = aFontEntry;
    NS_ASSERTION(mFontEntry, "Unable to find font entry for font.  Something is whack.");

    mFont = MakeHFONT(); // create the HFONT, compute metrics, etc
    NS_ASSERTION(mFont, "Failed to make HFONT");
}

gfxWindowsFont::~gfxWindowsFont()
{
    if (mFontFace)
        cairo_font_face_destroy(mFontFace);

    if (mScaledFont)
        cairo_scaled_font_destroy(mScaledFont);

    if (mFont)
        DeleteObject(mFont);

    ScriptFreeCache(&mScriptCache);

    delete mMetrics;
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
        mFontFace = cairo_win32_font_face_create_for_logfontw_hfont(&mLogFont, mFont);

    NS_ASSERTION(mFontFace, "Failed to make font face");

    return mFontFace;
}

cairo_scaled_font_t *
gfxWindowsFont::CairoScaledFont()
{
    if (!mScaledFont) {
        cairo_matrix_t sizeMatrix;
        cairo_matrix_t identityMatrix;

        cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);
        cairo_matrix_init_identity(&identityMatrix);

        cairo_font_options_t *fontOptions = cairo_font_options_create();
        mScaledFont = cairo_scaled_font_create(CairoFontFace(), &sizeMatrix,
                                               &identityMatrix, fontOptions);
        cairo_font_options_destroy(fontOptions);
    }

    NS_ASSERTION(mAdjustedSize == 0.0 ||
                 cairo_scaled_font_status(mScaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to make scaled font");

    return mScaledFont;
}

HFONT
gfxWindowsFont::MakeHFONT()
{
    if (mFont)
        return mFont;

    mAdjustedSize = GetStyle()->size;
    if (GetStyle()->sizeAdjust > 0.0) {
        if (!mFont) {
            FillLogFont(mAdjustedSize);
            mFont = CreateFontIndirectW(&mLogFont);
        }

        Metrics *oldMetrics = mMetrics;
        ComputeMetrics();
        gfxFloat aspect = mMetrics->xHeight / mMetrics->emHeight;
        mAdjustedSize = GetStyle()->GetAdjustedSize(aspect);

        if (mMetrics != oldMetrics) {
            delete mMetrics;
            mMetrics = oldMetrics;
        }
        DeleteObject(mFont);
        mFont = nsnull;
    }

    if (!mFont) {
        FillLogFont(mAdjustedSize);
        mFont = CreateFontIndirectW(&mLogFont);
    }

    return mFont;
}

void
gfxWindowsFont::ComputeMetrics()
{
    if (!mMetrics)
        mMetrics = new gfxFont::Metrics;
    else
        NS_WARNING("Calling ComputeMetrics multiple times");

    HDC dc = GetDC((HWND)nsnull);
    SetGraphicsMode(dc, GM_ADVANCED);

    HGDIOBJ oldFont = SelectObject(dc, mFont);

    // Get font metrics
    OUTLINETEXTMETRIC oMetrics;
    TEXTMETRIC& metrics = oMetrics.otmTextMetrics;

    if (0 < GetOutlineTextMetrics(dc, sizeof(oMetrics), &oMetrics)) {
        mMetrics->superscriptOffset = (double)oMetrics.otmptSuperscriptOffset.y;
        // Some fonts have wrong sign on their subscript offset, bug 410917.
        mMetrics->subscriptOffset = fabs((double)oMetrics.otmptSubscriptOffset.y);
        mMetrics->strikeoutSize = (double)oMetrics.otmsStrikeoutSize;
        mMetrics->strikeoutOffset = (double)oMetrics.otmsStrikeoutPosition;
        mMetrics->underlineSize = (double)oMetrics.otmsUnderscoreSize;
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
        mMetrics->emHeight = metrics.tmHeight - metrics.tmInternalLeading;
        gfxFloat typEmHeight = (double)oMetrics.otmAscent - (double)oMetrics.otmDescent;
        mMetrics->emAscent = ROUND(mMetrics->emHeight * (double)oMetrics.otmAscent / typEmHeight);
        mMetrics->emDescent = mMetrics->emHeight - mMetrics->emAscent;
    } else {
        // Make a best-effort guess at extended metrics
        // this is based on general typographic guidelines
        
        // GetTextMetrics can fail if the font file has been removed
        // or corrupted recently.
        BOOL result = GetTextMetrics(dc, &metrics);
        if (!result) {
            NS_WARNING("Missing or corrupt font data, fasten your seatbelt");
            mIsValid = PR_FALSE;
            memset(mMetrics, 0, sizeof(*mMetrics));
            SelectObject(dc, oldFont);
            ReleaseDC((HWND)nsnull, dc);
            return;
        }

        mMetrics->xHeight = ROUND((float)metrics.tmAscent * 0.56f); // 56% of ascent, best guess for non-true type
        mMetrics->superscriptOffset = mMetrics->xHeight;
        mMetrics->subscriptOffset = mMetrics->xHeight;
        mMetrics->strikeoutSize = 1;
        mMetrics->strikeoutOffset = ROUND(mMetrics->xHeight / 2.0f); // 50% of xHeight
        mMetrics->underlineSize = 1;
        mMetrics->underlineOffset = -ROUND((float)metrics.tmDescent * 0.30f); // 30% of descent
        mMetrics->emHeight = metrics.tmHeight - metrics.tmInternalLeading;
        mMetrics->emAscent = metrics.tmAscent - metrics.tmInternalLeading;
        mMetrics->emDescent = metrics.tmDescent;
    }

    mMetrics->internalLeading = metrics.tmInternalLeading;
    mMetrics->externalLeading = metrics.tmExternalLeading;
    mMetrics->maxHeight = metrics.tmHeight;
    mMetrics->maxAscent = metrics.tmAscent;
    mMetrics->maxDescent = metrics.tmDescent;
    mMetrics->maxAdvance = metrics.tmMaxCharWidth;
    mMetrics->aveCharWidth = PR_MAX(1, metrics.tmAveCharWidth);
    // The font is monospace when TMPF_FIXED_PITCH is *not* set!
    // See http://msdn2.microsoft.com/en-us/library/ms534202(VS.85).aspx
    if (!(metrics.tmPitchAndFamily & TMPF_FIXED_PITCH)) {
      mMetrics->maxAdvance = mMetrics->aveCharWidth;
    }

    // Cache the width of a single space.
    SIZE size;
    GetTextExtentPoint32W(dc, L" ", 1, &size);
    mMetrics->spaceWidth = ROUND(size.cx);

    // Cache the width of digit zero.
    // XXX MSDN (http://msdn.microsoft.com/en-us/library/ms534223.aspx)
    // does not say what the failure modes for GetTextExtentPoint32 are -
    // is it safe to assume it will fail iff the font has no '0'?
    if (GetTextExtentPoint32W(dc, L"0", 1, &size))
        mMetrics->zeroOrAveCharWidth = ROUND(size.cx);
    else
        mMetrics->zeroOrAveCharWidth = mMetrics->aveCharWidth;

    mSpaceGlyph = 0;
    if (metrics.tmPitchAndFamily & TMPF_TRUETYPE) {
        WORD glyph;
        DWORD ret = GetGlyphIndicesW(dc, L" ", 1, &glyph,
                                     GGI_MARK_NONEXISTING_GLYPHS);
        if (ret != GDI_ERROR && glyph != 0xFFFF) {
            mSpaceGlyph = glyph;
        }
    }

    SelectObject(dc, oldFont);

    ReleaseDC((HWND)nsnull, dc);

    SanitizeMetrics(mMetrics, GetFontEntry()->mIsBadUnderlineFont);
}

void
gfxWindowsFont::FillLogFont(gfxFloat aSize)
{
    FontEntry::FillLogFont(&mLogFont, GetFontEntry(), aSize, (GetStyle()->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)));
}


nsString
gfxWindowsFont::GetUniqueName()
{
    nsString uniqueName;

    // start with the family name
    uniqueName.Assign(GetName());

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

FontEntry*
gfxWindowsFont::GetFontEntry()
{
    return static_cast<FontEntry*> (mFontEntry.get()); 
}

PRBool
gfxWindowsFont::SetupCairoFont(gfxContext *aContext)
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
already_AddRefed<gfxWindowsFont>
gfxWindowsFont::GetOrMakeFont(FontEntry *aFontEntry, const gfxFontStyle *aStyle)
{
    // because we know the FontEntry has the weight we really want, use it for matching
    // things in the cache so we don't end up with things like 402 in there.
    gfxFontStyle style(*aStyle);
    style.weight = aFontEntry->mWeight;
    // also pre-round the size if there is no size adjust
    if (style.sizeAdjust == 0.0)
        style.size = ROUND(style.size);

    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(aFontEntry->Name(), &style);
    if (!font) {
        font = new gfxWindowsFont(aFontEntry, &style);
        if (!font)
            return nsnull;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nsnull;
    font.swap(f);
    return static_cast<gfxWindowsFont *>(f);
}

static PRBool
AddFontNameToArray(const nsAString& aName,
                   const nsACString& aGenericName,
                   void *closure)
{
    if (!aName.IsEmpty()) {
        nsTArray<nsString> *list = static_cast<nsTArray<nsString> *>(closure);

        if (list->IndexOf(aName) == list->NoIndex)
            list->AppendElement(aName);
    }

    return PR_TRUE;
}


void
gfxWindowsFontGroup::GroupFamilyListToArrayList(nsTArray<nsRefPtr<FontEntry> > *list)
{
    nsAutoTArray<nsString, 15> fonts;
    ForEachFont(AddFontNameToArray, &fonts);

    PRUint32 len = fonts.Length();
    for (PRUint32 i = 0; i < len; ++i) {
        nsRefPtr<FontEntry> fe;
        
        // first, look up in the user font set
        gfxFontEntry *gfe;
        PRBool needsBold;
        if (mUserFontSet && (gfe = mUserFontSet->FindFontEntry(fonts[i], mStyle, needsBold))) {
            // assume for now platform font if not SVG
            fe = static_cast<FontEntry*> (gfe);
        }
    
        // nothing in the user font set ==> check system fonts
        if (!fe) {
            fe = gfxWindowsPlatform::GetPlatform()->FindFontEntry(fonts[i], mStyle);
        }

        // if found, add to the list
        if (fe) {
            list->AppendElement(fe);
        }
    }
}

void
gfxWindowsFontGroup::FamilyListToArrayList(const nsString& aFamilies,
                                           const nsCString& aLangGroup,
                                           nsTArray<nsRefPtr<FontEntry> > *list)
{
    nsAutoTArray<nsString, 15> fonts;
    ForEachFont(aFamilies, aLangGroup, AddFontNameToArray, &fonts);

    PRUint32 len = fonts.Length();
    for (PRUint32 i = 0; i < len; ++i) {
        const nsString& str = fonts[i];
        nsRefPtr<FontEntry> fe = gfxWindowsPlatform::GetPlatform()->FindFontEntry(str, mStyle);
        list->AppendElement(fe);
    }
}

gfxWindowsFontGroup::gfxWindowsFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle, gfxUserFontSet *aUserFontSet)
    : gfxFontGroup(aFamilies, aStyle, aUserFontSet)
{
    InitFontList();
}

gfxWindowsFontGroup::~gfxWindowsFontGroup()
{
}

gfxWindowsFont *
gfxWindowsFontGroup::GetFontAt(PRInt32 i)
{
    // If it turns out to be hard for all clients that cache font
    // groups to call UpdateFontList at appropriate times, we could
    // instead consider just calling UpdateFontList from someplace
    // more central (such as here).
    NS_ASSERTION(!mUserFontSet || mCurrGeneration == GetGeneration(),
                 "Whoever was caching this font group should have "
                 "called UpdateFontList on it");

    if (!mFonts[i]) {
        nsRefPtr<gfxWindowsFont> font =
            gfxWindowsFont::GetOrMakeFont(mFontEntries[i], &mStyle);
        mFonts[i] = font;
    }

    return static_cast<gfxWindowsFont*>(mFonts[i].get());
}

gfxFontGroup *
gfxWindowsFontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxWindowsFontGroup(mFamilies, aStyle, mUserFontSet);
}

void 
gfxWindowsFontGroup::UpdateFontList()
{
    // if user font set is set, check to see if font list needs updating
    if (mUserFontSet && mCurrGeneration != GetGeneration()) {
        // xxx - can probably improve this to detect when all fonts were found, so no need to update list
        mFonts.Clear();
        mFontEntries.Clear();
        InitFontList();
        mCurrGeneration = GetGeneration();
    }

}

void 
gfxWindowsFontGroup::InitFontList()
{
    GroupFamilyListToArrayList(&mFontEntries);

    mFonts.AppendElements(mFontEntries.Length());

    // Ensure that the first font is usable. Precompute its metrics since
    // we'll surely need them anyway.
    while (mFontEntries.Length() > 0) {
        nsRefPtr<gfxWindowsFont> font =
            gfxWindowsFont::GetOrMakeFont(mFontEntries[0], &mStyle);
        if (!font->IsValid()) {
            mFontEntries.RemoveElementAt(0);
            mFonts.RemoveElementAt(0);
            continue;
        }
        mFonts[0] = font;
        break;
    }

    if (mFontEntries.Length() == 0) {
        // It is pretty important that we have at least one font, so
        // try a few system fonts that should be there.
        nsAutoString str;
        HGDIOBJ hGDI = ::GetStockObject(DEFAULT_GUI_FONT);
        LOGFONTW logFont;
        if (hGDI && ::GetObjectW(hGDI, sizeof(logFont), &logFont)) {
            str.AppendLiteral("\"");
            str.Append(nsDependentString(logFont.lfFaceName));
            str.AppendLiteral("\"");
        }

        NONCLIENTMETRICSW ncm;
        ncm.cbSize = sizeof(ncm);
        BOOL status = ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 
                                              sizeof(ncm), &ncm, 0);
        if (status) {
            str.AppendLiteral(",\"");
            str.Append(nsDependentString(ncm.lfMessageFont.lfFaceName));
            str.AppendLiteral("\"");
        }

        FamilyListToArrayList(str, mStyle.langGroup, &mFontEntries);

        // Keep length of mFonts in sync with length of mFontEntries.
        // Maybe we should eagerly set up mFonts[0] like we do above,
        // but if the resulting gfxWindowsFont is invalid then we can't
        // do much anyway. In that case the font will return zero metrics,
        // its mUnknownCMAP will be set to true, and HasCharacter will
        // just report false for all characters, so the fact that the font
        // is bogus should not cause problems.
        mFonts.AppendElements(mFontEntries.Length());
    }

    if (!mStyle.systemFont) {
        for (PRUint32 i = 0; i < mFontEntries.Length(); ++i) {
            if (mFontEntries[i]->mIsBadUnderlineFont) {
                gfxFloat first = GetFontAt(0)->GetMetrics().underlineOffset;
                gfxFloat bad = GetFontAt(i)->GetMetrics().underlineOffset;
                mUnderlineOffset = PR_MIN(first, bad);
                break;
            }
        }
    }

}

static PRBool
CanTakeFastPath(PRUint32 aFlags)
{
    // Can take fast path only if OPTIMIZE_SPEED is set and IS_RTL isn't
    // We need to always use Uniscribe for RTL text, in case glyph mirroring is required
    return (aFlags &
            (gfxTextRunFactory::TEXT_OPTIMIZE_SPEED | gfxTextRunFactory::TEXT_IS_RTL)) ==
        gfxTextRunFactory::TEXT_OPTIMIZE_SPEED;
}

gfxTextRun *
gfxWindowsFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                                 const Parameters *aParams, PRUint32 aFlags)
{
    // XXX comment out the assertion for now since it fires too much
    //    NS_ASSERTION(!(mFlags & TEXT_NEED_BOUNDING_BOX),
    //                 "Glyph extents not yet supported");

    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;
    NS_ASSERTION(aParams->mContext, "MakeTextRun called without a gfxContext");

    textRun->RecordSurrogates(aString);
    
#ifdef FORCE_UNISCRIBE
    const PRBool isComplex = PR_TRUE;
#else
    const PRBool isComplex = !CanTakeFastPath(aFlags) ||
                             ScriptIsComplex(aString, aLength, SIC_COMPLEX) == S_OK;
#endif
    if (isComplex)
        InitTextRunUniscribe(aParams->mContext, textRun, aString, aLength);
    else
        InitTextRunGDI(aParams->mContext, textRun, aString, aLength);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

gfxTextRun *
gfxWindowsFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                                 const Parameters *aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "should be marked 8bit");
 
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;
    NS_ASSERTION(aParams->mContext, "MakeTextRun called without a gfxContext");

#ifdef FORCE_UNISCRIBE
    const PRBool isComplex = PR_TRUE;
#else
    const PRBool isComplex = !CanTakeFastPath(aFlags);
#endif

    /* We can only call GDI "A" functions if this is a true 7bit ASCII string,
       because they interpret code points from 0x80-0xFF as if they were
       in the system code page. */
    if (!isComplex && (aFlags & TEXT_IS_ASCII)) {
        InitTextRunGDI(aParams->mContext, textRun,
                       reinterpret_cast<const char*>(aString), aLength);
    }
    else {
        nsDependentCSubstring cString(reinterpret_cast<const char*>(aString),
                                  reinterpret_cast<const char*>(aString + aLength));
        nsAutoString utf16;
        AppendASCIItoUTF16(cString, utf16);
        if (isComplex) {
            InitTextRunUniscribe(aParams->mContext, textRun, utf16.get(), aLength);
        } else {
            InitTextRunGDI(aParams->mContext, textRun, utf16.get(), aLength);
        }
    }

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

/**
 * Set the font in the given DC.  If something goes wrong or if the
 * font is not a Truetype font (hence GetGlyphIndices may be buggy)
 * then we're not successful and return PR_FALSE, otherwise PR_TRUE.
 */
static PRBool
SetupDCFont(HDC dc, gfxWindowsFont *aFont)
{
    HFONT hfont = aFont->GetHFONT();
    if (!hfont)
        return PR_FALSE;
    SelectObject(dc, hfont);

    // GetGlyphIndices is buggy for bitmap and vector fonts, so send them to uniscribe
    // Also sent Symbol fonts through Uniscribe as it has special code to deal with them
    if (!aFont->GetFontEntry()->IsTrueType() || aFont->GetFontEntry()->mSymbolFont)
        return PR_FALSE;

    return PR_TRUE;
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
    if (!partialWidthArray.SetLength(length))
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
        NS_ASSERTION(!gfxFontGroup::IsInvalidChar(aRun->GetChar(i)),
                     "Invalid character detected!");
        if (advanceAppUnits >= 0 &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advanceAppUnits) &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyph)) {
            aRun->SetSimpleGlyph(i, g.SetSimpleGlyph(advanceAppUnits, glyph));
        } else {
            gfxTextRun::DetailedGlyph details;
            details.mGlyphID = glyph;
            details.mAdvance = advanceAppUnits;
            details.mXOffset = 0;
            details.mYOffset = 0;
            aRun->SetGlyphs(i, g.SetComplex(PR_TRUE, PR_TRUE, 1), &details);
        }
    }
    return PR_TRUE;
}

void
gfxWindowsFontGroup::InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun,
                                    const char *aString, PRUint32 aLength)
{
    nsRefPtr<gfxWindowsFont> font = GetFontAt(0);
    DCFromContext dc(aContext);
    if (SetupDCFont(dc, font)) {
        nsAutoTArray<WCHAR,500> glyphArray;
        if (!glyphArray.SetLength(aLength))
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
    nsRefPtr<gfxWindowsFont> font = GetFontAt(0);
    DCFromContext dc(aContext);
    if (SetupDCFont(dc, font)) {
        nsAutoTArray<WCHAR,500> glyphArray;
        if (!glyphArray.SetLength(aLength))
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
    { "LANG_ARABIC",     "ar" }, // ara
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
    { "LANG_ORIYA",      "x-orya" }, // ori
    { "LANG_TAMIL",      "x-tamil" }, // tam
    { "LANG_TELUGU",     "x-telu" },  //tel
    { "LANG_KANNADA",    "x-knda" },  // kan
    { "LANG_MALAYALAM",  "x-mlym" }, // mal
    { "LANG_ASSAMESE",   "x-beng" },    // asm
    { "LANG_MARATHI",    "x-devanagari" }, // mar
    { "LANG_SANSKRIT",   "x-devanagari" }, // san
    { "LANG_MONGOLIAN",  "mon" },
    { "TIBETAN",         "tib" }, // tib/bod
    { nsnull, nsnull },
    { "KHMER",           "x-khmr" }, // khm
    { "LAO",             "lao" },
    { "MYANMAR",         "bur" }, // bur/mya
    { "LANG_GALICIAN",   "glg" },
    { "LANG_KONKANI",    "kok" },
    { "LANG_MANIPURI",   "mni" },
    { "LANG_SINDHI",     "snd" },
    { "LANG_SYRIAC",     "syr" },
    { "SINHALESE",       "x-sinh" }, // sin
    { "CHEROKEE",        "chr" },
    { "INUKTITUT",       "x-cans" }, // iku
    { "ETHIOPIC",        "x-ethi" }, // amh -- this is both Amharic and Tigrinya
    { nsnull, nsnull },
    { "LANG_KASHMIRI",   "kas" },
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

#define ESTIMATE_MAX_GLYPHS(L) (((3 * (L)) >> 1) + 16)

class UniscribeItem
{
public:
    UniscribeItem(gfxContext *aContext, HDC aDC,
                  const PRUnichar *aString, PRUint32 aLength,
                  SCRIPT_ITEM *aItem,
                  gfxWindowsFontGroup *aGroup) :
        mContext(aContext), mDC(aDC), mRangeString(nsnull), mRangeLength(0),
        mItemString(aString), mItemLength(aLength), 
        mAlternativeString(nsnull), mScriptItem(aItem),
        mScript(aItem->a.eScript), mGroup(aGroup),
        mNumGlyphs(0), mMaxGlyphs(ESTIMATE_MAX_GLYPHS(aLength)),
        mFontSelected(PR_FALSE)
    {
        NS_ASSERTION(mMaxGlyphs < 65535, "UniscribeItem is too big, ScriptShape() will fail!");
        mGlyphs.SetLength(mMaxGlyphs);
        mClusters.SetLength(mItemLength + 1);
        mAttr.SetLength(mMaxGlyphs);
    }

    ~UniscribeItem() {
        free(mAlternativeString);
    }

    /* possible return values:
     * S_OK - things succeeded
     * GDI_ERROR - things failed to shape.  Might want to try again after calling DisableShaping()
     */

    HRESULT ShapeUniscribe() {
        HRESULT rv;
        HDC shapeDC = nsnull;

        const PRUnichar *str = mAlternativeString ? mAlternativeString : mRangeString;

        mScriptItem->a.fLogicalOrder = PR_TRUE; 
        SCRIPT_ANALYSIS sa = mScriptItem->a;
        /*
          fLinkBefore and fLinkAfter in the SCRIPT_ANALYSIS structure refer to
          the whole item, so if the current range begins after the beginning
          of the item or ends before the end of the item, we need to override
          them here.
          This assumes that we won't split an item into ranges between two
          characters that need to be shaped together.
        */
        if (mRangeString > mItemString)
            sa.fLinkBefore = PR_FALSE;
        if (mRangeString + mRangeLength < mItemString + mItemLength)
            sa.fLinkAfter = PR_FALSE;

        while (PR_TRUE) {

            rv = ScriptShape(shapeDC, mCurrentFont->ScriptCache(),
                             str, mRangeLength,
                             mMaxGlyphs, &sa,
                             mGlyphs.Elements(), mClusters.Elements(),
                             mAttr.Elements(), &mNumGlyphs);

            if (rv == E_OUTOFMEMORY) {
                mMaxGlyphs *= 2;
                mGlyphs.SetLength(mMaxGlyphs);
                mAttr.SetLength(mMaxGlyphs);
                continue;
            }

            if (rv == E_PENDING) {
                if (shapeDC == mDC) {
                    // we already tried this once, something failed, give up
                    return E_PENDING;
                }

                SelectFont();

                shapeDC = mDC;
                continue;
            }

            return rv;
        }
    }

    HRESULT ShapeGDI() {
        SelectFont();

        mNumGlyphs = mRangeLength;
        GetGlyphIndicesW(mDC, mRangeString, mRangeLength,
                         (WORD*) mGlyphs.Elements(),
                         GGI_MARK_NONEXISTING_GLYPHS);

        for (PRUint32 i = 0; i < mRangeLength; ++i)
            mClusters[i] = i;

        return S_OK;
    }

    HRESULT Shape() {
        // Skip Uniscribe for fonts that need GDI
        if (mCurrentFont->GetFontEntry()->mForceGDI)
            return ShapeGDI();

        return ShapeUniscribe();
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
    void EnableShaping() {
        mScriptItem->a.eScript = mScript;
        if (mAlternativeString) {
            free(mAlternativeString);
            mAlternativeString = nsnull;
        }
    }

    PRBool IsGlyphMissing(SCRIPT_FONTPROPERTIES *aSFP, PRUint32 aGlyphIndex) {
        PRBool missing = PR_FALSE;
        if (GetCurrentFont()->GetFontEntry()->mForceGDI) {
            // Our GDI path marks missing glyphs as 0xFFFF. So just look for that.
            if (mGlyphs[aGlyphIndex] == 0xFFFF)
                missing = PR_TRUE;
        } else if (mGlyphs[aGlyphIndex] == aSFP->wgDefault) {
            missing = PR_TRUE;
        }
        return missing;
    }


    HRESULT PlaceUniscribe() {
        HRESULT rv;
        HDC placeDC = nsnull;

        while (PR_TRUE) {
            rv = ScriptPlace(placeDC, mCurrentFont->ScriptCache(),
                             mGlyphs.Elements(), mNumGlyphs,
                             mAttr.Elements(), &mScriptItem->a,
                             mAdvances.Elements(), mOffsets.Elements(), NULL);

            if (rv == E_PENDING) {
                SelectFont();
                placeDC = mDC;
                continue;
            }

            break;
        }

        return rv;
    }

    HRESULT PlaceGDI() {
        SelectFont();

        nsAutoTArray<int,500> partialWidthArray;
        // Callers incorrectly assume this code is infallible,
        // so we must abort on this OOM condition.
        if (!partialWidthArray.SetLength(mNumGlyphs))
            PR_Abort();
        SIZE size;

        GetTextExtentExPointI(mDC,
                              (WORD*) mGlyphs.Elements(),
                              mNumGlyphs,
                              INT_MAX,
                              NULL,
                              partialWidthArray.Elements(),
                              &size);

        PRInt32 lastWidth = 0;

        for (PRUint32 i = 0; i < mNumGlyphs; i++) {
            mAdvances[i] = partialWidthArray[i] - lastWidth;
            lastWidth = partialWidthArray[i];
            mOffsets[i].du = mOffsets[i].dv = 0;
        }
        return 0;
    }

    HRESULT Place() {
        mOffsets.SetLength(mNumGlyphs);
        mAdvances.SetLength(mNumGlyphs);

        PRBool allCJK = PR_TRUE;

        // Some fonts don't get along with Uniscribe so we'll use GDI to
        // render them.
        if (!mCurrentFont->GetFontEntry()->mForceGDI) {
            for (PRUint32 i = 0; i < mRangeLength; i++) {
                const PRUnichar ch = mRangeString[i];
                if (ch == ' ' || FindCharUnicodeRange(ch) == kRangeSetCJK)
                    continue;

                allCJK = PR_FALSE;
                break;
            }
        }

        if (allCJK)
            return PlaceGDI();

        return PlaceUniscribe();
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
        if (!logAttr.AppendElements(mRangeLength))
            return;
        HRESULT rv = ScriptBreak(mRangeString, mRangeLength,
                                 &mScriptItem->a, logAttr.Elements());
        if (FAILED(rv))
            return;
        gfxTextRun::CompressedGlyph g;
        // The first character is never inside a cluster. Windows might tell us
        // that it should be, but we have no before-character to cluster
        // it with so we just can't cluster it. So skip it here.
        for (PRUint32 i = 1; i < mRangeLength; ++i) {
            if (!logAttr[i].fCharStop) {
                aRun->SetGlyphs(i + aOffsetInRun, g.SetComplex(PR_FALSE, PR_TRUE, 0), nsnull);
            }
        }
    }

    void SaveGlyphs(gfxTextRun *aRun) {
        PRUint32 offsetInRun = mScriptItem->iCharPos + (mRangeString - mItemString);
        SetupClusterBoundaries(aRun, offsetInRun);

        aRun->AddGlyphRun(GetCurrentFont(), offsetInRun);

        // XXX We should store this in the item and only fetch it once
        SCRIPT_FONTPROPERTIES sfp;
        ScriptFontProperties(&sfp);

        PRUint32 offset = 0;
        nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;
        gfxTextRun::CompressedGlyph g;
        const PRUint32 appUnitsPerDevUnit = aRun->GetAppUnitsPerDevUnit();
        while (offset < mRangeLength) {
            PRUint32 runOffset = offsetInRun + offset;
            if (offset > 0 && mClusters[offset] == mClusters[offset - 1]) {
                g.SetComplex(aRun->IsClusterStart(runOffset), PR_FALSE, 0);
                aRun->SetGlyphs(runOffset, g, nsnull);
            } else {
                // Count glyphs for this character
                PRUint32 k = mClusters[offset];
                PRUint32 glyphCount = mNumGlyphs - k;
                PRUint32 nextClusterOffset;
                PRBool missing = IsGlyphMissing(&sfp, k);
                for (nextClusterOffset = offset + 1; nextClusterOffset < mRangeLength; ++nextClusterOffset) {
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
                NS_ASSERTION(!gfxFontGroup::IsInvalidChar(mRangeString[offset]),
                             "invalid character detected");
                if (missing) {
                    if (NS_IS_HIGH_SURROGATE(mRangeString[offset]) &&
                        offset + 1 < mRangeLength &&
                        NS_IS_LOW_SURROGATE(mRangeString[offset + 1])) {
                        aRun->SetMissingGlyph(runOffset,
                                              SURROGATE_TO_UCS4(mRangeString[offset],
                                                                mRangeString[offset + 1]));
                    } else {
                        aRun->SetMissingGlyph(runOffset, mRangeString[offset]);
                    }
                } else if (glyphCount == 1 && advance >= 0 &&
                    mOffsets[k].dv == 0 && mOffsets[k].du == 0 &&
                    gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
                    gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyph)) {
                    aRun->SetSimpleGlyph(runOffset, g.SetSimpleGlyph(advance, glyph));
                } else {
                    if (detailedGlyphs.Length() < glyphCount) {
                        if (!detailedGlyphs.AppendElements(glyphCount - detailedGlyphs.Length()))
                            return;
                    }
                    PRUint32 i;
                    for (i = 0; i < glyphCount; ++i) {
                        gfxTextRun::DetailedGlyph *details = &detailedGlyphs[i];
                        details->mGlyphID = mGlyphs[k + i];
                        details->mAdvance = mAdvances[k + i]*appUnitsPerDevUnit;
                        details->mXOffset = float(mOffsets[k + i].du)*appUnitsPerDevUnit*aRun->GetDirection();
                        details->mYOffset = float(mOffsets[k + i].dv)*appUnitsPerDevUnit;
                    }
                    aRun->SetGlyphs(runOffset,
                        g.SetComplex(PR_TRUE, PR_TRUE, glyphCount), detailedGlyphs.Elements());
                }
            }
            ++offset;
        }
    }

    void SetCurrentFont(gfxWindowsFont *aFont) {
        if (mCurrentFont != aFont) {
            mCurrentFont = aFont;
            cairo_scaled_font_t *scaledFont = mCurrentFont->CairoScaledFont();
            cairo_win32_scaled_font_done_font(scaledFont);
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
        cairo_scaled_font_t *scaledFont = mCurrentFont->CairoScaledFont();
        cairo_win32_scaled_font_select_font(scaledFont, mDC);

        mFontSelected = PR_TRUE;
    }

    nsTArray<gfxTextRange>& Ranges() { return mRanges; }

    void SetRange(PRUint32 i) {
        nsRefPtr<gfxWindowsFont> font;
        if (mRanges[i].font)
            font = static_cast<gfxWindowsFont*> (mRanges[i].font.get());
        else
            font = mGroup->GetFontAt(0);

        SetCurrentFont(font);

        mRangeString = mItemString + mRanges[i].start;
        mRangeLength = mRanges[i].Length();
    }


private:

    void GenerateAlternativeString() {
        if (mAlternativeString)
            free(mAlternativeString);
        mAlternativeString = (PRUnichar *)malloc(mRangeLength * sizeof(PRUnichar));
        if (!mAlternativeString)
            return;
        memcpy((void *)mAlternativeString, (const void *)mRangeString,
               mRangeLength * sizeof(PRUnichar));
        for (PRUint32 i = 0; i < mRangeLength; i++) {
            if (NS_IS_HIGH_SURROGATE(mRangeString[i]) || NS_IS_LOW_SURROGATE(mRangeString[i]))
                mAlternativeString[i] = PRUnichar(0xFFFD);
        }
    }

private:
    nsRefPtr<gfxContext> mContext;
    HDC mDC;

    SCRIPT_ITEM *mScriptItem;
    WORD mScript;

    // these point to the current range
    const PRUnichar *mRangeString;
    PRUint32 mRangeLength;

public:
    // these point to the full string/length of the item
    const PRUnichar *mItemString;
    const PRUint32 mItemLength;

private:
    PRUnichar *mAlternativeString;

    gfxWindowsFontGroup *mGroup;

#define AVERAGE_ITEM_LENGTH 40

    nsAutoTArray<WORD, PRUint32(ESTIMATE_MAX_GLYPHS(AVERAGE_ITEM_LENGTH))> mGlyphs;
    nsAutoTArray<WORD, AVERAGE_ITEM_LENGTH + 1> mClusters;
    nsAutoTArray<SCRIPT_VISATTR, PRUint32(ESTIMATE_MAX_GLYPHS(AVERAGE_ITEM_LENGTH))> mAttr;
 
    nsAutoTArray<GOFFSET, 2 * AVERAGE_ITEM_LENGTH> mOffsets;
    nsAutoTArray<int, 2 * AVERAGE_ITEM_LENGTH> mAdvances;

#undef AVERAGE_ITEM_LENGTH

    int mMaxGlyphs;
    int mNumGlyphs;

    nsRefPtr<gfxWindowsFont> mCurrentFont;

    PRPackedBool mFontSelected;

    nsTArray<gfxTextRange> mRanges;
};


#define MAX_ITEM_LENGTH 32768



static PRUint32 FindNextItemStart(int aOffset, int aLimit,
                                  nsTArray<SCRIPT_LOGATTR> &aLogAttr,
                                  const PRUnichar *aString)
{
    if (aOffset + MAX_ITEM_LENGTH >= aLimit) {
        // The item starting at aOffset can't be longer than the max length,
        // so starting the next item at aLimit won't cause ScriptShape() to fail.
        return aLimit;
    }

    // Try to start the next item before or after a space, since spaces
    // don't kern or ligate.
    PRUint32 off;
    int boundary = -1;
    for (off = MAX_ITEM_LENGTH; off > 1; --off) {
      if (aLogAttr[off].fCharStop) {
          if (off > boundary) {
              boundary = off;
          }
          if (aString[aOffset+off] == ' ' || aString[aOffset+off - 1] == ' ')
            return aOffset+off;
      }
    }

    // Try to start the next item at the last cluster boundary in the range.
    if (boundary > 0) {
      return aOffset+boundary;
    }

    // No nice cluster boundaries inside MAX_ITEM_LENGTH characters, break
    // on the size limit. It won't be visually plesaing, but at least it
    // won't cause ScriptShape() to fail.
    return aOffset + MAX_ITEM_LENGTH;
}

class Uniscribe
{
public:
    Uniscribe(gfxContext *aContext, HDC aDC, const PRUnichar *aString, PRUint32 aLength, PRBool aIsRTL) :
        mContext(aContext), mDC(aDC), mString(aString), mLength(aLength), mIsRTL(aIsRTL),
        mItems(nsnull) {
    }
    ~Uniscribe() {
    }

    void Init() {
        memset(&mControl, 0, sizeof(SCRIPT_CONTROL));
        memset(&mState, 0, sizeof(SCRIPT_STATE));
        // Lock the direction. Don't allow the itemizer to change directions
        // based on character type.
        mState.uBidiLevel = mIsRTL;
        mState.fOverrideDirection = PR_TRUE;
    }

private:

    // Append mItems[aIndex] to aDest, adding extra items to aDest to ensure
    // that no item is too long for ScriptShape() to handle. See bug 366643.
    nsresult CopyItemSplitOversize(int aIndex, nsTArray<SCRIPT_ITEM> &aDest) {
        aDest.AppendElement(mItems[aIndex]);
        const int itemLength = mItems[aIndex+1].iCharPos - mItems[aIndex].iCharPos;
        if (ESTIMATE_MAX_GLYPHS(itemLength) > 65535) {
            // This items length would cause ScriptShape() to fail. We need to
            // add extra items here so that no item's length could cause the fail.

            // Get cluster boundaries, so we can break cleanly if possible.
            nsTArray<SCRIPT_LOGATTR> logAttr;
            if (!logAttr.SetLength(itemLength))
                return NS_ERROR_FAILURE;
            HRESULT rv= ScriptBreak(mString+mItems[aIndex].iCharPos, itemLength,
                                    &mItems[aIndex].a, logAttr.Elements());
            if (FAILED(rv))
                return NS_ERROR_FAILURE;

            const int nextItemStart = mItems[aIndex+1].iCharPos;
            int start = FindNextItemStart(mItems[aIndex].iCharPos,
                                          nextItemStart, logAttr, mString);

            while (start < nextItemStart) {
                SCRIPT_ITEM item = mItems[aIndex];
                item.iCharPos = start;
                aDest.AppendElement(item);
                start = FindNextItemStart(start, nextItemStart, logAttr, mString);
            }
        } 
        return NS_OK;
    }

public:

    int Itemize() {
        HRESULT rv;

        int maxItems = 5;

        Init();

        // Allocate space for one more item than expected, to handle a rare
        // overflow in ScriptItemize (pre XP SP2). See bug 366643.
        if (!mItems.SetLength(maxItems + 1)) {
            return 0;
        }
        while ((rv = ScriptItemize(mString, mLength, maxItems, &mControl, &mState,
                                   mItems.Elements(), &mNumItems)) == E_OUTOFMEMORY) {
            maxItems *= 2;
            if (!mItems.SetLength(maxItems + 1)) {
                return 0;
            }
            Init();
        }

        if (ESTIMATE_MAX_GLYPHS(mLength) > 65535) {
            // Any item of length > 43680 will cause ScriptShape() to fail, as its
            // mMaxGlyphs value will be greater than 65535 (43680*1.5+16>65535). So we
            // need to break up items which are longer than that upon cluster boundaries.
            // See bug 394751 for details.
            nsTArray<SCRIPT_ITEM> items;
            for (int i=0; i<mNumItems; i++) {
                nsresult nrs = CopyItemSplitOversize(i, items);
                NS_ASSERTION(NS_SUCCEEDED(nrs), "CopyItemSplitOversize() failed");
            }
            items.AppendElement(mItems[mNumItems]); // copy terminator.

            mItems = items;
            mNumItems = items.Length() - 1; // Don't count the terminator.
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

    SCRIPT_CONTROL mControl;
    SCRIPT_STATE   mState;
    nsTArray<SCRIPT_ITEM> mItems;
    int mNumItems;
};

already_AddRefed<gfxWindowsFont>
gfxWindowsFontGroup::WhichFontSupportsChar(const nsTArray<nsRefPtr<FontEntry> >& fonts, PRUint32 ch) {
    for (PRUint32 i = 0; i < fonts.Length(); i++) {
        nsRefPtr<FontEntry> fe = fonts[i];
        if (fe->mSymbolFont && !mStyle.familyNameQuirks)
            continue;
        if (fe->HasCharacter(ch)) {
            nsRefPtr<gfxWindowsFont> font =
                gfxWindowsFont::GetOrMakeFont(fe, &mStyle);
            // Check that the font is still usable.
            if (!font->IsValid())
                continue;
            return font.forget();
        }
    }
    return nsnull;
}

// this function appends to the array passed in.
void gfxWindowsFontGroup::GetPrefFonts(const char *aLangGroup, nsTArray<nsRefPtr<FontEntry> >& array) {
    NS_ASSERTION(aLangGroup, "aLangGroup is null");
    gfxWindowsPlatform *platform = gfxWindowsPlatform::GetPlatform();
    nsAutoTArray<nsRefPtr<FontEntry>, 5> fonts;
    /* this lookup has to depend on weight and style */
    nsCAutoString key(aLangGroup);
    key.Append("-");
    key.AppendInt(GetStyle()->style);
    key.Append("-");
    key.AppendInt(GetStyle()->weight);
    if (!platform->GetPrefFontEntries(key, &fonts)) {
        nsString fontString;
        platform->GetPrefFonts(aLangGroup, fontString);
        if (fontString.IsEmpty())
            return;

        FamilyListToArrayList(fontString, nsDependentCString(aLangGroup),
                                      &fonts);

        platform->SetPrefFontEntries(key, fonts);
    }
    array.AppendElements(fonts);
}

static PRInt32 GetCJKLangGroupIndex(const char *aLangGroup) {
    PRInt32 i;
    for (i = 0; i < COUNT_OF_CJK_LANG_GROUP; i++) {
        if (!PL_strcasecmp(aLangGroup, sCJKLangGroup[i]))
            return i;
    }
    return -1;
}

// this function assigns to the array passed in.
void gfxWindowsFontGroup::GetCJKPrefFonts(nsTArray<nsRefPtr<FontEntry> >& array) {
    gfxWindowsPlatform *platform = gfxWindowsPlatform::GetPlatform();

    nsCAutoString key("x-internal-cjk-");
    key.AppendInt(mStyle.style);
    key.Append("-");
    key.AppendInt(mStyle.weight);

    if (!platform->GetPrefFontEntries(key, &array)) {
        nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (!prefs)
            return;

        nsCOMPtr<nsIPrefBranch> prefBranch;
        prefs->GetBranch(0, getter_AddRefs(prefBranch));
        if (!prefBranch)
            return;

        // Add the CJK pref fonts from accept languages, the order should be same order
        nsCAutoString list;
        nsCOMPtr<nsIPrefLocalizedString> val;
        nsresult rv = prefBranch->GetComplexValue("intl.accept_languages", NS_GET_IID(nsIPrefLocalizedString),
                                                  getter_AddRefs(val));
        if (NS_SUCCEEDED(rv) && val) {
            nsAutoString temp;
            val->ToString(getter_Copies(temp));
            LossyCopyUTF16toASCII(temp, list);
        }
        if (!list.IsEmpty()) {
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
                    GetPrefFonts(sCJKLangGroup[index], array);
                p++;
            }
        }

        // Add the system locale
        switch (::GetACP()) {
            case 932: GetPrefFonts(CJK_LANG_JA, array); break;
            case 936: GetPrefFonts(CJK_LANG_ZH_CN, array); break;
            case 949: GetPrefFonts(CJK_LANG_KO, array); break;
            // XXX Don't we need to append CJK_LANG_ZH_HK if the codepage is 950?
            case 950: GetPrefFonts(CJK_LANG_ZH_TW, array); break;
        }

        // last resort...
        GetPrefFonts(CJK_LANG_JA, array);
        GetPrefFonts(CJK_LANG_KO, array);
        GetPrefFonts(CJK_LANG_ZH_CN, array);
        GetPrefFonts(CJK_LANG_ZH_HK, array);
        GetPrefFonts(CJK_LANG_ZH_TW, array);

        platform->SetPrefFontEntries(key, array);
    }
}

already_AddRefed<gfxFont> 
gfxWindowsFontGroup::WhichPrefFontSupportsChar(PRUint32 aCh)
{
    nsRefPtr<gfxWindowsFont> selectedFont;

    // check out the style's language group
    if (!selectedFont) {
        nsAutoTArray<nsRefPtr<FontEntry>, 5> fonts;
        this->GetPrefFonts(mStyle.langGroup.get(), fonts);
        selectedFont = WhichFontSupportsChar(fonts, aCh);
    }

    // otherwise search prefs
    if (!selectedFont) {
        /* first check with the script properties to see what they think */
        if (mItemLangGroup) {
            PR_LOG(gFontLog, PR_LOG_DEBUG, (" - Trying to find fonts for: %s ", mItemLangGroup));

            nsAutoTArray<nsRefPtr<FontEntry>, 5> fonts;
            this->GetPrefFonts(mItemLangGroup, fonts);
            selectedFont = WhichFontSupportsChar(fonts, aCh);
        } else if (aCh <= 0xFFFF) {
            PRUint32 unicodeRange = FindCharUnicodeRange(aCh);

            /* special case CJK */
            if (unicodeRange == kRangeSetCJK) {
                if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                    PR_LOG(gFontLog, PR_LOG_DEBUG, (" - Trying to find fonts for: CJK"));

                nsAutoTArray<nsRefPtr<FontEntry>, 15> fonts;
                this->GetCJKPrefFonts(fonts);
                selectedFont = WhichFontSupportsChar(fonts, aCh);
            } else {
                const char *langGroup = LangGroupFromUnicodeRange(unicodeRange);
                if (langGroup) {
                    PR_LOG(gFontLog, PR_LOG_DEBUG, (" - Trying to find fonts for: %s", langGroup));

                    nsAutoTArray<nsRefPtr<FontEntry>, 5> fonts;
                    this->GetPrefFonts(langGroup, fonts);
                    selectedFont = WhichFontSupportsChar(fonts, aCh);
                }
            }
        }
    }

    if (selectedFont) {
        nsRefPtr<gfxFont> f = static_cast<gfxFont*>(selectedFont.get());
        return f.forget();
    }

    return nsnull;
}


already_AddRefed<gfxFont> 
gfxWindowsFontGroup::WhichSystemFontSupportsChar(PRUint32 aCh)
{
    nsRefPtr<gfxWindowsFont> selectedFont;

    // system font lookup
    PR_LOG(gFontLog, PR_LOG_DEBUG, (" - Looking for best match"));

    nsRefPtr<gfxWindowsFont> refFont = GetFontAt(0);
    gfxWindowsPlatform *platform = gfxWindowsPlatform::GetPlatform();
    selectedFont = platform->FindFontForChar(aCh, refFont);

    if (selectedFont) {
        nsRefPtr<gfxFont> f = static_cast<gfxFont*>(selectedFont.get());
        return f.forget();
    }

    return nsnull;
}


void
gfxWindowsFontGroup::InitTextRunUniscribe(gfxContext *aContext, gfxTextRun *aRun, const PRUnichar *aString,
                                          PRUint32 aLength)
{
    DCFromContext aDC(aContext);
 
    const PRBool isRTL = aRun->IsRightToLeft();

    HRESULT rv;

    Uniscribe us(aContext, aDC, aString, aLength, isRTL);

    /* itemize the string */
    int numItems = us.Itemize();

    for (int i = 0; i < numItems; ++i) {
        SaveDC(aDC);

        nsAutoPtr<UniscribeItem> item(us.GetItem(i, this));

        // jtdfix - push this into the pref handling code??
        mItemLangGroup = nsnull;

        const SCRIPT_PROPERTIES *sp = item->ScriptProperties();
        if (!sp->fAmbiguousCharSet) {
            WORD primaryId = PRIMARYLANGID(sp->langid);
            mItemLangGroup = gScriptToText[primaryId].langCode;
        }

        ComputeRanges(item->Ranges(), item->mItemString, 0, item->mItemLength);

        PRUint32 nranges = item->Ranges().Length();

        for (PRUint32 j = 0; j < nranges; ++j) {

            item->SetRange(j);

            if (!item->ShapingEnabled())
                item->EnableShaping();

            rv = item->Shape();
            if (FAILED(rv)) {
                PR_LOG(gFontLog, PR_LOG_DEBUG, ("shaping failed"));
                // we know we have the glyphs to display this font already
                // so Uniscribe just doesn't know how to shape the script.
                // Render the glyphs without shaping.
                item->DisableShaping();
                rv = item->Shape();
            }

            NS_ASSERTION(SUCCEEDED(rv), "Failed to shape, twice -- we should never hit this");

            if (SUCCEEDED(rv)) {
                rv = item->Place();
                NS_ASSERTION(SUCCEEDED(rv), "Failed to place -- this is pretty bad.");
            }

            if (FAILED(rv)) {
                aRun->ResetGlyphRuns();

                /* Uniscribe doesn't like this font, use GDI instead */
                item->GetCurrentFont()->GetFontEntry()->mForceGDI = PR_TRUE;
                break;
            }

            item->SaveGlyphs(aRun);
        }

        RestoreDC(aDC, -1);

        if (FAILED(rv)) {
            i = -1;
        }
    }
}

