/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "gfxPlatformFontList.h"
#include "gfxTextRun.h"
#include "gfxUserFontSet.h"

#include "nsCRT.h"
#include "nsGkAtoms.h"
#include "nsILocaleService.h"
#include "nsServiceManagerUtils.h"
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

#define LOG_FONTLIST(args) MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), \
                               LogLevel::Debug, args)
#define LOG_FONTLIST_ENABLED() MOZ_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontlist), \
                                   LogLevel::Debug)
#define LOG_FONTINIT(args) MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontinit), \
                               LogLevel::Debug, args)
#define LOG_FONTINIT_ENABLED() MOZ_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontinit), \
                                   LogLevel::Debug)

gfxPlatformFontList *gfxPlatformFontList::sPlatformFontList = nullptr;

// Character ranges that require complex-script shaping support in the font,
// and so should be masked out by ReadCMAP if the necessary layout tables
// are not present.
// Currently used by the Mac and FT2 implementations only, but probably should
// be supported on Windows as well.
const gfxFontEntry::ScriptRange gfxPlatformFontList::sComplexScriptRanges[] = {
    // Actually, now that harfbuzz supports presentation-forms shaping for
    // Arabic, we can render it without layout tables. So maybe we don't
    // want to mask the basic Arabic block here?
    // This affects the arabic-fallback-*.html reftests, which rely on
    // loading a font that *doesn't* have any GSUB table.
    { 0x0600, 0x06FF, { TRUETYPE_TAG('a','r','a','b'), 0, 0 } },
    { 0x0700, 0x074F, { TRUETYPE_TAG('s','y','r','c'), 0, 0 } },
    { 0x0750, 0x077F, { TRUETYPE_TAG('a','r','a','b'), 0, 0 } },
    { 0x08A0, 0x08FF, { TRUETYPE_TAG('a','r','a','b'), 0, 0 } },
    { 0x0900, 0x097F, { TRUETYPE_TAG('d','e','v','2'),
                        TRUETYPE_TAG('d','e','v','a'), 0 } },
    { 0x0980, 0x09FF, { TRUETYPE_TAG('b','n','g','2'),
                        TRUETYPE_TAG('b','e','n','g'), 0 } },
    { 0x0A00, 0x0A7F, { TRUETYPE_TAG('g','u','r','2'),
                        TRUETYPE_TAG('g','u','r','u'), 0 } },
    { 0x0A80, 0x0AFF, { TRUETYPE_TAG('g','j','r','2'),
                        TRUETYPE_TAG('g','u','j','r'), 0 } },
    { 0x0B00, 0x0B7F, { TRUETYPE_TAG('o','r','y','2'),
                        TRUETYPE_TAG('o','r','y','a'), 0 } },
    { 0x0B80, 0x0BFF, { TRUETYPE_TAG('t','m','l','2'),
                        TRUETYPE_TAG('t','a','m','l'), 0 } },
    { 0x0C00, 0x0C7F, { TRUETYPE_TAG('t','e','l','2'),
                        TRUETYPE_TAG('t','e','l','u'), 0 } },
    { 0x0C80, 0x0CFF, { TRUETYPE_TAG('k','n','d','2'),
                        TRUETYPE_TAG('k','n','d','a'), 0 } },
    { 0x0D00, 0x0D7F, { TRUETYPE_TAG('m','l','m','2'),
                        TRUETYPE_TAG('m','l','y','m'), 0 } },
    { 0x0D80, 0x0DFF, { TRUETYPE_TAG('s','i','n','h'), 0, 0 } },
    { 0x0E80, 0x0EFF, { TRUETYPE_TAG('l','a','o',' '), 0, 0 } },
    { 0x0F00, 0x0FFF, { TRUETYPE_TAG('t','i','b','t'), 0, 0 } },
    { 0x1000, 0x109f, { TRUETYPE_TAG('m','y','m','r'),
                        TRUETYPE_TAG('m','y','m','2'), 0 } },
    { 0x1780, 0x17ff, { TRUETYPE_TAG('k','h','m','r'), 0, 0 } },
    // Khmer Symbols (19e0..19ff) don't seem to need any special shaping
    { 0xaa60, 0xaa7f, { TRUETYPE_TAG('m','y','m','r'),
                        TRUETYPE_TAG('m','y','m','2'), 0 } },
    // Thai seems to be "renderable" without AAT morphing tables
    { 0, 0, { 0, 0, 0 } } // terminator
};

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

// xxx - this can probably be eliminated by reworking pref font handling code
static const char *gPrefLangNames[] = {
    #define FONT_PREF_LANG(enum_id_, str_, atom_id_) str_
    #include "gfxFontPrefLangList.h"
    #undef FONT_PREF_LANG
};

static_assert(MOZ_ARRAY_LENGTH(gPrefLangNames) == uint32_t(eFontPrefLang_Count),
              "size of pref lang name array doesn't match pref lang enum size");

