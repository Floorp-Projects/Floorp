/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/intl/MozLocale.h"
#include "mozilla/intl/OSPreferences.h"

#include "gfxPlatformFontList.h"
#include "gfxPrefs.h"
#include "gfxTextRun.h"
#include "gfxUserFontSet.h"
#include "SharedFontList-impl.h"

#include "nsCRT.h"
#include "nsGkAtoms.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"
#include "nsXULAppAPI.h"

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessMessageManager.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Unused.h"

#include "base/eintr_wrapper.h"
#include "base/file_util.h"

#include <locale.h>

using namespace mozilla;
using mozilla::intl::Locale;
using mozilla::intl::LocaleService;
using mozilla::intl::OSPreferences;

#define LOG_FONTLIST(args) \
  MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), LogLevel::Debug, args)
#define LOG_FONTLIST_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_fontlist), LogLevel::Debug)
#define LOG_FONTINIT(args) \
  MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontinit), LogLevel::Debug, args)
#define LOG_FONTINIT_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_fontinit), LogLevel::Debug)

gfxPlatformFontList* gfxPlatformFontList::sPlatformFontList = nullptr;

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
    {0x0600, 0x06FF, 1, {TRUETYPE_TAG('a', 'r', 'a', 'b'), 0, 0}},
    {0x0700, 0x074F, 1, {TRUETYPE_TAG('s', 'y', 'r', 'c'), 0, 0}},
    {0x0750, 0x077F, 1, {TRUETYPE_TAG('a', 'r', 'a', 'b'), 0, 0}},
    {0x08A0, 0x08FF, 1, {TRUETYPE_TAG('a', 'r', 'a', 'b'), 0, 0}},
    {0x0900,
     0x097F,
     2,
     {TRUETYPE_TAG('d', 'e', 'v', '2'), TRUETYPE_TAG('d', 'e', 'v', 'a'), 0}},
    {0x0980,
     0x09FF,
     2,
     {TRUETYPE_TAG('b', 'n', 'g', '2'), TRUETYPE_TAG('b', 'e', 'n', 'g'), 0}},
    {0x0A00,
     0x0A7F,
     2,
     {TRUETYPE_TAG('g', 'u', 'r', '2'), TRUETYPE_TAG('g', 'u', 'r', 'u'), 0}},
    {0x0A80,
     0x0AFF,
     2,
     {TRUETYPE_TAG('g', 'j', 'r', '2'), TRUETYPE_TAG('g', 'u', 'j', 'r'), 0}},
    {0x0B00,
     0x0B7F,
     2,
     {TRUETYPE_TAG('o', 'r', 'y', '2'), TRUETYPE_TAG('o', 'r', 'y', 'a'), 0}},
    {0x0B80,
     0x0BFF,
     2,
     {TRUETYPE_TAG('t', 'm', 'l', '2'), TRUETYPE_TAG('t', 'a', 'm', 'l'), 0}},
    {0x0C00,
     0x0C7F,
     2,
     {TRUETYPE_TAG('t', 'e', 'l', '2'), TRUETYPE_TAG('t', 'e', 'l', 'u'), 0}},
    {0x0C80,
     0x0CFF,
     2,
     {TRUETYPE_TAG('k', 'n', 'd', '2'), TRUETYPE_TAG('k', 'n', 'd', 'a'), 0}},
    {0x0D00,
     0x0D7F,
     2,
     {TRUETYPE_TAG('m', 'l', 'm', '2'), TRUETYPE_TAG('m', 'l', 'y', 'm'), 0}},
    {0x0D80, 0x0DFF, 1, {TRUETYPE_TAG('s', 'i', 'n', 'h'), 0, 0}},
    {0x0E80, 0x0EFF, 1, {TRUETYPE_TAG('l', 'a', 'o', ' '), 0, 0}},
    {0x0F00, 0x0FFF, 1, {TRUETYPE_TAG('t', 'i', 'b', 't'), 0, 0}},
    {0x1000,
     0x109f,
     2,
     {TRUETYPE_TAG('m', 'y', 'm', 'r'), TRUETYPE_TAG('m', 'y', 'm', '2'), 0}},
    {0x1780, 0x17ff, 1, {TRUETYPE_TAG('k', 'h', 'm', 'r'), 0, 0}},
    // Khmer Symbols (19e0..19ff) don't seem to need any special shaping
    {0xaa60,
     0xaa7f,
     2,
     {TRUETYPE_TAG('m', 'y', 'm', 'r'), TRUETYPE_TAG('m', 'y', 'm', '2'), 0}},
    // Thai seems to be "renderable" without AAT morphing tables
    {0, 0, 0, {0, 0, 0}}  // terminator
};

// prefs for the font info loader
#define FONT_LOADER_DELAY_PREF "gfx.font_loader.delay"
#define FONT_LOADER_INTERVAL_PREF "gfx.font_loader.interval"

static const char* kObservedPrefs[] = {"font.", "font.name-list.",
                                       "intl.accept_languages",  // hmmmm...
                                       nullptr};

static const char kFontSystemWhitelistPref[] = "font.system.whitelist";

// xxx - this can probably be eliminated by reworking pref font handling code
static const char* gPrefLangNames[] = {
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

static void FontListPrefChanged(const char* aPref, void* aData = nullptr) {
  // XXX this could be made to only clear out the cache for the prefs that were
  // changed but it probably isn't that big a deal.
  gfxPlatformFontList::PlatformFontList()->ClearLangGroupPrefFonts();
  gfxFontCache::GetCache()->AgeAllGenerations();
}

static gfxFontListPrefObserver* gFontListPrefObserver = nullptr;

NS_IMPL_ISUPPORTS(gfxFontListPrefObserver, nsIObserver)

#define LOCALES_CHANGED_TOPIC "intl:system-locales-changed"

NS_IMETHODIMP
gfxFontListPrefObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData) {
  NS_ASSERTION(!strcmp(aTopic, LOCALES_CHANGED_TOPIC), "invalid topic");
  FontListPrefChanged(nullptr);

  if (XRE_IsParentProcess()) {
    gfxPlatform::ForceGlobalReflow();
  }
  return NS_OK;
}

MOZ_DEFINE_MALLOC_SIZE_OF(FontListMallocSizeOf)

NS_IMPL_ISUPPORTS(gfxPlatformFontList::MemoryReporter, nsIMemoryReporter)

NS_IMETHODIMP
gfxPlatformFontList::MemoryReporter::CollectReports(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData,
    bool aAnonymize) {
  FontListSizes sizes;
  sizes.mFontListSize = 0;
  sizes.mFontTableCacheSize = 0;
  sizes.mCharMapsSize = 0;
  sizes.mLoaderSize = 0;
  sizes.mSharedSize = 0;

  gfxPlatformFontList::PlatformFontList()->AddSizeOfIncludingThis(
      &FontListMallocSizeOf, &sizes);

  MOZ_COLLECT_REPORT(
      "explicit/gfx/font-list", KIND_HEAP, UNITS_BYTES, sizes.mFontListSize,
      "Memory used to manage the list of font families and faces.");

  MOZ_COLLECT_REPORT(
      "explicit/gfx/font-charmaps", KIND_HEAP, UNITS_BYTES, sizes.mCharMapsSize,
      "Memory used to record the character coverage of individual fonts.");

  if (sizes.mFontTableCacheSize) {
    MOZ_COLLECT_REPORT(
        "explicit/gfx/font-tables", KIND_HEAP, UNITS_BYTES,
        sizes.mFontTableCacheSize,
        "Memory used for cached font metrics and layout tables.");
  }

  if (sizes.mLoaderSize) {
    MOZ_COLLECT_REPORT("explicit/gfx/font-loader", KIND_HEAP, UNITS_BYTES,
                       sizes.mLoaderSize,
                       "Memory used for (platform-specific) font loader.");
  }

  if (sizes.mSharedSize) {
    MOZ_COLLECT_REPORT(
        "font-list-shmem", KIND_NONHEAP, UNITS_BYTES, sizes.mSharedSize,
        "Shared memory for system font list and character coverage data.");
  }

  return NS_OK;
}

gfxPlatformFontList::gfxPlatformFontList(bool aNeedFullnamePostscriptNames)
    : mFontFamiliesMutex("gfxPlatformFontList::mFontFamiliesMutex"),
      mFontFamilies(64),
      mOtherFamilyNames(16),
      mSharedCmaps(8),
      mStartIndex(0),
      mNumFamilies(0),
      mFontlistInitCount(0),
      mFontFamilyWhitelistActive(false) {
  mOtherFamilyNamesInitialized = false;

  if (aNeedFullnamePostscriptNames) {
    mExtraNames = MakeUnique<ExtraNames>();
  }
  mFaceNameListsInitialized = false;

  mLangService = nsLanguageAtomService::GetService();

  LoadBadUnderlineList();

  // pref changes notification setup
  NS_ASSERTION(!gFontListPrefObserver,
               "There has been font list pref observer already");
  gFontListPrefObserver = new gfxFontListPrefObserver();
  NS_ADDREF(gFontListPrefObserver);

  Preferences::RegisterPrefixCallbacks(FontListPrefChanged, kObservedPrefs);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->AddObserver(gFontListPrefObserver, LOCALES_CHANGED_TOPIC, false);
  }

  // Only the parent process listens for whitelist changes; it will then
  // notify its children to rebuild their font lists.
  if (XRE_IsParentProcess()) {
    Preferences::RegisterCallback(FontWhitelistPrefChanged,
                                  kFontSystemWhitelistPref);
  }

  RegisterStrongMemoryReporter(new MemoryReporter());
}

