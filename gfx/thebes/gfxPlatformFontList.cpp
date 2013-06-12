/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "prlog.h"

#include "gfxPlatformFontList.h"

#include "nsUnicharUtils.h"
#include "nsUnicodeRange.h"
#include "nsUnicodeProperties.h"

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"

using namespace mozilla;

#ifdef PR_LOGGING

#define LOG_FONTLIST(args) PR_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), \
                               PR_LOG_DEBUG, args)
#define LOG_FONTLIST_ENABLED() PR_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontlist), \
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
                                 const PRUnichar *aData)
{
    NS_ASSERTION(!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID), "invalid topic");
    // XXX this could be made to only clear out the cache for the prefs that were changed
    // but it probably isn't that big a deal.
    gfxPlatformFontList::PlatformFontList()->ClearPrefFonts();
    gfxFontCache::GetCache()->AgeAllGenerations();
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(gfxPlatformFontList::MemoryReporter, nsIMemoryMultiReporter)

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(FontListMallocSizeOf)

NS_IMETHODIMP
gfxPlatformFontList::MemoryReporter::GetName(nsACString &aName)
{
    aName.AssignLiteral("font-list");
    return NS_OK;
}

NS_IMETHODIMP
gfxPlatformFontList::MemoryReporter::CollectReports
    (nsIMemoryMultiReporterCallback* aCb,
     nsISupports* aClosure)
{
    FontListSizes sizes;
    sizes.mFontListSize = 0;
    sizes.mFontTableCacheSize = 0;
    sizes.mCharMapsSize = 0;

    gfxPlatformFontList::PlatformFontList()->SizeOfIncludingThis(&FontListMallocSizeOf,
                                                                 &sizes);

    aCb->Callback(EmptyCString(),
                  NS_LITERAL_CSTRING("explicit/gfx/font-list"),
                  nsIMemoryReporter::KIND_HEAP, nsIMemoryReporter::UNITS_BYTES,
                  sizes.mFontListSize,
                  NS_LITERAL_CSTRING("Memory used to manage the list of font families and faces."),
                  aClosure);

    aCb->Callback(EmptyCString(),
                  NS_LITERAL_CSTRING("explicit/gfx/font-charmaps"),
                  nsIMemoryReporter::KIND_HEAP, nsIMemoryReporter::UNITS_BYTES,
                  sizes.mCharMapsSize,
                  NS_LITERAL_CSTRING("Memory used to record the character coverage of individual fonts."),
                  aClosure);

    if (sizes.mFontTableCacheSize) {
        aCb->Callback(EmptyCString(),
                      NS_LITERAL_CSTRING("explicit/gfx/font-tables"),
                      nsIMemoryReporter::KIND_HEAP, nsIMemoryReporter::UNITS_BYTES,
                      sizes.mFontTableCacheSize,
                      NS_LITERAL_CSTRING("Memory used for cached font metrics and layout tables."),
                      aClosure);
    }

    return NS_OK;
}

gfxPlatformFontList::gfxPlatformFontList(bool aNeedFullnamePostscriptNames)
    : mNeedFullnamePostscriptNames(aNeedFullnamePostscriptNames),
      mStartIndex(0), mIncrement(1), mNumFamilies(0)
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

    mSharedCmaps.Init(16);

    NS_RegisterMemoryMultiReporter(new MemoryReporter);
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
    if (mNeedFullnamePostscriptNames) {
        mFullnames.Clear();
        mPostscriptNames.Clear();
    }
    mFaceNamesInitialized = false;
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

void 
gfxPlatformFontList::InitOtherFamilyNames()
{
    mOtherFamilyNamesInitialized = true;

    Telemetry::AutoTimer<Telemetry::FONTLIST_INITOTHERFAMILYNAMES> timer;
    // iterate over all font families and read in other family names
    mFontFamilies.Enumerate(gfxPlatformFontList::InitOtherFamilyNamesProc, this);
}
                                                         
PLDHashOperator
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