class gfxFontListPrefObserver final : public nsIObserver {
    ~gfxFontListPrefObserver() {}
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

static gfxFontListPrefObserver* gFontListPrefObserver = nullptr;

NS_IMPL_ISUPPORTS(gfxFontListPrefObserver, nsIObserver)

NS_IMETHODIMP
gfxFontListPrefObserver::Observe(nsISupports     *aSubject,
                                 const char      *aTopic,
                                 const char16_t *aData)
{
    NS_ASSERTION(!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID), "invalid topic");
    // XXX this could be made to only clear out the cache for the prefs that were changed
    // but it probably isn't that big a deal.
    gfxPlatformFontList::PlatformFontList()->ClearLangGroupPrefFonts();
    gfxFontCache::GetCache()->AgeAllGenerations();
    return NS_OK;
}

MOZ_DEFINE_MALLOC_SIZE_OF(FontListMallocSizeOf)

NS_IMPL_ISUPPORTS(gfxPlatformFontList::MemoryReporter, nsIMemoryReporter)

NS_IMETHODIMP
gfxPlatformFontList::MemoryReporter::CollectReports(
    nsIMemoryReporterCallback* aCb, nsISupports* aClosure, bool aAnonymize)
{
    FontListSizes sizes;
    sizes.mFontListSize = 0;
    sizes.mFontTableCacheSize = 0;
    sizes.mCharMapsSize = 0;

    gfxPlatformFontList::PlatformFontList()->AddSizeOfIncludingThis(&FontListMallocSizeOf,
                                                                    &sizes);

    nsresult rv;
    rv = aCb->Callback(EmptyCString(),
                       NS_LITERAL_CSTRING("explicit/gfx/font-list"),
                       KIND_HEAP, UNITS_BYTES, sizes.mFontListSize,
                       NS_LITERAL_CSTRING("Memory used to manage the list of font families and faces."),
                       aClosure);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aCb->Callback(EmptyCString(),
                       NS_LITERAL_CSTRING("explicit/gfx/font-charmaps"),
                       KIND_HEAP, UNITS_BYTES, sizes.mCharMapsSize,
                       NS_LITERAL_CSTRING("Memory used to record the character coverage of individual fonts."),
                       aClosure);
    NS_ENSURE_SUCCESS(rv, rv);

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
    : mFontFamilies(64), mOtherFamilyNames(16),
      mBadUnderlineFamilyNames(8), mSharedCmaps(8),
      mStartIndex(0), mIncrement(1), mNumFamilies(0), mFontlistInitCount(0)
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
    ClearLangGroupPrefFonts();
    NS_ASSERTION(gFontListPrefObserver, "There is no font list pref observer");
    Preferences::RemoveObservers(gFontListPrefObserver, kObservedPrefs);
    NS_RELEASE(gFontListPrefObserver);
}

// number of CSS generic font families
const uint32_t kNumGenerics = 5;

nsresult
gfxPlatformFontList::InitFontList()
{
    mFontlistInitCount++;

    if (LOG_FONTINIT_ENABLED()) {
        LOG_FONTINIT(("(fontinit) system fontlist initialization\n"));
    }

    // rebuilding fontlist so clear out font/word caches
    gfxFontCache *fontCache = gfxFontCache::GetCache();
    if (fontCache) {
        fontCache->AgeAllGenerations();
        fontCache->FlushShapedWordCaches();
    }

    mFontFamilies.Clear();
    mOtherFamilyNames.Clear();
    mOtherFamilyNamesInitialized = false;
    if (mExtraNames) {
        mExtraNames->mFullnames.Clear();
        mExtraNames->mPostscriptNames.Clear();
    }
    mFaceNameListsInitialized = false;
    ClearLangGroupPrefFonts();
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

#define OTHERNAMES_TIMEOUT 200

void
gfxPlatformFontList::InitOtherFamilyNames()
{
    if (mOtherFamilyNamesInitialized) {
        return;
    }

    TimeStamp start = TimeStamp::Now();
    bool timedOut = false;

    for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
        nsRefPtr<gfxFontFamily>& family = iter.Data();
        family->ReadOtherFamilyNames(this);
        TimeDuration elapsed = TimeStamp::Now() - start;
        if (elapsed.ToMilliseconds() > OTHERNAMES_TIMEOUT) {
            timedOut = true;
            break;
        }
    }

    if (!timedOut) {
        mOtherFamilyNamesInitialized = true;
    }
    TimeStamp end = TimeStamp::Now();
    Telemetry::AccumulateTimeDelta(Telemetry::FONTLIST_INITOTHERFAMILYNAMES,
                                   start, end);

    if (LOG_FONTINIT_ENABLED()) {
        TimeDuration elapsed = end - start;
        LOG_FONTINIT(("(fontinit) InitOtherFamilyNames took %8.2f ms %s",
                      elapsed.ToMilliseconds(),
                      (timedOut ? "timeout" : "")));
    }
}

