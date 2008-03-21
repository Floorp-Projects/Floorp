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
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Masatoshi Kimura <VYV03354@nifty.ne.jp>
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

#include "gfxWindowsPlatform.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"

#include "nsUnicharUtils.h"

#include "nsIPref.h"
#include "nsServiceManagerUtils.h"

#include "nsIWindowsRegKey.h"

#include "gfxWindowsFonts.h"

#include <string>

#include "lcms.h"

//#define DEBUG_CMAP_SIZE 1

static nsresult ReadCMAP(HDC hdc, FontEntry *aFontEntry);

int PR_CALLBACK
gfxWindowsPlatform::PrefChangedCallback(const char *aPrefName, void *closure)
{
    // XXX this could be made to only clear out the cache for the prefs that were changed
    // but it probably isn't that big a deal.
    gfxWindowsPlatform *plat = static_cast<gfxWindowsPlatform *>(closure);
    plat->mPrefFonts.Clear();
    return 0;
}

gfxWindowsPlatform::gfxWindowsPlatform()
{
    mFonts.Init(200);
    mFontAliases.Init(20);
    mFontSubstitutes.Init(50);
    mPrefFonts.Init(10);

    UpdateFontList();

    nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID);
    pref->RegisterCallback("font.", PrefChangedCallback, this);
    pref->RegisterCallback("font.name-list.", PrefChangedCallback, this);
    pref->RegisterCallback("intl.accept_languages", PrefChangedCallback, this);
    // don't bother unregistering.  We'll get shutdown after the pref service
}

