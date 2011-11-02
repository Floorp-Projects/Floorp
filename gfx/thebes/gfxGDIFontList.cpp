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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "mozilla/Util.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "prlog.h"

#include "gfxGDIFontList.h"
#include "gfxWindowsPlatform.h"
#include "gfxUserFontSet.h"
#include "gfxFontUtils.h"
#include "gfxGDIFont.h"

#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"

#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"
#include "nsIWindowsRegKey.h"

#include "mozilla/Telemetry.h"

#include <usp10.h>

using namespace mozilla;

#define ROUND(x) floor((x) + 0.5)


#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY 5
#endif

#ifdef PR_LOGGING
#define LOG_FONTLIST(args) PR_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), \
                               PR_LOG_DEBUG, args)
#define LOG_FONTLIST_ENABLED() PR_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontlist), \
                                   PR_LOG_DEBUG)

#endif // PR_LOGGING

// font info loader constants

// avoid doing this during startup even on slow machines but try to start
// it soon enough so that system fallback doesn't happen first
static const PRUint32 kDelayBeforeLoadingFonts = 120 * 1000; // 2 minutes after init
static const PRUint32 kIntervalBetweenLoadingFonts = 2000;   // every 2 seconds until complete

static __inline void
BuildKeyNameFromFontName(nsAString &aName)
{
    if (aName.Length() >= LF_FACESIZE)
        aName.Truncate(LF_FACESIZE - 1);
    ToLowerCase(aName);
}

// Implementation of gfxPlatformFontList for Win32 GDI,
// using GDI font enumeration APIs to get the list of fonts

// from t2embapi.h, included in Platform SDK 6.1 but not 6.0

#ifndef __t2embapi__

#define TTLOAD_PRIVATE                  0x00000001
#define LICENSE_PREVIEWPRINT            0x0004
#define E_NONE                          0x0000L

typedef unsigned long( WINAPIV *READEMBEDPROC ) ( void*, void*, const unsigned long );

typedef struct
{
    unsigned short usStructSize;    // size in bytes of structure client should set to sizeof(TTLOADINFO)
    unsigned short usRefStrSize;    // size in wide characters of pusRefStr including NULL terminator
    unsigned short *pusRefStr;      // reference or actual string.
}TTLOADINFO;

LONG WINAPI TTLoadEmbeddedFont
(
    HANDLE*  phFontReference,           // on completion, contains handle to identify embedded font installed
                                        // on system
    ULONG    ulFlags,                   // flags specifying the request 
    ULONG*   pulPrivStatus,             // on completion, contains the embedding status
    ULONG    ulPrivs,                   // allows for the reduction of licensing privileges
    ULONG*   pulStatus,                 // on completion, may contain status flags for request 
    READEMBEDPROC lpfnReadFromStream,   // callback function for doc/disk reads
    LPVOID   lpvReadStream,             // the input stream tokin
    LPWSTR   szWinFamilyName,           // the new 16 bit windows family name can be NULL
    LPSTR    szMacFamilyName,           // the new 8 bit mac family name can be NULL
    TTLOADINFO* pTTLoadInfo             // optional security
);

#endif // __t2embapi__

typedef LONG( WINAPI *TTLoadEmbeddedFontProc ) (HANDLE* phFontReference, ULONG ulFlags, ULONG* pulPrivStatus, ULONG ulPrivs, ULONG* pulStatus, 
                                             READEMBEDPROC lpfnReadFromStream, LPVOID lpvReadStream, LPWSTR szWinFamilyName, 
                                             LPSTR szMacFamilyName, TTLOADINFO* pTTLoadInfo);

typedef LONG( WINAPI *TTDeleteEmbeddedFontProc ) (HANDLE hFontReference, ULONG ulFlags, ULONG* pulStatus);


static TTLoadEmbeddedFontProc TTLoadEmbeddedFontPtr = nsnull;
static TTDeleteEmbeddedFontProc TTDeleteEmbeddedFontPtr = nsnull;

class WinUserFontData : public gfxUserFontData {
public:
    WinUserFontData(HANDLE aFontRef, bool aIsEmbedded)
        : mFontRef(aFontRef), mIsEmbedded(aIsEmbedded)
    { }