gfxPlatformFontList::~gfxPlatformFontList() {
  mSharedCmaps.Clear();
  ClearLangGroupPrefFonts();
  NS_ASSERTION(gFontListPrefObserver, "There is no font list pref observer");

  Preferences::UnregisterPrefixCallbacks(FontListPrefChanged, kObservedPrefs);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(gFontListPrefObserver, LOCALES_CHANGED_TOPIC);
  }

  if (XRE_IsParentProcess()) {
    Preferences::UnregisterCallback(FontWhitelistPrefChanged,
                                    kFontSystemWhitelistPref);
  }
  NS_RELEASE(gFontListPrefObserver);
}

/* static */
void gfxPlatformFontList::FontWhitelistPrefChanged(const char* aPref,
                                                   void* aClosure) {
  MOZ_ASSERT(XRE_IsParentProcess());
  gfxPlatformFontList::PlatformFontList()->UpdateFontList();
  mozilla::dom::ContentParent::NotifyUpdatedFonts();
}

// number of CSS generic font families
const uint32_t kNumGenerics = 5;

void gfxPlatformFontList::ApplyWhitelist() {
  nsTArray<nsCString> list;
  gfxFontUtils::GetPrefsFontList(kFontSystemWhitelistPref, list);
  uint32_t numFonts = list.Length();
  mFontFamilyWhitelistActive = (numFonts > 0);
  if (!mFontFamilyWhitelistActive) {
    return;
  }
  nsTHashtable<nsCStringHashKey> familyNamesWhitelist;
  for (uint32_t i = 0; i < numFonts; i++) {
    nsAutoCString key;
    ToLowerCase(list[i], key);
    familyNamesWhitelist.PutEntry(key);
  }
  for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
    // Don't continue if we only have one font left.
    if (mFontFamilies.Count() == 1) {
      break;
    }
    nsAutoCString fontFamilyName(iter.Key());
    ToLowerCase(fontFamilyName);
    if (!familyNamesWhitelist.Contains(fontFamilyName)) {
      iter.Remove();
    }
  }
}

void gfxPlatformFontList::ApplyWhitelist(
    nsTArray<fontlist::Family::InitData>& aFamilies) {
  nsTArray<nsCString> list;
  gfxFontUtils::GetPrefsFontList(kFontSystemWhitelistPref, list);
  mFontFamilyWhitelistActive = !list.IsEmpty();
  if (!mFontFamilyWhitelistActive) {
    return;
  }
  nsTHashtable<nsCStringHashKey> familyNamesWhitelist;
  for (const auto& item : list) {
    nsAutoCString key;
    ToLowerCase(item, key);
    familyNamesWhitelist.PutEntry(key);
  }
  int count = int(aFamilies.Length());
  // Find the first non-hidden family; we won't delete this, if no other
  // non-hidden family has been kept.
  int firstNonHidden = 0;
  while (firstNonHidden < count && aFamilies[firstNonHidden].mHidden) {
    ++firstNonHidden;
  }
  bool keptNonHidden = false;
  for (int i = count - 1; i >= firstNonHidden; --i) {
    if (aFamilies[i].mHidden) {
      continue;
    }
    if (familyNamesWhitelist.Contains(aFamilies[i].mKey)) {
      keptNonHidden = true;
    } else if (keptNonHidden || i > firstNonHidden) {
      aFamilies.RemoveElementAt(i);
    }
  }
}

bool gfxPlatformFontList::AddWithLegacyFamilyName(const nsACString& aLegacyName,
                                                  gfxFontEntry* aFontEntry) {
  bool added = false;
  nsAutoCString key;
  ToLowerCase(aLegacyName, key);
  gfxFontFamily* family = mOtherFamilyNames.GetWeak(key);
  if (!family) {
    family = CreateFontFamily(aLegacyName);
    family->SetHasStyles(true);  // we don't want the family to search for
                                 // faces, we're adding them directly here
    mOtherFamilyNames.Put(key, family);
    added = true;
  }
  family->AddFontEntry(aFontEntry->Clone());
  return added;
}

nsresult gfxPlatformFontList::InitFontList() {
  mFontlistInitCount++;

  if (LOG_FONTINIT_ENABLED()) {
    LOG_FONTINIT(("(fontinit) system fontlist initialization\n"));
  }

  // rebuilding fontlist so clear out font/word caches
  gfxFontCache* fontCache = gfxFontCache::GetCache();
  if (fontCache) {
    fontCache->AgeAllGenerations();
    fontCache->FlushShapedWordCaches();
  }

  gfxPlatform::PurgeSkiaFontCache();

  CancelInitOtherFamilyNamesTask();
  MutexAutoLock lock(mFontFamiliesMutex);
  mFontFamilies.Clear();
  mOtherFamilyNames.Clear();
  mOtherFamilyNamesInitialized = false;

  if (mExtraNames) {
    mExtraNames->mFullnames.Clear();
    mExtraNames->mPostscriptNames.Clear();
  }
  mFaceNameListsInitialized = false;
  ClearLangGroupPrefFonts();
  CancelLoader();

  // initialize ranges of characters for which system-wide font search should be
  // skipped
  mCodepointsWithNoFonts.reset();
  mCodepointsWithNoFonts.SetRange(0, 0x1f);     // C0 controls
  mCodepointsWithNoFonts.SetRange(0x7f, 0x9f);  // C1 controls

  sPlatformFontList = this;

  // Try to initialize the cross-process shared font list if enabled by prefs,
  // but not if we're running in Safe Mode.
  if (gfxPrefs::SharedFontList() && !gfxPlatform::InSafeMode()) {
    for (auto i = mFontEntries.Iter(); !i.Done(); i.Next()) {
      i.Data()->mShmemCharacterMap = nullptr;
      i.Data()->mShmemFace = nullptr;
      i.Data()->mFamilyName = NS_LITERAL_CSTRING("");
    }
    mFontEntries.Clear();
    mShmemCharMaps.Clear();
    bool oldSharedList = mSharedFontList != nullptr;
    mSharedFontList.reset(new fontlist::FontList(mFontlistInitCount));
    InitSharedFontListForPlatform();
    if (mSharedFontList->Initialized()) {
      if (mLocalNameTable.Count()) {
        SharedFontList()->SetLocalNames(mLocalNameTable);
        mLocalNameTable.Clear();
      }
    } else {
      // something went wrong, fall back to in-process list
      mSharedFontList.reset(nullptr);
    }
    if (oldSharedList) {
      if (XRE_IsParentProcess()) {
        // notify all children of the change
        mozilla::dom::ContentParent::NotifyRebuildFontList();
      }
    }
  }

  if (!SharedFontList()) {
    nsresult rv = InitFontListForPlatform();
    if (NS_FAILED(rv)) {
      return rv;
    }
    ApplyWhitelist();
  }

  return NS_OK;
}

void gfxPlatformFontList::FontListChanged() {
  MOZ_ASSERT(!XRE_IsParentProcess());
  if (SharedFontList() && SharedFontList()->NumLocalFaces()) {
    // If we're using a shared local face-name list, this may have changed.
    RebuildLocalFonts();
  }
  ForceGlobalReflow();
}

void gfxPlatformFontList::GenerateFontListKey(const nsACString& aKeyName,
                                              nsACString& aResult) {
  aResult = aKeyName;
  ToLowerCase(aResult);
}

#define OTHERNAMES_TIMEOUT 200

void gfxPlatformFontList::InitOtherFamilyNames(
    bool aDeferOtherFamilyNamesLoading) {
  if (mOtherFamilyNamesInitialized) {
    return;
  }

  if (SharedFontList() && !XRE_IsParentProcess()) {
    dom::ContentChild::GetSingleton()->SendInitOtherFamilyNames(
        SharedFontList()->GetGeneration(), aDeferOtherFamilyNamesLoading,
        &mOtherFamilyNamesInitialized);
    return;
  }

  // If the font loader delay has been set to zero, we don't defer loading
  // additional family names (regardless of the aDefer... parameter), as we
  // take this to mean availability of font info is to be prioritized over
  // potential startup perf or main-thread jank.
  // (This is used so we can reliably run reftests that depend on localized
  // font-family names being available.)
  if (aDeferOtherFamilyNamesLoading &&
      Preferences::GetUint(FONT_LOADER_DELAY_PREF) > 0) {
    if (!mPendingOtherFamilyNameTask) {
      RefPtr<mozilla::CancelableRunnable> task =
          new InitOtherFamilyNamesRunnable();
      mPendingOtherFamilyNameTask = task;
      NS_DispatchToMainThreadQueue(task.forget(), EventQueuePriority::Idle);
    }
  } else {
    InitOtherFamilyNamesInternal(false);
  }
}

// time limit for loading facename lists (ms)
#define NAMELIST_TIMEOUT 200

gfxFontEntry* gfxPlatformFontList::SearchFamiliesForFaceName(
    const nsACString& aFaceName) {
  TimeStamp start = TimeStamp::Now();
  bool timedOut = false;
  // if mFirstChar is not 0, only load facenames for families
  // that start with this character
  char16_t firstChar = 0;
  gfxFontEntry* lookup = nullptr;

  // iterate over familes starting with the same letter
  firstChar = ToLowerCase(aFaceName.CharAt(0));

  for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
    nsCStringHashKey::KeyType key = iter.Key();
    RefPtr<gfxFontFamily>& family = iter.Data();

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
  Telemetry::AccumulateTimeDelta(Telemetry::FONTLIST_INITFACENAMELISTS, start,
                                 end);
  if (LOG_FONTINIT_ENABLED()) {
    TimeDuration elapsed = end - start;
    LOG_FONTINIT(("(fontinit) SearchFamiliesForFaceName took %8.2f ms %s %s",
                  elapsed.ToMilliseconds(), (lookup ? "found name" : ""),
                  (timedOut ? "timeout" : "")));
  }

  return lookup;
}

gfxFontEntry* gfxPlatformFontList::FindFaceName(const nsACString& aFaceName) {
  gfxFontEntry* lookup;

  // lookup in name lookup tables, return null if not found
  if (mExtraNames &&
      ((lookup = mExtraNames->mPostscriptNames.GetWeak(aFaceName)) ||
       (lookup = mExtraNames->mFullnames.GetWeak(aFaceName)))) {
    return lookup;
  }

  return nullptr;
}

