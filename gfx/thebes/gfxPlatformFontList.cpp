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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
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
 * Based in part on sample code provided by Apple Computer, Inc.,
 * under the following license:
 *
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "prlog.h"

#include "gfxPlatformFontList.h"

#include "nsUnicharUtils.h"
#include "nsUnicodeRange.h"
#include "nsUnicodeProperties.h"

#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"

using namespace mozilla;

// font info loader constants
static const PRUint32 kDelayBeforeLoadingCmaps = 8 * 1000; // 8secs
static const PRUint32 kIntervalBetweenLoadingCmaps = 150; // 150ms
static const PRUint32 kNumFontsPerSlice = 10; // read in info 10 fonts at a time

#ifdef PR_LOGGING

#define LOG_FONTLIST(args) PR_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), \
                               PR_LOG_DEBUG, args)
#define LOG_FONTLIST_ENABLED() PR_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontlist), \
                                   PR_LOG_DEBUG)

#endif // PR_LOGGING

gfxPlatformFontList *gfxPlatformFontList::sPlatformFontList = nsnull;


static const char* kObservedPrefs[] = {
    "font.",
    "font.name-list.",
    "intl.accept_languages",  // hmmmm...
    nsnull
};

class gfxFontListPrefObserver : public nsIObserver {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

static gfxFontListPrefObserver* gFontListPrefObserver = nsnull;

NS_IMPL_ISUPPORTS1(gfxFontListPrefObserver, nsIObserver)

NS_IMETHODIMP
gfxFontListPrefObserver::Observe(nsISupports     *aSubject,
                                 const char      *aTopic,
                                 const PRUnichar *aData)
{
    NS_ASSERTION(!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID), "invalid topic");
    // XXX this could be made to only clear out the cache for the prefs that were changed
    // but it probably isn't that big a deal.
    gfxPlatformFontList::PlatformFontList()->ClearPrefFonts();
    gfxFontCache::GetCache()->AgeAllGenerations();
    return NS_OK;
}


gfxPlatformFontList::gfxPlatformFontList(bool aNeedFullnamePostscriptNames)
    : mNeedFullnamePostscriptNames(aNeedFullnamePostscriptNames),
      mStartIndex(0), mIncrement(kNumFontsPerSlice), mNumFamilies(0)
{
    mFontFamilies.Init(100);
    mOtherFamilyNames.Init(30);
    mOtherFamilyNamesInitialized = false;

    if (mNeedFullnamePostscriptNames) {
        mFullnames.Init(100);
        mPostscriptNames.Init(100);
    }
    mFaceNamesInitialized = false;

    mPrefFonts.Init(10);

    mBadUnderlineFamilyNames.Init(10);
    LoadBadUnderlineList();

    // pref changes notification setup
    NS_ASSERTION(!gFontListPrefObserver,
                 "There has been font list pref observer already");
    gFontListPrefObserver = new gfxFontListPrefObserver();
    NS_ADDREF(gFontListPrefObserver);
    Preferences::AddStrongObservers(gFontListPrefObserver, kObservedPrefs);
}

gfxPlatformFontList::~gfxPlatformFontList()
{
    NS_ASSERTION(gFontListPrefObserver, "There is no font list pref observer");
    Preferences::RemoveObservers(gFontListPrefObserver, kObservedPrefs);
    NS_RELEASE(gFontListPrefObserver);
}

nsresult
gfxPlatformFontList::InitFontList()
{
    mFontFamilies.Clear();
    mOtherFamilyNames.Clear();
    mOtherFamilyNamesInitialized = false;
    if (mNeedFullnamePostscriptNames) {
        mFullnames.Clear();
        mPostscriptNames.Clear();
    }
    mFaceNamesInitialized = false;
    mPrefFonts.Clear();
    CancelLoader();

    // initialize ranges of characters for which system-wide font search should be skipped
    mCodepointsWithNoFonts.reset();
    mCodepointsWithNoFonts.SetRange(0,0x1f);     // C0 controls
    mCodepointsWithNoFonts.SetRange(0x7f,0x9f);  // C1 controls

    sPlatformFontList = this;

    return NS_OK;
}

void
gfxPlatformFontList::GenerateFontListKey(const nsAString& aKeyName, nsAString& aResult)
{
    aResult = aKeyName;
    ToLowerCase(aResult);
}