    virtual ~WinUserFontData()
    {
        if (mIsEmbedded) {
            ULONG pulStatus;
            LONG err;
            err = TTDeleteEmbeddedFontPtr(mFontRef, 0, &pulStatus);
#if DEBUG
            if (err != E_NONE) {
                char buf[256];
                sprintf(buf, "error deleting embedded font handle (%p) - TTDeleteEmbeddedFont returned %8.8x", mFontRef, err);
                NS_ASSERTION(err == E_NONE, buf);
            }
#endif
        } else {
            BOOL success;
            success = RemoveFontMemResourceEx(mFontRef);
#if DEBUG
            if (!success) {
                char buf[256];
                sprintf(buf, "error deleting font handle (%p) - RemoveFontMemResourceEx failed", mFontRef);
                NS_ASSERTION(success, buf);
            }
#endif
        }
    }
    
    HANDLE mFontRef;
    bool mIsEmbedded;
};

BYTE 
FontTypeToOutPrecision(PRUint8 fontType)
{
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
}

/***************************************************************
 *
 * GDIFontEntry
 *
 */

GDIFontEntry::GDIFontEntry(const nsAString& aFaceName,
                           gfxWindowsFontType aFontType,
                           bool aItalic, PRUint16 aWeight, PRInt16 aStretch,
                           gfxUserFontData *aUserFontData)
    : gfxFontEntry(aFaceName),
      mWindowsFamily(0), mWindowsPitch(0),
      mFontType(aFontType),
      mForceGDI(false), mUnknownCMAP(false),
      mCharset(), mUnicodeRanges()
{
    mUserFontData = aUserFontData;
    mItalic = aItalic;
    mWeight = aWeight;
    mStretch = aStretch;
    if (IsType1())
        mForceGDI = true;
    mIsUserFont = aUserFontData != nsnull;

    InitLogFont(aFaceName, aFontType);
}

nsresult
GDIFontEntry::ReadCMAP()
{
    // skip non-SFNT fonts completely
    if (mFontType != GFX_FONT_TYPE_PS_OPENTYPE && 
        mFontType != GFX_FONT_TYPE_TT_OPENTYPE &&
        mFontType != GFX_FONT_TYPE_TRUETYPE) 
    {
        return NS_ERROR_FAILURE;
    }

    // attempt this once, if errors occur leave a blank cmap
    if (mCmapInitialized)
        return NS_OK;
    mCmapInitialized = true;

    const PRUint32 kCmapTag = TRUETYPE_TAG('c','m','a','p');
    AutoFallibleTArray<PRUint8,16384> buffer;
    if (GetFontTable(kCmapTag, buffer) != NS_OK)
        return NS_ERROR_FAILURE;
    PRUint8 *cmap = buffer.Elements();

    bool          unicodeFont = false, symbolFont = false;
    nsresult rv = gfxFontUtils::ReadCMAP(cmap, buffer.Length(),
                                         mCharacterMap, mUVSOffset,
                                         unicodeFont, symbolFont);
    mSymbolFont = symbolFont;
    mHasCmapTable = NS_SUCCEEDED(rv);

#ifdef PR_LOGGING
    LOG_FONTLIST(("(fontlist-cmap) name: %s, size: %d\n",
                  NS_ConvertUTF16toUTF8(mName).get(), mCharacterMap.GetSize()));
#endif
    return rv;
}

bool
GDIFontEntry::IsSymbolFont()
{
    // initialize cmap first
    HasCmapTable();
    return mSymbolFont;  
}

gfxFont *
GDIFontEntry::CreateFontInstance(const gfxFontStyle* aFontStyle, bool aNeedsBold)
{
    bool isXP = (gfxWindowsPlatform::WindowsOSVersion() 
                       < gfxWindowsPlatform::kWindowsVista);

    bool useClearType = isXP && !aFontStyle->systemFont &&
        (gfxWindowsPlatform::GetPlatform()->UseClearTypeAlways() ||
         (mIsUserFont && !mIsLocalUserFont &&
          gfxWindowsPlatform::GetPlatform()->UseClearTypeForDownloadableFonts()));

    return new gfxGDIFont(this, aFontStyle, aNeedsBold, 
                          (useClearType ? gfxFont::kAntialiasSubpixel
                                        : gfxFont::kAntialiasDefault));
}