PLDHashOperator
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

    if (MOZ_UNLIKELY(log)) {
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
gfxPlatformFontList::FindFamily(const nsAString& aFamily)
{
    nsAutoString key;
    gfxFontFamily *familyEntry;
    GenerateFontListKey(aFamily, key);

    NS_ASSERTION(mFontFamilies.Count() != 0, "system font list was not initialized correctly");

    // lookup in canonical (i.e. English) family name list
    if ((familyEntry = mFontFamilies.GetWeak(key))) {
        return familyEntry;
    }

    // lookup in other family names list (mostly localized names)
    if ((familyEntry = mOtherFamilyNames.GetWeak(key)) != nullptr) {
        return familyEntry;
    }

    // name not found and other family names not yet fully initialized so
    // initialize the rest of the list and try again.  this is done lazily
    // since reading name table entries is expensive.
    // although ASCII localized family names are possible they don't occur
    // in practice so avoid pulling in names at startup
    if (!mOtherFamilyNamesInitialized && !IsASCII(aFamily)) {
        InitOtherFamilyNames();
        if ((familyEntry = mOtherFamilyNames.GetWeak(key)) != nullptr) {
            return familyEntry;
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
    if (!mFullnames.GetWeak(aFullname)) {
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
    if (!mPostscriptNames.GetWeak(aPostscriptName)) {
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
gfxPlatformFontList::InitLoader()
{
    GetFontFamilyList(mFontFamiliesToLoad);
    mStartIndex = 0;
    mNumFamilies = mFontFamiliesToLoad.Length();
}

bool
gfxPlatformFontList::RunLoader()
{
    uint32_t i, endIndex = (mStartIndex + mIncrement < mNumFamilies ? mStartIndex + mIncrement : mNumFamilies);
    bool loadCmaps = !UsesSystemFallback() ||
        gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback();

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

        // load the cmaps if needed
        if (loadCmaps) {
            familyEntry->ReadAllCMAPs();
        }

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

// Support for memory reporting

static size_t
SizeOfFamilyEntryExcludingThis(const nsAString&               aKey,
                               const nsRefPtr<gfxFontFamily>& aFamily,
                               nsMallocSizeOfFun              aMallocSizeOf,
                               void*                          aUserArg)
{
    FontListSizes *sizes = static_cast<FontListSizes*>(aUserArg);
    aFamily->SizeOfExcludingThis(aMallocSizeOf, sizes);

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
     nsMallocSizeOfFun              aMallocSizeOf,
     void*                          aUserArg)
{
    // we don't count the size of the family here, because this is an *extra*
    // reference to a family that will have already been counted in the main list
    return aKey.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

static size_t
SizeOfFontNameEntryExcludingThis(const nsAString&              aKey,
                                 const nsRefPtr<gfxFontEntry>& aFont,
                                 nsMallocSizeOfFun             aMallocSizeOf,
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
     nsMallocSizeOfFun                         aMallocSizeOf,
     void*                                     aUserArg)
{
    // again, we only care about the size of the array itself; we don't follow
    // the refPtrs stored in it, because they point to entries already owned
    // and accounted-for by the main font list
    return aList.SizeOfExcludingThis(aMallocSizeOf);
}

static size_t
SizeOfStringEntryExcludingThis(nsStringHashKey*  aHashEntry,
                               nsMallocSizeOfFun aMallocSizeOf,
                               void*             aUserArg)
{
    return aHashEntry->GetKey().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

static size_t
SizeOfSharedCmapExcludingThis(CharMapHashKey*   aHashEntry,
                              nsMallocSizeOfFun aMallocSizeOf,
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
gfxPlatformFontList::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                         FontListSizes*    aSizes) const
{
    aSizes->mFontListSize +=
        mFontFamilies.SizeOfExcludingThis(SizeOfFamilyEntryExcludingThis,
                                          aMallocSizeOf, aSizes);

    aSizes->mFontListSize +=
        mOtherFamilyNames.SizeOfExcludingThis(SizeOfFamilyNameEntryExcludingThis,
                                              aMallocSizeOf);

    if (mNeedFullnamePostscriptNames) {
        aSizes->mFontListSize +=
            mFullnames.SizeOfExcludingThis(SizeOfFontNameEntryExcludingThis,
                                           aMallocSizeOf);
        aSizes->mFontListSize +=
            mPostscriptNames.SizeOfExcludingThis(SizeOfFontNameEntryExcludingThis,
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
gfxPlatformFontList::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                         FontListSizes*    aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    SizeOfExcludingThis(aMallocSizeOf, aSizes);
}