gfxFontEntry* gfxPlatformFontList::LookupInFaceNameLists(
    const nsACString& aFaceName) {
  gfxFontEntry* lookup = nullptr;

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
        mFaceNamesMissed = MakeUnique<nsTHashtable<nsCStringHashKey>>(2);
      }
      mFaceNamesMissed->PutEntry(aFaceName);
    }
  }

  return lookup;
}

gfxFontEntry* gfxPlatformFontList::LookupInSharedFaceNameList(
    const nsACString& aFaceName, WeightRange aWeightForEntry,
    StretchRange aStretchForEntry, SlantStyleRange aStyleForEntry) {
  nsAutoCString keyName(aFaceName);
  ToLowerCase(keyName);
  fontlist::FontList* list = SharedFontList();
  fontlist::Family* family = nullptr;
  fontlist::Face* face = nullptr;
  if (list->NumLocalFaces()) {
    fontlist::LocalFaceRec* rec = list->FindLocalFace(keyName);
    if (rec) {
      family = &list->Families()[rec->mFamilyIndex];
      face = static_cast<fontlist::Face*>(
          family->Faces(list)[rec->mFaceIndex].ToPtr(list));
    }
  } else {
    list->SearchForLocalFace(keyName, &family, &face);
  }
  if (!face || !family) {
    return nullptr;
  }
  gfxFontEntry* fe = CreateFontEntry(face, family);
  if (fe) {
    fe->mIsLocalUserFont = true;
    fe->mWeightRange = aWeightForEntry;
    fe->mStretchRange = aStretchForEntry;
    fe->mStyleRange = aStyleForEntry;
  }
  return fe;
}

void gfxPlatformFontList::PreloadNamesList() {
  AutoTArray<nsCString, 10> preloadFonts;
  gfxFontUtils::GetPrefsFontList("font.preload-names-list", preloadFonts);

  uint32_t numFonts = preloadFonts.Length();
  for (uint32_t i = 0; i < numFonts; i++) {
    nsAutoCString key;
    GenerateFontListKey(preloadFonts[i], key);

    // only search canonical names!
    gfxFontFamily* familyEntry = mFontFamilies.GetWeak(key);
    if (familyEntry) {
      familyEntry->ReadOtherFamilyNames(this);
    }
  }
}

void gfxPlatformFontList::LoadBadUnderlineList() {
  gfxFontUtils::GetPrefsFontList("font.blacklist.underline_offset",
                                 mBadUnderlineFamilyNames);
  for (auto& fam : mBadUnderlineFamilyNames) {
    ToLowerCase(fam);
  }
  mBadUnderlineFamilyNames.Compact();
  mBadUnderlineFamilyNames.Sort();
}

void gfxPlatformFontList::UpdateFontList() {
  MOZ_ASSERT(NS_IsMainThread());
  InitFontList();
  RebuildLocalFonts();
}

void gfxPlatformFontList::GetFontList(nsAtom* aLangGroup,
                                      const nsACString& aGenericFamily,
                                      nsTArray<nsString>& aListOfFonts) {
  if (SharedFontList()) {
    fontlist::FontList* list = SharedFontList();
    const fontlist::Family* families = list->Families();
    for (uint32_t i = 0; i < list->NumFamilies(); i++) {
      auto& f = families[i];
      if (f.IsHidden()) {
        continue;
      }
      // XXX TODO: filter families for aGenericFamily, if supported by platform
      aListOfFonts.AppendElement(
          NS_ConvertUTF8toUTF16(f.DisplayName().AsString(list)));
    }
    return;
  }

  MutexAutoLock lock(mFontFamiliesMutex);
  for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<gfxFontFamily>& family = iter.Data();
    if (family->FilterForFontList(aLangGroup, aGenericFamily)) {
      nsAutoCString localizedFamilyName;
      family->LocalizedName(localizedFamilyName);
      aListOfFonts.AppendElement(NS_ConvertUTF8toUTF16(localizedFamilyName));
    }
  }

  aListOfFonts.Sort();
  aListOfFonts.Compact();
}

void gfxPlatformFontList::GetFontFamilyList(
    nsTArray<RefPtr<gfxFontFamily>>& aFamilyArray) {
  for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<gfxFontFamily>& family = iter.Data();
    aFamilyArray.AppendElement(family);
  }
}

gfxFontEntry* gfxPlatformFontList::SystemFindFontForChar(
    uint32_t aCh, uint32_t aNextCh, Script aRunScript,
    const gfxFontStyle* aStyle) {
  gfxFontEntry* fontEntry = nullptr;

  // is codepoint with no matching font? return null immediately
  if (mCodepointsWithNoFonts.test(aCh)) {
    return nullptr;
  }

  // Try to short-circuit font fallback for U+FFFD, used to represent
  // encoding errors: just use cached family from last time U+FFFD was seen.
  // This helps speed up pages with lots of encoding errors, binary-as-text,
  // etc.
  if (aCh == 0xFFFD) {
    if (mReplacementCharFallbackFamily.mIsShared &&
        mReplacementCharFallbackFamily.mShared) {
      fontlist::Face* face =
          mReplacementCharFallbackFamily.mShared->FindFaceForStyle(
              SharedFontList(), *aStyle);
      if (face) {
        fontEntry =
            GetOrCreateFontEntry(face, mReplacementCharFallbackFamily.mShared);
      }
    } else if (!mReplacementCharFallbackFamily.mIsShared &&
               mReplacementCharFallbackFamily.mUnshared) {
      fontEntry =
          mReplacementCharFallbackFamily.mUnshared->FindFontForStyle(*aStyle);
    }

    // this should never fail, as we must have found U+FFFD in order to set
    // mReplacementCharFallbackFamily at all, but better play it safe
    if (fontEntry && fontEntry->HasCharacter(aCh)) {
      return fontEntry;
    }
  }

  TimeStamp start = TimeStamp::Now();

  // search commonly available fonts
  bool common = true;
  FontFamily fallbackFamily;
  fontEntry =
      CommonFontFallback(aCh, aNextCh, aRunScript, aStyle, &fallbackFamily);

  // if didn't find a font, do system-wide fallback (except for specials)
  uint32_t cmapCount = 0;
  if (!fontEntry) {
    common = false;
    fontEntry =
        GlobalFontFallback(aCh, aRunScript, aStyle, cmapCount, &fallbackFamily);
  }
  TimeDuration elapsed = TimeStamp::Now() - start;

  LogModule* log = gfxPlatform::GetLog(eGfxLog_textrun);

  if (MOZ_UNLIKELY(MOZ_LOG_TEST(log, LogLevel::Warning))) {
    Script script = mozilla::unicode::GetScriptCode(aCh);
    MOZ_LOG(log, LogLevel::Warning,
            ("(textrun-systemfallback-%s) char: u+%6.6x "
             "script: %d match: [%s]"
             " time: %dus cmaps: %d\n",
             (common ? "common" : "global"), aCh, static_cast<int>(script),
             (fontEntry ? fontEntry->Name().get() : "<none>"),
             int32_t(elapsed.ToMicroseconds()), cmapCount));
  }

  // no match? add to set of non-matching codepoints
  if (!fontEntry) {
    mCodepointsWithNoFonts.set(aCh);
  } else if (aCh == 0xFFFD && fontEntry) {
    mReplacementCharFallbackFamily = fallbackFamily;
  }

  // track system fallback time
  static bool first = true;
  int32_t intElapsed =
      int32_t(first ? elapsed.ToMilliseconds() : elapsed.ToMicroseconds());
  Telemetry::Accumulate((first ? Telemetry::SYSTEM_FONT_FALLBACK_FIRST
                               : Telemetry::SYSTEM_FONT_FALLBACK),
                        intElapsed);
  first = false;

  // track the script for which fallback occurred (incremented one make it
  // 1-based)
  Telemetry::Accumulate(Telemetry::SYSTEM_FONT_FALLBACK_SCRIPT,
                        int(aRunScript) + 1);

  return fontEntry;
}

#define NUM_FALLBACK_FONTS 8

gfxFontEntry* gfxPlatformFontList::CommonFontFallback(
    uint32_t aCh, uint32_t aNextCh, Script aRunScript,
    const gfxFontStyle* aMatchStyle, FontFamily* aMatchedFamily) {
  AutoTArray<const char*, NUM_FALLBACK_FONTS> defaultFallbacks;
  gfxPlatform::GetPlatform()->GetCommonFallbackFonts(aCh, aNextCh, aRunScript,
                                                     defaultFallbacks);
  GlobalFontMatch data(aCh, *aMatchStyle);
  if (SharedFontList()) {
    for (const auto name : defaultFallbacks) {
      fontlist::Family* family = FindSharedFamily(nsDependentCString(name));
      if (!family) {
        continue;
      }
      if (family->IsHidden()) {
        continue;
      }
      family->SearchAllFontsForChar(SharedFontList(), &data);
      if (data.mBestMatch) {
        *aMatchedFamily = FontFamily(family);
        return data.mBestMatch;
      }
    }
  } else {
    for (const auto name : defaultFallbacks) {
      gfxFontFamily* fallback =
          FindFamilyByCanonicalName(nsDependentCString(name));
      if (fallback) {
        fallback->FindFontForChar(&data);
        if (data.mBestMatch) {
          *aMatchedFamily = FontFamily(fallback);
          return data.mBestMatch;
        }
      }
    }
  }
  return nullptr;
}