// time limit for loading facename lists (ms)
#define NAMELIST_TIMEOUT  200

gfxFontEntry*
gfxPlatformFontList::SearchFamiliesForFaceName(const nsAString& aFaceName)
{
    TimeStamp start = TimeStamp::Now();
    bool timedOut = false;
    // if mFirstChar is not 0, only load facenames for families
    // that start with this character
    char16_t firstChar = 0;
    gfxFontEntry *lookup = nullptr;

    // iterate over familes starting with the same letter
    firstChar = ToLowerCase(aFaceName.CharAt(0));

    for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
        nsStringHashKey::KeyType key = iter.Key();
        nsRefPtr<gfxFontFamily>& family = iter.Data();

        // when filtering, skip names that don't start with the filter character
        if (firstChar && ToLowerCase(key.CharAt(0)) != firstChar) {
            continue;
        }

        family->ReadFaceNames(this, NeedFullnamePostscriptNames());

        TimeDuration elapsed = TimeStamp::Now() - start;
        if (elapsed.ToMilliseconds() > NAMELIST_TIMEOUT) {
           timedOut = true;
           break;
        }
    }

    lookup = FindFaceName(aFaceName);

    TimeStamp end = TimeStamp::Now();
    Telemetry::AccumulateTimeDelta(Telemetry::FONTLIST_INITFACENAMELISTS,
                                   start, end);
    if (LOG_FONTINIT_ENABLED()) {
        TimeDuration elapsed = end - start;
        LOG_FONTINIT(("(fontinit) SearchFamiliesForFaceName took %8.2f ms %s %s",
                      elapsed.ToMilliseconds(),
                      (lookup ? "found name" : ""),
                      (timedOut ? "timeout" : "")));
    }

    return lookup;
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
                mFaceNamesMissed = new nsTHashtable<nsStringHashKey>(2);
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

void
gfxPlatformFontList::UpdateFontList()
{
    InitFontList();
    RebuildLocalFonts();
}

void
gfxPlatformFontList::GetFontList(nsIAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts)
{
    for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
        nsRefPtr<gfxFontFamily>& family = iter.Data();
        // use the first variation for now.  This data should be the same
        // for all the variations and should probably be moved up to
        // the Family
        gfxFontStyle style;
        style.language = aLangGroup;
        bool needsBold;
        nsRefPtr<gfxFontEntry> fontEntry = family->FindFontForStyle(style, needsBold);
        NS_ASSERTION(fontEntry, "couldn't find any font entry in family");
        if (!fontEntry) {
            continue;
        }

        /* skip symbol fonts */
        if (fontEntry->IsSymbolFont()) {
            continue;
        }

        if (fontEntry->SupportsLangGroup(aLangGroup) &&
            fontEntry->MatchesGenericFamily(aGenericFamily)) {
            nsAutoString localizedFamilyName;
            family->LocalizedName(localizedFamilyName);
            aListOfFonts.AppendElement(localizedFamilyName);
        }
    }

    aListOfFonts.Sort();
    aListOfFonts.Compact();
}

void
gfxPlatformFontList::GetFontFamilyList(nsTArray<nsRefPtr<gfxFontFamily> >& aFamilyArray)
{
    for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
        nsRefPtr<gfxFontFamily>& family = iter.Data();
        aFamilyArray.AppendElement(family);
    }
}

