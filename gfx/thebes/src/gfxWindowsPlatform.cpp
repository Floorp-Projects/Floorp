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

#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "nsIWindowsRegKey.h"
#include "nsILocalFile.h"
#include "plbase64.h"

#ifdef MOZ_FT2_FONTS
#include "ft2build.h"
#include FT_FREETYPE_H
#include "gfxFT2Fonts.h"
#include "cairo-ft.h"
#else
#include "gfxWindowsFonts.h"
#endif

#ifdef WINCE
#include <shlwapi.h>
#endif

#include "gfxUserFontSet.h"

#include <string>

#ifdef MOZ_FT2_FONTS
static FT_Library gPlatformFTLibrary = NULL;
#endif

// font info loader constants
static const PRUint32 kDelayBeforeLoadingCmaps = 8 * 1000; // 8secs
static const PRUint32 kIntervalBetweenLoadingCmaps = 150; // 150ms
static const PRUint32 kNumFontsPerSlice = 10; // read in info 10 fonts at a time

static __inline void
BuildKeyNameFromFontName(nsAString &aName)
{
    if (aName.Length() >= LF_FACESIZE)
        aName.Truncate(LF_FACESIZE - 1);
    ToLowerCase(aName);
}

class gfxWindowsPlatformPrefObserver : public nsIObserver {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(gfxWindowsPlatformPrefObserver, nsIObserver)

NS_IMETHODIMP
gfxWindowsPlatformPrefObserver::Observe(nsISupports     *aSubject,
                                        const char      *aTopic,
                                        const PRUnichar *aData)
{
    NS_ASSERTION(!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID), "invalid topic");
    // XXX this could be made to only clear out the cache for the prefs that were changed
    // but it probably isn't that big a deal.
    gfxWindowsPlatform::GetPlatform()->ClearPrefFonts();
    return NS_OK;
}

gfxWindowsPlatform::gfxWindowsPlatform()
    : mStartIndex(0), mIncrement(kNumFontsPerSlice), mNumFamilies(0)
{
    mFonts.Init(200);
    mFontAliases.Init(20);
    mFontSubstitutes.Init(50);
    mPrefFonts.Init(10);

#ifdef MOZ_FT2_FONTS
    FT_Init_FreeType(&gPlatformFTLibrary);
#else
    FontEntry::InitializeFontEmbeddingProcs();
#endif

    UpdateFontList();

    gfxWindowsPlatformPrefObserver *observer = new gfxWindowsPlatformPrefObserver();
    if (observer) {
        nsCOMPtr<nsIPrefBranch2> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (pref) {
            pref->AddObserver("font.", observer, PR_FALSE);
            pref->AddObserver("font.name-list.", observer, PR_FALSE);
            pref->AddObserver("intl.accept_languages", observer, PR_FALSE);
            // don't bother unregistering.  We'll get shutdown after the pref service
        } else {
            delete observer;
        }
    }
}