gfxFontEntry* gfxPlatformFontList::GlobalFontFallback(
    const uint32_t aCh, Script aRunScript, const gfxFontStyle* aMatchStyle,
    uint32_t& aCmapCount, FontFamily* aMatchedFamily) {
  bool useCmaps = IsFontFamilyWhitelistActive() ||
                  gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback();
  if (!useCmaps) {
    // Allow platform-specific fallback code to try and find a usable font
    gfxFontEntry* fe = PlatformGlobalFontFallback(aCh, aRunScript, aMatchStyle,
                                                  aMatchedFamily);
    if (fe) {
      return fe;
    }
  }

  // otherwise, try to find it among local fonts
  GlobalFontMatch data(aCh, *aMatchStyle);
  if (SharedFontList()) {
    fontlist::Family* families = SharedFontList()->Families();
    for (uint32_t i = 0; i < SharedFontList()->NumFamilies(); i++) {
      fontlist::Family& family = families[i];
      if (family.IsHidden()) {
        continue;
      }
      family.SearchAllFontsForChar(SharedFontList(), &data);
      if (data.mMatchDistance == 0.0) {
        // no better style match is possible, so stop searching
        break;
      }
    }
    if (data.mBestMatch) {
      *aMatchedFamily = FontFamily(data.mMatchedSharedFamily);
      return data.mBestMatch;
    }
  } else {
    // iterate over all font families to find a font that support the
    // character
    for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
      RefPtr<gfxFontFamily>& family = iter.Data();
      // evaluate all fonts in this family for a match
      family->FindFontForChar(&data);
      if (data.mMatchDistance == 0.0) {
        // no better style match is possible, so stop searching
        break;
      }
    }

    aCmapCount = data.mCmapsTested;
    if (data.mBestMatch) {
      *aMatchedFamily = FontFamily(data.mMatchedFamily);
      return data.mBestMatch;
    }
  }

  return nullptr;
}

gfxFontFamily* gfxPlatformFontList::CheckFamily(gfxFontFamily* aFamily) {
  if (aFamily && !aFamily->HasStyles()) {
    aFamily->FindStyleVariations();
    aFamily->CheckForSimpleFamily();
  }

  if (aFamily && aFamily->GetFontList().Length() == 0) {
    // failed to load any faces for this family, so discard it
    nsAutoCString key;
    GenerateFontListKey(aFamily->Name(), key);
    mFontFamilies.Remove(key);
    return nullptr;
  }

  return aFamily;
}

bool gfxPlatformFontList::FindAndAddFamilies(
    StyleGenericFontFamily aGeneric, const nsACString& aFamily,
    nsTArray<FamilyAndGeneric>* aOutput, FindFamiliesFlags aFlags,
    gfxFontStyle* aStyle, gfxFloat aDevToCssSize) {
  nsAutoCString key;
  GenerateFontListKey(aFamily, key);

  if (SharedFontList()) {
    fontlist::Family* family = SharedFontList()->FindFamily(key);
    if (family) {
      aOutput->AppendElement(FamilyAndGeneric(family, aGeneric));
      return true;
    }
    // If not found, and other family names have not yet been initialized,
    // initialize the rest of the list and try again. This is done lazily
    // since reading name table entries is expensive.
    // Although ASCII localized family names are possible they don't occur
    // in practice, so avoid pulling in names at startup.
    if (!mOtherFamilyNamesInitialized && !IsASCII(aFamily)) {
      InitOtherFamilyNames(
          !(aFlags & FindFamiliesFlags::eForceOtherFamilyNamesLoading));
      family = SharedFontList()->FindFamily(key);
      if (family) {
        aOutput->AppendElement(FamilyAndGeneric(family, aGeneric));
        return true;
      }
      if (!family && !mOtherFamilyNamesInitialized &&
          !(aFlags & FindFamiliesFlags::eNoAddToNamesMissedWhenSearching)) {
        // localized family names load timed out, add name to list of
        // names to check after localized names are loaded
        if (!mOtherNamesMissed) {
          mOtherNamesMissed = MakeUnique<nsTHashtable<nsCStringHashKey>>(2);
        }
        mOtherNamesMissed->PutEntry(key);
      }
    }
    return false;
  }

  NS_ASSERTION(mFontFamilies.Count() != 0,
               "system font list was not initialized correctly");

  // lookup in canonical (i.e. English) family name list
  gfxFontFamily* familyEntry = mFontFamilies.GetWeak(key);

  // if not found, lookup in other family names list (mostly localized names)
  if (!familyEntry) {
    familyEntry = mOtherFamilyNames.GetWeak(key);
  }

  // if still not found and other family names not yet fully initialized,
  // initialize the rest of the list and try again.  this is done lazily
  // since reading name table entries is expensive.
  // although ASCII localized family names are possible they don't occur
  // in practice so avoid pulling in names at startup
  if (!familyEntry && !mOtherFamilyNamesInitialized && !IsASCII(aFamily)) {
    InitOtherFamilyNames(
        !(aFlags & FindFamiliesFlags::eForceOtherFamilyNamesLoading));
    familyEntry = mOtherFamilyNames.GetWeak(key);
    if (!familyEntry && !mOtherFamilyNamesInitialized &&
        !(aFlags & FindFamiliesFlags::eNoAddToNamesMissedWhenSearching)) {
      // localized family names load timed out, add name to list of
      // names to check after localized names are loaded
      if (!mOtherNamesMissed) {
        mOtherNamesMissed = MakeUnique<nsTHashtable<nsCStringHashKey>>(2);
      }
      mOtherNamesMissed->PutEntry(key);
    }
  }

  familyEntry = CheckFamily(familyEntry);

  // If we failed to find the requested family, check for a space in the
  // name; if found, and if the "base" name (up to the last space) exists
  // as a family, then this might be a legacy GDI-style family name for
  // an additional weight/width. Try searching the faces of the base family
  // and create any corresponding legacy families.
  if (!familyEntry &&
      !(aFlags & FindFamiliesFlags::eNoSearchForLegacyFamilyNames)) {
    // We don't have nsAString::RFindChar, so look for a space manually
    const char* data = aFamily.BeginReading();
    int32_t index = aFamily.Length();
    while (--index > 0) {
      if (data[index] == ' ') {
        break;
      }
    }
    if (index > 0) {
      gfxFontFamily* base =
          FindUnsharedFamily(Substring(aFamily, 0, index),
                             FindFamiliesFlags::eNoSearchForLegacyFamilyNames);
      // If we found the "base" family name, and if it has members with
      // legacy names, this will add corresponding font-family entries to
      // the mOtherFamilyNames list; then retry the legacy-family search.
      if (base && base->CheckForLegacyFamilyNames(this)) {
        familyEntry = mOtherFamilyNames.GetWeak(key);
      }
    }
  }

  if (familyEntry) {
    aOutput->AppendElement(FamilyAndGeneric(familyEntry, aGeneric));
    return true;
  }

  return false;
}

fontlist::Family* gfxPlatformFontList::FindSharedFamily(
    const nsACString& aFamily, FindFamiliesFlags aFlags, gfxFontStyle* aStyle,
    gfxFloat aDevToCss) {
  if (!SharedFontList()) {
    return nullptr;
  }
  AutoTArray<FamilyAndGeneric, 1> families;
  if (!FindAndAddFamilies(StyleGenericFontFamily::None, aFamily, &families,
                          aFlags, aStyle, aDevToCss) ||
      !families[0].mFamily.mIsShared) {
    return nullptr;
  }
  fontlist::Family* family = families[0].mFamily.mShared;
  if (!family->IsInitialized()) {
    if (!InitializeFamily(family)) {
      return nullptr;
    }
  }
  return family;
}

bool gfxPlatformFontList::InitializeFamily(fontlist::Family* aFamily) {
  MOZ_ASSERT(SharedFontList());
  auto list = SharedFontList();
  if (!XRE_IsParentProcess()) {
    uint32_t index = aFamily - list->Families();
    MOZ_ASSERT(index < list->NumFamilies());
    dom::ContentChild::GetSingleton()->SendInitializeFamily(
        list->GetGeneration(), index);
    return aFamily->IsInitialized();
  }
  AutoTArray<fontlist::Face::InitData, 16> faceList;
  GetFacesInitDataForFamily(aFamily, faceList);
  aFamily->AddFaces(list, faceList);
  return aFamily->IsInitialized();
}

gfxFontEntry* gfxPlatformFontList::FindFontForFamily(
    const nsACString& aFamily, const gfxFontStyle* aStyle) {
  nsAutoCString key;
  GenerateFontListKey(aFamily, key);
  FontFamily family = FindFamily(key);
  if (family.IsNull()) {
    return nullptr;
  }
  if (family.mIsShared) {
    auto face = family.mShared->FindFaceForStyle(SharedFontList(), *aStyle);
    if (!face) {
      return nullptr;
    }
    return GetOrCreateFontEntry(face, family.mShared);
  }
  return family.mUnshared->FindFontForStyle(*aStyle);
}

gfxFontEntry* gfxPlatformFontList::GetOrCreateFontEntry(
    fontlist::Face* aFace, const fontlist::Family* aFamily) {
  gfxFontEntry* fe = mFontEntries.GetWeak(aFace);
  if (!fe) {
    fe = CreateFontEntry(aFace, aFamily);
    mFontEntries.Put(aFace, fe);
  }
  return fe;
}

void gfxPlatformFontList::AddOtherFamilyName(gfxFontFamily* aFamilyEntry,
                                             nsCString& aOtherFamilyName) {
  nsAutoCString key;
  GenerateFontListKey(aOtherFamilyName, key);

  if (!mOtherFamilyNames.GetWeak(key)) {
    mOtherFamilyNames.Put(key, aFamilyEntry);
    LOG_FONTLIST(
        ("(fontlist-otherfamily) canonical family: %s, "
         "other family: %s\n",
         aFamilyEntry->Name().get(), aOtherFamilyName.get()));
    if (mBadUnderlineFamilyNames.ContainsSorted(key)) {
      aFamilyEntry->SetBadUnderlineFamily();
    }
  }
}