gfxWindowsPlatform::~gfxWindowsPlatform()
{
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenSurface(const gfxIntSize& size,
                                           gfxASurface::gfxImageFormat imageFormat)
{
    gfxASurface *surf = new gfxWindowsSurface(size, imageFormat);
    NS_IF_ADDREF(surf);
    return surf;
}

int CALLBACK 
gfxWindowsPlatform::FontEnumProc(const ENUMLOGFONTEXW *lpelfe,
                                 const NEWTEXTMETRICEXW *nmetrics,
                                 DWORD fontType, LPARAM data)
{
    FontTable *ht = reinterpret_cast<FontTable*>(data);

    const NEWTEXTMETRICW& metrics = nmetrics->ntmTm;
    LOGFONTW logFont = lpelfe->elfLogFont;

    // Ignore vertical fonts
    if (logFont.lfFaceName[0] == L'@') {
        return 1;
    }

#ifdef DEBUG_pavlov
    printf("%s %d %d %d\n", NS_ConvertUTF16toUTF8(nsDependentString(logFont.lfFaceName)).get(),
           logFont.lfCharSet, logFont.lfItalic, logFont.lfWeight);
#endif

    nsString name(logFont.lfFaceName);
    ToLowerCase(name);

    nsRefPtr<FontFamily> ff;
    if (!ht->Get(name, &ff)) {
        ff = new FontFamily(nsDependentString(logFont.lfFaceName));
        ht->Put(name, ff);
    }

    nsRefPtr<FontEntry> fe;
    for (PRUint32 i = 0; i < ff->mVariations.Length(); ++i) {
        fe = ff->mVariations[i];
        if (fe->mWeight == logFont.lfWeight &&
            fe->mItalic == (logFont.lfItalic == 0xFF)) {
            return 1; /* we already know about this font */
        }
    }

    fe = new FontEntry(ff->mName);
    /* don't append it until the end in case of error */

    fe->mItalic = (logFont.lfItalic == 0xFF);
    fe->mWeight = logFont.lfWeight;

    if (metrics.ntmFlags & NTM_TYPE1)
        fe->mIsType1 = fe->mForceGDI = PR_TRUE;
    if (metrics.ntmFlags & (NTM_PS_OPENTYPE | NTM_TT_OPENTYPE))
        fe->mTrueType = PR_TRUE;

    // mark the charset bit
    fe->mCharset[metrics.tmCharSet] = 1;

    fe->mWindowsFamily = logFont.lfPitchAndFamily & 0xF0;
    fe->mWindowsPitch = logFont.lfPitchAndFamily & 0x0F;

    if (nmetrics->ntmFontSig.fsUsb[0] == 0x00000000 &&
        nmetrics->ntmFontSig.fsUsb[1] == 0x00000000 &&
        nmetrics->ntmFontSig.fsUsb[2] == 0x00000000 &&
        nmetrics->ntmFontSig.fsUsb[3] == 0x00000000) {
        // no unicode ranges
        fe->mUnicodeFont = PR_FALSE;
    } else {
        fe->mUnicodeFont = PR_TRUE;

        // set the unicode ranges
        PRUint32 x = 0;
        for (PRUint32 i = 0; i < 4; ++i) {
            DWORD range = nmetrics->ntmFontSig.fsUsb[i];
            for (PRUint32 k = 0; k < 32; ++k) {
                fe->mUnicodeRanges[x++] = (range & (1 << k)) != 0;
            }
        }
    }

    /* read in the character map */
    HDC hdc = GetDC(nsnull);
    logFont.lfCharSet = DEFAULT_CHARSET;
    HFONT font = CreateFontIndirectW(&logFont);

    NS_ASSERTION(font, "This font creation should never ever ever fail");
    if (font) {
        HFONT oldFont = (HFONT)SelectObject(hdc, font);

        TEXTMETRIC metrics;
        GetTextMetrics(hdc, &metrics);
        if (metrics.tmPitchAndFamily & TMPF_TRUETYPE)
            fe->mTrueType = PR_TRUE;

        if (NS_FAILED(ReadCMAP(hdc, fe))) {
            // Type1 fonts aren't necessarily Unicode but
            // this is the best guess we can make here
            if (fe->mIsType1)
                fe->mUnicodeFont = PR_TRUE;
            else
                fe->mUnicodeFont = PR_FALSE;

            //printf("%d, %s failed to get cmap\n", aFontEntry->mIsType1, NS_ConvertUTF16toUTF8(aFontEntry->mName).get());
        }

        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }

    ReleaseDC(nsnull, hdc);

    if (!fe->mUnicodeFont) {
        /* non-unicode fonts.. boy lets just set all code points
           between 0x20 and 0xFF.  All the ones on my system do...
           If we really wanted to test which characters in this
           range were supported we could just generate a string with
           each codepoint and do GetGlyphIndicies or similar to determine
           what is there.
        */
        fe->mCharacterMap.SetRange(0x20, 0xFF);
    }

    /* append the variation to the font family */
    ff->mVariations.AppendElement(fe);

    return 1;
}

// general cmap reading routines moved to gfxFontUtils.cpp

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
    
    return gfxFontUtils::ReadCMAP(buf, len, aFontEntry->mCharacterMap, aFontEntry->mUnicodeRanges,
                                  aFontEntry->mUnicodeFont, aFontEntry->mSymbolFont);
}