nsresult
GDIFontEntry::GetFontTable(PRUint32 aTableTag,
                           FallibleTArray<PRUint8>& aBuffer)
{
    if (!IsTrueType()) {
        return NS_ERROR_FAILURE;
    }

    AutoDC dc;
    AutoSelectFont font(dc.GetDC(), &mLogFont);
    if (font.IsValid()) {
        PRInt32 tableSize =
            ::GetFontData(dc.GetDC(), NS_SWAP32(aTableTag), 0, NULL, NULL);
        if (tableSize != GDI_ERROR) {
            if (aBuffer.SetLength(tableSize)) {
                ::GetFontData(dc.GetDC(), NS_SWAP32(aTableTag), 0,
                              aBuffer.Elements(), tableSize);
                return NS_OK;
            }
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    return NS_ERROR_FAILURE;
}

void
GDIFontEntry::FillLogFont(LOGFONTW *aLogFont, bool aItalic,
                          PRUint16 aWeight, gfxFloat aSize,
                          bool aUseCleartype)
{
    memcpy(aLogFont, &mLogFont, sizeof(LOGFONTW));

    aLogFont->lfHeight = (LONG)-ROUND(aSize);

    if (aLogFont->lfHeight == 0)
        aLogFont->lfHeight = -1;

    // always force lfItalic if we want it.  Font selection code will
    // do its best to give us an italic font entry, but if no face exists
    // it may give us a regular one based on weight.  Windows should
    // do fake italic for us in that case.
    aLogFont->lfItalic         = aItalic;
    aLogFont->lfWeight         = aWeight;
    aLogFont->lfQuality        = (aUseCleartype ? CLEARTYPE_QUALITY : DEFAULT_QUALITY);
}

#define MISSING_GLYPH 0x1F // glyph index returned for missing characters
                           // on WinXP with .fon fonts, but not Type1 (.pfb)

bool 
GDIFontEntry::TestCharacterMap(PRUint32 aCh)
{
    if (ReadCMAP() != NS_OK) {
        // For fonts where we failed to read the character map,
        // we can take a slow path to look up glyphs character by character
        mUnknownCMAP = true;
    }

    if (mUnknownCMAP) {
        if (aCh > 0xFFFF)
            return false;

        // previous code was using the group style
        gfxFontStyle fakeStyle;  
        if (mItalic)
            fakeStyle.style = FONT_STYLE_ITALIC;
        fakeStyle.weight = mWeight * 100;

        nsRefPtr<gfxFont> tempFont = FindOrMakeFont(&fakeStyle, false);
        if (!tempFont || !tempFont->Valid())
            return false;
        gfxGDIFont *font = static_cast<gfxGDIFont*>(tempFont.get());

        HDC dc = GetDC((HWND)nsnull);
        SetGraphicsMode(dc, GM_ADVANCED);
        HFONT hfont = font->GetHFONT();
        HFONT oldFont = (HFONT)SelectObject(dc, hfont);

        PRUnichar str[1] = { (PRUnichar)aCh };
        WORD glyph[1];

        bool hasGlyph = false;

        // Bug 573038 - in some cases GetGlyphIndicesW returns 0xFFFF for a 
        // missing glyph or 0x1F in other cases to indicate the "invalid" 
        // glyph.  Map both cases to "not found"
        if (IsType1() || mForceGDI) {
            // Type1 fonts and uniscribe APIs don't get along.  
            // ScriptGetCMap will return E_HANDLE
            DWORD ret = GetGlyphIndicesW(dc, str, 1, 
                                         glyph, GGI_MARK_NONEXISTING_GLYPHS);
            if (ret != GDI_ERROR
                && glyph[0] != 0xFFFF
                && (IsType1() || glyph[0] != MISSING_GLYPH))
            {
                hasGlyph = true;
            }
        } else {
            // ScriptGetCMap works better than GetGlyphIndicesW 
            // for things like bitmap/vector fonts
            SCRIPT_CACHE sc = NULL;
            HRESULT rv = ScriptGetCMap(dc, &sc, str, 1, 0, glyph);
            if (rv == S_OK)
                hasGlyph = true;
        }

        SelectObject(dc, oldFont);
        ReleaseDC(NULL, dc);

        if (hasGlyph) {
            mCharacterMap.set(aCh);
            return true;
        }
    } else {
        // font had a cmap so simply check that
        return mCharacterMap.test(aCh);
    }

    return false;
}

void
GDIFontEntry::InitLogFont(const nsAString& aName,
                          gfxWindowsFontType aFontType)
{
#define CLIP_TURNOFF_FONTASSOCIATION 0x40
    
    mLogFont.lfHeight = -1;

    // Fill in logFont structure
    mLogFont.lfWidth          = 0;
    mLogFont.lfEscapement     = 0;
    mLogFont.lfOrientation    = 0;
    mLogFont.lfUnderline      = FALSE;
    mLogFont.lfStrikeOut      = FALSE;
    mLogFont.lfCharSet        = DEFAULT_CHARSET;
    mLogFont.lfOutPrecision   = FontTypeToOutPrecision(aFontType);
    mLogFont.lfClipPrecision  = CLIP_TURNOFF_FONTASSOCIATION;
    mLogFont.lfQuality        = DEFAULT_QUALITY;
    mLogFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    // always force lfItalic if we want it.  Font selection code will
    // do its best to give us an italic font entry, but if no face exists
    // it may give us a regular one based on weight.  Windows should
    // do fake italic for us in that case.
    mLogFont.lfItalic         = mItalic;
    mLogFont.lfWeight         = mWeight;

    int len = NS_MIN<int>(aName.Length(), LF_FACESIZE - 1);
    memcpy(&mLogFont.lfFaceName, nsPromiseFlatString(aName).get(), len * 2);
    mLogFont.lfFaceName[len] = '\0';
}

GDIFontEntry* 
GDIFontEntry::CreateFontEntry(const nsAString& aName,
                              gfxWindowsFontType aFontType, bool aItalic,
                              PRUint16 aWeight, PRInt16 aStretch,
                              gfxUserFontData* aUserFontData)
{
    // jtdfix - need to set charset, unicode ranges, pitch/family

    GDIFontEntry *fe = new GDIFontEntry(aName, aFontType, aItalic,
                                        aWeight, aStretch, aUserFontData);

    return fe;
}

/***************************************************************
 *
 * GDIFontFamily
 *
 */

int CALLBACK
GDIFontFamily::FamilyAddStylesProc(const ENUMLOGFONTEXW *lpelfe,
                                        const NEWTEXTMETRICEXW *nmetrics,
                                        DWORD fontType, LPARAM data)
{
    const NEWTEXTMETRICW& metrics = nmetrics->ntmTm;
    LOGFONTW logFont = lpelfe->elfLogFont;
    GDIFontFamily *ff = reinterpret_cast<GDIFontFamily*>(data);

    // Some fonts claim to support things > 900, but we don't so clamp the sizes
    logFont.lfWeight = clamped(logFont.lfWeight, LONG(100), LONG(900));

    gfxWindowsFontType feType = GDIFontEntry::DetermineFontType(metrics, fontType);

    GDIFontEntry *fe = nsnull;
    for (PRUint32 i = 0; i < ff->mAvailableFonts.Length(); ++i) {
        fe = static_cast<GDIFontEntry*>(ff->mAvailableFonts[i].get());
        if (feType > fe->mFontType) {
            // if the new type is better than the old one, remove the old entries
            ff->mAvailableFonts.RemoveElementAt(i);
            --i;
        } else if (feType < fe->mFontType) {
            // otherwise if the new type is worse, skip it
            return 1;
        }
    }

    for (PRUint32 i = 0; i < ff->mAvailableFonts.Length(); ++i) {
        fe = static_cast<GDIFontEntry*>(ff->mAvailableFonts[i].get());
        // check if we already know about this face
        if (fe->mWeight == logFont.lfWeight &&
            fe->mItalic == (logFont.lfItalic == 0xFF)) {
            // update the charset bit here since this could be different
            fe->mCharset.set(metrics.tmCharSet);
            return 1; 
        }
    }

    fe = GDIFontEntry::CreateFontEntry(nsDependentString(lpelfe->elfFullName),
                                       feType, (logFont.lfItalic == 0xFF),
                                       (PRUint16) (logFont.lfWeight), 0,
                                       nsnull);
    if (!fe)
        return 1;

    ff->AddFontEntry(fe);

    // mark the charset bit
    fe->mCharset.set(metrics.tmCharSet);

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
                fe->mUnicodeRanges.set(x++, (range & (1 << k)) != 0);
            }
        }
    }