void gfxPlatformFontList::AddFullname(gfxFontEntry* aFontEntry,
                                      const nsCString& aFullname) {
  if (!mExtraNames->mFullnames.GetWeak(aFullname)) {
    mExtraNames->mFullnames.Put(aFullname, aFontEntry);
    LOG_FONTLIST(("(fontlist-fullname) name: %s, fullname: %s\n",
                  aFontEntry->Name().get(), aFullname.get()));
  }
}

void gfxPlatformFontList::AddPostscriptName(gfxFontEntry* aFontEntry,
                                            const nsCString& aPostscriptName) {
  if (!mExtraNames->mPostscriptNames.GetWeak(aPostscriptName)) {
    mExtraNames->mPostscriptNames.Put(aPostscriptName, aFontEntry);
    LOG_FONTLIST(("(fontlist-postscript) name: %s, psname: %s\n",
                  aFontEntry->Name().get(), aPostscriptName.get()));
  }
}

bool gfxPlatformFontList::GetStandardFamilyName(const nsCString& aFontName,
                                                nsACString& aFamilyName) {
  FontFamily family = FindFamily(aFontName);
  if (family.IsNull()) {
    return false;
  }
  if (family.mIsShared) {
    aFamilyName = family.mShared->DisplayName().AsString(SharedFontList());
    return true;
  }
  family.mUnshared->LocalizedName(aFamilyName);
  return true;
}

FamilyAndGeneric gfxPlatformFontList::GetDefaultFontFamily(
    const nsACString& aLangGroup, const nsACString& aGenericFamily) {
  if (NS_WARN_IF(aLangGroup.IsEmpty()) ||
      NS_WARN_IF(aGenericFamily.IsEmpty())) {
    return FamilyAndGeneric();
  }

  AutoTArray<nsCString, 4> names;
  gfxFontUtils::AppendPrefsFontList(
      NameListPref(aGenericFamily, aLangGroup).get(), names);

  for (const nsCString& name : names) {
    FontFamily family = FindFamily(name);
    if (!family.IsNull()) {
      return FamilyAndGeneric(family);
    }
  }

  return FamilyAndGeneric();
}

ShmemCharMapHashEntry::ShmemCharMapHashEntry(const gfxSparseBitSet* aCharMap)
    : mList(gfxPlatformFontList::PlatformFontList()->SharedFontList()),
      mCharMap(),
      mHash(aCharMap->GetChecksum()) {
  size_t len = SharedBitSet::RequiredSize(*aCharMap);
  mCharMap = mList->Alloc(len);
  SharedBitSet::Create(mCharMap.ToPtr(mList), len, *aCharMap);
}

fontlist::Pointer gfxPlatformFontList::GetShmemCharMap(
    const gfxSparseBitSet* aCmap) {
  auto* entry = mShmemCharMaps.GetEntry(aCmap);
  if (!entry) {
    entry = mShmemCharMaps.PutEntry(aCmap);
  }
  return entry->GetCharMap();
}

gfxCharacterMap* gfxPlatformFontList::FindCharMap(gfxCharacterMap* aCmap) {
  aCmap->CalcHash();
  gfxCharacterMap* cmap = AddCmap(aCmap);
  cmap->mShared = true;
  return cmap;
}

// add a cmap to the shared cmap set
gfxCharacterMap* gfxPlatformFontList::AddCmap(const gfxCharacterMap* aCharMap) {
  CharMapHashKey* found =
      mSharedCmaps.PutEntry(const_cast<gfxCharacterMap*>(aCharMap));
  return found->GetKey();
}

// remove the cmap from the shared cmap set
void gfxPlatformFontList::RemoveCmap(const gfxCharacterMap* aCharMap) {
  // skip lookups during teardown
  if (mSharedCmaps.Count() == 0) {
    return;
  }

  // cmap needs to match the entry *and* be the same ptr before removing
  CharMapHashKey* found =
      mSharedCmaps.GetEntry(const_cast<gfxCharacterMap*>(aCharMap));
  if (found && found->GetKey() == aCharMap) {
    mSharedCmaps.RemoveEntry(found);
  }
}

void gfxPlatformFontList::ResolveGenericFontNames(
    StyleGenericFontFamily aGenericType, eFontPrefLang aPrefLang,
    PrefFontList* aGenericFamilies) {
  const char* langGroupStr = GetPrefLangName(aPrefLang);
  const char* generic = GetGenericName(aGenericType);

  if (!generic) {
    return;
  }

  AutoTArray<nsCString, 4> genericFamilies;

  // load family for "font.name.generic.lang"
  gfxFontUtils::AppendPrefsFontList(NamePref(generic, langGroupStr).get(),
                                    genericFamilies);

  // load fonts for "font.name-list.generic.lang"
  gfxFontUtils::AppendPrefsFontList(NameListPref(generic, langGroupStr).get(),
                                    genericFamilies);

  nsAtom* langGroup = GetLangGroupForPrefLang(aPrefLang);
  NS_ASSERTION(langGroup, "null lang group for pref lang");

  GetFontFamiliesFromGenericFamilies(aGenericType, genericFamilies, langGroup,
                                     aGenericFamilies);

#if 0  // dump out generic mappings
    printf("%s ===> ", NamePref(generic, langGroupStr).get());
    for (uint32_t k = 0; k < aGenericFamilies->Length(); k++) {
        if (k > 0) printf(", ");
        printf("%s", (*aGenericFamilies)[k].mIsShared
            ? (*aGenericFamilies)[k].mShared->DisplayName().AsString(SharedFontList()).get()
            : (*aGenericFamilies)[k].mUnshared->Name().get());
    }
    printf("\n");
#endif
}

void gfxPlatformFontList::ResolveEmojiFontNames(
    PrefFontList* aGenericFamilies) {
  // emoji preference has no lang name
  AutoTArray<nsCString, 4> genericFamilies;

  nsAutoCString prefFontListName("font.name-list.emoji");
  gfxFontUtils::AppendPrefsFontList(prefFontListName.get(), genericFamilies);

  GetFontFamiliesFromGenericFamilies(StyleGenericFontFamily::MozEmoji,
                                     genericFamilies, nullptr,
                                     aGenericFamilies);
}

void gfxPlatformFontList::GetFontFamiliesFromGenericFamilies(
    StyleGenericFontFamily aGenericType,
    nsTArray<nsCString>& aGenericNameFamilies, nsAtom* aLangGroup,
    PrefFontList* aGenericFamilies) {
  // lookup and add platform fonts uniquely
  for (const nsCString& genericFamily : aGenericNameFamilies) {
    gfxFontStyle style;
    style.language = aLangGroup;
    style.systemFont = false;
    AutoTArray<FamilyAndGeneric, 10> families;
    FindAndAddFamilies(aGenericType, genericFamily, &families,
                       FindFamiliesFlags(0), &style);
    for (const FamilyAndGeneric& f : families) {
      if (!aGenericFamilies->Contains(f.mFamily)) {
        aGenericFamilies->AppendElement(f.mFamily);
      }
    }
  }
}

gfxPlatformFontList::PrefFontList* gfxPlatformFontList::GetPrefFontsLangGroup(
    StyleGenericFontFamily aGenericType, eFontPrefLang aPrefLang) {
  if (aGenericType == StyleGenericFontFamily::MozEmoji) {
    // Emoji font has no lang
    PrefFontList* prefFonts = mEmojiPrefFont.get();
    if (MOZ_UNLIKELY(!prefFonts)) {
      prefFonts = new PrefFontList;
      ResolveEmojiFontNames(prefFonts);
      mEmojiPrefFont.reset(prefFonts);
    }
    return prefFonts;
  }

  auto index = static_cast<size_t>(aGenericType);
  PrefFontList* prefFonts = mLangGroupPrefFonts[aPrefLang][index].get();
  if (MOZ_UNLIKELY(!prefFonts)) {
    prefFonts = new PrefFontList;
    ResolveGenericFontNames(aGenericType, aPrefLang, prefFonts);
    mLangGroupPrefFonts[aPrefLang][index].reset(prefFonts);
  }
  return prefFonts;
}

void gfxPlatformFontList::AddGenericFonts(
    StyleGenericFontFamily aGenericType, nsAtom* aLanguage,
    nsTArray<FamilyAndGeneric>& aFamilyList) {
  // map lang ==> langGroup
  nsAtom* langGroup = GetLangGroup(aLanguage);

  // langGroup ==> prefLang
  eFontPrefLang prefLang = GetFontPrefLangFor(langGroup);

  // lookup pref fonts
  PrefFontList* prefFonts = GetPrefFontsLangGroup(aGenericType, prefLang);

  if (!prefFonts->IsEmpty()) {
    aFamilyList.SetCapacity(aFamilyList.Length() + prefFonts->Length());
    for (auto& f : *prefFonts) {
      aFamilyList.AppendElement(FamilyAndGeneric(f, aGenericType));
    }
  }
}

static nsAtom* PrefLangToLangGroups(uint32_t aIndex) {
  // static array here avoids static constructor
  static nsAtom* gPrefLangToLangGroups[] = {
#define FONT_PREF_LANG(enum_id_, str_, atom_id_) nsGkAtoms::atom_id_
#include "gfxFontPrefLangList.h"
#undef FONT_PREF_LANG
  };

  return aIndex < ArrayLength(gPrefLangToLangGroups)
             ? gPrefLangToLangGroups[aIndex]
             : nsGkAtoms::Unicode;
}