PLDHashOperator PR_CALLBACK
gfxWindowsPlatform::FontGetStylesProc(nsStringHashKey::KeyType aKey,
                                      nsRefPtr<FontFamily>& aFontFamily,
                                      void* userArg)
{
    NS_ASSERTION(aFontFamily->mVariations.Length() == 1, "We should only have 1 variation here");
    nsRefPtr<FontEntry> aFontEntry = aFontFamily->mVariations[0];

    HDC hdc = GetDC(nsnull);

    LOGFONTW logFont;
    memset(&logFont, 0, sizeof(LOGFONTW));
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfPitchAndFamily = 0;
    PRUint32 l = PR_MIN(aFontEntry->GetName().Length(), LF_FACESIZE - 1);
    memcpy(logFont.lfFaceName,
           nsPromiseFlatString(aFontEntry->GetName()).get(),
           l * sizeof(PRUnichar));
    logFont.lfFaceName[l] = 0;

    EnumFontFamiliesExW(hdc, &logFont, (FONTENUMPROCW)gfxWindowsPlatform::FontEnumProc, (LPARAM)userArg, 0);

    ReleaseDC(nsnull, hdc);

    // Look for font families without bold variations and add a FontEntry
    // with synthetic bold (weight 600) for them.
    nsRefPtr<FontEntry> darkestItalic;
    nsRefPtr<FontEntry> darkestNonItalic;
    PRUint8 highestItalic = 0, highestNonItalic = 0;
    for (PRUint32 i = 0; i < aFontFamily->mVariations.Length(); i++) {
        nsRefPtr<FontEntry> fe = aFontFamily->mVariations[i];
        if (fe->mItalic) {
            if (!darkestItalic || fe->mWeight > darkestItalic->mWeight)
                darkestItalic = fe;
        } else {
            if (!darkestNonItalic || fe->mWeight > darkestNonItalic->mWeight)
                darkestNonItalic = fe;
        }
    }

    if (darkestItalic && darkestItalic->mWeight < 600) {
        nsRefPtr<FontEntry> newEntry = new FontEntry(*darkestItalic.get());
        newEntry->mWeight = 600;
        aFontFamily->mVariations.AppendElement(newEntry);
    }
    if (darkestNonItalic && darkestNonItalic->mWeight < 600) {
        nsRefPtr<FontEntry> newEntry = new FontEntry(*darkestNonItalic.get());
        newEntry->mWeight = 600;
        aFontFamily->mVariations.AppendElement(newEntry);
    }

    return PL_DHASH_NEXT;
}

struct FontListData {
    FontListData(const nsACString& aLangGroup, const nsACString& aGenericFamily, nsStringArray& aListOfFonts) :
        mLangGroup(aLangGroup), mGenericFamily(aGenericFamily), mStringArray(aListOfFonts) {}
    const nsACString& mLangGroup;
    const nsACString& mGenericFamily;
    nsStringArray& mStringArray;
};

PLDHashOperator PR_CALLBACK
gfxWindowsPlatform::HashEnumFunc(nsStringHashKey::KeyType aKey,
                                 nsRefPtr<FontFamily>& aFontFamily,
                                 void* userArg)
{
    FontListData *data = (FontListData*)userArg;

    // use the first variation for now.  This data should be the same
    // for all the variations and should probably be moved up to
    // the Family
    nsRefPtr<FontEntry> aFontEntry = aFontFamily->mVariations[0];

    /* skip symbol fonts */
    if (aFontEntry->mSymbolFont)
        return PL_DHASH_NEXT;

    if (aFontEntry->SupportsLangGroup(data->mLangGroup) &&
        aFontEntry->MatchesGenericFamily(data->mGenericFamily))
        data->mStringArray.AppendString(aFontFamily->mName);

    return PL_DHASH_NEXT;
}

nsresult
gfxWindowsPlatform::GetFontList(const nsACString& aLangGroup,
                                const nsACString& aGenericFamily,
                                nsStringArray& aListOfFonts)
{
    FontListData data(aLangGroup, aGenericFamily, aListOfFonts);

    mFonts.Enumerate(gfxWindowsPlatform::HashEnumFunc, &data);

    aListOfFonts.Sort();
    aListOfFonts.Compact();

    return NS_OK;
}

static void
RemoveCharsetFromFontSubstitute(nsAString &aName)
{
    PRInt32 comma = aName.FindChar(PRUnichar(','));
    if (comma >= 0)
        aName.Truncate(comma);
}

static void
BuildKeyNameFromFontName(nsAString &aName)
{
    if (aName.Length() >= LF_FACESIZE)
        aName.Truncate(LF_FACESIZE - 1);
    ToLowerCase(aName);
}