void 
gfxPlatformFontList::InitOtherFamilyNames()
{
    mOtherFamilyNamesInitialized = true;

    Telemetry::AutoTimer<Telemetry::FONTLIST_INITOTHERFAMILYNAMES> timer;
    // iterate over all font families and read in other family names
    mFontFamilies.Enumerate(gfxPlatformFontList::InitOtherFamilyNamesProc, this);
}
                                                         
PLDHashOperator PR_CALLBACK
gfxPlatformFontList::InitOtherFamilyNamesProc(nsStringHashKey::KeyType aKey,
                                              nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                              void* userArg)
{
    gfxPlatformFontList *fc = static_cast<gfxPlatformFontList*>(userArg);
    aFamilyEntry->ReadOtherFamilyNames(fc);
    return PL_DHASH_NEXT;
}

void
gfxPlatformFontList::InitFaceNameLists()
{
    mFaceNamesInitialized = true;

    // iterate over all font families and read in other family names
    Telemetry::AutoTimer<Telemetry::FONTLIST_INITFACENAMELISTS> timer;
    mFontFamilies.Enumerate(gfxPlatformFontList::InitFaceNameListsProc, this);
}

PLDHashOperator PR_CALLBACK
gfxPlatformFontList::InitFaceNameListsProc(nsStringHashKey::KeyType aKey,
                                           nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                           void* userArg)
{
    gfxPlatformFontList *fc = static_cast<gfxPlatformFontList*>(userArg);
    aFamilyEntry->ReadFaceNames(fc, fc->NeedFullnamePostscriptNames());
    return PL_DHASH_NEXT;
}

void
gfxPlatformFontList::PreloadNamesList()
{
    nsAutoTArray<nsString, 10> preloadFonts;
    gfxFontUtils::GetPrefsFontList("font.preload-names-list", preloadFonts);

    PRUint32 numFonts = preloadFonts.Length();
    for (PRUint32 i = 0; i < numFonts; i++) {
        bool found;
        nsAutoString key;
        GenerateFontListKey(preloadFonts[i], key);
        
        // only search canonical names!
        gfxFontFamily *familyEntry = mFontFamilies.GetWeak(key, &found);
        if (familyEntry) {
            familyEntry->ReadOtherFamilyNames(this);
        }
    }

}

void 
gfxPlatformFontList::SetFixedPitch(const nsAString& aFamilyName)
{
    gfxFontFamily *family = FindFamily(aFamilyName);
    if (!family) return;

    family->FindStyleVariations();
    nsTArray<nsRefPtr<gfxFontEntry> >& fontlist = family->GetFontList();

    PRUint32 i, numFonts = fontlist.Length();

    for (i = 0; i < numFonts; i++) {
        fontlist[i]->mFixedPitch = 1;
    }
}

void
gfxPlatformFontList::LoadBadUnderlineList()
{
    nsAutoTArray<nsString, 10> blacklist;
    gfxFontUtils::GetPrefsFontList("font.blacklist.underline_offset", blacklist);
    PRUint32 numFonts = blacklist.Length();
    for (PRUint32 i = 0; i < numFonts; i++) {
        nsAutoString key;
        GenerateFontListKey(blacklist[i], key);
        mBadUnderlineFamilyNames.PutEntry(key);
    }
}

bool 
gfxPlatformFontList::ResolveFontName(const nsAString& aFontName, nsAString& aResolvedFontName)
{
    gfxFontFamily *family = FindFamily(aFontName);
    if (family) {
        aResolvedFontName = family->Name();
        return true;
    }
    return false;
}

struct FontListData {
    FontListData(nsIAtom *aLangGroup,
                 const nsACString& aGenericFamily,
                 nsTArray<nsString>& aListOfFonts) :
        mLangGroup(aLangGroup), mGenericFamily(aGenericFamily),
        mListOfFonts(aListOfFonts) {}
    nsIAtom *mLangGroup;
    const nsACString& mGenericFamily;
    nsTArray<nsString>& mListOfFonts;
};

PLDHashOperator PR_CALLBACK
gfxPlatformFontList::HashEnumFuncForFamilies(nsStringHashKey::KeyType aKey,
                                             nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                             void *aUserArg)
{
    FontListData *data = static_cast<FontListData*>(aUserArg);

    // use the first variation for now.  This data should be the same
    // for all the variations and should probably be moved up to
    // the Family
    gfxFontStyle style;
    style.language = data->mLangGroup;
    bool needsBold;
    nsRefPtr<gfxFontEntry> aFontEntry = aFamilyEntry->FindFontForStyle(style, needsBold);
    NS_ASSERTION(aFontEntry, "couldn't find any font entry in family");
    if (!aFontEntry)
        return PL_DHASH_NEXT;

    /* skip symbol fonts */
    if (aFontEntry->IsSymbolFont())
        return PL_DHASH_NEXT;

    if (aFontEntry->SupportsLangGroup(data->mLangGroup) &&
        aFontEntry->MatchesGenericFamily(data->mGenericFamily)) {
        nsAutoString localizedFamilyName;
        aFamilyEntry->LocalizedName(localizedFamilyName);
        data->mListOfFonts.AppendElement(localizedFamilyName);
    }

    return PL_DHASH_NEXT;
}