#ifdef PR_LOGGING
    if (LOG_FONTLIST_ENABLED()) {
        LOG_FONTLIST(("(fontlist) added (%s) to family (%s)"
             " with style: %s weight: %d stretch: %d",
             NS_ConvertUTF16toUTF8(fe->Name()).get(), 
             NS_ConvertUTF16toUTF8(ff->Name()).get(), 
             (logFont.lfItalic == 0xff) ? "italic" : "normal",
             logFont.lfWeight, fe->Stretch()));
    }
#endif
    return 1;
}

void
GDIFontFamily::FindStyleVariations()
{
    if (mHasStyles)
        return;
    mHasStyles = true;

    HDC hdc = GetDC(nsnull);
    SetGraphicsMode(hdc, GM_ADVANCED);

    LOGFONTW logFont;
    memset(&logFont, 0, sizeof(LOGFONTW));
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfPitchAndFamily = 0;
    PRUint32 l = NS_MIN<PRUint32>(mName.Length(), LF_FACESIZE - 1);
    memcpy(logFont.lfFaceName,
           nsPromiseFlatString(mName).get(),
           l * sizeof(PRUnichar));
    logFont.lfFaceName[l] = 0;

    EnumFontFamiliesExW(hdc, &logFont,
                        (FONTENUMPROCW)GDIFontFamily::FamilyAddStylesProc,
                        (LPARAM)this, 0);
#ifdef PR_LOGGING
    if (LOG_FONTLIST_ENABLED() && mAvailableFonts.Length() == 0) {
        LOG_FONTLIST(("(fontlist) no styles available in family \"%s\"",
                      NS_ConvertUTF16toUTF8(mName).get()));
    }
#endif

    ReleaseDC(nsnull, hdc);

    if (mIsBadUnderlineFamily)
        SetBadUnderlineFonts();
}