nsresult
gfxWindowsPlatform::UpdateFontList()
{
    gfxFontCache *fc = gfxFontCache::GetCache();
    if (fc)
        fc->AgeAllGenerations();
    mFonts.Clear();
    mFontAliases.Clear();
    mNonExistingFonts.Clear();
    mFontSubstitutes.Clear();
    mPrefFonts.Clear();
    mCodepointsWithNoFonts.reset();

    LOGFONTW logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfFaceName[0] = 0;
    logFont.lfPitchAndFamily = 0;

    // Use the screen DC here.. should we use something else for printing?
    HDC dc = ::GetDC(nsnull);
    EnumFontFamiliesExW(dc, &logFont, (FONTENUMPROCW)gfxWindowsPlatform::FontEnumProc, (LPARAM)&mFonts, 0);
    ::ReleaseDC(nsnull, dc);

    // Look for additional styles
    mFonts.Enumerate(gfxWindowsPlatform::FontGetStylesProc, &mFonts);

    // Create the list of FontSubstitutes
    nsCOMPtr<nsIWindowsRegKey> regKey = do_CreateInstance("@mozilla.org/windows-registry-key;1");
    if (!regKey)
        return NS_ERROR_FAILURE;
     NS_NAMED_LITERAL_STRING(kFontSubstitutesKey, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes");

    nsresult rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                               kFontSubstitutesKey, nsIWindowsRegKey::ACCESS_READ);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 count;
    rv = regKey->GetValueCount(&count);
    if (NS_FAILED(rv) || count == 0)
        return rv;
    for (PRUint32 i = 0; i < count; i++) {
        nsAutoString substituteName;
        rv = regKey->GetValueName(i, substituteName);
        if (NS_FAILED(rv) || substituteName.IsEmpty() ||
            substituteName.CharAt(1) == PRUnichar('@'))
            continue;
        PRUint32 valueType;
        rv = regKey->GetValueType(substituteName, &valueType);
        if (NS_FAILED(rv) || valueType != nsIWindowsRegKey::TYPE_STRING)
            continue;
        nsAutoString actualFontName;
        rv = regKey->ReadStringValue(substituteName, actualFontName);
        if (NS_FAILED(rv))
            continue;

        RemoveCharsetFromFontSubstitute(substituteName);
        BuildKeyNameFromFontName(substituteName);
        RemoveCharsetFromFontSubstitute(actualFontName);
        BuildKeyNameFromFontName(actualFontName);
        nsRefPtr<FontFamily> ff;
        if (!actualFontName.IsEmpty() && mFonts.Get(actualFontName, &ff))
            mFontSubstitutes.Put(substituteName, ff);
        else
            mNonExistingFonts.AppendString(substituteName);
    }

    // initialize ranges of characters for which system-wide font search should be skipped
    mCodepointsWithNoFonts.SetRange(0,0x1f);     // C0 controls
    mCodepointsWithNoFonts.SetRange(0x7f,0x9f);  // C1 controls

    InitBadUnderlineList();

    return NS_OK;
}

static PRBool SimpleResolverCallback(const nsAString& aName, void* aClosure)
{
    nsString* result = static_cast<nsString*>(aClosure);
    result->Assign(aName);
    return PR_FALSE;
}

void
gfxWindowsPlatform::InitBadUnderlineList()
{
    nsAutoTArray<nsAutoString, 10> blacklist;
    gfxFontUtils::GetPrefsFontList("font.blacklist.underline_offset", blacklist);
    PRUint32 numFonts = blacklist.Length();
    for (PRUint32 i = 0; i < numFonts; i++) {
        PRBool aborted;
        nsAutoString resolved;
        ResolveFontName(blacklist[i], SimpleResolverCallback, &resolved, aborted);
        if (resolved.IsEmpty())
            continue;
        FontFamily *ff = FindFontFamily(resolved);
        if (!ff)
            continue;
        for (PRUint32 j = 0; j < ff->mVariations.Length(); ++j) {
            nsRefPtr<FontEntry> fe = ff->mVariations[j];
            fe->mIsBadUnderlineFont = 1;
        }
    }
}