gfxWindowsPlatform::~gfxWindowsPlatform()
{
    // not calling FT_Done_FreeType because cairo may still hold references to
    // these FT_Faces.  See bug 458169.
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenSurface(const gfxIntSize& size,
                                           gfxASurface::gfxImageFormat imageFormat)
{
#ifndef WINCE
    gfxASurface *surf = new gfxWindowsSurface(size, imageFormat);
#else
    gfxASurface *surf = new gfxImageSurface(size, imageFormat);
#endif
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
    const LOGFONTW& logFont = lpelfe->elfLogFont;

    // Ignore vertical fonts
    if (logFont.lfFaceName[0] == L'@')
        return 1;

    nsAutoString name(logFont.lfFaceName);
    BuildKeyNameFromFontName(name);

    nsRefPtr<FontFamily> ff;
    if (!ht->Get(name, &ff)) {
        ff = new FontFamily(nsDependentString(logFont.lfFaceName));
        ht->Put(name, ff);
    }

    return 1;
}


// general cmap reading routines moved to gfxFontUtils.cpp

struct FontListData {
    FontListData(const nsACString& aLangGroup, const nsACString& aGenericFamily, nsTArray<nsString>& aListOfFonts) :
        mLangGroup(aLangGroup), mGenericFamily(aGenericFamily), mStringArray(aListOfFonts) {}
    const nsACString& mLangGroup;
    const nsACString& mGenericFamily;
    nsTArray<nsString>& mStringArray;
};

PLDHashOperator
gfxWindowsPlatform::HashEnumFunc(nsStringHashKey::KeyType aKey,
                                 nsRefPtr<FontFamily>& aFontFamily,
                                 void* userArg)
{
    FontListData *data = (FontListData*)userArg;

    // use the first variation for now.  This data should be the same
    // for all the variations and should probably be moved up to
    // the Family
    gfxFontStyle style;
    style.langGroup = data->mLangGroup;
    nsRefPtr<FontEntry> aFontEntry = aFontFamily->FindFontEntry(style);
    NS_ASSERTION(aFontEntry, "couldn't find any font entry in family");
    if (!aFontEntry)
        return PL_DHASH_NEXT;


#ifndef MOZ_FT2_FONTS
    /* skip symbol fonts */
    if (aFontEntry->mSymbolFont)
        return PL_DHASH_NEXT;

    if (aFontEntry->SupportsLangGroup(data->mLangGroup) &&
        aFontEntry->MatchesGenericFamily(data->mGenericFamily))
#endif
    data->mStringArray.AppendElement(aFontFamily->Name());

    return PL_DHASH_NEXT;
}

nsresult
gfxWindowsPlatform::GetFontList(const nsACString& aLangGroup,
                                const nsACString& aGenericFamily,
                                nsTArray<nsString>& aListOfFonts)
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

#ifdef MOZ_FT2_FONTS
void gfxWindowsPlatform::AppendFacesFromFontFile(const PRUnichar *aFileName) {
    char fileName[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, aFileName, -1, fileName, MAX_PATH, NULL, NULL);
    FT_Face dummy;
    if (FT_Err_Ok == FT_New_Face(GetFTLibrary(), fileName, -1, &dummy)) {
        for (FT_Long i = 0; i < dummy->num_faces; i++) {
            FT_Face face;
            if (FT_Err_Ok != FT_New_Face(GetFTLibrary(), fileName, 
                                         i, &face))
                continue;

            FontEntry* fe = FontEntry::CreateFontEntryFromFace(face);
            if (fe) {
                NS_ConvertUTF8toUTF16 name(face->family_name);
                BuildKeyNameFromFontName(name);       
                nsRefPtr<FontFamily> ff;
                if (!mFonts.Get(name, &ff)) {
                    ff = new FontFamily(name);
                    mFonts.Put(name, ff);
                }
                ff->mFaces.AppendElement(fe);
            }
        }
        FT_Done_Face(dummy);
    }
}

void
gfxWindowsPlatform::FindFonts()
{
    nsTArray<nsString> searchPaths(2);
    nsTArray<nsString> fontPatterns(3);
    fontPatterns.AppendElement(NS_LITERAL_STRING("\\*.ttf"));
    fontPatterns.AppendElement(NS_LITERAL_STRING("\\*.ttc"));
    fontPatterns.AppendElement(NS_LITERAL_STRING("\\*.otf"));
    wchar_t pathBuf[256];
    SHGetSpecialFolderPathW(0, pathBuf, CSIDL_WINDOWS, 0);
    searchPaths.AppendElement(pathBuf);
    SHGetSpecialFolderPathW(0, pathBuf, CSIDL_FONTS, 0);
    searchPaths.AppendElement(pathBuf);
    WIN32_FIND_DATAW results;
    for (PRUint32 i = 0;  i < searchPaths.Length(); i++) {
        const nsString& path(searchPaths[i]);
        for (PRUint32 j = 0; j < fontPatterns.Length(); j++) { 
            nsAutoString pattern(path);
            pattern.Append(fontPatterns[j]);
            HANDLE handle = FindFirstFileExW(pattern.get(),
                                             FindExInfoStandard,
                                             &results,
                                             FindExSearchNameMatch,
                                             NULL,
                                             0);
            PRBool moreFiles = handle != INVALID_HANDLE_VALUE;
            while (moreFiles) {
                nsAutoString filePath(path);
                filePath.AppendLiteral("\\");
                filePath.Append(results.cFileName);
                AppendFacesFromFontFile(static_cast<const PRUnichar*>(filePath.get()));
                moreFiles = FindNextFile(handle, &results);
            }
            if (handle != INVALID_HANDLE_VALUE)
                FindClose(handle);
        }
    }
}

#endif


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
    CancelLoader();
#ifdef MOZ_FT2_FONTS
    FindFonts();
#else    
    LOGFONTW logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfFaceName[0] = 0;
    logFont.lfPitchAndFamily = 0;

    // Use the screen DC here.. should we use something else for printing?
    HDC dc = ::GetDC(nsnull);
    EnumFontFamiliesExW(dc, &logFont, (FONTENUMPROCW)gfxWindowsPlatform::FontEnumProc, (LPARAM)&mFonts, 0);
    ::ReleaseDC(nsnull, dc);
#endif
    // initialize the cmap loading process after font list has been initialized
    StartLoader(kDelayBeforeLoadingCmaps, kIntervalBetweenLoadingCmaps); 

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
            mNonExistingFonts.AppendElement(substituteName);
    }

    // initialize ranges of characters for which system-wide font search should be skipped
    mCodepointsWithNoFonts.SetRange(0,0x1f);     // C0 controls
    mCodepointsWithNoFonts.SetRange(0x7f,0x9f);  // C1 controls

    InitBadUnderlineList();

    return NS_OK;
}