/***************************************************************
 *
 * gfxGDIFontList
 *
 */

gfxGDIFontList::gfxGDIFontList()
{
    mFontSubstitutes.Init(50);

    InitializeFontEmbeddingProcs();
}

static void
RemoveCharsetFromFontSubstitute(nsAString &aName)
{
    PRInt32 comma = aName.FindChar(PRUnichar(','));
    if (comma >= 0)
        aName.Truncate(comma);
}

#define MAX_VALUE_NAME 512
#define MAX_VALUE_DATA 512

nsresult
gfxGDIFontList::GetFontSubstitutes()
{
    HKEY hKey;
    DWORD i, rv, lenAlias, lenActual, valueType;
    WCHAR aliasName[MAX_VALUE_NAME];
    WCHAR actualName[MAX_VALUE_DATA];

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
          L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes",
          0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        return NS_ERROR_FAILURE;
    }

    for (i = 0, rv = ERROR_SUCCESS; rv != ERROR_NO_MORE_ITEMS; i++) {
        aliasName[0] = 0;
        lenAlias = ArrayLength(aliasName);
        actualName[0] = 0;
        lenActual = sizeof(actualName);
        rv = RegEnumValueW(hKey, i, aliasName, &lenAlias, NULL, &valueType, 
                (LPBYTE)actualName, &lenActual);

        if (rv != ERROR_SUCCESS || valueType != REG_SZ || lenAlias == 0) {
            continue;
        }

        if (aliasName[0] == WCHAR('@')) {
            continue;
        }

        nsAutoString substituteName((PRUnichar*) aliasName);
        nsAutoString actualFontName((PRUnichar*) actualName);
        RemoveCharsetFromFontSubstitute(substituteName);
        BuildKeyNameFromFontName(substituteName);
        RemoveCharsetFromFontSubstitute(actualFontName);
        BuildKeyNameFromFontName(actualFontName);
        gfxFontFamily *ff;
        if (!actualFontName.IsEmpty() && 
            (ff = mFontFamilies.GetWeak(actualFontName))) {
            mFontSubstitutes.Put(substituteName, ff);
        } else {
            mNonExistingFonts.AppendElement(substituteName);
        }
    }

    // "Courier" on a default Windows install is an ugly bitmap font.
    // If there is no substitution for Courier in the registry
    // substitute "Courier" with "Courier New".
    nsAutoString substituteName;
    substituteName.AssignLiteral("Courier");
    BuildKeyNameFromFontName(substituteName);
    if (!mFontSubstitutes.Get(substituteName)) {
        gfxFontFamily *ff;
        nsAutoString actualFontName;
        actualFontName.AssignLiteral("Courier New");
        BuildKeyNameFromFontName(actualFontName);
        ff = mFontFamilies.GetWeak(actualFontName);
        if (ff) {
            mFontSubstitutes.Put(substituteName, ff);
        }
    }
    return NS_OK;
}

nsresult
gfxGDIFontList::InitFontList()
{
    Telemetry::AutoTimer<Telemetry::GDI_INITFONTLIST_TOTAL> timer;
    gfxFontCache *fc = gfxFontCache::GetCache();
    if (fc)
        fc->AgeAllGenerations();

    // reset font lists
    gfxPlatformFontList::InitFontList();
    
    mFontSubstitutes.Clear();
    mNonExistingFonts.Clear();

    // iterate over available families
    LOGFONTW logfont;
    memset(&logfont, 0, sizeof(logfont));
    logfont.lfCharSet = DEFAULT_CHARSET;

    AutoDC hdc;
    int result = EnumFontFamiliesExW(hdc.GetDC(), &logfont,
                                     (FONTENUMPROCW)&EnumFontFamExProc,
                                     0, 0);

    GetFontSubstitutes();

    StartLoader(kDelayBeforeLoadingFonts, kIntervalBetweenLoadingFonts);

    return NS_OK;
}