nsresult
gfxWindowsPlatform::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    aFamilyName.Truncate();
    PRBool aborted;
    return ResolveFontName(aFontName, SimpleResolverCallback, &aFamilyName, aborted);
}

struct ResolveData {
    ResolveData(gfxPlatform::FontResolverCallback aCallback,
                gfxWindowsPlatform *aCaller, const nsAString *aFontName,
                void *aClosure) :
        mFoundCount(0), mCallback(aCallback), mCaller(aCaller),
        mFontName(aFontName), mClosure(aClosure) {}
    PRUint32 mFoundCount;
    gfxPlatform::FontResolverCallback mCallback;
    gfxWindowsPlatform *mCaller;
    const nsAString *mFontName;
    void *mClosure;
};

nsresult
gfxWindowsPlatform::ResolveFontName(const nsAString& aFontName,
                                    FontResolverCallback aCallback,
                                    void *aClosure,
                                    PRBool& aAborted)
{
    if (aFontName.IsEmpty())
        return NS_ERROR_FAILURE;

    nsAutoString keyName(aFontName);
    BuildKeyNameFromFontName(keyName);

    nsRefPtr<FontFamily> ff;
    if (mFonts.Get(keyName, &ff) ||
        mFontSubstitutes.Get(keyName, &ff) ||
        mFontAliases.Get(keyName, &ff)) {
        aAborted = !(*aCallback)(ff->mName, aClosure);
        // XXX If the font has font link, we should add the linked font.
        return NS_OK;
    }

    if (mNonExistingFonts.IndexOf(keyName) >= 0) {
        aAborted = PR_FALSE;
        return NS_OK;
    }

    LOGFONTW logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfPitchAndFamily = 0;
    PRInt32 len = aFontName.Length();
    if (len >= LF_FACESIZE)
        len = LF_FACESIZE - 1;
    memcpy(logFont.lfFaceName,
           nsPromiseFlatString(aFontName).get(), len * sizeof(PRUnichar));
    logFont.lfFaceName[len] = 0;

    HDC dc = ::GetDC(nsnull);
    ResolveData data(aCallback, this, &keyName, aClosure);
    aAborted = !EnumFontFamiliesExW(dc, &logFont,
                                    (FONTENUMPROCW)gfxWindowsPlatform::FontResolveProc,
                                    (LPARAM)&data, 0);
    if (data.mFoundCount == 0)
        mNonExistingFonts.AppendString(keyName);
    ::ReleaseDC(nsnull, dc);

    return NS_OK;
}

int CALLBACK 
gfxWindowsPlatform::FontResolveProc(const ENUMLOGFONTEXW *lpelfe,
                                    const NEWTEXTMETRICEXW *nmetrics,
                                    DWORD fontType, LPARAM data)
{
    const LOGFONTW& logFont = lpelfe->elfLogFont;
    // Ignore vertical fonts
    if (logFont.lfFaceName[0] == L'@' || logFont.lfFaceName[0] == 0)
        return 1;

    ResolveData *rData = reinterpret_cast<ResolveData*>(data);

    nsAutoString name(logFont.lfFaceName);

    // Save the alias name to cache
    nsRefPtr<FontFamily> ff;
    nsAutoString keyName(name);
    BuildKeyNameFromFontName(keyName);
    if (!rData->mCaller->mFonts.Get(keyName, &ff)) {
        // This case only occurs on failing to build
        // the list of font substitue. In this case, the user should
        // reboot the Windows. Probably, we don't have the good way for
        // resolving in this time.
        NS_WARNING("Cannot find actual font");
        return 1;
    }

    rData->mFoundCount++;
    rData->mCaller->mFontAliases.Put(*(rData->mFontName), ff);

    return (rData->mCallback)(name, rData->mClosure);

    // XXX If the font has font link, we should add the linked font.
}