gfxFontEntry*
gfxPlatformFontList::SystemFindFontForChar(uint32_t aCh, uint32_t aNextCh,
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
        if (fontEntry && fontEntry->HasCharacter(aCh)) {
            return fontEntry;
        }
    }

    TimeStamp start = TimeStamp::Now();

    // search commonly available fonts
    bool common = true;
    gfxFontFamily *fallbackFamily = nullptr;
    fontEntry = CommonFontFallback(aCh, aNextCh, aRunScript, aStyle,
                                   &fallbackFamily);
 
    // if didn't find a font, do system-wide fallback (except for specials)
    uint32_t cmapCount = 0;
    if (!fontEntry) {
        common = false;
        fontEntry = GlobalFontFallback(aCh, aRunScript, aStyle, cmapCount,
                                       &fallbackFamily);
    }
    TimeDuration elapsed = TimeStamp::Now() - start;

    PRLogModuleInfo *log = gfxPlatform::GetLog(eGfxLog_textrun);

    if (MOZ_UNLIKELY(MOZ_LOG_TEST(log, LogLevel::Warning))) {
        uint32_t unicodeRange = FindCharUnicodeRange(aCh);
        int32_t script = mozilla::unicode::GetScriptCode(aCh);
        MOZ_LOG(log, LogLevel::Warning,\
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

#define NUM_FALLBACK_FONTS        8

gfxFontEntry*
gfxPlatformFontList::CommonFontFallback(uint32_t aCh, uint32_t aNextCh,
                                        int32_t aRunScript,
                                        const gfxFontStyle* aMatchStyle,
                                        gfxFontFamily** aMatchedFamily)
{
    nsAutoTArray<const char*,NUM_FALLBACK_FONTS> defaultFallbacks;
    uint32_t i, numFallbacks;

    gfxPlatform::GetPlatform()->GetCommonFallbackFonts(aCh, aNextCh,
                                                       aRunScript,
                                                       defaultFallbacks);
    numFallbacks = defaultFallbacks.Length();
    for (i = 0; i < numFallbacks; i++) {
        nsAutoString familyName;
        const char *fallbackFamily = defaultFallbacks[i];

        familyName.AppendASCII(fallbackFamily);
        gfxFontFamily *fallback = FindFamilyByCanonicalName(familyName);
        if (!fallback)
            continue;

        gfxFontEntry *fontEntry;
        bool needsBold;  // ignored in the system fallback case

        // use first font in list that supports a given character
        fontEntry = fallback->FindFontForStyle(*aMatchStyle, needsBold);
        if (fontEntry && fontEntry->HasCharacter(aCh)) {
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
    for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
      nsRefPtr<gfxFontFamily>& family = iter.Data();
      // evaluate all fonts in this family for a match
      family->FindFontForChar(&data);
    }

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
gfxPlatformFontList::FindFamily(const nsAString& aFamily,
                                nsIAtom* aLanguage,
                                bool aUseSystemFonts)
{
    nsAutoString key;
    gfxFontFamily *familyEntry;
    GenerateFontListKey(aFamily, key);

    NS_ASSERTION(mFontFamilies.Count() != 0, "system font list was not initialized correctly");

    // lookup in canonical (i.e. English) family name list
    if ((familyEntry = mFontFamilies.GetWeak(key))) {
        return CheckFamily(familyEntry);
    }

#if defined(XP_MACOSX)
    // for system font types allow hidden system fonts to be referenced
    if (aUseSystemFonts) {
        if ((familyEntry = mSystemFontFamilies.GetWeak(key)) != nullptr) {
            return CheckFamily(familyEntry);
        }
    }
#endif

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
                mOtherNamesMissed = new nsTHashtable<nsStringHashKey>(2);
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

void 
gfxPlatformFontList::AddOtherFamilyName(gfxFontFamily *aFamilyEntry, nsAString& aOtherFamilyName)
{
    nsAutoString key;
    GenerateFontListKey(aOtherFamilyName, key);

    if (!mOtherFamilyNames.GetWeak(key)) {
        mOtherFamilyNames.Put(key, aFamilyEntry);
        LOG_FONTLIST(("(fontlist-otherfamily) canonical family: %s, "
                      "other family: %s\n",
                      NS_ConvertUTF16toUTF8(aFamilyEntry->Name()).get(),
                      NS_ConvertUTF16toUTF8(aOtherFamilyName).get()));
        if (mBadUnderlineFamilyNames.Contains(key))
            aFamilyEntry->SetBadUnderlineFamily();
    }
}

void
gfxPlatformFontList::AddFullname(gfxFontEntry *aFontEntry, nsAString& aFullname)
{
    if (!mExtraNames->mFullnames.GetWeak(aFullname)) {
        mExtraNames->mFullnames.Put(aFullname, aFontEntry);
        LOG_FONTLIST(("(fontlist-fullname) name: %s, fullname: %s\n",
                      NS_ConvertUTF16toUTF8(aFontEntry->Name()).get(),
                      NS_ConvertUTF16toUTF8(aFullname).get()));
    }
}

void
gfxPlatformFontList::AddPostscriptName(gfxFontEntry *aFontEntry, nsAString& aPostscriptName)
{
    if (!mExtraNames->mPostscriptNames.GetWeak(aPostscriptName)) {
        mExtraNames->mPostscriptNames.Put(aPostscriptName, aFontEntry);
        LOG_FONTLIST(("(fontlist-postscript) name: %s, psname: %s\n",
                      NS_ConvertUTF16toUTF8(aFontEntry->Name()).get(),
                      NS_ConvertUTF16toUTF8(aPostscriptName).get()));
    }
}

bool
gfxPlatformFontList::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    aFamilyName.Truncate();
    gfxFontFamily *ff = FindFamily(aFontName);
    if (!ff) {
        return false;
    }
    aFamilyName.Assign(ff->Name());
    return true;
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

void
gfxPlatformFontList::ResolveGenericFontNames(
    FontFamilyType aGenericType,
    eFontPrefLang aPrefLang,
    nsTArray<nsRefPtr<gfxFontFamily>>* aGenericFamilies
)
{
    const char* langGroupStr = GetPrefLangName(aPrefLang);

    static const char kGeneric_serif[] = "serif";
    static const char kGeneric_sans_serif[] = "sans-serif";
    static const char kGeneric_monospace[] = "monospace";
    static const char kGeneric_cursive[] = "cursive";
    static const char kGeneric_fantasy[] = "fantasy";

    // type should be standard generic type at this point
    NS_ASSERTION(aGenericType >= eFamily_serif &&
                 aGenericType <= eFamily_fantasy,
                 "standard generic font family type required");

    // map generic type to string
    const char *generic = nullptr;
    switch (aGenericType) {
        case eFamily_serif:
            generic = kGeneric_serif;
            break;
        case eFamily_sans_serif:
            generic = kGeneric_sans_serif;
            break;
        case eFamily_monospace:
            generic = kGeneric_monospace;
            break;
        case eFamily_cursive:
            generic = kGeneric_cursive;
            break;
        case eFamily_fantasy:
            generic = kGeneric_fantasy;
            break;
        default:
            break;
    }

    if (!generic) {
        return;
    }

    nsAutoTArray<nsString,4> genericFamilies;

    // load family for "font.name.generic.lang"
    nsAutoCString prefFontName("font.name.");
    prefFontName.Append(generic);
    prefFontName.Append('.');
    prefFontName.Append(langGroupStr);
    gfxFontUtils::AppendPrefsFontList(prefFontName.get(), genericFamilies);

    // load fonts for "font.name-list.generic.lang"
    nsAutoCString prefFontListName("font.name-list.");
    prefFontListName.Append(generic);
    prefFontListName.Append('.');
    prefFontListName.Append(langGroupStr);
    gfxFontUtils::AppendPrefsFontList(prefFontListName.get(), genericFamilies);

    nsIAtom* langGroup = GetLangGroupForPrefLang(aPrefLang);
    NS_ASSERTION(langGroup, "null lang group for pref lang");

    // lookup and add platform fonts uniquely
    for (const nsString& genericFamily : genericFamilies) {
        nsRefPtr<gfxFontFamily> family =
            FindFamily(genericFamily, langGroup, false);
        if (family) {
            bool notFound = true;
            for (const gfxFontFamily* f : *aGenericFamilies) {
                if (f == family) {
                    notFound = false;
                    break;
                }
            }
            if (notFound) {
                aGenericFamilies->AppendElement(family);
            }
        }
    }

#if 0  // dump out generic mappings
    printf("%s ===> ", prefFontName.get());
    for (uint32_t k = 0; k < aGenericFamilies->Length(); k++) {
        if (k > 0) printf(", ");
        printf("%s", NS_ConvertUTF16toUTF8(aGenericFamilies[k]->Name()).get());
    }
    printf("\n");
#endif
}

nsTArray<nsRefPtr<gfxFontFamily>>*
gfxPlatformFontList::GetPrefFontsLangGroup(mozilla::FontFamilyType aGenericType,
                                           eFontPrefLang aPrefLang)
{
    // treat -moz-fixed as monospace
    if (aGenericType == eFamily_moz_fixed) {
        aGenericType = eFamily_monospace;
    }

    PrefFontList* prefFonts = mLangGroupPrefFonts[aPrefLang][aGenericType];
    if (MOZ_UNLIKELY(!prefFonts)) {
        prefFonts = new PrefFontList;
        ResolveGenericFontNames(aGenericType, aPrefLang, prefFonts);
        mLangGroupPrefFonts[aPrefLang][aGenericType] = prefFonts;
    }
    return prefFonts;
}

void
gfxPlatformFontList::AddGenericFonts(mozilla::FontFamilyType aGenericType,
                                     nsIAtom* aLanguage,
                                     nsTArray<gfxFontFamily*>& aFamilyList)
{
    // map lang ==> langGroup
    nsIAtom *langGroup = nullptr;
    if (aLanguage) {
        if (!mLangService) {
            mLangService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
        }
        if (mLangService) {
            nsresult rv;
            langGroup = mLangService->GetLanguageGroup(aLanguage, &rv);
        }
    }
    if (!langGroup) {
        langGroup = nsGkAtoms::Unicode;
    }

    // langGroup ==> prefLang
    eFontPrefLang prefLang = GetFontPrefLangFor(langGroup);

    // lookup pref fonts
    nsTArray<nsRefPtr<gfxFontFamily>>* prefFonts =
        GetPrefFontsLangGroup(aGenericType, prefLang);

    if (!prefFonts->IsEmpty()) {
        aFamilyList.AppendElements(*prefFonts);
    }
}

static nsIAtom* PrefLangToLangGroups(uint32_t aIndex)
{
    // static array here avoids static constructor
    static nsIAtom* gPrefLangToLangGroups[] = {
        #define FONT_PREF_LANG(enum_id_, str_, atom_id_) nsGkAtoms::atom_id_
        #include "gfxFontPrefLangList.h"
        #undef FONT_PREF_LANG
    };

    return aIndex < ArrayLength(gPrefLangToLangGroups)
         ? gPrefLangToLangGroups[aIndex]
         : nsGkAtoms::Unicode;
}

eFontPrefLang
gfxPlatformFontList::GetFontPrefLangFor(const char* aLang)
{
    if (!aLang || !aLang[0]) {
        return eFontPrefLang_Others;
    }
    for (uint32_t i = 0; i < ArrayLength(gPrefLangNames); ++i) {
        if (!PL_strcasecmp(gPrefLangNames[i], aLang)) {
            return eFontPrefLang(i);
        }
    }
    return eFontPrefLang_Others;
}

eFontPrefLang
gfxPlatformFontList::GetFontPrefLangFor(nsIAtom *aLang)
{
    if (!aLang)
        return eFontPrefLang_Others;
    nsAutoCString lang;
    aLang->ToUTF8String(lang);
    return GetFontPrefLangFor(lang.get());
}

nsIAtom*
gfxPlatformFontList::GetLangGroupForPrefLang(eFontPrefLang aLang)
{
    // the special CJK set pref lang should be resolved into separate
    // calls to individual CJK pref langs before getting here
    NS_ASSERTION(aLang != eFontPrefLang_CJKSet, "unresolved CJK set pref lang");

    return PrefLangToLangGroups(uint32_t(aLang));
}

const char*
gfxPlatformFontList::GetPrefLangName(eFontPrefLang aLang)
{
    if (uint32_t(aLang) < ArrayLength(gPrefLangNames)) {
        return gPrefLangNames[uint32_t(aLang)];
    }
    return nullptr;
}

eFontPrefLang
gfxPlatformFontList::GetFontPrefLangFor(uint8_t aUnicodeRange)
{
    switch (aUnicodeRange) {
        case kRangeSetLatin:   return eFontPrefLang_Western;
        case kRangeCyrillic:   return eFontPrefLang_Cyrillic;
        case kRangeGreek:      return eFontPrefLang_Greek;
        case kRangeHebrew:     return eFontPrefLang_Hebrew;
        case kRangeArabic:     return eFontPrefLang_Arabic;
        case kRangeThai:       return eFontPrefLang_Thai;
        case kRangeKorean:     return eFontPrefLang_Korean;
        case kRangeJapanese:   return eFontPrefLang_Japanese;
        case kRangeSChinese:   return eFontPrefLang_ChineseCN;
        case kRangeTChinese:   return eFontPrefLang_ChineseTW;
        case kRangeDevanagari: return eFontPrefLang_Devanagari;
        case kRangeTamil:      return eFontPrefLang_Tamil;
        case kRangeArmenian:   return eFontPrefLang_Armenian;
        case kRangeBengali:    return eFontPrefLang_Bengali;
        case kRangeCanadian:   return eFontPrefLang_Canadian;
        case kRangeEthiopic:   return eFontPrefLang_Ethiopic;
        case kRangeGeorgian:   return eFontPrefLang_Georgian;
        case kRangeGujarati:   return eFontPrefLang_Gujarati;
        case kRangeGurmukhi:   return eFontPrefLang_Gurmukhi;
        case kRangeKhmer:      return eFontPrefLang_Khmer;
        case kRangeMalayalam:  return eFontPrefLang_Malayalam;
        case kRangeOriya:      return eFontPrefLang_Oriya;
        case kRangeTelugu:     return eFontPrefLang_Telugu;
        case kRangeKannada:    return eFontPrefLang_Kannada;
        case kRangeSinhala:    return eFontPrefLang_Sinhala;
        case kRangeTibetan:    return eFontPrefLang_Tibetan;
        case kRangeSetCJK:     return eFontPrefLang_CJKSet;
        default:               return eFontPrefLang_Others;
    }
}

bool
gfxPlatformFontList::IsLangCJK(eFontPrefLang aLang)
{
    switch (aLang) {
        case eFontPrefLang_Japanese:
        case eFontPrefLang_ChineseTW:
        case eFontPrefLang_ChineseCN:
        case eFontPrefLang_ChineseHK:
        case eFontPrefLang_Korean:
        case eFontPrefLang_CJKSet:
            return true;
        default:
            return false;
    }
}

void
gfxPlatformFontList::GetLangPrefs(eFontPrefLang aPrefLangs[], uint32_t &aLen, eFontPrefLang aCharLang, eFontPrefLang aPageLang)
{
    if (IsLangCJK(aCharLang)) {
        AppendCJKPrefLangs(aPrefLangs, aLen, aCharLang, aPageLang);
    } else {
        AppendPrefLang(aPrefLangs, aLen, aCharLang);
    }

    AppendPrefLang(aPrefLangs, aLen, eFontPrefLang_Others);
}

void
gfxPlatformFontList::AppendCJKPrefLangs(eFontPrefLang aPrefLangs[], uint32_t &aLen, eFontPrefLang aCharLang, eFontPrefLang aPageLang)
{
    // prefer the lang specified by the page *if* CJK
    if (IsLangCJK(aPageLang)) {
        AppendPrefLang(aPrefLangs, aLen, aPageLang);
    }

    // if not set up, set up the default CJK order, based on accept lang settings and locale
    if (mCJKPrefLangs.Length() == 0) {

        // temp array
        eFontPrefLang tempPrefLangs[kMaxLenPrefLangList];
        uint32_t tempLen = 0;

        // Add the CJK pref fonts from accept languages, the order should be same order
        nsAdoptingCString list = Preferences::GetLocalizedCString("intl.accept_languages");
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
                nsAutoCString lang(Substring(start, p));
                lang.CompressWhitespace(false, true);
                eFontPrefLang fpl = gfxPlatformFontList::GetFontPrefLangFor(lang.get());
                switch (fpl) {
                    case eFontPrefLang_Japanese:
                    case eFontPrefLang_Korean:
                    case eFontPrefLang_ChineseCN:
                    case eFontPrefLang_ChineseHK:
                    case eFontPrefLang_ChineseTW:
                        AppendPrefLang(tempPrefLangs, tempLen, fpl);
                        break;
                    default:
                        break;
                }
                p++;
            }
        }

        do { // to allow 'break' to abort this block if a call fails
            nsresult rv;
            nsCOMPtr<nsILocaleService> ls =
                do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
            if (NS_FAILED(rv))
                break;

            nsCOMPtr<nsILocale> appLocale;
            rv = ls->GetApplicationLocale(getter_AddRefs(appLocale));
            if (NS_FAILED(rv))
                break;

            nsString localeStr;
            rv = appLocale->
                GetCategory(NS_LITERAL_STRING(NSILOCALE_MESSAGE), localeStr);
            if (NS_FAILED(rv))
                break;

            const nsAString& lang = Substring(localeStr, 0, 2);
            if (lang.EqualsLiteral("ja")) {
                AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Japanese);
            } else if (lang.EqualsLiteral("zh")) {
                const nsAString& region = Substring(localeStr, 3, 2);
                if (region.EqualsLiteral("CN")) {
                    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseCN);
                } else if (region.EqualsLiteral("TW")) {
                    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseTW);
                } else if (region.EqualsLiteral("HK")) {
                    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseHK);
                }
            } else if (lang.EqualsLiteral("ko")) {
                AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Korean);
            }
        } while (0);

        // last resort... (the order is same as old gfx.)
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Japanese);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Korean);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseCN);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseHK);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseTW);

        // copy into the cached array
        uint32_t j;
        for (j = 0; j < tempLen; j++) {
            mCJKPrefLangs.AppendElement(tempPrefLangs[j]);
        }
    }

    // append in cached CJK langs
    uint32_t  i, numCJKlangs = mCJKPrefLangs.Length();

    for (i = 0; i < numCJKlangs; i++) {
        AppendPrefLang(aPrefLangs, aLen, (eFontPrefLang) (mCJKPrefLangs[i]));
    }

}