eFontPrefLang gfxPlatformFontList::GetFontPrefLangFor(const char* aLang) {
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

eFontPrefLang gfxPlatformFontList::GetFontPrefLangFor(nsAtom* aLang) {
  if (!aLang) return eFontPrefLang_Others;
  nsAutoCString lang;
  aLang->ToUTF8String(lang);
  return GetFontPrefLangFor(lang.get());
}

nsAtom* gfxPlatformFontList::GetLangGroupForPrefLang(eFontPrefLang aLang) {
  // the special CJK set pref lang should be resolved into separate
  // calls to individual CJK pref langs before getting here
  NS_ASSERTION(aLang != eFontPrefLang_CJKSet, "unresolved CJK set pref lang");

  return PrefLangToLangGroups(uint32_t(aLang));
}

const char* gfxPlatformFontList::GetPrefLangName(eFontPrefLang aLang) {
  if (uint32_t(aLang) < ArrayLength(gPrefLangNames)) {
    return gPrefLangNames[uint32_t(aLang)];
  }
  return nullptr;
}

eFontPrefLang gfxPlatformFontList::GetFontPrefLangFor(uint32_t aCh) {
  switch (ublock_getCode(aCh)) {
    case UBLOCK_BASIC_LATIN:
    case UBLOCK_LATIN_1_SUPPLEMENT:
    case UBLOCK_LATIN_EXTENDED_A:
    case UBLOCK_LATIN_EXTENDED_B:
    case UBLOCK_IPA_EXTENSIONS:
    case UBLOCK_SPACING_MODIFIER_LETTERS:
    case UBLOCK_LATIN_EXTENDED_ADDITIONAL:
    case UBLOCK_LATIN_EXTENDED_C:
    case UBLOCK_LATIN_EXTENDED_D:
    case UBLOCK_LATIN_EXTENDED_E:
    case UBLOCK_PHONETIC_EXTENSIONS:
      return eFontPrefLang_Western;
    case UBLOCK_GREEK:
    case UBLOCK_GREEK_EXTENDED:
      return eFontPrefLang_Greek;
    case UBLOCK_CYRILLIC:
    case UBLOCK_CYRILLIC_SUPPLEMENT:
    case UBLOCK_CYRILLIC_EXTENDED_A:
    case UBLOCK_CYRILLIC_EXTENDED_B:
    case UBLOCK_CYRILLIC_EXTENDED_C:
      return eFontPrefLang_Cyrillic;
    case UBLOCK_ARMENIAN:
      return eFontPrefLang_Armenian;
    case UBLOCK_HEBREW:
      return eFontPrefLang_Hebrew;
    case UBLOCK_ARABIC:
    case UBLOCK_ARABIC_PRESENTATION_FORMS_A:
    case UBLOCK_ARABIC_PRESENTATION_FORMS_B:
    case UBLOCK_ARABIC_SUPPLEMENT:
    case UBLOCK_ARABIC_EXTENDED_A:
    case UBLOCK_ARABIC_MATHEMATICAL_ALPHABETIC_SYMBOLS:
      return eFontPrefLang_Arabic;
    case UBLOCK_DEVANAGARI:
    case UBLOCK_DEVANAGARI_EXTENDED:
      return eFontPrefLang_Devanagari;
    case UBLOCK_BENGALI:
      return eFontPrefLang_Bengali;
    case UBLOCK_GURMUKHI:
      return eFontPrefLang_Gurmukhi;
    case UBLOCK_GUJARATI:
      return eFontPrefLang_Gujarati;
    case UBLOCK_ORIYA:
      return eFontPrefLang_Oriya;
    case UBLOCK_TAMIL:
      return eFontPrefLang_Tamil;
    case UBLOCK_TELUGU:
      return eFontPrefLang_Telugu;
    case UBLOCK_KANNADA:
      return eFontPrefLang_Kannada;
    case UBLOCK_MALAYALAM:
      return eFontPrefLang_Malayalam;
    case UBLOCK_SINHALA:
    case UBLOCK_SINHALA_ARCHAIC_NUMBERS:
      return eFontPrefLang_Sinhala;
    case UBLOCK_THAI:
      return eFontPrefLang_Thai;
    case UBLOCK_TIBETAN:
      return eFontPrefLang_Tibetan;
    case UBLOCK_GEORGIAN:
    case UBLOCK_GEORGIAN_SUPPLEMENT:
    case UBLOCK_GEORGIAN_EXTENDED:
      return eFontPrefLang_Georgian;
    case UBLOCK_HANGUL_JAMO:
    case UBLOCK_HANGUL_COMPATIBILITY_JAMO:
    case UBLOCK_HANGUL_SYLLABLES:
    case UBLOCK_HANGUL_JAMO_EXTENDED_A:
    case UBLOCK_HANGUL_JAMO_EXTENDED_B:
      return eFontPrefLang_Korean;
    case UBLOCK_ETHIOPIC:
    case UBLOCK_ETHIOPIC_EXTENDED:
    case UBLOCK_ETHIOPIC_SUPPLEMENT:
    case UBLOCK_ETHIOPIC_EXTENDED_A:
      return eFontPrefLang_Ethiopic;
    case UBLOCK_UNIFIED_CANADIAN_ABORIGINAL_SYLLABICS:
    case UBLOCK_UNIFIED_CANADIAN_ABORIGINAL_SYLLABICS_EXTENDED:
      return eFontPrefLang_Canadian;
    case UBLOCK_KHMER:
    case UBLOCK_KHMER_SYMBOLS:
      return eFontPrefLang_Khmer;
    case UBLOCK_CJK_RADICALS_SUPPLEMENT:
    case UBLOCK_KANGXI_RADICALS:
    case UBLOCK_IDEOGRAPHIC_DESCRIPTION_CHARACTERS:
    case UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION:
    case UBLOCK_HIRAGANA:
    case UBLOCK_KATAKANA:
    case UBLOCK_BOPOMOFO:
    case UBLOCK_KANBUN:
    case UBLOCK_BOPOMOFO_EXTENDED:
    case UBLOCK_ENCLOSED_CJK_LETTERS_AND_MONTHS:
    case UBLOCK_CJK_COMPATIBILITY:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_A:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS:
    case UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS:
    case UBLOCK_CJK_COMPATIBILITY_FORMS:
    case UBLOCK_SMALL_FORM_VARIANTS:
    case UBLOCK_HALFWIDTH_AND_FULLWIDTH_FORMS:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_B:
    case UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS_SUPPLEMENT:
    case UBLOCK_KATAKANA_PHONETIC_EXTENSIONS:
    case UBLOCK_CJK_STROKES:
    case UBLOCK_VERTICAL_FORMS:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_C:
    case UBLOCK_KANA_SUPPLEMENT:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_D:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_E:
    case UBLOCK_IDEOGRAPHIC_SYMBOLS_AND_PUNCTUATION:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_F:
    case UBLOCK_KANA_EXTENDED_A:
      return eFontPrefLang_CJKSet;
    default:
      return eFontPrefLang_Others;
  }
}

bool gfxPlatformFontList::IsLangCJK(eFontPrefLang aLang) {
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

void gfxPlatformFontList::GetLangPrefs(eFontPrefLang aPrefLangs[],
                                       uint32_t& aLen, eFontPrefLang aCharLang,
                                       eFontPrefLang aPageLang) {
  if (IsLangCJK(aCharLang)) {
    AppendCJKPrefLangs(aPrefLangs, aLen, aCharLang, aPageLang);
  } else {
    AppendPrefLang(aPrefLangs, aLen, aCharLang);
  }

  AppendPrefLang(aPrefLangs, aLen, eFontPrefLang_Others);
}

void gfxPlatformFontList::AppendCJKPrefLangs(eFontPrefLang aPrefLangs[],
                                             uint32_t& aLen,
                                             eFontPrefLang aCharLang,
                                             eFontPrefLang aPageLang) {
  // prefer the lang specified by the page *if* CJK
  if (IsLangCJK(aPageLang)) {
    AppendPrefLang(aPrefLangs, aLen, aPageLang);
  }

  // if not set up, set up the default CJK order, based on accept lang
  // settings and locale
  if (mCJKPrefLangs.Length() == 0) {
    // temp array
    eFontPrefLang tempPrefLangs[kMaxLenPrefLangList];
    uint32_t tempLen = 0;

    // Add the CJK pref fonts from accept languages, the order should be same
    // order
    nsAutoCString list;
    Preferences::GetLocalizedCString("intl.accept_languages", list);
    if (!list.IsEmpty()) {
      const char kComma = ',';
      const char *p, *p_end;
      list.BeginReading(p);
      list.EndReading(p_end);
      while (p < p_end) {
        while (nsCRT::IsAsciiSpace(*p)) {
          if (++p == p_end) break;
        }
        if (p == p_end) break;
        const char* start = p;
        while (++p != p_end && *p != kComma) /* nothing */
          ;
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

    // Try using app's locale
    nsAutoCString localeStr;
    LocaleService::GetInstance()->GetAppLocaleAsLangTag(localeStr);

    {
      Locale locale(localeStr);
      if (locale.GetLanguage().Equals("ja")) {
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Japanese);
      } else if (locale.GetLanguage().Equals("zh")) {
        if (locale.GetRegion().Equals("CN")) {
          AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseCN);
        } else if (locale.GetRegion().Equals("TW")) {
          AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseTW);
        } else if (locale.GetRegion().Equals("HK")) {
          AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseHK);
        }
      } else if (locale.GetLanguage().Equals("ko")) {
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Korean);
      }
    }

    // Then add the known CJK prefs in order of system preferred locales
    AutoTArray<nsCString, 5> prefLocales;
    prefLocales.AppendElement(NS_LITERAL_CSTRING("ja"));
    prefLocales.AppendElement(NS_LITERAL_CSTRING("zh-CN"));
    prefLocales.AppendElement(NS_LITERAL_CSTRING("zh-TW"));
    prefLocales.AppendElement(NS_LITERAL_CSTRING("zh-HK"));
    prefLocales.AppendElement(NS_LITERAL_CSTRING("ko"));

    AutoTArray<nsCString, 16> sysLocales;
    AutoTArray<nsCString, 16> negLocales;
    if (NS_SUCCEEDED(
            OSPreferences::GetInstance()->GetSystemLocales(sysLocales))) {
      LocaleService::GetInstance()->NegotiateLanguages(
          sysLocales, prefLocales, NS_LITERAL_CSTRING(""),
          LocaleService::kLangNegStrategyFiltering, negLocales);
      for (const auto& localeStr : negLocales) {
        Locale locale(localeStr);

        if (locale.GetLanguage().Equals("ja")) {
          AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Japanese);
        } else if (locale.GetLanguage().Equals("zh")) {
          if (locale.GetRegion().Equals("CN")) {
            AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseCN);
          } else if (locale.GetRegion().Equals("TW")) {
            AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseTW);
          } else if (locale.GetRegion().Equals("HK")) {
            AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseHK);
          }
        } else if (locale.GetLanguage().Equals("ko")) {
          AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Korean);
        }
      }
    }

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
  uint32_t i, numCJKlangs = mCJKPrefLangs.Length();

  for (i = 0; i < numCJKlangs; i++) {
    AppendPrefLang(aPrefLangs, aLen, (eFontPrefLang)(mCJKPrefLangs[i]));
  }
}