struct FontSearch {
    FontSearch(PRUint32 aCh, gfxWindowsFont *aFont) :
        ch(aCh), fontToMatch(aFont), matchRank(0) {
    }
    PRUint32 ch;
    nsRefPtr<gfxWindowsFont> fontToMatch;
    PRInt32 matchRank;
    nsRefPtr<FontEntry> bestMatch;
};

PLDHashOperator PR_CALLBACK
gfxWindowsPlatform::FindFontForCharProc(nsStringHashKey::KeyType aKey,
                                        nsRefPtr<FontFamily>& aFontFamily,
                                        void* userArg)
{
    FontSearch *data = (FontSearch*)userArg;

    const PRUint32 ch = data->ch;

    nsRefPtr<FontEntry> fe = GetPlatform()->FindFontEntry(aFontFamily, data->fontToMatch->GetStyle());

    // skip over non-unicode and bitmap fonts and fonts that don't have
    // the code point we're looking for
    if (fe->IsCrappyFont() || !fe->mCharacterMap.test(ch))
        return PL_DHASH_NEXT;

    PRInt32 rank = 0;
    // fonts that claim to support the range are more
    // likely to be "better fonts" than ones that don't... (in theory)
    if (fe->SupportsRange(gfxFontUtils::CharRangeBit(ch)))
        rank += 1;

    if (fe->SupportsLangGroup(data->fontToMatch->GetStyle()->langGroup))
        rank += 2;

    if (fe->mWindowsFamily == data->fontToMatch->GetFontEntry()->mWindowsFamily)
        rank += 3;
    if (fe->mWindowsPitch == data->fontToMatch->GetFontEntry()->mWindowsFamily)
        rank += 3;

    /* italic */
    const PRBool italic = (data->fontToMatch->GetStyle()->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) ? PR_TRUE : PR_FALSE;
    if (fe->mItalic != italic)
        rank += 3;

    /* weight */
    PRInt8 baseWeight, weightDistance;
    data->fontToMatch->GetStyle()->ComputeWeightAndOffset(&baseWeight, &weightDistance);
    if (fe->mWeight == (baseWeight * 100) + (weightDistance * 100))
        rank += 2;
    else if (fe->mWeight == data->fontToMatch->GetFontEntry()->mWeight)
        rank += 1;

    if (rank > data->matchRank ||
        (rank == data->matchRank && Compare(fe->GetName(), data->bestMatch->GetName()) > 0)) {
        data->bestMatch = fe;
        data->matchRank = rank;
    }

    return PL_DHASH_NEXT;
}


FontEntry *
gfxWindowsPlatform::FindFontForChar(PRUint32 aCh, gfxWindowsFont *aFont)
{
    // is codepoint with no matching font? return null immediately
    if (mCodepointsWithNoFonts.test(aCh)) {
        return nsnull;
    }

    FontSearch data(aCh, aFont);

    // find fonts that support the character
    mFonts.Enumerate(gfxWindowsPlatform::FindFontForCharProc, &data);

    // no match? add to set of non-matching codepoints
    if (!data.bestMatch) {
        mCodepointsWithNoFonts.set(aCh);
    }
    
    return data.bestMatch;
}

gfxFontGroup *
gfxWindowsPlatform::CreateFontGroup(const nsAString &aFamilies,
                                    const gfxFontStyle *aStyle)
{
    return new gfxWindowsFontGroup(aFamilies, aStyle);
}

FontFamily *
gfxWindowsPlatform::FindFontFamily(const nsAString& aName)
{
    nsString name(aName);
    ToLowerCase(name);

    nsRefPtr<FontFamily> ff;
    if (!mFonts.Get(name, &ff) &&
        !mFontSubstitutes.Get(name, &ff) &&
        !mFontAliases.Get(name, &ff)) {
        return nsnull;
    }
    return ff.get();
}