void
gfxPlatformFontList::AppendPrefLang(eFontPrefLang aPrefLangs[], uint32_t& aLen, eFontPrefLang aAddLang)
{
    if (aLen >= kMaxLenPrefLangList) return;

    // make sure
    uint32_t  i = 0;
    while (i < aLen && aPrefLangs[i] != aAddLang) {
        i++;
    }

    if (i == aLen) {
        aPrefLangs[aLen] = aAddLang;
        aLen++;
    }
}

mozilla::FontFamilyType
gfxPlatformFontList::GetDefaultGeneric(eFontPrefLang aLang)
{
    // initialize lang group pref font defaults (i.e. serif/sans-serif)
    if (MOZ_UNLIKELY(mDefaultGenericsLangGroup.IsEmpty())) {
        mDefaultGenericsLangGroup.AppendElements(ArrayLength(gPrefLangNames));
        for (uint32_t i = 0; i < ArrayLength(gPrefLangNames); i++) {
            nsAutoCString prefDefaultFontType("font.default.");
            prefDefaultFontType.Append(GetPrefLangName(eFontPrefLang(i)));
            nsAdoptingCString serifOrSans =
                Preferences::GetCString(prefDefaultFontType.get());
            if (serifOrSans.EqualsLiteral("sans-serif")) {
                mDefaultGenericsLangGroup[i] = eFamily_sans_serif;
            } else {
                mDefaultGenericsLangGroup[i] = eFamily_serif;
            }
        }
    }

    if (uint32_t(aLang) < ArrayLength(gPrefLangNames)) {
        return mDefaultGenericsLangGroup[uint32_t(aLang)];
    }
    return eFamily_serif;
}

