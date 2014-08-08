/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include <algorithm>

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
#include "gfxFontConstants.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WindowsVersion.h"

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

#define LOG_CMAPDATA_ENABLED() PR_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_cmapdata), \
                                   PR_LOG_DEBUG)

#endif // PR_LOGGING

static __inline void
BuildKeyNameFromFontName(nsAString &aName)
{
    if (aName.Length() >= LF_FACESIZE)
        aName.Truncate(LF_FACESIZE - 1);
    ToLowerCase(aName);
}

// Implementation of gfxPlatformFontList for Win32 GDI,
// using GDI font enumeration APIs to get the list of fonts

class WinUserFontData : public gfxUserFontData {
public:
    WinUserFontData(HANDLE aFontRef)
        : mFontRef(aFontRef)
    { }

    virtual ~WinUserFontData()
    {
        DebugOnly<BOOL> success;
        success = RemoveFontMemResourceEx(mFontRef);
#if DEBUG
        if (!success) {
            char buf[256];
            sprintf(buf, "error deleting font handle (%p) - RemoveFontMemResourceEx failed", mFontRef);
            NS_ASSERTION(success, buf);
        }
#endif
    }
    
    HANDLE mFontRef;
};

BYTE 
FontTypeToOutPrecision(uint8_t fontType)
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
                           bool aItalic, uint16_t aWeight, int16_t aStretch,
                           gfxUserFontData *aUserFontData,
                           bool aFamilyHasItalicFace)
    : gfxFontEntry(aFaceName),
      mWindowsFamily(0), mWindowsPitch(0),
      mFontType(aFontType),
      mForceGDI(false),
      mFamilyHasItalicFace(aFamilyHasItalicFace),
      mCharset(), mUnicodeRanges()
{
    mUserFontData = aUserFontData;
    mItalic = aItalic;
    mWeight = aWeight;
    mStretch = aStretch;
    if (IsType1())
        mForceGDI = true;
    mIsUserFont = aUserFontData != nullptr;

    InitLogFont(aFaceName, aFontType);
}

nsresult
GDIFontEntry::ReadCMAP(FontInfoData *aFontInfoData)
{
    // attempt this once, if errors occur leave a blank cmap
    if (mCharacterMap) {
        return NS_OK;
    }

    // skip non-SFNT fonts completely
    if (mFontType != GFX_FONT_TYPE_PS_OPENTYPE && 
        mFontType != GFX_FONT_TYPE_TT_OPENTYPE &&
        mFontType != GFX_FONT_TYPE_TRUETYPE) 
    {
        mCharacterMap = new gfxCharacterMap();
        mCharacterMap->mBuildOnTheFly = true;
        return NS_ERROR_FAILURE;
    }

    nsRefPtr<gfxCharacterMap> charmap;
    nsresult rv;
    bool unicodeFont = false, symbolFont = false;

    if (aFontInfoData && (charmap = GetCMAPFromFontInfo(aFontInfoData,
                                                        mUVSOffset,
                                                        symbolFont))) {
        mSymbolFont = symbolFont;
        rv = NS_OK;
    } else {
        uint32_t kCMAP = TRUETYPE_TAG('c','m','a','p');
        charmap = new gfxCharacterMap();
        AutoFallibleTArray<uint8_t,16384> cmap;
        rv = CopyFontTable(kCMAP, cmap);

        if (NS_SUCCEEDED(rv)) {
            rv = gfxFontUtils::ReadCMAP(cmap.Elements(), cmap.Length(),
                                        *charmap, mUVSOffset,
                                        unicodeFont, symbolFont);
        }
        mSymbolFont = symbolFont;
    }

    mHasCmapTable = NS_SUCCEEDED(rv);
    if (mHasCmapTable) {
        gfxPlatformFontList *pfl = gfxPlatformFontList::PlatformFontList();
        mCharacterMap = pfl->FindCharMap(charmap);
    } else {
        // if error occurred, initialize to null cmap
        mCharacterMap = new gfxCharacterMap();
        // For fonts where we failed to read the character map,
        // we can take a slow path to look up glyphs character by character
        mCharacterMap->mBuildOnTheFly = true;
    }

#ifdef PR_LOGGING
    LOG_FONTLIST(("(fontlist-cmap) name: %s, size: %d hash: %8.8x%s\n",
                  NS_ConvertUTF16toUTF8(mName).get(),
                  charmap->SizeOfIncludingThis(moz_malloc_size_of),
                  charmap->mHash, mCharacterMap == charmap ? " new" : ""));
    if (LOG_CMAPDATA_ENABLED()) {
        char prefix[256];
        sprintf(prefix, "(cmapdata) name: %.220s",
                NS_ConvertUTF16toUTF8(mName).get());
        charmap->Dump(prefix, eGfxLog_cmapdata);
    }
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
    bool isXP = !IsVistaOrLater();

    bool useClearType = isXP && !aFontStyle->systemFont &&
        (gfxWindowsPlatform::GetPlatform()->UseClearTypeAlways() ||
         (mIsUserFont && !mIsLocalUserFont &&
          gfxWindowsPlatform::GetPlatform()->UseClearTypeForDownloadableFonts()));

    return new gfxGDIFont(this, aFontStyle, aNeedsBold, 
                          (useClearType ? gfxFont::kAntialiasSubpixel
                                        : gfxFont::kAntialiasDefault));
}