void
gfxPlatformFontList::GetFontList(nsIAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts)
{
    FontListData data(aLangGroup, aGenericFamily, aListOfFonts);

    mFontFamilies.Enumerate(gfxPlatformFontList::HashEnumFuncForFamilies, &data);

    aListOfFonts.Sort();
    aListOfFonts.Compact();
}

struct FontFamilyListData {
    FontFamilyListData(nsTArray<nsRefPtr<gfxFontFamily> >& aFamilyArray) 
        : mFamilyArray(aFamilyArray)
    {}

    static PLDHashOperator PR_CALLBACK AppendFamily(nsStringHashKey::KeyType aKey,
                                                    nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                                    void *aUserArg)
    {
        FontFamilyListData *data = static_cast<FontFamilyListData*>(aUserArg);
        data->mFamilyArray.AppendElement(aFamilyEntry);
        return PL_DHASH_NEXT;
    }

    nsTArray<nsRefPtr<gfxFontFamily> >& mFamilyArray;
};

void
gfxPlatformFontList::GetFontFamilyList(nsTArray<nsRefPtr<gfxFontFamily> >& aFamilyArray)
{
    FontFamilyListData data(aFamilyArray);
    mFontFamilies.Enumerate(FontFamilyListData::AppendFamily, &data);
}

gfxFontEntry*
gfxPlatformFontList::SystemFindFontForChar(const PRUint32 aCh,
                                           PRInt32 aRunScript,
                                           const gfxFontStyle* aStyle)
 {
    gfxFontEntry* fontEntry = nsnull;

    // is codepoint with no matching font? return null immediately
    if (mCodepointsWithNoFonts.test(aCh)) {
        return nsnull;
    }

    // try to short-circuit font fallback for U+FFFD, used to represent
    // encoding errors: just use a platform-specific fallback system
    // font that is guaranteed (or at least highly likely) to be around,
    // or a cached family from last time U+FFFD was seen. this helps
    // speed up pages with lots of encoding errors, binary-as-text, etc.
    if (aCh == 0xFFFD && mReplacementCharFallbackFamily.Length() > 0) {
        bool needsBold;  // ignored in the system fallback case

        fontEntry = FindFontForFamily(mReplacementCharFallbackFamily,
                                      aStyle, needsBold);

        if (fontEntry && fontEntry->TestCharacterMap(aCh))
            return fontEntry;
    }

    TimeStamp start = TimeStamp::Now();

    // search commonly available fonts
    bool common = true;
    fontEntry = CommonFontFallback(aCh, aRunScript, aStyle);
 
    // if didn't find a font, do system-wide fallback (except for specials)
    PRUint32 cmapCount = 0;
    if (!fontEntry) {
        common = false;
        fontEntry = GlobalFontFallback(aCh, aRunScript, aStyle, cmapCount);
    }
    TimeDuration elapsed = TimeStamp::Now() - start;

#ifdef PR_LOGGING
    PRLogModuleInfo *log = gfxPlatform::GetLog(eGfxLog_textrun);

    if (NS_UNLIKELY(log)) {
        PRUint32 charRange = gfxFontUtils::CharRangeBit(aCh);
        PRUint32 unicodeRange = FindCharUnicodeRange(aCh);
        PRInt32 script = mozilla::unicode::GetScriptCode(aCh);
        PR_LOG(log, PR_LOG_WARNING,\
               ("(textrun-systemfallback-%s) char: u+%6.6x "
                 "char-range: %d unicode-range: %d script: %d match: [%s]"
                " time: %dus cmaps: %d\n",
                (common ? "common" : "global"), aCh,
                 charRange, unicodeRange, script,
                (fontEntry ? NS_ConvertUTF16toUTF8(fontEntry->Name()).get() :
                    "<none>"),
                PRInt32(elapsed.ToMicroseconds()),
                cmapCount));
    }
#endif

    // no match? add to set of non-matching codepoints
    if (!fontEntry) {
         mCodepointsWithNoFonts.set(aCh);
    } else if (aCh == 0xFFFD && fontEntry) {
        mReplacementCharFallbackFamily = fontEntry->FamilyName();
     }
 
    // track system fallback time
    static bool first = true;
    PRInt32 intElapsed = PRInt32(first ? elapsed.ToMilliseconds() :
                                         elapsed.ToMicroseconds());
    Telemetry::Accumulate((first ? Telemetry::SYSTEM_FONT_FALLBACK_FIRST :
                                   Telemetry::SYSTEM_FONT_FALLBACK),
                          intElapsed);
    first = false;

    return fontEntry;
}

