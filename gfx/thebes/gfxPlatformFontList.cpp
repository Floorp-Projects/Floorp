/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "prlog.h"

#include "gfxPlatformFontList.h"
#include "gfxUserFontSet.h"

#include "nsUnicharUtils.h"
#include "nsUnicodeRange.h"
#include "nsUnicodeProperties.h"

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/gfx/2D.h"

using namespace mozilla;

#ifdef PR_LOGGING

#define LOG_FONTLIST(args) PR_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), \
                               PR_LOG_DEBUG, args)
#define LOG_FONTLIST_ENABLED() PR_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontlist), \
                                   PR_LOG_DEBUG)
#define LOG_FONTINIT(args) PR_LOG(gfxPlatform::GetLog(eGfxLog_fontinit), \
                               PR_LOG_DEBUG, args)
#define LOG_FONTINIT_ENABLED() PR_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontinit), \
                                   PR_LOG_DEBUG)

#endif // PR_LOGGING

gfxPlatformFontList *gfxPlatformFontList::sPlatformFontList = nullptr;

// prefs for the font info loader
#define FONT_LOADER_FAMILIES_PER_SLICE_PREF "gfx.font_loader.families_per_slice"
#define FONT_LOADER_DELAY_PREF              "gfx.font_loader.delay"
#define FONT_LOADER_INTERVAL_PREF           "gfx.font_loader.interval"

static const char* kObservedPrefs[] = {
    "font.",
    "font.name-list.",
    "intl.accept_languages",  // hmmmm...
    nullptr
};

class gfxFontListPrefObserver MOZ_FINAL : public nsIObserver {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

static gfxFontListPrefObserver* gFontListPrefObserver = nullptr;

NS_IMPL_ISUPPORTS1(gfxFontListPrefObserver, nsIObserver)

NS_IMETHODIMP
gfxFontListPrefObserver::Observe(nsISupports     *aSubject,
                                 const char      *aTopic,
                                 const char16_t *aData)
{
    NS_ASSERTION(!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID), "invalid topic");
    // XXX this could be made to only clear out the cache for the prefs that were changed
    // but it probably isn't that big a deal.
    gfxPlatformFontList::PlatformFontList()->ClearPrefFonts();
    gfxFontCache::GetCache()->AgeAllGenerations();
    return NS_OK;
}

MOZ_DEFINE_MALLOC_SIZE_OF(FontListMallocSizeOf)

NS_IMPL_ISUPPORTS1(gfxPlatformFontList::MemoryReporter, nsIMemoryReporter)

NS_IMETHODIMP
gfxPlatformFontList::MemoryReporter::CollectReports
    (nsIMemoryReporterCallback* aCb,
     nsISupports* aClosure)
{
    FontListSizes sizes;
    sizes.mFontListSize = 0;
    sizes.mFontTableCacheSize = 0;
    sizes.mCharMapsSize = 0;

    gfxPlatformFontList::PlatformFontList()->AddSizeOfIncludingThis(&FontListMallocSizeOf,
                                                                    &sizes);

    aCb->Callback(EmptyCString(),
                  NS_LITERAL_CSTRING("explicit/gfx/font-list"),
                  KIND_HEAP, UNITS_BYTES, sizes.mFontListSize,
                  NS_LITERAL_CSTRING("Memory used to manage the list of font families and faces."),
                  aClosure);

    aCb->Callback(EmptyCString(),
                  NS_LITERAL_CSTRING("explicit/gfx/font-charmaps"),
                  KIND_HEAP, UNITS_BYTES, sizes.mCharMapsSize,
                  NS_LITERAL_CSTRING("Memory used to record the character coverage of individual fonts."),
                  aClosure);

    if (sizes.mFontTableCacheSize) {
        aCb->Callback(EmptyCString(),
                      NS_LITERAL_CSTRING("explicit/gfx/font-tables"),
                      KIND_HEAP, UNITS_BYTES, sizes.mFontTableCacheSize,
                      NS_LITERAL_CSTRING("Memory used for cached font metrics and layout tables."),
                      aClosure);
    }

    return NS_OK;
}