int CALLBACK
gfxGDIFontList::EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe,
                                      NEWTEXTMETRICEXW *lpntme,
                                      DWORD fontType,
                                      LPARAM lParam)
{
    const LOGFONTW& lf = lpelfe->elfLogFont;

    if (lf.lfFaceName[0] == '@') {
        return 1;
    }

    nsAutoString name(lf.lfFaceName);
    BuildKeyNameFromFontName(name);

    gfxGDIFontList *fontList = PlatformFontList();

    if (!fontList->mFontFamilies.GetWeak(name)) {
        nsDependentString faceName(lf.lfFaceName);
        nsRefPtr<gfxFontFamily> family = new GDIFontFamily(faceName);
        fontList->mFontFamilies.Put(name, family);

        // if locale is such that CJK font names are the default coming from
        // GDI, then if a family name is non-ASCII immediately read in other
        // family names.  This assures that MS Gothic, MS Mincho are all found
        // before lookups begin.
        if (!IsASCII(faceName)) {
            family->ReadOtherFamilyNames(gfxPlatformFontList::PlatformFontList());
        }

        if (fontList->mBadUnderlineFamilyNames.Contains(name))
            family->SetBadUnderlineFamily();
    }

    return 1;
}

gfxFontEntry* 
gfxGDIFontList::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                const nsAString& aFullname)
{
    bool found;
    gfxFontEntry *lookup;

    // initialize name lookup tables if needed
    if (!mFaceNamesInitialized) {
        InitFaceNameLists();
    }

    // lookup in name lookup tables, return null if not found
    if (!(lookup = mPostscriptNames.GetWeak(aFullname, &found)) &&
        !(lookup = mFullnames.GetWeak(aFullname, &found))) 
    {
        return nsnull;
    }

    // create a new font entry with the proxy entry style characteristics
    PRUint16 w = (aProxyEntry->mWeight == 0 ? 400 : aProxyEntry->mWeight);
    bool isCFF = false; // jtdfix -- need to determine this
    
    // use the face name from the lookup font entry, which will be the localized
    // face name which GDI mapping tables use (e.g. with the system locale set to
    // Dutch, a fullname of 'Arial Bold' will find a font entry with the face name
    // 'Arial Vet' which can be used as a key in GDI font lookups).
    gfxFontEntry *fe = GDIFontEntry::CreateFontEntry(lookup->Name(), 
        gfxWindowsFontType(isCFF ? GFX_FONT_TYPE_PS_OPENTYPE : GFX_FONT_TYPE_TRUETYPE) /*type*/, 
        PRUint32(aProxyEntry->mItalic ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL), 
        w, aProxyEntry->mStretch, nsnull);
        
    if (!fe)
        return nsnull;

    fe->mIsUserFont = true;
    fe->mIsLocalUserFont = true;
    return fe;
}

void gfxGDIFontList::InitializeFontEmbeddingProcs()
{
    HMODULE fontlib = LoadLibraryW(L"t2embed.dll");
    if (!fontlib)
        return;
    TTLoadEmbeddedFontPtr = (TTLoadEmbeddedFontProc) GetProcAddress(fontlib, "TTLoadEmbeddedFont");
    TTDeleteEmbeddedFontPtr = (TTDeleteEmbeddedFontProc) GetProcAddress(fontlib, "TTDeleteEmbeddedFont");
}

// used to control stream read by Windows TTLoadEmbeddedFont API

class EOTFontStreamReader {
public:
    EOTFontStreamReader(const PRUint8 *aFontData, PRUint32 aLength, PRUint8 *aEOTHeader, 
                           PRUint32 aEOTHeaderLen, FontDataOverlay *aNameOverlay)
        : mCurrentChunk(0), mChunkOffset(0)
    {
        NS_ASSERTION(aFontData, "null font data ptr passed in");
        NS_ASSERTION(aEOTHeader, "null EOT header ptr passed in");
        NS_ASSERTION(aNameOverlay, "null name overlay struct passed in");

        if (aNameOverlay->overlaySrc) {
            mNumChunks = 4;
            // 0 : EOT header
            mDataChunks[0].mData = aEOTHeader;
            mDataChunks[0].mLength = aEOTHeaderLen;
            // 1 : start of font data to overlayDest
            mDataChunks[1].mData = aFontData;
            mDataChunks[1].mLength = aNameOverlay->overlayDest;
            // 2 : overlay data
            mDataChunks[2].mData = aFontData + aNameOverlay->overlaySrc;
            mDataChunks[2].mLength = aNameOverlay->overlaySrcLen;
            // 3 : rest of font data
            mDataChunks[3].mData = aFontData + aNameOverlay->overlayDest + aNameOverlay->overlaySrcLen;
            mDataChunks[3].mLength = aLength - aNameOverlay->overlayDest - aNameOverlay->overlaySrcLen;
        } else {
            mNumChunks = 2;
            // 0 : EOT header
            mDataChunks[0].mData = aEOTHeader;
            mDataChunks[0].mLength = aEOTHeaderLen;
            // 1 : font data
            mDataChunks[1].mData = aFontData;
            mDataChunks[1].mLength = aLength;
        }
    }