void
gfxPlatformFontList::GetFontFamilyNames(nsTArray<nsString>& aFontFamilyNames)
{
    for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
        nsRefPtr<gfxFontFamily>& family = iter.Data();
        aFontFamilyNames.AppendElement(family->Name());
    }
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

    if (LOG_FONTINIT_ENABLED()) {
        TimeDuration elapsed = TimeStamp::Now() - start;
        LOG_FONTINIT(("(fontinit) fontloader load pass %8.2f ms done %s\n",
                      elapsed.ToMilliseconds(), (done ? "true" : "false")));
    }

    if (done) {
        mOtherFamilyNamesInitialized = true;
        mFaceNameListsInitialized = true;
    }

    return done;
}

void 
gfxPlatformFontList::CleanupLoader()
{
    mFontFamiliesToLoad.Clear();
    mNumFamilies = 0;
    bool rebuilt = false, forceReflow = false;

    // if had missed face names that are now available, force reflow all
    if (mFaceNamesMissed) {
        for (auto it = mFaceNamesMissed->Iter(); !it.Done(); it.Next()) {
            if (FindFaceName(it.Get()->GetKey())) {
                rebuilt = true;
                RebuildLocalFonts();
                break;
            }
        }
        mFaceNamesMissed = nullptr;
    }

    if (mOtherNamesMissed) {
        for (auto it = mOtherNamesMissed->Iter(); !it.Done(); it.Next()) {
            if (FindFamily(it.Get()->GetKey())) {
                forceReflow = true;
                ForceGlobalReflow();
                break;
            }
        }
        mOtherNamesMissed = nullptr;
    }

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

void
gfxPlatformFontList::RebuildLocalFonts()
{
    for (auto it = mUserFontSetList.Iter(); !it.Done(); it.Next()) {
        it.Get()->GetKey()->RebuildLocalRules();
    }
}

void
gfxPlatformFontList::ClearLangGroupPrefFonts()
{
    for (uint32_t i = eFontPrefLang_First;
         i < eFontPrefLang_First + eFontPrefLang_Count; i++) {
        auto& prefFontsLangGroup = mLangGroupPrefFonts[i];
        for (uint32_t j = eFamily_generic_first;
             j < eFamily_generic_first + eFamily_generic_count; j++) {
            prefFontsLangGroup[j] = nullptr;
        }
    }
}

// Support for memory reporting

// this is also used by subclasses that hold additional font tables
/*static*/ size_t
gfxPlatformFontList::SizeOfFontFamilyTableExcludingThis(
    const FontFamilyTable& aTable,
    MallocSizeOf aMallocSizeOf)
{
    size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
        // We don't count the size of the family here, because this is an
        // *extra* reference to a family that will have already been counted in
        // the main list.
        n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
    return n;
}

/*static*/ size_t
gfxPlatformFontList::SizeOfFontEntryTableExcludingThis(
    const FontEntryTable& aTable,
    MallocSizeOf aMallocSizeOf)
{
    size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
        // The font itself is counted by its owning family; here we only care
        // about the names stored in the hashtable keys.
        n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
    return n;
}

void
gfxPlatformFontList::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                            FontListSizes* aSizes) const
{
    aSizes->mFontListSize +=
        mFontFamilies.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = mFontFamilies.ConstIter(); !iter.Done(); iter.Next()) {
        aSizes->mFontListSize +=
            iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
        iter.Data()->AddSizeOfIncludingThis(aMallocSizeOf, aSizes);
    }

    aSizes->mFontListSize +=
        SizeOfFontFamilyTableExcludingThis(mOtherFamilyNames, aMallocSizeOf);

    if (mExtraNames) {
        aSizes->mFontListSize +=
            SizeOfFontEntryTableExcludingThis(mExtraNames->mFullnames,
                                              aMallocSizeOf);
        aSizes->mFontListSize +=
            SizeOfFontEntryTableExcludingThis(mExtraNames->mPostscriptNames,
                                              aMallocSizeOf);
    }

    for (uint32_t i = eFontPrefLang_First;
         i < eFontPrefLang_First + eFontPrefLang_Count; i++) {
        auto& prefFontsLangGroup = mLangGroupPrefFonts[i];
        for (uint32_t j = eFamily_generic_first;
             j < eFamily_generic_first + eFamily_generic_count; j++) {
            PrefFontList* pf = prefFontsLangGroup[j];
            if (pf) {
                aSizes->mFontListSize +=
                    pf->ShallowSizeOfExcludingThis(aMallocSizeOf);
            }
        }
    }

    aSizes->mFontListSize +=
        mCodepointsWithNoFonts.SizeOfExcludingThis(aMallocSizeOf);
    aSizes->mFontListSize +=
        mFontFamiliesToLoad.ShallowSizeOfExcludingThis(aMallocSizeOf);

    aSizes->mFontListSize +=
        mBadUnderlineFamilyNames.SizeOfExcludingThis(aMallocSizeOf);

    aSizes->mFontListSize +=
        mSharedCmaps.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = mSharedCmaps.ConstIter(); !iter.Done(); iter.Next()) {
        aSizes->mCharMapsSize +=
            iter.Get()->GetKey()->SizeOfIncludingThis(aMallocSizeOf);
    }
}

void
gfxPlatformFontList::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                            FontListSizes* aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}