struct FontFamilyListData {
    FontFamilyListData(nsTArray<nsRefPtr<FontFamily> >& aFamilyArray) 
        : mFamilyArray(aFamilyArray)
    {}

    static PLDHashOperator AppendFamily(nsStringHashKey::KeyType aKey,
                                        nsRefPtr<FontFamily>& aFamilyEntry,
                                        void *aUserArg)
    {
        FontFamilyListData *data = (FontFamilyListData*)aUserArg;
        data->mFamilyArray.AppendElement(aFamilyEntry);
        return PL_DHASH_NEXT;
    }

    nsTArray<nsRefPtr<FontFamily> >& mFamilyArray;
};

void
gfxWindowsPlatform::GetFontFamilyList(nsTArray<nsRefPtr<FontFamily> >& aFamilyArray)
{
    FontFamilyListData data(aFamilyArray);
    mFonts.Enumerate(FontFamilyListData::AppendFamily, &data);
}

static PRBool SimpleResolverCallback(const nsAString& aName, void* aClosure)
{
    nsString *result = static_cast<nsString*>(aClosure);
    result->Assign(aName);
    return PR_FALSE;
}

void
gfxWindowsPlatform::InitBadUnderlineList()
{
// Only windows fonts have mIsBadUnderlineFontFamily flag
#ifndef MOZ_FT2_FONTS
    nsAutoTArray<nsString, 10> blacklist;
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
        ff->mIsBadUnderlineFontFamily = 1;
    }
#endif
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
        aAborted = !(*aCallback)(ff->Name(), aClosure);
        // XXX If the font has font link, we should add the linked font.
        return NS_OK;
    }

    if (mNonExistingFonts.Contains(keyName)) {
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
        mNonExistingFonts.AppendElement(keyName);
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
    FontSearch(PRUint32 aCh, gfxFont *aFont) :
        ch(aCh), fontToMatch(aFont), matchRank(-1) {
    }
    PRUint32 ch;
    nsRefPtr<gfxFont> fontToMatch;
    PRInt32 matchRank;
    nsRefPtr<FontEntry> bestMatch;
};

PLDHashOperator
gfxWindowsPlatform::FindFontForCharProc(nsStringHashKey::KeyType aKey,
                                        nsRefPtr<FontFamily>& aFontFamily,
                                        void* userArg)
{
    FontSearch *data = (FontSearch*)userArg;

    const PRUint32 ch = data->ch;

    nsRefPtr<FontEntry> fe = aFontFamily->FindFontEntry(*data->fontToMatch->GetStyle());
    NS_ASSERTION(fe, "couldn't find any font entry in family");
    if (!fe)
        return PL_DHASH_NEXT;

    PRInt32 rank = 0;

#ifndef MOZ_FT2_FONTS
    // skip over non-unicode and bitmap fonts and fonts that don't have
    // the code point we're looking for
    if (fe->IsCrappyFont() || !fe->mCharacterMap.test(ch))
        return PL_DHASH_NEXT;

    // fonts that claim to support the range are more
    // likely to be "better fonts" than ones that don't... (in theory)
    if (fe->SupportsRange(gfxFontUtils::CharRangeBit(ch)))
        rank += 1;

    if (fe->SupportsLangGroup(data->fontToMatch->GetStyle()->langGroup))
        rank += 2;

    FontEntry* mfe = static_cast<FontEntry*>(data->fontToMatch->GetFontEntry());

    if (fe->mWindowsFamily == mfe->mWindowsFamily)
        rank += 3;
    if (fe->mWindowsPitch == mfe->mWindowsPitch)
        rank += 3;
#endif
    /* italic */
    const PRBool italic = (data->fontToMatch->GetStyle()->style != FONT_STYLE_NORMAL);
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
        (rank == data->matchRank && Compare(fe->Name(), data->bestMatch->Name()) > 0)) {
        data->bestMatch = fe;
        data->matchRank = rank;
    }

    return PL_DHASH_NEXT;
}