void gfxPlatformFontList::AppendPrefLang(eFontPrefLang aPrefLangs[],
                                         uint32_t& aLen,
                                         eFontPrefLang aAddLang) {
  if (aLen >= kMaxLenPrefLangList) return;

  // make sure
  uint32_t i = 0;
  while (i < aLen && aPrefLangs[i] != aAddLang) {
    i++;
  }

  if (i == aLen) {
    aPrefLangs[aLen] = aAddLang;
    aLen++;
  }
}

StyleGenericFontFamily gfxPlatformFontList::GetDefaultGeneric(
    eFontPrefLang aLang) {
  if (aLang == eFontPrefLang_Emoji) {
    return StyleGenericFontFamily::MozEmoji;
  }

  // initialize lang group pref font defaults (i.e. serif/sans-serif)
  if (MOZ_UNLIKELY(mDefaultGenericsLangGroup.IsEmpty())) {
    mDefaultGenericsLangGroup.AppendElements(ArrayLength(gPrefLangNames));
    for (uint32_t i = 0; i < ArrayLength(gPrefLangNames); i++) {
      nsAutoCString prefDefaultFontType("font.default.");
      prefDefaultFontType.Append(GetPrefLangName(eFontPrefLang(i)));
      nsAutoCString serifOrSans;
      Preferences::GetCString(prefDefaultFontType.get(), serifOrSans);
      if (serifOrSans.EqualsLiteral("sans-serif")) {
        mDefaultGenericsLangGroup[i] = StyleGenericFontFamily::SansSerif;
      } else {
        mDefaultGenericsLangGroup[i] = StyleGenericFontFamily::Serif;
      }
    }
  }

  if (uint32_t(aLang) < ArrayLength(gPrefLangNames)) {
    return mDefaultGenericsLangGroup[uint32_t(aLang)];
  }
  return StyleGenericFontFamily::Serif;
}

FontFamily gfxPlatformFontList::GetDefaultFont(const gfxFontStyle* aStyle) {
  FontFamily family = GetDefaultFontForPlatform(aStyle);
  if (!family.IsNull()) {
    return family;
  }
  // Something has gone wrong and we were unable to retrieve a default font
  // from the platform. (Likely the whitelist has blocked all potential
  // default fonts.) As a last resort, we return the first font in our list.
  if (SharedFontList()) {
    MOZ_RELEASE_ASSERT(SharedFontList()->NumFamilies() > 0);
    return FontFamily(SharedFontList()->Families());
  }
  MOZ_RELEASE_ASSERT(mFontFamilies.Count() > 0);
  return FontFamily(mFontFamilies.Iter().Data());
}

void gfxPlatformFontList::GetFontFamilyNames(
    nsTArray<nsCString>& aFontFamilyNames) {
  if (SharedFontList()) {
    fontlist::FontList* list = SharedFontList();
    const fontlist::Family* families = list->Families();
    for (uint32_t i = 0, n = list->NumFamilies(); i < n; i++) {
      aFontFamilyNames.AppendElement(families[i].DisplayName().AsString(list));
    }
  } else {
    for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
      RefPtr<gfxFontFamily>& family = iter.Data();
      aFontFamilyNames.AppendElement(family->Name());
    }
  }
}

nsAtom* gfxPlatformFontList::GetLangGroup(nsAtom* aLanguage) {
  // map lang ==> langGroup
  nsAtom* langGroup = nullptr;
  if (aLanguage) {
    langGroup = mLangService->GetLanguageGroup(aLanguage);
  }
  if (!langGroup) {
    langGroup = nsGkAtoms::Unicode;
  }
  return langGroup;
}

/* static */ const char* gfxPlatformFontList::GetGenericName(
    StyleGenericFontFamily aGenericType) {
  static const char kGeneric_serif[] = "serif";
  static const char kGeneric_sans_serif[] = "sans-serif";
  static const char kGeneric_monospace[] = "monospace";
  static const char kGeneric_cursive[] = "cursive";
  static const char kGeneric_fantasy[] = "fantasy";

  // type should be standard generic type at this point
  // map generic type to string
  switch (aGenericType) {
    case StyleGenericFontFamily::Serif:
      return kGeneric_serif;
    case StyleGenericFontFamily::SansSerif:
      return kGeneric_sans_serif;
    case StyleGenericFontFamily::Monospace:
      return kGeneric_monospace;
    case StyleGenericFontFamily::Cursive:
      return kGeneric_cursive;
    case StyleGenericFontFamily::Fantasy:
      return kGeneric_fantasy;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown generic");
      return nullptr;
  }
}

void gfxPlatformFontList::InitLoader() {
  GetFontFamilyNames(mFontInfo->mFontFamiliesToLoad);
  mStartIndex = 0;
  mNumFamilies = mFontInfo->mFontFamiliesToLoad.Length();
  memset(&(mFontInfo->mLoadStats), 0, sizeof(mFontInfo->mLoadStats));
}

#define FONT_LOADER_MAX_TIMESLICE \
  100  // max time for one pass through RunLoader = 100ms

bool gfxPlatformFontList::LoadFontInfo() {
  TimeStamp start = TimeStamp::Now();
  uint32_t i, endIndex = mNumFamilies;
  fontlist::FontList* list = SharedFontList();
  bool loadCmaps =
    !list && (!UsesSystemFallback() ||
              gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback());

  // for each font family, load in various font info
  for (i = mStartIndex; i < endIndex; i++) {
    nsAutoCString key;
    GenerateFontListKey(mFontInfo->mFontFamiliesToLoad[i], key);

    if (list) {
      fontlist::Family* family = list->FindFamily(key);
      if (!family) {
        continue;
      }
      if (family->IsHidden()) {
        continue;
      }
      ReadFaceNamesForFamily(family, NeedFullnamePostscriptNames());
    } else {
      // lookup in canonical (i.e. English) family name list
      gfxFontFamily* familyEntry = mFontFamilies.GetWeak(key);
      if (!familyEntry) {
        continue;
      }

      // read in face names
      familyEntry->ReadFaceNames(this, NeedFullnamePostscriptNames(),
                                 mFontInfo);

      // load the cmaps if needed
      if (loadCmaps) {
        familyEntry->ReadAllCMAPs(mFontInfo);
      }
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
    CancelInitOtherFamilyNamesTask();
    mFaceNameListsInitialized = true;
  }

  return done;
}

void gfxPlatformFontList::CleanupLoader() {
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
      if (FindUnsharedFamily(
              it.Get()->GetKey(),
              (FindFamiliesFlags::eForceOtherFamilyNamesLoading |
               FindFamiliesFlags::eNoAddToNamesMissedWhenSearching))) {
        forceReflow = true;
        ForceGlobalReflow();
        break;
      }
    }
    mOtherNamesMissed = nullptr;
  }

  if (LOG_FONTINIT_ENABLED() && mFontInfo) {
    LOG_FONTINIT(
        ("(fontinit) fontloader load thread took %8.2f ms "
         "%d families %d fonts %d cmaps "
         "%d facenames %d othernames %s %s",
         mLoadTime.ToMilliseconds(), mFontInfo->mLoadStats.families,
         mFontInfo->mLoadStats.fonts, mFontInfo->mLoadStats.cmaps,
         mFontInfo->mLoadStats.facenames, mFontInfo->mLoadStats.othernames,
         (rebuilt ? "(userfont sets rebuilt)" : ""),
         (forceReflow ? "(global reflow)" : "")));
  }

  gfxFontInfoLoader::CleanupLoader();
}

void gfxPlatformFontList::GetPrefsAndStartLoader() {
  uint32_t delay = std::max(1u, Preferences::GetUint(FONT_LOADER_DELAY_PREF));
  uint32_t interval =
      std::max(1u, Preferences::GetUint(FONT_LOADER_INTERVAL_PREF));

  StartLoader(delay, interval);
}

void gfxPlatformFontList::ForceGlobalReflow() {
  gfxPlatform::ForceGlobalReflow();
}

void gfxPlatformFontList::RebuildLocalFonts() {
  for (auto it = mUserFontSetList.Iter(); !it.Done(); it.Next()) {
    it.Get()->GetKey()->RebuildLocalRules();
  }
}

void gfxPlatformFontList::ClearLangGroupPrefFonts() {
  for (uint32_t i = eFontPrefLang_First;
       i < eFontPrefLang_First + eFontPrefLang_Count; i++) {
    auto& prefFontsLangGroup = mLangGroupPrefFonts[i];
    for (auto& pref : prefFontsLangGroup) {
      pref = nullptr;
    }
  }
  mCJKPrefLangs.Clear();
  mEmojiPrefFont = nullptr;
}

// Support for memory reporting

