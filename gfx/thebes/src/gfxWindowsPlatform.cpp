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
#include "nsILocalFile.h"
#include "plbase64.h"

#include "gfxWindowsFonts.h"
#include "gfxUserFontSet.h"

#include <string>

#include "lcms.h"

static void InitializeFontEmbeddingProcs();

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

int
gfxWindowsPlatform::PrefChangedCallback(const char *aPrefName, void *closure)
{
    // XXX this could be made to only clear out the cache for the prefs that were changed
    // but it probably isn't that big a deal.
    gfxWindowsPlatform *plat = static_cast<gfxWindowsPlatform *>(closure);
    plat->mPrefFonts.Clear();
    return 0;
}

gfxWindowsPlatform::gfxWindowsPlatform()
    : mStartIndex(0), mIncrement(kNumFontsPerSlice), mNumFamilies(0)
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

    InitializeFontEmbeddingProcs();
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
    FontListData(const nsACString& aLangGroup, const nsACString& aGenericFamily, nsStringArray& aListOfFonts) :
        mLangGroup(aLangGroup), mGenericFamily(aGenericFamily), mStringArray(aListOfFonts) {}
    const nsACString& mLangGroup;
    const nsACString& mGenericFamily;
    nsStringArray& mStringArray;
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
    
    LOGFONTW logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfFaceName[0] = 0;
    logFont.lfPitchAndFamily = 0;

    // Use the screen DC here.. should we use something else for printing?
    HDC dc = ::GetDC(nsnull);
    EnumFontFamiliesExW(dc, &logFont, (FONTENUMPROCW)gfxWindowsPlatform::FontEnumProc, (LPARAM)&mFonts, 0);
    ::ReleaseDC(nsnull, dc);

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
            mNonExistingFonts.AppendString(substituteName);
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
        ch(aCh), fontToMatch(aFont), matchRank(-1) {
    }
    PRUint32 ch;
    nsRefPtr<gfxWindowsFont> fontToMatch;
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

already_AddRefed<gfxWindowsFont>
gfxWindowsPlatform::FindFontForChar(PRUint32 aCh, gfxWindowsFont *aFont)
{
    // is codepoint with no matching font? return null immediately
    if (mCodepointsWithNoFonts.test(aCh)) {
        return nsnull;
    }

    FontSearch data(aCh, aFont);

    // find fonts that support the character
    mFonts.Enumerate(gfxWindowsPlatform::FindFontForCharProc, &data);

    if (data.bestMatch) {
        nsRefPtr<gfxWindowsFont> font =
            gfxWindowsFont::GetOrMakeFont(data.bestMatch, aFont->GetStyle());
        if (font->IsValid())
            return font.forget();
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
    return new gfxWindowsFontGroup(aFamilies, aStyle, aUserFontSet);
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
    }

    if (data->mFound)
        return PL_DHASH_STOP;

    return PL_DHASH_NEXT;
}

gfxFontEntry* 
gfxWindowsPlatform::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                    const nsAString& aFontName)
{
    // walk over list of names
    FullFontNameSearch data(aFontName);

    // find fonts that support the character
    mFonts.Enumerate(FindFullName, &data);

    if (data.mDC)
        ReleaseDC(nsnull, data.mDC);
    
    return data.mFontEntry;
}

static void MakeUniqueFontName(nsAString& aName)
{
    char buf[50];

    static PRUint32 fontCount = 0;
    ++fontCount;

    sprintf(buf, "mozfont%8.8x%8.8x", ::GetTickCount(), fontCount);  // slightly retarded, figure something better later...
    aName.AssignASCII(buf);
}

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

static void InitializeFontEmbeddingProcs()
{
    HMODULE fontlib = LoadLibraryW(L"t2embed.dll");
    if (!fontlib)
        return;
    TTLoadEmbeddedFontPtr = (TTLoadEmbeddedFontProc) GetProcAddress(fontlib, "TTLoadEmbeddedFont");
    TTDeleteEmbeddedFontPtr = (TTDeleteEmbeddedFontProc) GetProcAddress(fontlib, "TTDeleteEmbeddedFont");
}

class WinUserFontData : public gfxUserFontData {
public:
    WinUserFontData(HANDLE aFontRef, PRBool aIsCFF)
        : mFontRef(aFontRef), mIsCFF(aIsCFF)
    { }

    virtual ~WinUserFontData()
    {
        if (mIsCFF) {
            RemoveFontMemResourceEx(mFontRef);
        } else {
            ULONG pulStatus;
            TTDeleteEmbeddedFontPtr(mFontRef, 0, &pulStatus);
        }
    }
    
    HANDLE mFontRef;
    PRPackedBool mIsCFF;
};

// used to control stream read by Windows TTLoadEmbeddedFont API