gfxPlatformFontList::gfxPlatformFontList(bool aNeedFullnamePostscriptNames)
    : mFontFamilies(100), mOtherFamilyNames(30),
      mPrefFonts(10), mBadUnderlineFamilyNames(10), mSharedCmaps(16),
      mStartIndex(0), mIncrement(1), mNumFamilies(0)
{
    mOtherFamilyNamesInitialized = false;

    if (aNeedFullnamePostscriptNames) {
        mExtraNames = new ExtraNames();
    }
    mFaceNameListsInitialized = false;

    LoadBadUnderlineList();

    // pref changes notification setup
    NS_ASSERTION(!gFontListPrefObserver,
                 "There has been font list pref observer already");
    gFontListPrefObserver = new gfxFontListPrefObserver();
    NS_ADDREF(gFontListPrefObserver);
    Preferences::AddStrongObservers(gFontListPrefObserver, kObservedPrefs);

    RegisterStrongMemoryReporter(new MemoryReporter());
}

gfxPlatformFontList::~gfxPlatformFontList()
{
    mSharedCmaps.Clear();
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
    if (mExtraNames) {
        mExtraNames->mFullnames.Clear();
        mExtraNames->mPostscriptNames.Clear();
    }
    mFaceNameListsInitialized = false;
    mPrefFonts.Clear();
    mReplacementCharFallbackFamily = nullptr;
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

struct InitOtherNamesData {
    InitOtherNamesData(gfxPlatformFontList *aFontList,
                       TimeStamp aStartTime)
        : mFontList(aFontList), mStartTime(aStartTime), mTimedOut(false)
    {}

    gfxPlatformFontList *mFontList;
    TimeStamp mStartTime;
    bool mTimedOut;
};

void 
gfxPlatformFontList::InitOtherFamilyNames()
{
    if (mOtherFamilyNamesInitialized) {
        return;
    }

    TimeStamp start = TimeStamp::Now();

    // iterate over all font families and read in other family names
    InitOtherNamesData otherNamesData(this, start);

    mFontFamilies.Enumerate(gfxPlatformFontList::InitOtherFamilyNamesProc,
                            &otherNamesData);

    if (!otherNamesData.mTimedOut) {
        mOtherFamilyNamesInitialized = true;
    }
    TimeStamp end = TimeStamp::Now();
    Telemetry::AccumulateTimeDelta(Telemetry::FONTLIST_INITOTHERFAMILYNAMES,
                                   start, end);

#ifdef PR_LOGGING
    if (LOG_FONTINIT_ENABLED()) {
        TimeDuration elapsed = end - start;
        LOG_FONTINIT(("(fontinit) InitOtherFamilyNames took %8.2f ms %s",
                      elapsed.ToMilliseconds(),
                      (otherNamesData.mTimedOut ? "timeout" : "")));
    }
#endif
}

#define OTHERNAMES_TIMEOUT 200

PLDHashOperator
gfxPlatformFontList::InitOtherFamilyNamesProc(nsStringHashKey::KeyType aKey,
                                              nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                              void* userArg)
{
    InitOtherNamesData *data = static_cast<InitOtherNamesData*>(userArg);

    aFamilyEntry->ReadOtherFamilyNames(data->mFontList);
    TimeDuration elapsed = TimeStamp::Now() - data->mStartTime;
    if (elapsed.ToMilliseconds() > OTHERNAMES_TIMEOUT) {
        data->mTimedOut = true;
        return PL_DHASH_STOP;
    }
    return PL_DHASH_NEXT;
}
 
struct ReadFaceNamesData {
    ReadFaceNamesData(gfxPlatformFontList *aFontList, TimeStamp aStartTime)
        : mFontList(aFontList), mStartTime(aStartTime), mTimedOut(false)
    {}

    gfxPlatformFontList *mFontList;
    TimeStamp mStartTime;
    bool mTimedOut;

    // if mFirstChar is not empty, only load facenames for families
    // that start with this character
    nsString mFirstChar;
};

gfxFontEntry*
gfxPlatformFontList::SearchFamiliesForFaceName(const nsAString& aFaceName)
{
    TimeStamp start = TimeStamp::Now();
    gfxFontEntry *lookup = nullptr;

    ReadFaceNamesData faceNameListsData(this, start);

    // iterate over familes starting with the same letter
    faceNameListsData.mFirstChar.Assign(aFaceName.CharAt(0));
    ToLowerCase(faceNameListsData.mFirstChar);
    mFontFamilies.Enumerate(gfxPlatformFontList::ReadFaceNamesProc,
                            &faceNameListsData);
    lookup = FindFaceName(aFaceName);

    TimeStamp end = TimeStamp::Now();
    Telemetry::AccumulateTimeDelta(Telemetry::FONTLIST_INITFACENAMELISTS,
                                   start, end);
#ifdef PR_LOGGING
    if (LOG_FONTINIT_ENABLED()) {
        TimeDuration elapsed = end - start;
        LOG_FONTINIT(("(fontinit) SearchFamiliesForFaceName took %8.2f ms %s %s",
                      elapsed.ToMilliseconds(),
                      (lookup ? "found name" : ""),
                      (faceNameListsData.mTimedOut ? "timeout" : "")));
    }
#endif

    return lookup;
}

// time limit for loading facename lists (ms)
#define NAMELIST_TIMEOUT  200

PLDHashOperator
gfxPlatformFontList::ReadFaceNamesProc(nsStringHashKey::KeyType aKey,
                                       nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                       void* userArg)
{
    ReadFaceNamesData *data = static_cast<ReadFaceNamesData*>(userArg);
    gfxPlatformFontList *fc = data->mFontList;

    // when filtering, skip names that don't start with the filter character
    if (!(data->mFirstChar.IsEmpty())) {
        char16_t firstChar = aKey.CharAt(0);
        nsAutoString firstCharStr(&firstChar, 1);
        ToLowerCase(firstCharStr);
        if (!firstCharStr.Equals(data->mFirstChar)) {
            return PL_DHASH_NEXT;
        }
    }
    aFamilyEntry->ReadFaceNames(fc, fc->NeedFullnamePostscriptNames());

    TimeDuration elapsed = TimeStamp::Now() - data->mStartTime;
    if (elapsed.ToMilliseconds() > NAMELIST_TIMEOUT) {
        data->mTimedOut = true;
        return PL_DHASH_STOP;
    }
    return PL_DHASH_NEXT;
}

gfxFontEntry*
gfxPlatformFontList::FindFaceName(const nsAString& aFaceName)
{
    gfxFontEntry *lookup;

    // lookup in name lookup tables, return null if not found
    if (mExtraNames &&
        ((lookup = mExtraNames->mPostscriptNames.GetWeak(aFaceName)) ||
         (lookup = mExtraNames->mFullnames.GetWeak(aFaceName)))) {
        return lookup;
    }

    return nullptr;
}

gfxFontEntry*
gfxPlatformFontList::LookupInFaceNameLists(const nsAString& aFaceName)
{
    gfxFontEntry *lookup = nullptr;

    // initialize facename lookup tables if needed
    // note: this can terminate early or time out, in which case
    //       mFaceNameListsInitialized remains false
    if (!mFaceNameListsInitialized) {
        lookup = SearchFamiliesForFaceName(aFaceName);
        if (lookup) {
            return lookup;
        }
    }

    // lookup in name lookup tables, return null if not found
    if (!(lookup = FindFaceName(aFaceName))) {
        // names not completely initialized, so keep track of lookup misses
        if (!mFaceNameListsInitialized) {
            if (!mFaceNamesMissed) {
                mFaceNamesMissed = new nsTHashtable<nsStringHashKey>(4);
            }
            mFaceNamesMissed->PutEntry(aFaceName);
        }
    }

    return lookup;
}

void
gfxPlatformFontList::PreloadNamesList()
{
    nsAutoTArray<nsString, 10> preloadFonts;
    gfxFontUtils::GetPrefsFontList("font.preload-names-list", preloadFonts);

    uint32_t numFonts = preloadFonts.Length();
    for (uint32_t i = 0; i < numFonts; i++) {
        nsAutoString key;
        GenerateFontListKey(preloadFonts[i], key);
        
        // only search canonical names!
        gfxFontFamily *familyEntry = mFontFamilies.GetWeak(key);
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

    uint32_t i, numFonts = fontlist.Length();

    for (i = 0; i < numFonts; i++) {
        fontlist[i]->mFixedPitch = 1;
    }
}

void
gfxPlatformFontList::LoadBadUnderlineList()
{
    nsAutoTArray<nsString, 10> blacklist;
    gfxFontUtils::GetPrefsFontList("font.blacklist.underline_offset", blacklist);
    uint32_t numFonts = blacklist.Length();
    for (uint32_t i = 0; i < numFonts; i++) {
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

static PLDHashOperator
RebuildLocalFonts(nsPtrHashKey<gfxUserFontSet>* aKey,
                  void* aClosure)
{
    aKey->GetKey()->RebuildLocalRules();
    return PL_DHASH_NEXT;
}

void
gfxPlatformFontList::UpdateFontList()
{
    InitFontList();
    mUserFontSetList.EnumerateEntries(RebuildLocalFonts, nullptr);
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

PLDHashOperator
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

    static PLDHashOperator AppendFamily(nsStringHashKey::KeyType aKey,
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
gfxPlatformFontList::SystemFindFontForChar(const uint32_t aCh,
                                           int32_t aRunScript,
                                           const gfxFontStyle* aStyle)
 {
    gfxFontEntry* fontEntry = nullptr;

    // is codepoint with no matching font? return null immediately
    if (mCodepointsWithNoFonts.test(aCh)) {
        return nullptr;
    }

    // Try to short-circuit font fallback for U+FFFD, used to represent
    // encoding errors: just use cached family from last time U+FFFD was seen.
    // This helps speed up pages with lots of encoding errors, binary-as-text,
    // etc.
    if (aCh == 0xFFFD && mReplacementCharFallbackFamily) {
        bool needsBold;  // ignored in the system fallback case

        fontEntry =
            mReplacementCharFallbackFamily->FindFontForStyle(*aStyle,
                                                             needsBold);

        // this should never fail, as we must have found U+FFFD in order to set
        // mReplacementCharFallbackFamily at all, but better play it safe
        if (fontEntry && fontEntry->TestCharacterMap(aCh)) {
            return fontEntry;
        }
    }

    TimeStamp start = TimeStamp::Now();

    // search commonly available fonts
    bool common = true;
    gfxFontFamily *fallbackFamily = nullptr;
    fontEntry = CommonFontFallback(aCh, aRunScript, aStyle, &fallbackFamily);
 
    // if didn't find a font, do system-wide fallback (except for specials)
    uint32_t cmapCount = 0;
    if (!fontEntry) {
        common = false;
        fontEntry = GlobalFontFallback(aCh, aRunScript, aStyle, cmapCount,
                                       &fallbackFamily);
    }
    TimeDuration elapsed = TimeStamp::Now() - start;

#ifdef PR_LOGGING
    PRLogModuleInfo *log = gfxPlatform::GetLog(eGfxLog_textrun);

    if (MOZ_UNLIKELY(PR_LOG_TEST(log, PR_LOG_WARNING))) {
        uint32_t unicodeRange = FindCharUnicodeRange(aCh);
        int32_t script = mozilla::unicode::GetScriptCode(aCh);
        PR_LOG(log, PR_LOG_WARNING,\
               ("(textrun-systemfallback-%s) char: u+%6.6x "
                 "unicode-range: %d script: %d match: [%s]"
                " time: %dus cmaps: %d\n",
                (common ? "common" : "global"), aCh,
                 unicodeRange, script,
                (fontEntry ? NS_ConvertUTF16toUTF8(fontEntry->Name()).get() :
                    "<none>"),
                int32_t(elapsed.ToMicroseconds()),
                cmapCount));
    }
#endif

    // no match? add to set of non-matching codepoints
    if (!fontEntry) {
        mCodepointsWithNoFonts.set(aCh);
    } else if (aCh == 0xFFFD && fontEntry && fallbackFamily) {
        mReplacementCharFallbackFamily = fallbackFamily;
    }
 
    // track system fallback time
    static bool first = true;
    int32_t intElapsed = int32_t(first ? elapsed.ToMilliseconds() :
                                         elapsed.ToMicroseconds());
    Telemetry::Accumulate((first ? Telemetry::SYSTEM_FONT_FALLBACK_FIRST :
                                   Telemetry::SYSTEM_FONT_FALLBACK),
                          intElapsed);
    first = false;

    // track the script for which fallback occurred (incremented one make it
    // 1-based)
    Telemetry::Accumulate(Telemetry::SYSTEM_FONT_FALLBACK_SCRIPT, aRunScript + 1);

    return fontEntry;
}

PLDHashOperator 
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
gfxPlatformFontList::CommonFontFallback(const uint32_t aCh,
                                        int32_t aRunScript,
                                        const gfxFontStyle* aMatchStyle,
                                        gfxFontFamily** aMatchedFamily)
{
    nsAutoTArray<const char*,NUM_FALLBACK_FONTS> defaultFallbacks;
    uint32_t i, numFallbacks;

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
            *aMatchedFamily = fallback;
            return fontEntry;
        }
    }

    return nullptr;
}

gfxFontEntry*
gfxPlatformFontList::GlobalFontFallback(const uint32_t aCh,
                                        int32_t aRunScript,
                                        const gfxFontStyle* aMatchStyle,
                                        uint32_t& aCmapCount,
                                        gfxFontFamily** aMatchedFamily)
{
    // otherwise, try to find it among local fonts
    GlobalFontMatch data(aCh, aRunScript, aMatchStyle);

    // iterate over all font families to find a font that support the character
    mFontFamilies.Enumerate(gfxPlatformFontList::FindFontForCharProc, &data);

    aCmapCount = data.mCmapsTested;
    *aMatchedFamily = data.mMatchedFamily;

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
gfxPlatformFontList::CheckFamily(gfxFontFamily *aFamily)
{
    if (aFamily && !aFamily->HasStyles()) {
        aFamily->FindStyleVariations();
        aFamily->CheckForSimpleFamily();
    }

    if (aFamily && aFamily->GetFontList().Length() == 0) {
        // failed to load any faces for this family, so discard it
        nsAutoString key;
        GenerateFontListKey(aFamily->Name(), key);
        mFontFamilies.Remove(key);
        return nullptr;
    }

    return aFamily;
}

gfxFontFamily* 
gfxPlatformFontList::FindFamily(const nsAString& aFamily)
{
    nsAutoString key;
    gfxFontFamily *familyEntry;
    GenerateFontListKey(aFamily, key);

    NS_ASSERTION(mFontFamilies.Count() != 0, "system font list was not initialized correctly");

    // lookup in canonical (i.e. English) family name list
    if ((familyEntry = mFontFamilies.GetWeak(key))) {
        return CheckFamily(familyEntry);
    }

    // lookup in other family names list (mostly localized names)
    if ((familyEntry = mOtherFamilyNames.GetWeak(key)) != nullptr) {
        return CheckFamily(familyEntry);
    }

    // name not found and other family names not yet fully initialized so
    // initialize the rest of the list and try again.  this is done lazily
    // since reading name table entries is expensive.
    // although ASCII localized family names are possible they don't occur
    // in practice so avoid pulling in names at startup
    if (!mOtherFamilyNamesInitialized && !IsASCII(aFamily)) {
        InitOtherFamilyNames();
        if ((familyEntry = mOtherFamilyNames.GetWeak(key)) != nullptr) {
            return CheckFamily(familyEntry);
        } else if (!mOtherFamilyNamesInitialized) {
            // localized family names load timed out, add name to list of
            // names to check after localized names are loaded
            if (!mOtherNamesMissed) {
                mOtherNamesMissed = new nsTHashtable<nsStringHashKey>(4);
            }
            mOtherNamesMissed->PutEntry(key);
        }
    }

    return nullptr;
}

gfxFontEntry*
gfxPlatformFontList::FindFontForFamily(const nsAString& aFamily, const gfxFontStyle* aStyle, bool& aNeedsBold)
{
    gfxFontFamily *familyEntry = FindFamily(aFamily);

    aNeedsBold = false;

    if (familyEntry)
        return familyEntry->FindFontForStyle(*aStyle, aNeedsBold);

    return nullptr;
}

bool
gfxPlatformFontList::GetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<gfxFontFamily> > *array)
{
    return mPrefFonts.Get(uint32_t(aLangGroup), array);
}

void
gfxPlatformFontList::SetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<gfxFontFamily> >& array)
{
    mPrefFonts.Put(uint32_t(aLangGroup), array);
}

void 
gfxPlatformFontList::AddOtherFamilyName(gfxFontFamily *aFamilyEntry, nsAString& aOtherFamilyName)
{
    nsAutoString key;
    GenerateFontListKey(aOtherFamilyName, key);

    if (!mOtherFamilyNames.GetWeak(key)) {
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
    if (!mExtraNames->mFullnames.GetWeak(aFullname)) {
        mExtraNames->mFullnames.Put(aFullname, aFontEntry);
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
    if (!mExtraNames->mPostscriptNames.GetWeak(aPostscriptName)) {
        mExtraNames->mPostscriptNames.Put(aPostscriptName, aFontEntry);
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

gfxCharacterMap*
gfxPlatformFontList::FindCharMap(gfxCharacterMap *aCmap)
{
    aCmap->CalcHash();
    gfxCharacterMap *cmap = AddCmap(aCmap);
    cmap->mShared = true;
    return cmap;
}

// add a cmap to the shared cmap set
gfxCharacterMap*
gfxPlatformFontList::AddCmap(const gfxCharacterMap* aCharMap)
{
    CharMapHashKey *found =
        mSharedCmaps.PutEntry(const_cast<gfxCharacterMap*>(aCharMap));
    return found->GetKey();
}

// remove the cmap from the shared cmap set
void
gfxPlatformFontList::RemoveCmap(const gfxCharacterMap* aCharMap)
{
    // skip lookups during teardown
    if (mSharedCmaps.Count() == 0) {
        return;
    }

    // cmap needs to match the entry *and* be the same ptr before removing
    CharMapHashKey *found =
        mSharedCmaps.GetEntry(const_cast<gfxCharacterMap*>(aCharMap));
    if (found && found->GetKey() == aCharMap) {
        mSharedCmaps.RemoveEntry(const_cast<gfxCharacterMap*>(aCharMap));
    }
}

static PLDHashOperator AppendFamilyToList(nsStringHashKey::KeyType aKey,
                                          nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                          void *aUserArg)
{
    nsTArray<nsString> *familyNames = static_cast<nsTArray<nsString> *>(aUserArg);
    familyNames->AppendElement(aFamilyEntry->Name());
    return PL_DHASH_NEXT;
}

void
gfxPlatformFontList::GetFontFamilyNames(nsTArray<nsString>& aFontFamilyNames)
{
    mFontFamilies.Enumerate(AppendFamilyToList, &aFontFamilyNames);
}

void 
gfxPlatformFontList::InitLoader()
{
    GetFontFamilyNames(mFontInfo->mFontFamiliesToLoad);
    mStartIndex = 0;
    mNumFamilies = mFontInfo->mFontFamiliesToLoad.Length();
    memset(&(mFontInfo->mLoadStats), 0, sizeof(mFontInfo->mLoadStats));
}

#define FONT_LOADER_MAX_TIMESLICE 100  // max time for one pass through RunLoader = 100ms

bool
gfxPlatformFontList::LoadFontInfo()
{
    TimeStamp start = TimeStamp::Now();
    uint32_t i, endIndex = mNumFamilies;
    bool loadCmaps = !UsesSystemFallback() ||
        gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback();

    // for each font family, load in various font info
    for (i = mStartIndex; i < endIndex; i++) {
        nsAutoString key;
        gfxFontFamily *familyEntry;
        GenerateFontListKey(mFontInfo->mFontFamiliesToLoad[i], key);

        // lookup in canonical (i.e. English) family name list
        if (!(familyEntry = mFontFamilies.GetWeak(key))) {
            continue;
        }

        // read in face names
        familyEntry->ReadFaceNames(this, NeedFullnamePostscriptNames(), mFontInfo);

        // load the cmaps if needed
        if (loadCmaps) {
            familyEntry->ReadAllCMAPs(mFontInfo);
        }

        // limit the time spent reading fonts in one pass
        TimeDuration elapsed = TimeStamp::Now() - start;
        if (elapsed.ToMilliseconds() > FONT_LOADER_MAX_TIMESLICE &&
                i + 1 != endIndex) {
            endIndex = i + 1;
            break;
        }
    }

    mStartIndex = endIndex;
    bool done = mStartIndex >= mNumFamilies;

#ifdef PR_LOGGING
    if (LOG_FONTINIT_ENABLED()) {
        TimeDuration elapsed = TimeStamp::Now() - start;
        LOG_FONTINIT(("(fontinit) fontloader load pass %8.2f ms done %s\n",
                      elapsed.ToMilliseconds(), (done ? "true" : "false")));
    }
#endif

    if (done) {
        mOtherFamilyNamesInitialized = true;
        mFaceNameListsInitialized = true;
    }

    return done;
}

struct LookupMissedFaceNamesData {
    LookupMissedFaceNamesData(gfxPlatformFontList *aFontList)
        : mFontList(aFontList), mFoundName(false) {}

    gfxPlatformFontList *mFontList;
    bool mFoundName;
};

/*static*/ PLDHashOperator
gfxPlatformFontList::LookupMissedFaceNamesProc(nsStringHashKey *aKey,
                                               void *aUserArg)
{
    LookupMissedFaceNamesData *data =
        reinterpret_cast<LookupMissedFaceNamesData*>(aUserArg);

    if (data->mFontList->FindFaceName(aKey->GetKey())) {
        data->mFoundName = true;
        return PL_DHASH_STOP;
    }
    return PL_DHASH_NEXT;
}

struct LookupMissedOtherNamesData {
    LookupMissedOtherNamesData(gfxPlatformFontList *aFontList)
        : mFontList(aFontList), mFoundName(false) {}

    gfxPlatformFontList *mFontList;
    bool mFoundName;
};

/*static*/ PLDHashOperator
gfxPlatformFontList::LookupMissedOtherNamesProc(nsStringHashKey *aKey,
                                                void *aUserArg)
{
    LookupMissedOtherNamesData *data =
        reinterpret_cast<LookupMissedOtherNamesData*>(aUserArg);

    if (data->mFontList->FindFamily(aKey->GetKey())) {
        data->mFoundName = true;
        return PL_DHASH_STOP;
    }
    return PL_DHASH_NEXT;
}

void 
gfxPlatformFontList::CleanupLoader()
{
    mFontFamiliesToLoad.Clear();
    mNumFamilies = 0;
    bool rebuilt = false, forceReflow = false;

    // if had missed face names that are now available, force reflow all
    if (mFaceNamesMissed &&
        mFaceNamesMissed->Count() != 0) {
        LookupMissedFaceNamesData namedata(this);
        mFaceNamesMissed->EnumerateEntries(LookupMissedFaceNamesProc, &namedata);
        if (namedata.mFoundName) {
            rebuilt = true;
            mUserFontSetList.EnumerateEntries(RebuildLocalFonts, nullptr);
        }
        mFaceNamesMissed = nullptr;
    }

    if (mOtherNamesMissed) {
        LookupMissedOtherNamesData othernamesdata(this);
        mOtherNamesMissed->EnumerateEntries(LookupMissedOtherNamesProc,
                                            &othernamesdata);
        mOtherNamesMissed = nullptr;
        if (othernamesdata.mFoundName) {
            forceReflow = true;
            ForceGlobalReflow();
        }
    }

#ifdef PR_LOGGING
    if (LOG_FONTINIT_ENABLED() && mFontInfo) {
        LOG_FONTINIT(("(fontinit) fontloader load thread took %8.2f ms "
                      "%d families %d fonts %d cmaps "
                      "%d facenames %d othernames %s %s",
                      mLoadTime.ToMilliseconds(),
                      mFontInfo->mLoadStats.families,
                      mFontInfo->mLoadStats.fonts,
                      mFontInfo->mLoadStats.cmaps,
                      mFontInfo->mLoadStats.facenames,
                      mFontInfo->mLoadStats.othernames,
                      (rebuilt ? "(userfont sets rebuilt)" : ""),
                      (forceReflow ? "(global reflow)" : "")));
    }
#endif

    gfxFontInfoLoader::CleanupLoader();
}

void
gfxPlatformFontList::GetPrefsAndStartLoader()
{
    mIncrement =
        std::max(1u, Preferences::GetUint(FONT_LOADER_FAMILIES_PER_SLICE_PREF));

    uint32_t delay =
        std::max(1u, Preferences::GetUint(FONT_LOADER_DELAY_PREF));
    uint32_t interval =
        std::max(1u, Preferences::GetUint(FONT_LOADER_INTERVAL_PREF));

    StartLoader(delay, interval);
}

void
gfxPlatformFontList::ForceGlobalReflow()
{
    // modify a preference that will trigger reflow everywhere
    static const char kPrefName[] = "font.internaluseonly.changed";
    bool fontInternalChange = Preferences::GetBool(kPrefName, false);
    Preferences::SetBool(kPrefName, !fontInternalChange);
}

// Support for memory reporting

static size_t
SizeOfFamilyEntryExcludingThis(const nsAString&               aKey,
                               const nsRefPtr<gfxFontFamily>& aFamily,
                               MallocSizeOf                   aMallocSizeOf,
                               void*                          aUserArg)
{
    FontListSizes *sizes = static_cast<FontListSizes*>(aUserArg);
    aFamily->AddSizeOfExcludingThis(aMallocSizeOf, sizes);

    sizes->mFontListSize += aKey.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

    // we return zero here because the measurements have been added directly
    // to the relevant fields of the FontListSizes record
    return 0;
}

// this is also used by subclasses that hold additional hashes of family names
/*static*/ size_t
gfxPlatformFontList::SizeOfFamilyNameEntryExcludingThis
    (const nsAString&               aKey,
     const nsRefPtr<gfxFontFamily>& aFamily,
     MallocSizeOf                   aMallocSizeOf,
     void*                          aUserArg)
{
    // we don't count the size of the family here, because this is an *extra*
    // reference to a family that will have already been counted in the main list
    return aKey.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

static size_t
SizeOfFontNameEntryExcludingThis(const nsAString&              aKey,
                                 const nsRefPtr<gfxFontEntry>& aFont,
                                 MallocSizeOf                  aMallocSizeOf,
                                 void*                         aUserArg)
{
    // the font itself is counted by its owning family; here we only care about
    // the name stored in the hashtable key
    return aKey.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

static size_t
SizeOfPrefFontEntryExcludingThis
    (const uint32_t&                           aKey,
     const nsTArray<nsRefPtr<gfxFontFamily> >& aList,
     MallocSizeOf                              aMallocSizeOf,
     void*                                     aUserArg)
{
    // again, we only care about the size of the array itself; we don't follow
    // the refPtrs stored in it, because they point to entries already owned
    // and accounted-for by the main font list
    return aList.SizeOfExcludingThis(aMallocSizeOf);
}

static size_t
SizeOfStringEntryExcludingThis(nsStringHashKey*  aHashEntry,
                               MallocSizeOf      aMallocSizeOf,
                               void*             aUserArg)
{
    return aHashEntry->GetKey().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

static size_t
SizeOfSharedCmapExcludingThis(CharMapHashKey*   aHashEntry,
                              MallocSizeOf      aMallocSizeOf,
                              void*             aUserArg)
{
    FontListSizes *sizes = static_cast<FontListSizes*>(aUserArg);

    uint32_t size = aHashEntry->GetKey()->SizeOfIncludingThis(aMallocSizeOf);
    sizes->mCharMapsSize += size;

    // we return zero here because the measurements have been added directly
    // to the relevant fields of the FontListSizes record
    return 0;
}

void
gfxPlatformFontList::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                            FontListSizes* aSizes) const
{
    aSizes->mFontListSize +=
        mFontFamilies.SizeOfExcludingThis(SizeOfFamilyEntryExcludingThis,
                                          aMallocSizeOf, aSizes);

    aSizes->mFontListSize +=
        mOtherFamilyNames.SizeOfExcludingThis(SizeOfFamilyNameEntryExcludingThis,
                                              aMallocSizeOf);

    if (mExtraNames) {
        aSizes->mFontListSize +=
            mExtraNames->mFullnames.SizeOfExcludingThis(SizeOfFontNameEntryExcludingThis,
                                                        aMallocSizeOf);
        aSizes->mFontListSize +=
            mExtraNames->mPostscriptNames.SizeOfExcludingThis(SizeOfFontNameEntryExcludingThis,
                                                              aMallocSizeOf);
    }

    aSizes->mFontListSize +=
        mCodepointsWithNoFonts.SizeOfExcludingThis(aMallocSizeOf);
    aSizes->mFontListSize +=
        mFontFamiliesToLoad.SizeOfExcludingThis(aMallocSizeOf);

    aSizes->mFontListSize +=
        mPrefFonts.SizeOfExcludingThis(SizeOfPrefFontEntryExcludingThis,
                                       aMallocSizeOf);

    aSizes->mFontListSize +=
        mBadUnderlineFamilyNames.SizeOfExcludingThis(SizeOfStringEntryExcludingThis,
                                                     aMallocSizeOf);

    aSizes->mFontListSize +=
        mSharedCmaps.SizeOfExcludingThis(SizeOfSharedCmapExcludingThis,
                                         aMallocSizeOf, aSizes);
}

void
gfxPlatformFontList::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                            FontListSizes* aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}