already_AddRefed<gfxFont>
gfxWindowsPlatform::FindFontForChar(PRUint32 aCh, gfxFont *aFont)
{
    // is codepoint with no matching font? return null immediately
    if (mCodepointsWithNoFonts.test(aCh)) {
        return nsnull;
    }

    FontSearch data(aCh, aFont);

    // find fonts that support the character
    mFonts.Enumerate(gfxWindowsPlatform::FindFontForCharProc, &data);

    if (data.bestMatch) {
#ifdef MOZ_FT2_FONTS
        nsRefPtr<gfxFT2Font> font =
            gfxFT2Font::GetOrMakeFont(data.bestMatch->mName, 
                                      aFont->GetStyle()); 
            gfxFont* ret = font.forget().get();
            return already_AddRefed<gfxFont>(ret);
#else
        nsRefPtr<gfxWindowsFont> font =
            gfxWindowsFont::GetOrMakeFont(data.bestMatch, aFont->GetStyle());
        if (font->IsValid()) {
            gfxFont* ret = font.forget().get();
            return already_AddRefed<gfxFont>(ret);
        }
#endif
        return nsnull;
    }
    // no match? add to set of non-matching codepoints
    mCodepointsWithNoFonts.set(aCh);
    return nsnull;
}

gfxFontGroup *
gfxWindowsPlatform::CreateFontGroup(const nsAString &aFamilies,
                                    const gfxFontStyle *aStyle,
                                    gfxUserFontSet *aUserFontSet)
{
#ifdef MOZ_FT2_FONTS
    return new gfxFT2FontGroup(aFamilies, aStyle);
#else
    return new gfxWindowsFontGroup(aFamilies, aStyle, aUserFontSet);
#endif
}


struct FullFontNameSearch {
    FullFontNameSearch(const nsAString& aFullName)
        : mFound(PR_FALSE), mFullName(aFullName), mDC(nsnull), mFontEntry(nsnull)
    { }

    PRPackedBool mFound;
    nsString     mFullName;
    nsString     mFamilyName;
    HDC          mDC;
    gfxFontEntry *mFontEntry;
};

#ifndef MOZ_FT2_FONTS
// callback called for each face within a single family
// match against elfFullName

static int CALLBACK 
FindFullNameForFace(const ENUMLOGFONTEXW *lpelfe,
                    const NEWTEXTMETRICEXW *nmetrics,
                    DWORD fontType, LPARAM userArg)
{
    FullFontNameSearch *data = reinterpret_cast<FullFontNameSearch*>(userArg);

    // does the full name match?
    if (!data->mFullName.Equals(nsDependentString(lpelfe->elfFullName)))
        return 1;  // continue

    // found match, create a new font entry
    data->mFound = PR_TRUE;

    const NEWTEXTMETRICW& metrics = nmetrics->ntmTm;
    LOGFONTW logFont = lpelfe->elfLogFont;

    // Some fonts claim to support things > 900, but we don't so clamp the sizes
    logFont.lfWeight = PR_MAX(PR_MIN(logFont.lfWeight, 900), 100);

    gfxWindowsFontType feType = FontEntry::DetermineFontType(metrics, fontType);

    data->mFontEntry = FontEntry::CreateFontEntry(data->mFamilyName, feType, (logFont.lfItalic == 0xFF), (PRUint16) (logFont.lfWeight), nsnull, data->mDC, &logFont);

    return 0;  // stop iteration
}
#endif

// callback called for each family name, based on the assumption that the 
// first part of the full name is the family name

static PLDHashOperator
FindFullName(nsStringHashKey::KeyType aKey,
             nsRefPtr<FontFamily>& aFontFamily,
             void* userArg)
{
    FullFontNameSearch *data = reinterpret_cast<FullFontNameSearch*>(userArg);

    // does the family name match up to the length of the family name?
    const nsString& family = aFontFamily->Name();
    
    nsString fullNameFamily;
    data->mFullName.Left(fullNameFamily, family.Length());

    // if so, iterate over faces in this family to see if there is a match
    if (family.Equals(fullNameFamily)) {
#ifdef MOZ_FT2_FONTS
        int len = aFontFamily->mFaces.Length();
        int index = 0;
        for (; index < len && 
                 !aFontFamily->mFaces[index]->Name().Equals(data->mFullName); index++);
        if (index < len) {
            data->mFound = PR_TRUE;
            data->mFontEntry = aFontFamily->mFaces[index];
        }
#else
        HDC hdc;
        
        if (!data->mDC) {
            data->mDC= GetDC(nsnull);
            SetGraphicsMode(data->mDC, GM_ADVANCED);
        }
        hdc = data->mDC;

        LOGFONTW logFont;
        memset(&logFont, 0, sizeof(LOGFONTW));
        logFont.lfCharSet = DEFAULT_CHARSET;
        logFont.lfPitchAndFamily = 0;
        PRUint32 l = PR_MIN(family.Length(), LF_FACESIZE - 1);
        memcpy(logFont.lfFaceName,
               nsPromiseFlatString(family).get(),
               l * sizeof(PRUnichar));
        logFont.lfFaceName[l] = 0;
        data->mFamilyName.Assign(family);

        EnumFontFamiliesExW(hdc, &logFont, (FONTENUMPROCW)FindFullNameForFace, (LPARAM)data, 0);
#endif
    }

    if (data->mFound)
        return PL_DHASH_STOP;

    return PL_DHASH_NEXT;
}