PLDHashOperator PR_CALLBACK 
gfxPlatformFontList::FindFontForCharProc(nsStringHashKey::KeyType aKey, nsRefPtr<gfxFontFamily>& aFamilyEntry,
     void *userArg)
{
    GlobalFontMatch *data = static_cast<GlobalFontMatch*>(userArg);
 
     // evaluate all fonts in this family for a match
     aFamilyEntry->FindFontForChar(data);

     return PL_DHASH_NEXT;
}

#define NUM_FALLBACK_FONTS        8

gfxFontEntry*
gfxPlatformFontList::CommonFontFallback(const PRUint32 aCh,
                                        PRInt32 aRunScript,
                                        const gfxFontStyle* aMatchStyle)
{
    nsAutoTArray<const char*,NUM_FALLBACK_FONTS> defaultFallbacks;
    PRUint32 i, numFallbacks;

    gfxPlatform::GetPlatform()->GetCommonFallbackFonts(aCh, aRunScript,
                                                       defaultFallbacks);
    numFallbacks = defaultFallbacks.Length();
    for (i = 0; i < numFallbacks; i++) {
        nsAutoString familyName;
        const char *fallbackFamily = defaultFallbacks[i];

        familyName.AppendASCII(fallbackFamily);
        gfxFontFamily *fallback =
                gfxPlatformFontList::PlatformFontList()->FindFamily(familyName);
        if (!fallback)
            continue;

        gfxFontEntry *fontEntry;
        bool needsBold;  // ignored in the system fallback case

        // use first font in list that supports a given character
        fontEntry = fallback->FindFontForStyle(*aMatchStyle, needsBold);
        if (fontEntry && fontEntry->TestCharacterMap(aCh)) {
            return fontEntry;
        }
    }

    return nsnull;
}

gfxFontEntry*
gfxPlatformFontList::GlobalFontFallback(const PRUint32 aCh,
                                        PRInt32 aRunScript,
                                        const gfxFontStyle* aMatchStyle,
                                        PRUint32& aCmapCount)
{
    // otherwise, try to find it among local fonts
    GlobalFontMatch data(aCh, aRunScript, aMatchStyle);

    // iterate over all font families to find a font that support the character
    mFontFamilies.Enumerate(gfxPlatformFontList::FindFontForCharProc, &data);

    aCmapCount = data.mCmapsTested;

    return data.mBestMatch;
}

#ifdef XP_WIN
#include <windows.h>

// crude hack for using when monitoring process
static void LogRegistryEvent(const wchar_t *msg)
{
  HKEY dummyKey;
  HRESULT hr;
  wchar_t buf[512];

  wsprintfW(buf, L" log %s", msg);
  hr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, buf, 0, KEY_READ, &dummyKey);
  if (SUCCEEDED(hr)) {
    RegCloseKey(dummyKey);
  }
}
#endif

gfxFontFamily* 
gfxPlatformFontList::FindFamily(const nsAString& aFamily)
{
    nsAutoString key;
    gfxFontFamily *familyEntry;
    bool found;
    GenerateFontListKey(aFamily, key);

    NS_ASSERTION(mFontFamilies.Count() != 0, "system font list was not initialized correctly");

    // lookup in canonical (i.e. English) family name list
    if ((familyEntry = mFontFamilies.GetWeak(key, &found))) {
        return familyEntry;
    }

    // lookup in other family names list (mostly localized names)
    if ((familyEntry = mOtherFamilyNames.GetWeak(key, &found)) != nsnull) {
        return familyEntry;
    }

    // name not found and other family names not yet fully initialized so
    // initialize the rest of the list and try again.  this is done lazily
    // since reading name table entries is expensive.
    // although ASCII localized family names are possible they don't occur
    // in practice so avoid pulling in names at startup
    if (!mOtherFamilyNamesInitialized && !IsASCII(aFamily)) {
        InitOtherFamilyNames();
        if ((familyEntry = mOtherFamilyNames.GetWeak(key, &found)) != nsnull) {
            return familyEntry;
        }
    }

    return nsnull;
}