FontEntry *
gfxWindowsPlatform::FindFontEntry(const nsAString& aName, const gfxFontStyle *aFontStyle)
{
    nsRefPtr<FontFamily> ff = FindFontFamily(aName);
    if (!ff)
        return nsnull;

    return FindFontEntry(ff, aFontStyle);
}

FontEntry *
gfxWindowsPlatform::FindFontEntry(FontFamily *aFontFamily, const gfxFontStyle *aFontStyle)
{
    PRUint8 bestMatch = 0;
    PRBool italic = (aFontStyle->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) != 0;

    nsAutoTArray<nsRefPtr<FontEntry>, 10> weightList;
    weightList.AppendElements(10);
    for (PRUint32 j = 0; j < 2; j++) {
        PRBool matchesSomething = PR_FALSE;
        // build up an array of weights that match the italicness we're looking for
        for (PRUint32 i = 0; i < aFontFamily->mVariations.Length(); i++) {
            nsRefPtr<FontEntry> fe = aFontFamily->mVariations[i];
            const PRUint8 weight = (fe->mWeight / 100);
            if (fe->mItalic == italic) {
                weightList[weight] = fe;
                matchesSomething = PR_TRUE;
            }
        }
        if (matchesSomething)
            break;
        italic = !italic;
    }

    PRInt8 baseWeight, weightDistance;
    aFontStyle->ComputeWeightAndOffset(&baseWeight, &weightDistance);

    // 500 isn't quite bold so we want to treat it as 400 if we don't
    // have a 500 weight
    if (baseWeight == 5 && weightDistance == 0) {
        // If we have a 500 weight then use it
        if (weightList[5])
            return weightList[5];

        // Otherwise treat as 400
        baseWeight = 4;
    }


    PRInt8 matchBaseWeight = 0;
    PRInt8 direction = (baseWeight > 5) ? 1 : -1;
    for (PRInt8 i = baseWeight; ; i += direction) {
        if (weightList[i]) {
            matchBaseWeight = i;
            break;
        }

        // if we've reached one side without finding a font,
        // go the other direction until we find a match
        if (i == 1 || i == 9)
            direction = -direction;
    }

    nsRefPtr<FontEntry> matchFE;
    const PRInt8 absDistance = abs(weightDistance);
    direction = (weightDistance >= 0) ? 1 : -1;
    for (PRInt8 i = matchBaseWeight, k = 0; i < 10 && i > 0; i += direction) {
        if (weightList[i]) {
            matchFE = weightList[i];
            k++;
        }
        if (k > absDistance)
            break;
    }

    if (!matchFE) {
        /* if we still don't have a match, grab the closest thing in the other direction */
        direction = -direction;
        for (PRInt8 i = matchBaseWeight; i < 10 && i > 0; i += direction) {
            if (weightList[i]) {
                matchFE = weightList[i];
            }
        }
    }


    NS_ASSERTION(matchFE, "we should always be able to return something here");
    return matchFE;
}

cmsHPROFILE
gfxWindowsPlatform::GetPlatformCMSOutputProfile()
{
    WCHAR str[1024+1];
    DWORD size = 1024;

    HDC dc = GetDC(nsnull);
    GetICMProfileW(dc, &size, (LPWSTR)&str);
    ReleaseDC(nsnull, dc);

    cmsHPROFILE profile =
        cmsOpenProfileFromFile(NS_ConvertUTF16toUTF8(str).get(), "r");
#ifdef DEBUG_tor
    if (profile)
        fprintf(stderr,
                "ICM profile read from %s successfully\n",
                NS_ConvertUTF16toUTF8(str).get());
#endif
    return profile;
}

PRBool
gfxWindowsPlatform::GetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<FontEntry> > *array)
{
    return mPrefFonts.Get(aKey, array);
}

void
gfxWindowsPlatform::SetPrefFontEntries(const nsCString& aKey, nsTArray<nsRefPtr<FontEntry> >& array)
{
    mPrefFonts.Put(aKey, array);
}