class EOTFontStreamReader {
public:
    EOTFontStreamReader(const PRUint8 *aFontData, PRUint32 aLength, PRUint8 *aEOTHeader, 
                        PRUint32 aEOTHeaderLen)
        : mInHeader(PR_TRUE), mHeaderOffset(0), mEOTHeader(aEOTHeader), 
          mEOTHeaderLen(aEOTHeaderLen), mFontData(aFontData), mFontDataLen(aLength),
          mFontDataOffset(0)
    {
    
    }

    ~EOTFontStreamReader() 
    { 

    }

    PRPackedBool            mInHeader;
    PRUint32                mHeaderOffset;
    PRUint8                 *mEOTHeader;
    PRUint32                mEOTHeaderLen;
    const PRUint8           *mFontData;
    PRUint32                mFontDataLen;
    PRUint32                mFontDataOffset;

    unsigned long Read(void *outBuffer, const unsigned long aBytesToRead)
    {
        PRUint32 bytesLeft = aBytesToRead;
        PRUint8 *out = static_cast<PRUint8*> (outBuffer);

        // read from EOT header
        if (mInHeader) {
            PRUint32 toCopy = PR_MIN(aBytesToRead, mEOTHeaderLen - mHeaderOffset);
            memcpy(out, mEOTHeader + mHeaderOffset, toCopy);
            bytesLeft -= toCopy;
            mHeaderOffset += toCopy;
            out += toCopy;
            if (mHeaderOffset == mEOTHeaderLen)
                mInHeader = PR_FALSE;
        }

        if (bytesLeft) {
            PRInt32 bytesRead = PR_MIN(bytesLeft, mFontDataLen - mFontDataOffset);
            memcpy(out, mFontData, bytesRead);
            mFontData += bytesRead;
            mFontDataOffset += bytesRead;
            if (bytesRead > 0)
                bytesLeft -= bytesRead;
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
gfxWindowsPlatform::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                     nsISupports *aLoader,
                                     const PRUint8 *aFontData, PRUint32 aLength)
{
    // if calls aren't available, bail
    if (!TTLoadEmbeddedFontPtr || !TTDeleteEmbeddedFontPtr)
        return nsnull;

    PRBool isCFF;
    if (!gfxFontUtils::ValidateSFNTHeaders(aFontData, aLength, &isCFF))
        return nsnull;
        
    nsresult rv;
    HANDLE fontRef;

    nsAutoString uniqueName;
    MakeUniqueFontName(uniqueName);

    if (isCFF) {
        // Postscript-style glyphs, swizzle name table, load directly
        nsTArray<PRUint8> newFontData;

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
        if (fontRef && numFonts != 1) {
            RemoveFontMemResourceEx(fontRef);
            return nsnull;
        }
    } else {
        // TrueType-style glyphs, use EOT library
        nsAutoTArray<PRUint8,2048> eotHeader;
        PRUint8 *buffer;
        PRUint32 eotlen;

        PRUint32 nameLen = PR_MIN(uniqueName.Length(), LF_FACESIZE - 1);
        nsPromiseFlatString fontName(Substring(uniqueName, 0, nameLen));

        rv = gfxFontUtils::MakeEOTHeader(aFontData, aLength, &eotHeader);
        if (NS_FAILED(rv))
            return nsnull;

        // load in embedded font data
        eotlen = eotHeader.Length();
        buffer = reinterpret_cast<PRUint8*> (eotHeader.Elements());
        
        PRInt32 ret;
        ULONG privStatus, pulStatus;
        EOTFontStreamReader eotReader(aFontData, aLength, buffer, eotlen);

        ret = TTLoadEmbeddedFontPtr(&fontRef, TTLOAD_PRIVATE, &privStatus, 
                                   LICENSE_PREVIEWPRINT, &pulStatus, 
                                   EOTFontStreamReader::ReadEOTStream, 
                                   &eotReader, (PRUnichar*)(fontName.get()), 0, 0);
        if (ret != E_NONE)
            return nsnull;
    }

    
    // make a new font entry using the unique name
    WinUserFontData *winUserFontData = new WinUserFontData(fontRef, isCFF);
    PRUint16 w = (aProxyEntry->mWeight == 0 ? 400 : aProxyEntry->mWeight);

    FontEntry *fe = FontEntry::CreateFontEntry(uniqueName, 
        gfxWindowsFontType(isCFF ? GFX_FONT_TYPE_PS_OPENTYPE : GFX_FONT_TYPE_TRUETYPE) /*type*/, 
        PRUint32(aProxyEntry->mItalic ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL), 
        w, winUserFontData);

    if (fe && isCFF)
        fe->mForceGDI = PR_TRUE;

    return fe;
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

cmsHPROFILE
gfxWindowsPlatform::GetPlatformCMSOutputProfile()
{
#ifndef WINCE
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

    // for each font family, load in various font info
    for (i = mStartIndex; i < endIndex; i++) {
        // load the cmaps for all variations
        mFontFamilies[i]->FindStyleVariations();
    }

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