gfxFontEntry*
gfxPlatformFontList::FindFontForFamily(const nsAString& aFamily, const gfxFontStyle* aStyle, bool& aNeedsBold)
{
    gfxFontFamily *familyEntry = FindFamily(aFamily);

    aNeedsBold = false;

    if (familyEntry)
        return familyEntry->FindFontForStyle(*aStyle, aNeedsBold);

    return nsnull;
}

bool
gfxPlatformFontList::GetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<gfxFontFamily> > *array)
{
    return mPrefFonts.Get(PRUint32(aLangGroup), array);
}

void
gfxPlatformFontList::SetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<gfxFontFamily> >& array)
{
    mPrefFonts.Put(PRUint32(aLangGroup), array);
}

void 
gfxPlatformFontList::AddOtherFamilyName(gfxFontFamily *aFamilyEntry, nsAString& aOtherFamilyName)
{
    nsAutoString key;
    bool found;
    GenerateFontListKey(aOtherFamilyName, key);

    if (!mOtherFamilyNames.GetWeak(key, &found)) {
        mOtherFamilyNames.Put(key, aFamilyEntry);
#ifdef PR_LOGGING
        LOG_FONTLIST(("(fontlist-otherfamily) canonical family: %s, "
                      "other family: %s\n",
                      NS_ConvertUTF16toUTF8(aFamilyEntry->Name()).get(),
                      NS_ConvertUTF16toUTF8(aOtherFamilyName).get()));
#endif
        if (mBadUnderlineFamilyNames.Contains(key))
            aFamilyEntry->SetBadUnderlineFamily();
    }
}

void
gfxPlatformFontList::AddFullname(gfxFontEntry *aFontEntry, nsAString& aFullname)
{
    bool found;

    if (!mFullnames.GetWeak(aFullname, &found)) {
        mFullnames.Put(aFullname, aFontEntry);
#ifdef PR_LOGGING
        LOG_FONTLIST(("(fontlist-fullname) name: %s, fullname: %s\n",
                      NS_ConvertUTF16toUTF8(aFontEntry->Name()).get(),
                      NS_ConvertUTF16toUTF8(aFullname).get()));
#endif
    }
}

void
gfxPlatformFontList::AddPostscriptName(gfxFontEntry *aFontEntry, nsAString& aPostscriptName)
{
    bool found;

    if (!mPostscriptNames.GetWeak(aPostscriptName, &found)) {
        mPostscriptNames.Put(aPostscriptName, aFontEntry);
#ifdef PR_LOGGING
        LOG_FONTLIST(("(fontlist-postscript) name: %s, psname: %s\n",
                      NS_ConvertUTF16toUTF8(aFontEntry->Name()).get(),
                      NS_ConvertUTF16toUTF8(aPostscriptName).get()));
#endif
    }
}

bool
gfxPlatformFontList::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    aFamilyName.Truncate();
    ResolveFontName(aFontName, aFamilyName);
    return !aFamilyName.IsEmpty();
}

void 
gfxPlatformFontList::InitLoader()
{
    GetFontFamilyList(mFontFamiliesToLoad);
    mStartIndex = 0;
    mNumFamilies = mFontFamiliesToLoad.Length();
}

bool 
gfxPlatformFontList::RunLoader()
{
    PRUint32 i, endIndex = (mStartIndex + mIncrement < mNumFamilies ? mStartIndex + mIncrement : mNumFamilies);

    // for each font family, load in various font info
    for (i = mStartIndex; i < endIndex; i++) {
        gfxFontFamily* familyEntry = mFontFamiliesToLoad[i];

        // find all faces that are members of this family
        familyEntry->FindStyleVariations();
        if (familyEntry->GetFontList().Length() == 0) {
            // failed to load any faces for this family, so discard it
            nsAutoString key;
            GenerateFontListKey(familyEntry->Name(), key);
            mFontFamilies.Remove(key);
            continue;
        }

        // load the cmaps
        familyEntry->ReadCMAP();

        // read in face names
        familyEntry->ReadFaceNames(this, mNeedFullnamePostscriptNames);

        // check whether the family can be considered "simple" for style matching
        familyEntry->CheckForSimpleFamily();
    }

    mStartIndex = endIndex;

    return (mStartIndex >= mNumFamilies);
}

void 
gfxPlatformFontList::FinishLoader()
{
    mFontFamiliesToLoad.Clear();
    mNumFamilies = 0;
}