nsresult
GDIFontEntry::CopyFontTable(uint32_t aTableTag,
                            FallibleTArray<uint8_t>& aBuffer)
{
    if (!IsTrueType()) {
        return NS_ERROR_FAILURE;
    }

    AutoDC dc;
    AutoSelectFont font(dc.GetDC(), &mLogFont);
    if (font.IsValid()) {
        uint32_t tableSize =
            ::GetFontData(dc.GetDC(),
                          NativeEndian::swapToBigEndian(aTableTag),
                          0, nullptr, 0);
        if (tableSize != GDI_ERROR) {
            if (aBuffer.SetLength(tableSize)) {
                ::GetFontData(dc.GetDC(),
                              NativeEndian::swapToBigEndian(aTableTag), 0,
                              aBuffer.Elements(), tableSize);
                return NS_OK;
            }
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    return NS_ERROR_FAILURE;
}

void
GDIFontEntry::FillLogFont(LOGFONTW *aLogFont,
                          uint16_t aWeight, gfxFloat aSize,
                          bool aUseCleartype)
{
    memcpy(aLogFont, &mLogFont, sizeof(LOGFONTW));

    aLogFont->lfHeight = (LONG)-ROUND(aSize);

    if (aLogFont->lfHeight == 0) {
        aLogFont->lfHeight = -1;
    }

    // If a non-zero weight is passed in, use this to override the original
    // weight in the entry's logfont. This is used to control synthetic bolding
    // for installed families with no bold face, and for downloaded fonts
    // (but NOT for local user fonts, because it could cause a different,
    // glyph-incompatible face to be used)
    if (aWeight) {
        aLogFont->lfWeight = aWeight;
    }

    // for non-local() user fonts, we never want to apply italics here;
    // if the face is described as italic, we should use it as-is,
    // and if it's not, but then the element is styled italic, we'll use
    // a cairo transform to create fake italic (oblique)
    if (IsUserFont() && !IsLocalUserFont()) {
        aLogFont->lfItalic = 0;
    }

    aLogFont->lfQuality = (aUseCleartype ? CLEARTYPE_QUALITY : DEFAULT_QUALITY);
}

#define MISSING_GLYPH 0x1F // glyph index returned for missing characters
                           // on WinXP with .fon fonts, but not Type1 (.pfb)

bool 
GDIFontEntry::TestCharacterMap(uint32_t aCh)
{
    if (!mCharacterMap) {
        ReadCMAP();
        NS_ASSERTION(mCharacterMap, "failed to initialize a character map");
    }

    if (mCharacterMap->mBuildOnTheFly) {
        if (aCh > 0xFFFF)
            return false;

        // previous code was using the group style
        gfxFontStyle fakeStyle;  
        if (mItalic)
            fakeStyle.style = NS_FONT_STYLE_ITALIC;
        fakeStyle.weight = mWeight * 100;

        nsRefPtr<gfxFont> tempFont = FindOrMakeFont(&fakeStyle, false);
        if (!tempFont || !tempFont->Valid())
            return false;
        gfxGDIFont *font = static_cast<gfxGDIFont*>(tempFont.get());

        HDC dc = GetDC((HWND)nullptr);
        SetGraphicsMode(dc, GM_ADVANCED);
        HFONT hfont = font->GetHFONT();
        HFONT oldFont = (HFONT)SelectObject(dc, hfont);

        wchar_t str[1] = { aCh };
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
            SCRIPT_CACHE sc = nullptr;
            HRESULT rv = ScriptGetCMap(dc, &sc, str, 1, 0, glyph);
            if (rv == S_OK)
                hasGlyph = true;
        }

        SelectObject(dc, oldFont);
        ReleaseDC(nullptr, dc);

        if (hasGlyph) {
            mCharacterMap->set(aCh);
            return true;
        }
    } else {
        // font had a cmap so simply check that
        return mCharacterMap->test(aCh);
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

    int len = std::min<int>(aName.Length(), LF_FACESIZE - 1);
    memcpy(&mLogFont.lfFaceName, aName.BeginReading(), len * sizeof(char16_t));
    mLogFont.lfFaceName[len] = '\0';
}

GDIFontEntry* 
GDIFontEntry::CreateFontEntry(const nsAString& aName,
                              gfxWindowsFontType aFontType, bool aItalic,
                              uint16_t aWeight, int16_t aStretch,
                              gfxUserFontData* aUserFontData,
                              bool aFamilyHasItalicFace)
{
    // jtdfix - need to set charset, unicode ranges, pitch/family

    GDIFontEntry *fe = new GDIFontEntry(aName, aFontType, aItalic,
                                        aWeight, aStretch, aUserFontData,
                                        aFamilyHasItalicFace);

    return fe;
}

void
GDIFontEntry::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                     FontListSizes* aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
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

    GDIFontEntry *fe = nullptr;
    for (uint32_t i = 0; i < ff->mAvailableFonts.Length(); ++i) {
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

    for (uint32_t i = 0; i < ff->mAvailableFonts.Length(); ++i) {
        fe = static_cast<GDIFontEntry*>(ff->mAvailableFonts[i].get());
        // check if we already know about this face
        if (fe->mWeight == logFont.lfWeight &&
            fe->mItalic == (logFont.lfItalic == 0xFF)) {
            // update the charset bit here since this could be different
            fe->mCharset.set(metrics.tmCharSet);
            return 1; 
        }
    }

    // We can't set the hasItalicFace flag correctly here,
    // because we might not have seen the family's italic face(s) yet.
    // So we'll set that flag for all members after loading all the faces.
    fe = GDIFontEntry::CreateFontEntry(nsDependentString(lpelfe->elfFullName),
                                       feType, (logFont.lfItalic == 0xFF),
                                       (uint16_t) (logFont.lfWeight), 0,
                                       nullptr, false);
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
        uint32_t x = 0;
        for (uint32_t i = 0; i < 4; ++i) {
            DWORD range = nmetrics->ntmFontSig.fsUsb[i];
            for (uint32_t k = 0; k < 32; ++k) {
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
GDIFontFamily::FindStyleVariations(FontInfoData *aFontInfoData)
{
    if (mHasStyles)
        return;
    mHasStyles = true;

    HDC hdc = GetDC(nullptr);
    SetGraphicsMode(hdc, GM_ADVANCED);

    LOGFONTW logFont;
    memset(&logFont, 0, sizeof(LOGFONTW));
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfPitchAndFamily = 0;
    uint32_t l = std::min<uint32_t>(mName.Length(), LF_FACESIZE - 1);
    memcpy(logFont.lfFaceName, mName.get(), l * sizeof(char16_t));

    EnumFontFamiliesExW(hdc, &logFont,
                        (FONTENUMPROCW)GDIFontFamily::FamilyAddStylesProc,
                        (LPARAM)this, 0);
#ifdef PR_LOGGING
    if (LOG_FONTLIST_ENABLED() && mAvailableFonts.Length() == 0) {
        LOG_FONTLIST(("(fontlist) no styles available in family \"%s\"",
                      NS_ConvertUTF16toUTF8(mName).get()));
    }
#endif

    ReleaseDC(nullptr, hdc);

    if (mIsBadUnderlineFamily) {
        SetBadUnderlineFonts();
    }

    // check for existence of italic face(s); if present, set the
    // FamilyHasItalic flag on all faces so that we'll know *not*
    // to use GDI's fake-italic effect with them
    size_t count = mAvailableFonts.Length();
    for (size_t i = 0; i < count; ++i) {
        if (mAvailableFonts[i]->IsItalic()) {
            for (uint32_t j = 0; j < count; ++j) {
                static_cast<GDIFontEntry*>(mAvailableFonts[j].get())->
                    mFamilyHasItalicFace = true;
            }
            break;
        }
    }
}

/***************************************************************
 *
 * gfxGDIFontList
 *
 */

gfxGDIFontList::gfxGDIFontList()
    : mFontSubstitutes(32)
{
#ifdef MOZ_BUNDLED_FONTS
    ActivateBundledFonts();
#endif
}

static void
RemoveCharsetFromFontSubstitute(nsAString &aName)
{
    int32_t comma = aName.FindChar(char16_t(','));
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
        rv = RegEnumValueW(hKey, i, aliasName, &lenAlias, nullptr, &valueType,
                (LPBYTE)actualName, &lenActual);

        if (rv != ERROR_SUCCESS || valueType != REG_SZ || lenAlias == 0) {
            continue;
        }

        if (aliasName[0] == WCHAR('@')) {
            continue;
        }

        nsAutoString substituteName((char16_t*) aliasName);
        nsAutoString actualFontName((char16_t*) actualName);
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
    if (!mFontSubstitutes.GetWeak(substituteName)) {
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

    GetPrefsAndStartLoader();

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
    gfxFontEntry *lookup;

    lookup = LookupInFaceNameLists(aFullname);
    if (!lookup) {
        return nullptr;
    }

    bool isCFF = false; // jtdfix -- need to determine this
    
    // use the face name from the lookup font entry, which will be the localized
    // face name which GDI mapping tables use (e.g. with the system locale set to
    // Dutch, a fullname of 'Arial Bold' will find a font entry with the face name
    // 'Arial Vet' which can be used as a key in GDI font lookups).
    GDIFontEntry *fe = GDIFontEntry::CreateFontEntry(lookup->Name(), 
        gfxWindowsFontType(isCFF ? GFX_FONT_TYPE_PS_OPENTYPE : GFX_FONT_TYPE_TRUETYPE) /*type*/, 
        lookup->mItalic ? NS_FONT_STYLE_ITALIC : NS_FONT_STYLE_NORMAL,
        lookup->mWeight, aProxyEntry->mStretch, nullptr,
        static_cast<GDIFontEntry*>(lookup)->mFamilyHasItalicFace);
        
    if (!fe)
        return nullptr;

    fe->mIsUserFont = true;
    fe->mIsLocalUserFont = true;

    // make the new font entry match the proxy entry style characteristics
    fe->mWeight = (aProxyEntry->mWeight == 0 ? 400 : aProxyEntry->mWeight);
    fe->mItalic = aProxyEntry->mItalic;

    return fe;
}

gfxFontEntry* 
gfxGDIFontList::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry, 
                                 const uint8_t *aFontData,
                                 uint32_t aLength)
{
    // MakePlatformFont is responsible for deleting the font data with NS_Free
    // so we set up a stack object to ensure it is freed even if we take an
    // early exit
    struct FontDataDeleter {
        FontDataDeleter(const uint8_t *aFontData)
            : mFontData(aFontData) { }
        ~FontDataDeleter() { NS_Free((void*)mFontData); }
        const uint8_t *mFontData;
    };
    FontDataDeleter autoDelete(aFontData);

    bool isCFF = gfxFontUtils::IsCffFont(aFontData);

    nsresult rv;
    HANDLE fontRef = nullptr;

    nsAutoString uniqueName;
    rv = gfxFontUtils::MakeUniqueUserFontName(uniqueName);
    if (NS_FAILED(rv))
        return nullptr;

    FallibleTArray<uint8_t> newFontData;

    rv = gfxFontUtils::RenameFont(uniqueName, aFontData, aLength, &newFontData);

    if (NS_FAILED(rv))
        return nullptr;
        
    DWORD numFonts = 0;

    uint8_t *fontData = reinterpret_cast<uint8_t*> (newFontData.Elements());
    uint32_t fontLength = newFontData.Length();
    NS_ASSERTION(fontData, "null font data after renaming");

    // http://msdn.microsoft.com/en-us/library/ms533942(VS.85).aspx
    // "A font that is added by AddFontMemResourceEx is always private 
    //  to the process that made the call and is not enumerable."
    fontRef = AddFontMemResourceEx(fontData, fontLength, 
                                    0 /* reserved */, &numFonts);
    if (!fontRef)
        return nullptr;

    // only load fonts with a single face contained in the data
    // AddFontMemResourceEx generates an additional face name for
    // vertical text if the font supports vertical writing but since
    // the font is referenced via the name this can be ignored
    if (fontRef && numFonts > 2) {
        RemoveFontMemResourceEx(fontRef);
        return nullptr;
    }

    // make a new font entry using the unique name
    WinUserFontData *winUserFontData = new WinUserFontData(fontRef);
    uint16_t w = (aProxyEntry->mWeight == 0 ? 400 : aProxyEntry->mWeight);

    GDIFontEntry *fe = GDIFontEntry::CreateFontEntry(uniqueName, 
        gfxWindowsFontType(isCFF ? GFX_FONT_TYPE_PS_OPENTYPE : GFX_FONT_TYPE_TRUETYPE) /*type*/, 
        uint32_t(aProxyEntry->mItalic ? NS_FONT_STYLE_ITALIC : NS_FONT_STYLE_NORMAL), 
        w, aProxyEntry->mStretch, winUserFontData, false);

    if (!fe)
        return fe;

    fe->mIsUserFont = true;

    // Uniscribe doesn't place CFF fonts loaded privately 
    // via AddFontMemResourceEx on XP/Vista
    if (isCFF && !IsWin7OrLater()) {
        fe->mForceGDI = true;
    }

    return fe;
}

gfxFontFamily*
gfxGDIFontList::FindFamily(const nsAString& aFamily)
{
    nsAutoString keyName(aFamily);
    BuildKeyNameFromFontName(keyName);

    gfxFontFamily *ff = mFontSubstitutes.GetWeak(keyName);
    if (ff) {
        return ff;
    }

    if (mNonExistingFonts.Contains(keyName)) {
        return nullptr;
    }

    return gfxPlatformFontList::FindFamily(aFamily);
}

gfxFontFamily*
gfxGDIFontList::GetDefaultFont(const gfxFontStyle* aStyle)
{
    gfxFontFamily *ff = nullptr;

    // this really shouldn't fail to find a font....
    HGDIOBJ hGDI = ::GetStockObject(DEFAULT_GUI_FONT);
    LOGFONTW logFont;
    if (hGDI && ::GetObjectW(hGDI, sizeof(logFont), &logFont)) {
        ff = FindFamily(nsDependentString(logFont.lfFaceName));
        if (ff) {
            return ff;
        }
    }

    // ...but just in case, try another approach as well
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(ncm);
    BOOL status = ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 
                                          sizeof(ncm), &ncm, 0);
    if (status) {
        ff = FindFamily(nsDependentString(ncm.lfMessageFont.lfFaceName));
    }

    return ff;
}

void
gfxGDIFontList::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                       FontListSizes* aSizes) const
{
    gfxPlatformFontList::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
    aSizes->mFontListSize +=
        mFontSubstitutes.SizeOfExcludingThis(SizeOfFamilyNameEntryExcludingThis,
                                             aMallocSizeOf);
    aSizes->mFontListSize +=
        mNonExistingFonts.SizeOfExcludingThis(aMallocSizeOf);
    for (uint32_t i = 0; i < mNonExistingFonts.Length(); ++i) {
        aSizes->mFontListSize +=
            mNonExistingFonts[i].SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
}

void
gfxGDIFontList::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                       FontListSizes* aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

// used to load system-wide font info on off-main thread
class GDIFontInfo : public FontInfoData {
public:
    GDIFontInfo(bool aLoadOtherNames,
                bool aLoadFaceNames,
                bool aLoadCmaps) :
        FontInfoData(aLoadOtherNames, aLoadFaceNames, aLoadCmaps)
    {}

    virtual ~GDIFontInfo() {}

    virtual void Load() {
        mHdc = GetDC(nullptr);
        SetGraphicsMode(mHdc, GM_ADVANCED);
        FontInfoData::Load();
        ReleaseDC(nullptr, mHdc);
    }

    // loads font data for all members of a given family
    virtual void LoadFontFamilyData(const nsAString& aFamilyName);

    // callback for GDI EnumFontFamiliesExW call
    static int CALLBACK EnumerateFontsForFamily(const ENUMLOGFONTEXW *lpelfe,
                                                const NEWTEXTMETRICEXW *nmetrics,
                                                DWORD fontType, LPARAM data);

    HDC mHdc;
};

struct EnumerateFontsForFamilyData {
    EnumerateFontsForFamilyData(const nsAString& aFamilyName,
                                GDIFontInfo& aFontInfo)
        : mFamilyName(aFamilyName), mFontInfo(aFontInfo)
    {}

    nsString mFamilyName;
    nsTArray<nsString> mOtherFamilyNames;
    GDIFontInfo& mFontInfo;
    nsString mPreviousFontName;
};

int CALLBACK GDIFontInfo::EnumerateFontsForFamily(
                 const ENUMLOGFONTEXW *lpelfe,
                 const NEWTEXTMETRICEXW *nmetrics,
                 DWORD fontType, LPARAM data)
{
    EnumerateFontsForFamilyData *famData =
        reinterpret_cast<EnumerateFontsForFamilyData*>(data);
    HDC hdc = famData->mFontInfo.mHdc;
    LOGFONTW logFont = lpelfe->elfLogFont;
    const NEWTEXTMETRICW& metrics = nmetrics->ntmTm;

    AutoSelectFont font(hdc, &logFont);
    if (!font.IsValid()) {
        return 1;
    }

    FontFaceData fontData;
    nsDependentString fontName(lpelfe->elfFullName);

    // callback called for each style-charset so return if style already seen
    if (fontName.Equals(famData->mPreviousFontName)) {
        return 1;
    }
    famData->mPreviousFontName = fontName;
    famData->mFontInfo.mLoadStats.fonts++;

    // read name table info
    bool nameDataLoaded = false;
    if (famData->mFontInfo.mLoadFaceNames || famData->mFontInfo.mLoadOtherNames) {
        uint32_t kNAME =
            NativeEndian::swapToBigEndian(TRUETYPE_TAG('n','a','m','e'));
        uint32_t nameSize;
        AutoFallibleTArray<uint8_t, 1024> nameData;

        nameSize = ::GetFontData(hdc, kNAME, 0, nullptr, 0);
        if (nameSize != GDI_ERROR &&
            nameSize > 0 &&
            nameData.SetLength(nameSize)) {
            ::GetFontData(hdc, kNAME, 0, nameData.Elements(), nameSize);

            // face names
            if (famData->mFontInfo.mLoadFaceNames) {
                gfxFontUtils::ReadCanonicalName((const char*)(nameData.Elements()), nameSize,
                                                gfxFontUtils::NAME_ID_FULL,
                                                fontData.mFullName);
                gfxFontUtils::ReadCanonicalName((const char*)(nameData.Elements()), nameSize,
                                                gfxFontUtils::NAME_ID_POSTSCRIPT,
                                                fontData.mPostscriptName);
                nameDataLoaded = true;
                famData->mFontInfo.mLoadStats.facenames++;
            }

            // other family names
            if (famData->mFontInfo.mLoadOtherNames) {
                gfxFontFamily::ReadOtherFamilyNamesForFace(famData->mFamilyName,
                                                           (const char*)(nameData.Elements()),
                                                           nameSize,
                                                           famData->mOtherFamilyNames,
                                                           false);
            }
        }
    }

    // read cmap
    bool cmapLoaded = false;
    gfxWindowsFontType feType =
        GDIFontEntry::DetermineFontType(metrics, fontType);
    if (famData->mFontInfo.mLoadCmaps &&
        (feType == GFX_FONT_TYPE_PS_OPENTYPE ||
         feType == GFX_FONT_TYPE_TT_OPENTYPE ||
         feType == GFX_FONT_TYPE_TRUETYPE))
    {
        uint32_t kCMAP =
            NativeEndian::swapToBigEndian(TRUETYPE_TAG('c','m','a','p'));
        uint32_t cmapSize;
        AutoFallibleTArray<uint8_t, 1024> cmapData;

        cmapSize = ::GetFontData(hdc, kCMAP, 0, nullptr, 0);
        if (cmapSize != GDI_ERROR &&
            cmapSize > 0 &&
            cmapData.SetLength(cmapSize)) {
            ::GetFontData(hdc, kCMAP, 0, cmapData.Elements(), cmapSize);
            bool cmapLoaded = false;
            bool unicodeFont = false, symbolFont = false;
            nsRefPtr<gfxCharacterMap> charmap = new gfxCharacterMap();
            uint32_t offset;

            if (NS_SUCCEEDED(gfxFontUtils::ReadCMAP(cmapData.Elements(),
                                                    cmapSize, *charmap,
                                                    offset, unicodeFont,
                                                    symbolFont))) {
                fontData.mCharacterMap = charmap;
                fontData.mUVSOffset = offset;
                fontData.mSymbolFont = symbolFont;
                cmapLoaded = true;
                famData->mFontInfo.mLoadStats.cmaps++;
            }
        }
    }

    if (cmapLoaded || nameDataLoaded) {
        famData->mFontInfo.mFontFaceData.Put(fontName, fontData);
    }

    return 1;
}

void
GDIFontInfo::LoadFontFamilyData(const nsAString& aFamilyName)
{
    // iterate over the family
    LOGFONTW logFont;
    memset(&logFont, 0, sizeof(LOGFONTW));
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfPitchAndFamily = 0;
    uint32_t l = std::min<uint32_t>(aFamilyName.Length(), LF_FACESIZE - 1);
    memcpy(logFont.lfFaceName, aFamilyName.BeginReading(), l * sizeof(char16_t));

    EnumerateFontsForFamilyData data(aFamilyName, *this);

    EnumFontFamiliesExW(mHdc, &logFont,
                        (FONTENUMPROCW)GDIFontInfo::EnumerateFontsForFamily,
                        (LPARAM)(&data), 0);

    // if found other names, insert them
    if (data.mOtherFamilyNames.Length() != 0) {
        mOtherFamilyNames.Put(aFamilyName, data.mOtherFamilyNames);
        mLoadStats.othernames += data.mOtherFamilyNames.Length();
    }
}

already_AddRefed<FontInfoData>
gfxGDIFontList::CreateFontInfoData()
{
    bool loadCmaps = !UsesSystemFallback() ||
        gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback();

    nsRefPtr<GDIFontInfo> fi =
        new GDIFontInfo(true, NeedFullnamePostscriptNames(), loadCmaps);

    return fi.forget();
}

#ifdef MOZ_BUNDLED_FONTS

void
gfxGDIFontList::ActivateBundledFonts()
{
    nsCOMPtr<nsIFile> localDir;
    nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(localDir));
    if (NS_FAILED(rv)) {
        return;
    }
    if (NS_FAILED(localDir->Append(NS_LITERAL_STRING("fonts")))) {
        return;
    }
    bool isDir;
    if (NS_FAILED(localDir->IsDirectory(&isDir)) || !isDir) {
        return;
    }

    nsCOMPtr<nsISimpleEnumerator> e;
    rv = localDir->GetDirectoryEntries(getter_AddRefs(e));
    if (NS_FAILED(rv)) {
        return;
    }

    bool hasMore;
    while (NS_SUCCEEDED(e->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> entry;
        if (NS_FAILED(e->GetNext(getter_AddRefs(entry)))) {
            break;
        }
        nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
        if (!file) {
            continue;
        }
        nsCString path;
        if (NS_FAILED(file->GetNativePath(path))) {
            continue;
        }
        AddFontResourceEx(path.get(), FR_PRIVATE, nullptr);
    }
}

#endif