gfxFontEntry* 
gfxWindowsPlatform::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                    const nsAString& aFontName)
{
#ifdef MOZ_FT2_FONTS
    // walk over list of names
    FullFontNameSearch data(aFontName);

    // find fonts that support the character
    mFonts.Enumerate(FindFullName, &data);

    if (data.mDC)
        ReleaseDC(nsnull, data.mDC);
    
    return data.mFontEntry;
#else
    return FontEntry::LoadLocalFont(*aProxyEntry, aFontName);
#endif
}

gfxFontEntry* 
gfxWindowsPlatform::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                     nsISupports *aLoader,
                                     const PRUint8 *aFontData, PRUint32 aLength)
{
#ifdef MOZ_FT2_FONTS
    return FontEntry::CreateFontEntry(*aProxyEntry, aLoader, aFontData, aLength);
#else
    return FontEntry::LoadFont(*aProxyEntry, aLoader, aFontData, aLength);
#endif    
}

PRBool
gfxWindowsPlatform::IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
    if (aFormatFlags & (gfxUserFontSet::FLAG_FORMAT_OPENTYPE | 
                        gfxUserFontSet::FLAG_FORMAT_TRUETYPE)) {
        return PR_TRUE;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return PR_FALSE;
    }

    // no format hint set, need to look at data
    return PR_TRUE;
}

FontFamily *
gfxWindowsPlatform::FindFontFamily(const nsAString& aName)
{
    nsAutoString name(aName);
    BuildKeyNameFromFontName(name);

    nsRefPtr<FontFamily> ff;
    if (!mFonts.Get(name, &ff) &&
        !mFontSubstitutes.Get(name, &ff) &&
        !mFontAliases.Get(name, &ff)) {
        return nsnull;
    }
    return ff.get();
}

FontEntry *
gfxWindowsPlatform::FindFontEntry(const nsAString& aName, const gfxFontStyle& aFontStyle)
{
    nsRefPtr<FontFamily> ff = FindFontFamily(aName);
    if (!ff)
        return nsnull;

    return ff->FindFontEntry(aFontStyle);
}

qcms_profile*
gfxWindowsPlatform::GetPlatformCMSOutputProfile()
{
#ifndef MOZ_FT2_FONTS
    WCHAR str[1024+1];
    DWORD size = 1024;

    HDC dc = GetDC(nsnull);
    GetICMProfileW(dc, &size, (LPWSTR)&str);
    ReleaseDC(nsnull, dc);

    qcms_profile* profile =
        qcms_profile_from_path(NS_ConvertUTF16toUTF8(str).get());
#ifdef DEBUG_tor
    if (profile)
        fprintf(stderr,
                "ICM profile read from %s successfully\n",
                NS_ConvertUTF16toUTF8(str).get());
#endif
    return profile;
#else
    return nsnull;
#endif
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

void 
gfxWindowsPlatform::InitLoader()
{
    GetFontFamilyList(mFontFamilies);
    mStartIndex = 0;
    mNumFamilies = mFontFamilies.Length();
}

PRBool 
gfxWindowsPlatform::RunLoader()
{
    PRUint32 i, endIndex = ( mStartIndex + mIncrement < mNumFamilies ? mStartIndex + mIncrement : mNumFamilies );

#ifndef MOZ_FT2_FONTS
    // for each font family, load in various font info
    for (i = mStartIndex; i < endIndex; i++) {
        // load the cmaps for all variations
        mFontFamilies[i]->FindStyleVariations();
    }
#endif
    mStartIndex += mIncrement;
    if (mStartIndex < mNumFamilies)
        return PR_FALSE;
    return PR_TRUE;
}

void 
gfxWindowsPlatform::FinishLoader()
{
    mFontFamilies.Clear();
    mNumFamilies = 0;
}

#ifdef MOZ_FT2_FONTS
FT_Library
gfxWindowsPlatform::GetFTLibrary()
{
    return gPlatformFTLibrary;
}
#endif