    ~EOTFontStreamReader() 
    { 

    }

    struct FontDataChunk {
        const PRUint8 *mData;
        PRUint32       mLength;
    };

    PRUint32                mNumChunks;
    FontDataChunk           mDataChunks[4];
    PRUint32                mCurrentChunk;
    PRUint32                mChunkOffset;

    unsigned long Read(void *outBuffer, const unsigned long aBytesToRead)
    {
        PRUint32 bytesLeft = aBytesToRead;  // bytes left in the out buffer
        PRUint8 *out = static_cast<PRUint8*> (outBuffer);

        while (mCurrentChunk < mNumChunks && bytesLeft) {
            FontDataChunk& currentChunk = mDataChunks[mCurrentChunk];
            PRUint32 bytesToCopy = NS_MIN(bytesLeft, 
                                          currentChunk.mLength - mChunkOffset);
            memcpy(out, currentChunk.mData + mChunkOffset, bytesToCopy);
            bytesLeft -= bytesToCopy;
            mChunkOffset += bytesToCopy;
            out += bytesToCopy;

            NS_ASSERTION(mChunkOffset <= currentChunk.mLength, "oops, buffer overrun");

            if (mChunkOffset == currentChunk.mLength) {
                mCurrentChunk++;
                mChunkOffset = 0;
            }
        }

        return aBytesToRead - bytesLeft;
    }

    static unsigned long ReadEOTStream(void *aReadStream, void *outBuffer, 
                                       const unsigned long aBytesToRead) 
    {
        EOTFontStreamReader *eotReader = 
                               static_cast<EOTFontStreamReader*> (aReadStream);
        return eotReader->Read(outBuffer, aBytesToRead);
    }        
        
};