// this is also used by subclasses that hold additional font tables
/*static*/
size_t gfxPlatformFontList::SizeOfFontFamilyTableExcludingThis(
    const FontFamilyTable& aTable, MallocSizeOf aMallocSizeOf) {
  size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
    // We don't count the size of the family here, because this is an
    // *extra* reference to a family that will have already been counted in
    // the main list.
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
  return n;
}

/*static*/
size_t gfxPlatformFontList::SizeOfFontEntryTableExcludingThis(
    const FontEntryTable& aTable, MallocSizeOf aMallocSizeOf) {
  size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
    // The font itself is counted by its owning family; here we only care
    // about the names stored in the hashtable keys.
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
  return n;
}

void gfxPlatformFontList::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                                 FontListSizes* aSizes) const {
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
    aSizes->mFontListSize += SizeOfFontEntryTableExcludingThis(
        mExtraNames->mFullnames, aMallocSizeOf);
    aSizes->mFontListSize += SizeOfFontEntryTableExcludingThis(
        mExtraNames->mPostscriptNames, aMallocSizeOf);
  }

  for (uint32_t i = eFontPrefLang_First;
       i < eFontPrefLang_First + eFontPrefLang_Count; i++) {
    auto& prefFontsLangGroup = mLangGroupPrefFonts[i];
    for (const UniquePtr<PrefFontList>& pf : prefFontsLangGroup) {
      if (pf) {
        aSizes->mFontListSize += pf->ShallowSizeOfExcludingThis(aMallocSizeOf);
      }
    }
  }

  aSizes->mFontListSize +=
      mCodepointsWithNoFonts.SizeOfExcludingThis(aMallocSizeOf);
  aSizes->mFontListSize +=
      mFontFamiliesToLoad.ShallowSizeOfExcludingThis(aMallocSizeOf);

  aSizes->mFontListSize +=
      mBadUnderlineFamilyNames.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& i : mBadUnderlineFamilyNames) {
    aSizes->mFontListSize += i.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  aSizes->mFontListSize +=
      mSharedCmaps.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mSharedCmaps.ConstIter(); !iter.Done(); iter.Next()) {
    aSizes->mCharMapsSize +=
        iter.Get()->GetKey()->SizeOfIncludingThis(aMallocSizeOf);
  }

  aSizes->mFontListSize +=
      mFontEntries.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mFontEntries.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Data()->AddSizeOfIncludingThis(aMallocSizeOf, aSizes);
  }

  if (SharedFontList()) {
    aSizes->mFontListSize +=
        SharedFontList()->SizeOfIncludingThis(aMallocSizeOf);
    if (XRE_IsParentProcess()) {
      aSizes->mSharedSize += SharedFontList()->AllocatedShmemSize();
    }
  }
}

void gfxPlatformFontList::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                                 FontListSizes* aSizes) const {
  aSizes->mFontListSize += aMallocSizeOf(this);
  AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

bool gfxPlatformFontList::IsFontFamilyWhitelistActive() {
  return mFontFamilyWhitelistActive;
}

void gfxPlatformFontList::InitOtherFamilyNamesInternal(
    bool aDeferOtherFamilyNamesLoading) {
  if (mOtherFamilyNamesInitialized) {
    return;
  }

  if (aDeferOtherFamilyNamesLoading) {
    TimeStamp start = TimeStamp::Now();
    bool timedOut = false;

    auto list = SharedFontList();
    if (list) {
      for (auto& f : mozilla::Range<fontlist::Family>(list->Families(),
                                                      list->NumFamilies())) {
        ReadFaceNamesForFamily(&f, false);
        TimeDuration elapsed = TimeStamp::Now() - start;
        if (elapsed.ToMilliseconds() > OTHERNAMES_TIMEOUT) {
          timedOut = true;
          break;
        }
      }
    } else {
      for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
        RefPtr<gfxFontFamily>& family = iter.Data();
        family->ReadOtherFamilyNames(this);
        TimeDuration elapsed = TimeStamp::Now() - start;
        if (elapsed.ToMilliseconds() > OTHERNAMES_TIMEOUT) {
          timedOut = true;
          break;
        }
      }
    }

    if (!timedOut) {
      mOtherFamilyNamesInitialized = true;
      CancelInitOtherFamilyNamesTask();
    }
    TimeStamp end = TimeStamp::Now();
    Telemetry::AccumulateTimeDelta(Telemetry::FONTLIST_INITOTHERFAMILYNAMES,
                                   start, end);

    if (LOG_FONTINIT_ENABLED()) {
      TimeDuration elapsed = end - start;
      LOG_FONTINIT(("(fontinit) InitOtherFamilyNames took %8.2f ms %s",
                    elapsed.ToMilliseconds(), (timedOut ? "timeout" : "")));
    }
  } else {
    TimeStamp start = TimeStamp::Now();

    auto list = SharedFontList();
    if (list) {
      for (auto& f : mozilla::Range<fontlist::Family>(list->Families(),
                                                      list->NumFamilies())) {
        ReadFaceNamesForFamily(&f, false);
      }
    } else {
      for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
        RefPtr<gfxFontFamily>& family = iter.Data();
        family->ReadOtherFamilyNames(this);
      }
    }

    mOtherFamilyNamesInitialized = true;
    CancelInitOtherFamilyNamesTask();

    TimeStamp end = TimeStamp::Now();
    Telemetry::AccumulateTimeDelta(
        Telemetry::FONTLIST_INITOTHERFAMILYNAMES_NO_DEFERRING, start, end);

    if (LOG_FONTINIT_ENABLED()) {
      TimeDuration elapsed = end - start;
      LOG_FONTINIT(
          ("(fontinit) InitOtherFamilyNames without deferring took %8.2f ms",
           elapsed.ToMilliseconds()));
    }
  }
}

void gfxPlatformFontList::CancelInitOtherFamilyNamesTask() {
  if (mPendingOtherFamilyNameTask) {
    mPendingOtherFamilyNameTask->Cancel();
    mPendingOtherFamilyNameTask = nullptr;
  }
  auto list = SharedFontList();
  if (list) {
    bool forceReflow = false;
    if (!mAliasTable.IsEmpty()) {
      list->SetAliases(mAliasTable);
      mAliasTable.Clear();
      forceReflow = true;
    }
    if (mLocalNameTable.Count()) {
      list->SetLocalNames(mLocalNameTable);
      mLocalNameTable.Clear();
      forceReflow = true;
    }
    if (forceReflow) {
      dom::ContentParent::BroadcastFontListChanged();
    }
  }
}

void gfxPlatformFontList::ShareFontListShmBlockToProcess(
    uint32_t aGeneration, uint32_t aIndex, /*base::ProcessId*/ uint32_t aPid,
    /*mozilla::ipc::SharedMemoryBasic::Handle*/ void* aOut) {
  auto list = SharedFontList();
  if (!list) {
    return;
  }
  auto out = static_cast<mozilla::ipc::SharedMemoryBasic::Handle*>(aOut);
  if (!aGeneration || list->GetGeneration() == aGeneration) {
    list->ShareShmBlockToProcess(aIndex, aPid, out);
  } else {
    *out = mozilla::ipc::SharedMemoryBasic::NULLHandle();
  }
}

void gfxPlatformFontList::InitializeFamily(uint32_t aGeneration,
                                           uint32_t aFamilyIndex) {
  auto list = SharedFontList();
  MOZ_ASSERT(list);
  if (!list) {
    return;
  }
  if (list->GetGeneration() != aGeneration) {
    return;
  }
  if (aFamilyIndex >= list->NumFamilies()) {
    return;
  }
  fontlist::Family* family = list->Families() + aFamilyIndex;
  if (!family->IsInitialized()) {
    Unused << InitializeFamily(family);
  }
}

void gfxPlatformFontList::SetCharacterMap(uint32_t aGeneration,
                                          const fontlist::Pointer& aFacePtr,
                                          const gfxSparseBitSet& aMap) {
  auto list = SharedFontList();
  MOZ_ASSERT(list);
  if (!list) {
    return;
  }
  if (list->GetGeneration() != aGeneration) {
    return;
  }
  fontlist::Face* face = static_cast<fontlist::Face*>(aFacePtr.ToPtr(list));
  if (face) {
    face->SetCharacterMap(list, &aMap);
  }
}

void gfxPlatformFontList::SetupFamilyCharMap(
    uint32_t aGeneration, const fontlist::Pointer& aFamilyPtr) {
  auto list = SharedFontList();
  MOZ_ASSERT(list);
  if (!list) {
    return;
  }
  if (list->GetGeneration() != aGeneration) {
    return;
  }
  auto family = static_cast<fontlist::Family*>(aFamilyPtr.ToPtr(list));
  // validate family pointer before trying to use it
  if (family >= list->Families() &&
      family < list->Families() + list->NumFamilies()) {
    size_t offset = (char*)family - (char*)list->Families();
    if (offset % sizeof(fontlist::Family) != 0) {
      MOZ_DIAGNOSTIC_ASSERT(false, "misaligned Family pointer");
      return;
    }
  } else if (family >= list->AliasFamilies() &&
             family < list->AliasFamilies() + list->NumAliases()) {
    size_t offset = (char*)family - (char*)list->AliasFamilies();
    if (offset % sizeof(fontlist::Family) != 0) {
      MOZ_DIAGNOSTIC_ASSERT(false, "misaligned Family pointer");
      return;
    }
  } else {
    MOZ_DIAGNOSTIC_ASSERT(false, "not a valid Family or AliasFamily pointer");
    return;
  }
  family->SetupFamilyCharMap(list);
}

void gfxPlatformFontList::InitOtherFamilyNames(uint32_t aGeneration,
                                               bool aDefer) {
  auto list = SharedFontList();
  MOZ_ASSERT(list);
  if (!list) {
    return;
  }
  if (list->GetGeneration() != aGeneration) {
    return;
  }
  InitOtherFamilyNames(aDefer);
}

#undef LOG
#undef LOG_ENABLED