gfxFontEntry* 
gfxGDIFontList::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry, 
                                 const PRUint8 *aFontData,
                                 PRUint32 aLength)
{
    // MakePlatformFont is responsible for deleting the font data with NS_Free
    // so we set up a stack object to ensure it is freed even if we take an
    // early exit
    struct FontDataDeleter {
        FontDataDeleter(const PRUint8 *aFontData)
            : mFontData(aFontData) { }
        ~FontDataDeleter() { NS_Free((void*)mFontData); }
        const PRUint8 *mFontData;
    };
    FontDataDeleter autoDelete(aFontData);

    // if calls aren't available, bail
    if (!TTLoadEmbeddedFontPtr || !TTDeleteEmbeddedFontPtr)
        return nsnull;

    bool hasVertical;
    bool isCFF = gfxFontUtils::IsCffFont(aFontData, hasVertical);

    nsresult rv;
    HANDLE fontRef = nsnull;
    bool isEmbedded = false;

    nsAutoString uniqueName;
    rv = gfxFontUtils::MakeUniqueUserFontName(uniqueName);
    if (NS_FAILED(rv))
        return nsnull;

    // for TTF fonts, first try using the t2embed library
    if (!isCFF) {
        // TrueType-style glyphs, use EOT library
        AutoFallibleTArray<PRUint8,2048> eotHeader;
        PRUint8 *buffer;
        PRUint32 eotlen;

        isEmbedded = true;
        PRUint32 nameLen = NS_MIN<PRUint32>(uniqueName.Length(), LF_FACESIZE - 1);
        nsAutoString fontName(Substring(uniqueName, 0, nameLen));
        
        FontDataOverlay overlayNameData = {0, 0, 0};

        rv = gfxFontUtils::MakeEOTHeader(aFontData, aLength, &eotHeader, 
                                         &overlayNameData);
        if (NS_SUCCEEDED(rv)) {

            // load in embedded font data
            eotlen = eotHeader.Length();
            buffer = reinterpret_cast<PRUint8*> (eotHeader.Elements());
            
            PRInt32 ret;
            ULONG privStatus, pulStatus;
            EOTFontStreamReader eotReader(aFontData, aLength, buffer, eotlen,
                                          &overlayNameData);

            ret = TTLoadEmbeddedFontPtr(&fontRef, TTLOAD_PRIVATE, &privStatus,
                                       LICENSE_PREVIEWPRINT, &pulStatus,
                                       EOTFontStreamReader::ReadEOTStream,
                                       &eotReader,
                                       (PRUnichar*)(fontName.get()), 0, 0);
            if (ret != E_NONE) {
                fontRef = nsnull;
                char buf[256];
                sprintf(buf, "font (%s) not loaded using TTLoadEmbeddedFont - error %8.8x", NS_ConvertUTF16toUTF8(aProxyEntry->FamilyName()).get(), ret);
                NS_WARNING(buf);
            }
        }
    }

    // load CFF fonts or fonts that failed with t2embed loader
    if (fontRef == nsnull) {
        // Postscript-style glyphs, swizzle name table, load directly
        FallibleTArray<PRUint8> newFontData;

        isEmbedded = false;
        rv = gfxFontUtils::RenameFont(uniqueName, aFontData, aLength, &newFontData);

        if (NS_FAILED(rv))
            return nsnull;
        
        DWORD numFonts = 0;

        PRUint8 *fontData = reinterpret_cast<PRUint8*> (newFontData.Elements());
        PRUint32 fontLength = newFontData.Length();
        NS_ASSERTION(fontData, "null font data after renaming");

        // http://msdn.microsoft.com/en-us/library/ms533942(VS.85).aspx
        // "A font that is added by AddFontMemResourceEx is always private 
        //  to the process that made the call and is not enumerable."
        fontRef = AddFontMemResourceEx(fontData, fontLength, 
                                       0 /* reserved */, &numFonts);
        if (!fontRef)
            return nsnull;

        // only load fonts with a single face contained in the data
        // AddFontMemResourceEx generates an additional face name for
        // vertical text if the font supports vertical writing
        if (fontRef && numFonts != 1 + !!hasVertical) {
            RemoveFontMemResourceEx(fontRef);
            return nsnull;
        }
    }

    // make a new font entry using the unique name
    WinUserFontData *winUserFontData = new WinUserFontData(fontRef, isEmbedded);
    PRUint16 w = (aProxyEntry->mWeight == 0 ? 400 : aProxyEntry->mWeight);

    GDIFontEntry *fe = GDIFontEntry::CreateFontEntry(uniqueName, 
        gfxWindowsFontType(isCFF ? GFX_FONT_TYPE_PS_OPENTYPE : GFX_FONT_TYPE_TRUETYPE) /*type*/, 
        PRUint32(aProxyEntry->mItalic ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL), 
        w, aProxyEntry->mStretch, winUserFontData);

    if (!fe)
        return fe;

    fe->mIsUserFont = true;

    // Uniscribe doesn't place CFF fonts loaded privately 
    // via AddFontMemResourceEx on XP/Vista
    if (isCFF && gfxWindowsPlatform::WindowsOSVersion() 
                 < gfxWindowsPlatform::kWindows7) {
        fe->mForceGDI = true;
    }
 
    return fe;
}

gfxFontEntry*
gfxGDIFontList::GetDefaultFont(const gfxFontStyle* aStyle, bool& aNeedsBold)
{
    // this really shouldn't fail to find a font....
    HGDIOBJ hGDI = ::GetStockObject(DEFAULT_GUI_FONT);
    LOGFONTW logFont;
    if (hGDI && ::GetObjectW(hGDI, sizeof(logFont), &logFont)) {
        nsAutoString resolvedName;
        if (ResolveFontName(nsDependentString(logFont.lfFaceName), resolvedName)) {
            return FindFontForFamily(resolvedName, aStyle, aNeedsBold);
        }
    }

    // ...but just in case, try another approach as well
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(ncm);
    BOOL status = ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 
                                          sizeof(ncm), &ncm, 0);
    if (status) {
        nsAutoString resolvedName;
        if (ResolveFontName(nsDependentString(ncm.lfMessageFont.lfFaceName), resolvedName)) {
            return FindFontForFamily(resolvedName, aStyle, aNeedsBold);
        }
    }

    return nsnull;
}


bool 
gfxGDIFontList::ResolveFontName(const nsAString& aFontName, nsAString& aResolvedFontName)
{
    nsAutoString keyName(aFontName);
    BuildKeyNameFromFontName(keyName);

    nsRefPtr<gfxFontFamily> ff;
    if (mFontSubstitutes.Get(keyName, &ff)) {
        aResolvedFontName = ff->Name();
        return true;
    }

    if (mNonExistingFonts.Contains(keyName))
        return false;

    if (gfxPlatformFontList::ResolveFontName(aFontName, aResolvedFontName))
        return true;

    return false;
}
