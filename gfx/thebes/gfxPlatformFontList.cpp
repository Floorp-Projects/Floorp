/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/intl/MozLocale.h"
#include "mozilla/intl/OSPreferences.h"

#include "gfxPlatformFontList.h"
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
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessMessageManager.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Unused.h"

#include "base/eintr_wrapper.h"

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

static const char* kObservedPrefs[] = {"font.",
                                       "font.name-list.",
                                       "intl.accept_languages",  // hmmmm...
                                       "layout.css.font-visibility.level",
                                       "privacy.resistFingerprinting",
                                       nullptr};

static const char kFontSystemWhitelistPref[] = "font.system.whitelist";

static const char kCJKFallbackOrderPref[] = "font.cjk_pref_fallback_order";

// xxx - this can probably be eliminated by reworking pref font handling code
static const char* gPrefLangNames[] = {
#define FONT_PREF_LANG(enum_id_, str_, atom_id_) str_
#include "gfxFontPrefLangList.h"
#undef FONT_PREF_LANG
};

static_assert(MOZ_ARRAY_LENGTH(gPrefLangNames) == uint32_t(eFontPrefLang_Count),
              "size of pref lang name array doesn't match pref lang enum size");

class gfxFontListPrefObserver final : public nsIObserver {
  ~gfxFontListPrefObserver() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

static void FontListPrefChanged(const char* aPref, void* aData = nullptr) {
  // XXX this could be made to only clear out the cache for the prefs that were
  // changed but it probably isn't that big a deal.
  gfxPlatformFontList::PlatformFontList()->ClearLangGroupPrefFonts();
  gfxFontCache::GetCache()->AgeAllGenerations();
  if (aPref && (!strcmp(aPref, "layout.css.font-visibility.level") ||
                !strcmp(aPref, "privacy.resistFingerprinting"))) {
    gfxPlatformFontList::PlatformFontList()->SetVisibilityLevel();
    if (XRE_IsParentProcess()) {
      gfxPlatform::ForceGlobalReflow();
    }
  }
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
  auto* pfl = gfxPlatformFontList::PlatformFontList();
  pfl->UpdateFontList(true);
  dom::ContentParent::NotifyUpdatedFonts(true);
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
  AutoTArray<RefPtr<gfxFontFamily>, 128> accepted;
  bool whitelistedFontFound = false;
  for (const auto& entry : mFontFamilies) {
    if (entry.GetData()->IsHidden()) {
      // Hidden system fonts are exempt from whitelisting, but don't count
      // towards determining whether we "kept" any (user-visible) fonts
      accepted.AppendElement(entry.GetData());
      continue;
    }
    nsAutoCString fontFamilyName(entry.GetKey());
    ToLowerCase(fontFamilyName);
    if (familyNamesWhitelist.Contains(fontFamilyName)) {
      accepted.AppendElement(entry.GetData());
      whitelistedFontFound = true;
    }
  }
  if (!whitelistedFontFound) {
    // No whitelisted fonts found! Ignore the whitelist.
    return;
  }
  // Replace the original full list with the accepted subset.
  mFontFamilies.Clear();
  for (auto& f : accepted) {
    nsAutoCString fontFamilyName(f->Name());
    ToLowerCase(fontFamilyName);
    mFontFamilies.InsertOrUpdate(fontFamilyName, std::move(f));
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
  AutoTArray<fontlist::Family::InitData, 128> accepted;
  bool keptNonHidden = false;
  for (auto& f : aFamilies) {
    if (f.mVisibility == FontVisibility::Hidden ||
        familyNamesWhitelist.Contains(f.mKey)) {
      accepted.AppendElement(f);
      if (f.mVisibility != FontVisibility::Hidden) {
        keptNonHidden = true;
      }
    }
  }
  if (!keptNonHidden) {
    // No (visible) families were whitelisted: ignore the whitelist
    // and just leave the fontlist unchanged.
    return;
  }
  aFamilies = std::move(accepted);
}

bool gfxPlatformFontList::FamilyInList(const nsACString& aName,
                                       const char* aList[], size_t aCount) {
  auto cmp = [&](const char* const aVal) -> int {
    return nsCaseInsensitiveUTF8StringComparator(aName.BeginReading(), aVal,
                                                 aName.Length(), strlen(aVal));
  };
  size_t result;
  return BinarySearchIf(aList, 0, aCount, cmp, &result);
}

void gfxPlatformFontList::CheckFamilyList(const char* aList[], size_t aCount) {
#ifdef DEBUG
  MOZ_ASSERT(aCount > 0, "empty font family list?");
  const char* a = aList[0];
  uint32_t aLen = strlen(a);
  for (size_t i = 1; i < aCount; ++i) {
    const char* b = aList[i];
    uint32_t bLen = strlen(b);
    MOZ_ASSERT(nsCaseInsensitiveUTF8StringComparator(a, b, aLen, bLen) < 0,
               "incorrectly sorted font family list!");
    a = b;
    aLen = bLen;
  }
#endif
}

bool gfxPlatformFontList::AddWithLegacyFamilyName(const nsACString& aLegacyName,
                                                  gfxFontEntry* aFontEntry,
                                                  FontVisibility aVisibility) {
  bool added = false;
  nsAutoCString key;
  ToLowerCase(aLegacyName, key);
  mOtherFamilyNames
      .LookupOrInsertWith(
          key,
          [&] {
            RefPtr<gfxFontFamily> family =
                CreateFontFamily(aLegacyName, aVisibility);
            family->SetHasStyles(
                true);  // we don't want the family to search for
                        // faces, we're adding them directly here
            added = true;
            return family;
          })
      ->AddFontEntry(aFontEntry->Clone());
  return added;
}

nsresult gfxPlatformFontList::InitFontList() {
  // This shouldn't be called from stylo threads!
  MOZ_ASSERT(NS_IsMainThread());

  mFontlistInitCount++;

  if (LOG_FONTINIT_ENABLED()) {
    LOG_FONTINIT(("(fontinit) system fontlist initialization\n"));
  }

  // rebuilding fontlist so clear out font/word caches
  gfxFontCache* fontCache = gfxFontCache::GetCache();
  if (fontCache) {
    fontCache->FlushShapedWordCaches();
    fontCache->Flush();
  }

  gfxPlatform::PurgeSkiaFontCache();

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    // Notify any current presContexts that fonts are being updated, so existing
    // caches will no longer be valid.
    obs->NotifyObservers(nullptr, "font-info-updated", nullptr);
  }

  mAliasTable.Clear();
  mLocalNameTable.Clear();

  CancelLoadCmapsTask();
  mStartedLoadingCmapsFrom = 0xffffffffu;

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

  // Ensure that SetVisibilityLevel will clear the mCodepointsWithNoFonts set.
  mVisibilityLevel = FontVisibility::Unknown;
  SetVisibilityLevel();

  sPlatformFontList = this;

  // Try to initialize the cross-process shared font list if enabled by prefs,
  // but not if we're running in Safe Mode.
  if (StaticPrefs::gfx_e10s_font_list_shared_AtStartup() &&
      !gfxPlatform::InSafeMode()) {
    for (const auto& entry : mFontEntries) {
      if (!entry.GetData()) {
        continue;
      }
      entry.GetData()->mShmemCharacterMap = nullptr;
      entry.GetData()->mShmemFace = nullptr;
      entry.GetData()->mFamilyName.Truncate();
    }
    mFontEntries.Clear();
    mShmemCharMaps.Clear();
    bool oldSharedList = mSharedFontList != nullptr;
    mSharedFontList.reset(new fontlist::FontList(mFontlistInitCount));
    InitSharedFontListForPlatform();
    if (mSharedFontList && mSharedFontList->Initialized()) {
      if (mLocalNameTable.Count()) {
        SharedFontList()->SetLocalNames(mLocalNameTable);
        mLocalNameTable.Clear();
      }
    } else {
      // something went wrong, fall back to in-process list
      gfxCriticalNote << "Failed to initialize shared font list, "
                         "falling back to in-process list.";
      mSharedFontList.reset(nullptr);
    }
    if (oldSharedList) {
      if (XRE_IsParentProcess()) {
        // notify all children of the change
        dom::ContentParent::NotifyUpdatedFonts(true);
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

  // Set up mDefaultFontEntry as a "last resort" default that we can use
  // to avoid crashing if the font list is otherwise unusable.
  gfxFontStyle defStyle;
  FontFamily fam = GetDefaultFont(&defStyle);
  if (fam.mIsShared) {
    auto face = fam.mShared->FindFaceForStyle(SharedFontList(), defStyle);
    if (!face) {
      mDefaultFontEntry = nullptr;
    } else {
      mDefaultFontEntry = GetOrCreateFontEntry(face, fam.mShared);
    }
  } else {
    mDefaultFontEntry = fam.mUnshared->FindFontForStyle(defStyle);
  }

  return NS_OK;
}

void gfxPlatformFontList::InitializeCodepointsWithNoFonts() {
  mCodepointsWithNoFonts.reset();
  mCodepointsWithNoFonts.SetRange(0, 0x1f);            // C0 controls
  mCodepointsWithNoFonts.SetRange(0x7f, 0x9f);         // C1 controls
  mCodepointsWithNoFonts.SetRange(0xE000, 0xF8FF);     // PUA
  mCodepointsWithNoFonts.SetRange(0xF0000, 0x10FFFD);  // Supplementary PUA
  mCodepointsWithNoFonts.SetRange(0xfdd0, 0xfdef);     // noncharacters
  for (unsigned i = 0; i <= 0x100000; i += 0x10000) {
    mCodepointsWithNoFonts.SetRange(i + 0xfffe, i + 0xffff);  // noncharacters
  }
}

void gfxPlatformFontList::SetVisibilityLevel() {
  FontVisibility newLevel;
  if (StaticPrefs::privacy_resistFingerprinting()) {
    newLevel = FontVisibility::Base;
  } else {
    newLevel = FontVisibility(
        std::min(int32_t(FontVisibility::User),
                 std::max(int32_t(FontVisibility::Base),
                          StaticPrefs::layout_css_font_visibility_level())));
  }
  if (newLevel != mVisibilityLevel) {
    mVisibilityLevel = newLevel;
    // (Re-)initialize ranges of characters for which system-wide font search
    // should be skipped
    InitializeCodepointsWithNoFonts();
    // Forget any font family we previously chose for U+FFFD.
    mReplacementCharFallbackFamily = FontFamily();
  }
}

void gfxPlatformFontList::FontListChanged() {
  MOZ_ASSERT(!XRE_IsParentProcess());
  InitializeCodepointsWithNoFonts();
  if (SharedFontList()) {
    // If we're using a shared local face-name list, this may have changed
    // such that existing font entries held by user font sets are no longer
    // safe to use: ensure they all get flushed.
    RebuildLocalFonts(/*aForgetLocalFaces*/ true);
  }
  ForceGlobalReflow();
}

void gfxPlatformFontList::GenerateFontListKey(const nsACString& aKeyName,
                                              nsACString& aResult) {
  aResult = aKeyName;
  ToLowerCase(aResult);
}

// Used if a stylo thread wants to trigger InitOtherFamilyNames in the main
// process: we can't do IPC from the stylo thread so we post this to the main
// thread instead.
class InitOtherFamilyNamesForStylo : public mozilla::Runnable {
 public:
  explicit InitOtherFamilyNamesForStylo(bool aDeferOtherFamilyNamesLoading)
      : Runnable("gfxPlatformFontList::InitOtherFamilyNamesForStylo"),
        mDefer(aDeferOtherFamilyNamesLoading) {}

  NS_IMETHOD Run() override {
    auto pfl = gfxPlatformFontList::PlatformFontList();
    auto list = pfl->SharedFontList();
    if (!list) {
      return NS_OK;
    }
    dom::ContentChild::GetSingleton()->SendInitOtherFamilyNames(
        list->GetGeneration(), mDefer, &pfl->mOtherFamilyNamesInitialized);
    return NS_OK;
  }

 private:
  bool mDefer;
};

#define OTHERNAMES_TIMEOUT 200

bool gfxPlatformFontList::InitOtherFamilyNames(
    bool aDeferOtherFamilyNamesLoading) {
  if (mOtherFamilyNamesInitialized) {
    return true;
  }

  if (SharedFontList() && !XRE_IsParentProcess()) {
    if (NS_IsMainThread()) {
      dom::ContentChild::GetSingleton()->SendInitOtherFamilyNames(
          SharedFontList()->GetGeneration(), aDeferOtherFamilyNamesLoading,
          &mOtherFamilyNamesInitialized);
    } else {
      NS_DispatchToMainThread(
          new InitOtherFamilyNamesForStylo(aDeferOtherFamilyNamesLoading));
    }
    return mOtherFamilyNamesInitialized;
  }

  // If the font loader delay has been set to zero, we don't defer loading
  // additional family names (regardless of the aDefer... parameter), as we
  // take this to mean availability of font info is to be prioritized over
  // potential startup perf or main-thread jank.
  // (This is used so we can reliably run reftests that depend on localized
  // font-family names being available.)
  if (aDeferOtherFamilyNamesLoading &&
      StaticPrefs::gfx_font_loader_delay_AtStartup() > 0) {
    if (!mPendingOtherFamilyNameTask) {
      RefPtr<mozilla::CancelableRunnable> task =
          new InitOtherFamilyNamesRunnable();
      mPendingOtherFamilyNameTask = task;
      NS_DispatchToMainThreadQueue(task.forget(), EventQueuePriority::Idle);
    }
  } else {
    InitOtherFamilyNamesInternal(false);
  }
  return mOtherFamilyNamesInitialized;
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

  for (const auto& entry : mFontFamilies) {
    nsCStringHashKey::KeyType key = entry.GetKey();
    const RefPtr<gfxFontFamily>& family = entry.GetData();

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
      auto* families = list->Families();
      if (families) {
        family = &families[rec->mFamilyIndex];
        face = static_cast<fontlist::Face*>(
            family->Faces(list)[rec->mFaceIndex].ToPtr(list));
      }
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

void gfxPlatformFontList::UpdateFontList(bool aFullRebuild) {
  MOZ_ASSERT(NS_IsMainThread());
  if (aFullRebuild) {
    InitFontList();
    RebuildLocalFonts();
  } else {
    // The font list isn't being fully rebuilt, we're just being notified that
    // character maps have been updated and so font fallback needs to be re-
    // done. We only care about this if we have previously encountered a
    // fallback that required cmaps that were not yet available, and so we
    // asked for the async cmap loader to run.
    if (mStartedLoadingCmapsFrom != 0xffffffffu) {
      InitializeCodepointsWithNoFonts();
      mStartedLoadingCmapsFrom = 0xffffffffu;
      gfxPlatform::ForceGlobalReflow();
    }
  }
}

bool gfxPlatformFontList::IsVisibleToCSS(const gfxFontFamily& aFamily) const {
  return aFamily.Visibility() <= mVisibilityLevel ||
         IsFontFamilyWhitelistActive();
}

bool gfxPlatformFontList::IsVisibleToCSS(
    const fontlist::Family& aFamily) const {
  return aFamily.Visibility() <= mVisibilityLevel ||
         IsFontFamilyWhitelistActive();
}

void gfxPlatformFontList::GetFontList(nsAtom* aLangGroup,
                                      const nsACString& aGenericFamily,
                                      nsTArray<nsString>& aListOfFonts) {
  if (SharedFontList()) {
    fontlist::FontList* list = SharedFontList();
    const fontlist::Family* families = list->Families();
    if (families) {
      for (uint32_t i = 0; i < list->NumFamilies(); i++) {
        auto& f = families[i];
        if (!IsVisibleToCSS(f) || f.IsAltLocaleFamily()) {
          continue;
        }
        // XXX TODO: filter families for aGenericFamily, if supported by
        // platform
        aListOfFonts.AppendElement(
            NS_ConvertUTF8toUTF16(list->LocalizedFamilyName(&f)));
      }
    }
    return;
  }

  MutexAutoLock lock(mFontFamiliesMutex);
  for (const auto& entry : mFontFamilies) {
    const RefPtr<gfxFontFamily>& family = entry.GetData();
    if (!IsVisibleToCSS(*family)) {
      continue;
    }
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
  for (const auto& entry : mFontFamilies) {
    const RefPtr<gfxFontFamily>& family = entry.GetData();
    aFamilyArray.AppendElement(family);
  }
}

gfxFont* gfxPlatformFontList::SystemFindFontForChar(
    uint32_t aCh, uint32_t aNextCh, Script aRunScript,
    eFontPresentation aPresentation, const gfxFontStyle* aStyle,
    FontVisibility* aVisibility, FontMatchingStats* aFontMatchingStats) {
  MOZ_ASSERT(!mCodepointsWithNoFonts.test(aCh),
             "don't call for codepoints already known to be unsupported");

  // Try to short-circuit font fallback for U+FFFD, used to represent
  // encoding errors: just use cached family from last time U+FFFD was seen.
  // This helps speed up pages with lots of encoding errors, binary-as-text,
  // etc.
  if (aCh == 0xFFFD) {
    gfxFontEntry* fontEntry = nullptr;
    if (mReplacementCharFallbackFamily.mIsShared &&
        mReplacementCharFallbackFamily.mShared) {
      fontlist::Face* face =
          mReplacementCharFallbackFamily.mShared->FindFaceForStyle(
              SharedFontList(), *aStyle);
      if (face) {
        fontEntry =
            GetOrCreateFontEntry(face, mReplacementCharFallbackFamily.mShared);
        *aVisibility = mReplacementCharFallbackFamily.mShared->Visibility();
      }
    } else if (!mReplacementCharFallbackFamily.mIsShared &&
               mReplacementCharFallbackFamily.mUnshared) {
      fontEntry =
          mReplacementCharFallbackFamily.mUnshared->FindFontForStyle(*aStyle);
      *aVisibility = mReplacementCharFallbackFamily.mUnshared->Visibility();
    }

    // this should never fail, as we must have found U+FFFD in order to set
    // mReplacementCharFallbackFamily at all, but better play it safe
    if (fontEntry && fontEntry->HasCharacter(aCh)) {
      return fontEntry->FindOrMakeFont(aStyle);
    }
  }

  TimeStamp start = TimeStamp::Now();

  // search commonly available fonts
  bool common = true;
  FontFamily fallbackFamily;
  gfxFont* candidate = CommonFontFallback(
      aCh, aNextCh, aRunScript, aPresentation, aStyle, fallbackFamily);
  gfxFont* font = nullptr;
  if (candidate) {
    if (aPresentation == eFontPresentation::Any) {
      font = candidate;
    } else {
      bool hasColorGlyph = candidate->HasColorGlyphFor(aCh, aNextCh);
      if (hasColorGlyph == PrefersColor(aPresentation)) {
        font = candidate;
      }
    }
  }

  // If we didn't find a common font, or it was not the preferred type (color
  // or monochrome), do system-wide fallback (except for specials).
  uint32_t cmapCount = 0;
  if (!font) {
    common = false;
    font = GlobalFontFallback(aCh, aNextCh, aRunScript, aPresentation, aStyle,
                              cmapCount, fallbackFamily, aFontMatchingStats);
    // If the font we found doesn't match the requested type, and we also found
    // a candidate above, prefer that one.
    if (font && aPresentation != eFontPresentation::Any && candidate) {
      bool hasColorGlyph = font->HasColorGlyphFor(aCh, aNextCh);
      if (hasColorGlyph != PrefersColor(aPresentation)) {
        // We're discarding `font` and using `candidate` instead, so ensure
        // `font` is known to the global cache expiration tracker.
        RefPtr<gfxFont> autoRefDeref(font);
        font = candidate;
      }
    }
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
             (font ? font->GetFontEntry()->Name().get() : "<none>"),
             int32_t(elapsed.ToMicroseconds()), cmapCount));
  }

  // no match? add to set of non-matching codepoints
  if (!font) {
    mCodepointsWithNoFonts.set(aCh);
  } else {
    *aVisibility = fallbackFamily.mIsShared
                       ? fallbackFamily.mShared->Visibility()
                       : fallbackFamily.mUnshared->Visibility();
    if (aCh == 0xFFFD) {
      mReplacementCharFallbackFamily = fallbackFamily;
    }
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

  return font;
}

#define NUM_FALLBACK_FONTS 8

gfxFont* gfxPlatformFontList::CommonFontFallback(
    uint32_t aCh, uint32_t aNextCh, Script aRunScript,
    eFontPresentation aPresentation, const gfxFontStyle* aMatchStyle,
    FontFamily& aMatchedFamily) {
  AutoTArray<const char*, NUM_FALLBACK_FONTS> defaultFallbacks;
  gfxPlatform::GetPlatform()->GetCommonFallbackFonts(
      aCh, aRunScript, aPresentation, defaultFallbacks);
  GlobalFontMatch data(aCh, aNextCh, *aMatchStyle, aPresentation);
  if (SharedFontList()) {
    for (const auto name : defaultFallbacks) {
      fontlist::Family* family = FindSharedFamily(nsDependentCString(name));
      if (!family || !IsVisibleToCSS(*family)) {
        continue;
      }
      // XXX(jfkthame) Should we fire the async cmap-loader here, or let it
      // always do a potential sync initialization of the family?
      family->SearchAllFontsForChar(SharedFontList(), &data);
      if (data.mBestMatch) {
        aMatchedFamily = FontFamily(family);
        return data.mBestMatch->FindOrMakeFont(aMatchStyle);
      }
    }
  } else {
    for (const auto name : defaultFallbacks) {
      gfxFontFamily* fallback =
          FindFamilyByCanonicalName(nsDependentCString(name));
      if (!fallback || !IsVisibleToCSS(*fallback)) {
        continue;
      }
      fallback->FindFontForChar(&data);
      if (data.mBestMatch) {
        aMatchedFamily = FontFamily(fallback);
        return data.mBestMatch->FindOrMakeFont(aMatchStyle);
      }
    }
  }
  return nullptr;
}

gfxFont* gfxPlatformFontList::GlobalFontFallback(
    uint32_t aCh, uint32_t aNextCh, Script aRunScript,
    eFontPresentation aPresentation, const gfxFontStyle* aMatchStyle,
    uint32_t& aCmapCount, FontFamily& aMatchedFamily,
    FontMatchingStats* aFontMatchingStats) {
  bool useCmaps = IsFontFamilyWhitelistActive() ||
                  gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback();
  FontVisibility rejectedFallbackVisibility = FontVisibility::Unknown;
  if (!useCmaps) {
    // Allow platform-specific fallback code to try and find a usable font
    gfxFontEntry* fe = PlatformGlobalFontFallback(aCh, aRunScript, aMatchStyle,
                                                  aMatchedFamily);
    if (fe) {
      if (aMatchedFamily.mIsShared) {
        if (IsVisibleToCSS(*aMatchedFamily.mShared)) {
          gfxFont* font = fe->FindOrMakeFont(aMatchStyle);
          if (font) {
            if (aPresentation == eFontPresentation::Any) {
              return font;
            }
            bool hasColorGlyph = font->HasColorGlyphFor(aCh, aNextCh);
            if (hasColorGlyph == PrefersColor(aPresentation)) {
              return font;
            }
            // If we don't use this font, we need to touch its refcount
            // to trigger gfxFontCache expiration tracking.
            RefPtr<gfxFont> autoRefDeref(font);
          }
        }
        rejectedFallbackVisibility = aMatchedFamily.mShared->Visibility();
      } else {
        if (IsVisibleToCSS(*aMatchedFamily.mUnshared)) {
          gfxFont* font = fe->FindOrMakeFont(aMatchStyle);
          if (font) {
            if (aPresentation == eFontPresentation::Any) {
              return font;
            }
            bool hasColorGlyph = font->HasColorGlyphFor(aCh, aNextCh);
            if (hasColorGlyph == PrefersColor(aPresentation)) {
              return font;
            }
            RefPtr<gfxFont> autoRefDeref(font);
          }
        }
        rejectedFallbackVisibility = aMatchedFamily.mUnshared->Visibility();
      }
    }
  }

  // otherwise, try to find it among local fonts
  GlobalFontMatch data(aCh, aNextCh, *aMatchStyle, aPresentation);
  if (SharedFontList()) {
    fontlist::Family* families = SharedFontList()->Families();
    if (families) {
      for (uint32_t i = 0; i < SharedFontList()->NumFamilies(); i++) {
        fontlist::Family& family = families[i];
        if (!IsVisibleToCSS(family)) {
          continue;
        }
        if (!family.IsFullyInitialized() &&
            StaticPrefs::gfx_font_rendering_fallback_async()) {
          // Start loading all the missing charmaps; but this is async,
          // so for now we just continue, ignoring this family.
          StartCmapLoadingFromFamily(i);
        } else {
          family.SearchAllFontsForChar(SharedFontList(), &data);
          if (data.mMatchDistance == 0.0) {
            // no better style match is possible, so stop searching
            break;
          }
        }
      }
      if (data.mBestMatch) {
        aMatchedFamily = FontFamily(data.mMatchedSharedFamily);
        return data.mBestMatch->FindOrMakeFont(aMatchStyle);
      }
    }
  } else {
    // iterate over all font families to find a font that support the
    // character
    for (const auto& entry : mFontFamilies) {
      const RefPtr<gfxFontFamily>& family = entry.GetData();
      if (!IsVisibleToCSS(*family)) {
        continue;
      }
      // evaluate all fonts in this family for a match
      family->FindFontForChar(&data);
      if (data.mMatchDistance == 0.0) {
        // no better style match is possible, so stop searching
        break;
      }
    }

    aCmapCount = data.mCmapsTested;
    if (data.mBestMatch) {
      aMatchedFamily = FontFamily(data.mMatchedFamily);
      return data.mBestMatch->FindOrMakeFont(aMatchStyle);
    }
  }

  if (aFontMatchingStats) {
    if (rejectedFallbackVisibility == FontVisibility::LangPack) {
      aFontMatchingStats->mFallbacks |= FallbackTypes::MissingFontLangPack;
    } else if (rejectedFallbackVisibility == FontVisibility::User) {
      aFontMatchingStats->mFallbacks |= FallbackTypes::MissingFontUser;
    }
  }

  return nullptr;
}

class StartCmapLoadingRunnable : public mozilla::Runnable {
 public:
  explicit StartCmapLoadingRunnable(uint32_t aStartIndex)
      : Runnable("gfxPlatformFontList::StartCmapLoadingRunnable"),
        mStartIndex(aStartIndex) {}

  NS_IMETHOD Run() override {
    auto* pfl = gfxPlatformFontList::PlatformFontList();
    auto* list = pfl->SharedFontList();
    if (!list) {
      return NS_OK;
    }
    if (mStartIndex >= list->NumFamilies()) {
      return NS_OK;
    }
    if (XRE_IsParentProcess()) {
      pfl->StartCmapLoading(list->GetGeneration(), mStartIndex);
    } else {
      dom::ContentChild::GetSingleton()->SendStartCmapLoading(
          list->GetGeneration(), mStartIndex);
    }
    return NS_OK;
  }

 private:
  uint32_t mStartIndex;
};

void gfxPlatformFontList::StartCmapLoadingFromFamily(uint32_t aStartIndex) {
  if (aStartIndex > mStartedLoadingCmapsFrom) {
    // We already initiated cmap-loading from somewhere earlier in the list;
    // no need to do it again here.
    return;
  }
  mStartedLoadingCmapsFrom = aStartIndex;

  // If we're already on the main thread, don't bother dispatching a runnable
  // here to kick off the loading process, just do it directly.
  if (NS_IsMainThread()) {
    auto* list = SharedFontList();
    if (XRE_IsParentProcess()) {
      StartCmapLoading(list->GetGeneration(), aStartIndex);
    } else {
      dom::ContentChild::GetSingleton()->SendStartCmapLoading(
          list->GetGeneration(), aStartIndex);
    }
  } else {
    NS_DispatchToMainThread(new StartCmapLoadingRunnable(aStartIndex));
  }
}

class LoadCmapsRunnable : public CancelableRunnable {
 public:
  explicit LoadCmapsRunnable(uint32_t aGeneration, uint32_t aFamilyIndex)
      : CancelableRunnable("gfxPlatformFontList::LoadCmapsRunnable"),
        mGeneration(aGeneration),
        mStartIndex(aFamilyIndex),
        mIndex(aFamilyIndex) {}

  // Reset the current family index, if the value passed is earlier than our
  // original starting position. We don't "reset" if it would move the current
  // position forward, or back into the already-scanned range.
  // We could optimize further by remembering the current position reached,
  // and then skipping ahead from the original start, but it doesn't seem worth
  // extra complexity for a task that usually only happens once, and already-
  // processed families will be skipped pretty quickly in Run() anyhow.
  void MaybeResetIndex(uint32_t aFamilyIndex) {
    if (aFamilyIndex < mStartIndex) {
      mStartIndex = aFamilyIndex;
      mIndex = aFamilyIndex;
    }
  }

  nsresult Cancel() override {
    mIsCanceled = true;
    return NS_OK;
  }

  NS_IMETHOD Run() override {
    if (mIsCanceled) {
      return NS_OK;
    }
    auto* pfl = gfxPlatformFontList::PlatformFontList();
    auto* list = pfl->SharedFontList();
    MOZ_ASSERT(list);
    if (!list) {
      return NS_OK;
    }
    if (mGeneration != list->GetGeneration()) {
      return NS_OK;
    }
    uint32_t numFamilies = list->NumFamilies();
    if (mIndex >= numFamilies) {
      return NS_OK;
    }
    auto* families = list->Families();
    // Skip any families that are already initialized.
    while (mIndex < numFamilies && families[mIndex].IsFullyInitialized()) {
      ++mIndex;
    }
    // Fully process one family, and advance index.
    if (mIndex < numFamilies) {
      Unused << pfl->InitializeFamily(&families[mIndex], true);
      ++mIndex;
    }
    // If there are more families to initialize, post ourselves back to the
    // idle queue to handle the next one; otherwise we're finished and we need
    // to notify content processes to update their rendering.
    if (mIndex < numFamilies) {
      RefPtr<CancelableRunnable> task = this;
      NS_DispatchToMainThreadQueue(task.forget(), EventQueuePriority::Idle);
    } else {
      pfl->CancelLoadCmapsTask();
      pfl->InitializeCodepointsWithNoFonts();
      dom::ContentParent::NotifyUpdatedFonts(false);
    }
    return NS_OK;
  }

 private:
  uint32_t mGeneration;
  uint32_t mStartIndex;
  uint32_t mIndex;
  bool mIsCanceled = false;
};

void gfxPlatformFontList::StartCmapLoading(uint32_t aGeneration,
                                           uint32_t aStartIndex) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  if (aGeneration != SharedFontList()->GetGeneration()) {
    return;
  }
  if (mLoadCmapsRunnable) {
    // We already have a runnable; just make sure it covers the full range of
    // families needed.
    static_cast<LoadCmapsRunnable*>(mLoadCmapsRunnable.get())
        ->MaybeResetIndex(aStartIndex);
    return;
  }
  mLoadCmapsRunnable = new LoadCmapsRunnable(aGeneration, aStartIndex);
  RefPtr<CancelableRunnable> task = mLoadCmapsRunnable;
  NS_DispatchToMainThreadQueue(task.forget(), EventQueuePriority::Idle);
}

gfxFontFamily* gfxPlatformFontList::CheckFamily(gfxFontFamily* aFamily) {
  if (aFamily && !aFamily->HasStyles()) {
    aFamily->FindStyleVariations();
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
    gfxFontStyle* aStyle, nsAtom* aLanguage, gfxFloat aDevToCssSize) {
  nsAutoCString key;
  GenerateFontListKey(aFamily, key);

  bool allowHidden = bool(aFlags & FindFamiliesFlags::eSearchHiddenFamilies);
  if (SharedFontList()) {
    fontlist::Family* family = SharedFontList()->FindFamily(key);
    // If not found, and other family names have not yet been initialized,
    // initialize the rest of the list and try again. This is done lazily
    // since reading name table entries is expensive.
    // Although ASCII localized family names are possible they don't occur
    // in practice, so avoid pulling in names at startup.
    if (!family && !mOtherFamilyNamesInitialized) {
      bool triggerLoading = true;
      bool mayDefer =
          !(aFlags & FindFamiliesFlags::eForceOtherFamilyNamesLoading);
      if (IsAscii(key)) {
        // If `key` is an ASCII name, only trigger loading if it includes a
        // space, and the "base" name (up to the last space) exists as a known
        // family, so that this might be a legacy styled-family name.
        const char* data = key.BeginReading();
        int32_t index = key.Length();
        while (--index > 0) {
          if (data[index] == ' ') {
            break;
          }
        }
        if (index <= 0 ||
            !SharedFontList()->FindFamily(nsAutoCString(key.get(), index))) {
          triggerLoading = false;
        }
      }
      if (triggerLoading) {
        if (InitOtherFamilyNames(mayDefer)) {
          family = SharedFontList()->FindFamily(key);
        }
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
    // Check whether the family we found is actually allowed to be looked up,
    // according to current font-visibility prefs.
    if (family &&
        (IsVisibleToCSS(*family) || (allowHidden && family->IsHidden()))) {
      aOutput->AppendElement(FamilyAndGeneric(family, aGeneric));
      return true;
    }
    return false;
  }

  NS_ASSERTION(mFontFamilies.Count() != 0,
               "system font list was not initialized correctly");

  // lookup in canonical (i.e. English) family name list
  gfxFontFamily* familyEntry = mFontFamilies.GetWeak(key);
  if (familyEntry && !IsVisibleToCSS(*familyEntry) &&
      !(allowHidden && familyEntry->IsHidden())) {
    return false;
  }

  // if not found, lookup in other family names list (mostly localized names)
  if (!familyEntry) {
    familyEntry = mOtherFamilyNames.GetWeak(key);
  }
  if (familyEntry && !IsVisibleToCSS(*familyEntry) &&
      !(allowHidden && familyEntry->IsHidden())) {
    return false;
  }

  // if still not found and other family names not yet fully initialized,
  // initialize the rest of the list and try again.  this is done lazily
  // since reading name table entries is expensive.
  // although ASCII localized family names are possible they don't occur
  // in practice so avoid pulling in names at startup
  if (!familyEntry && !mOtherFamilyNamesInitialized && !IsAscii(aFamily)) {
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
    if (familyEntry && !IsVisibleToCSS(*familyEntry) &&
        !(allowHidden && familyEntry->IsHidden())) {
      return false;
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
      if (familyEntry && !IsVisibleToCSS(*familyEntry) &&
          !(allowHidden && familyEntry->IsHidden())) {
        return false;
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
    nsAtom* aLanguage, gfxFloat aDevToCss) {
  if (!SharedFontList()) {
    return nullptr;
  }
  AutoTArray<FamilyAndGeneric, 1> families;
  if (!FindAndAddFamilies(StyleGenericFontFamily::None, aFamily, &families,
                          aFlags, aStyle, aLanguage, aDevToCss) ||
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

class InitializeFamilyRunnable : public mozilla::Runnable {
 public:
  explicit InitializeFamilyRunnable(uint32_t aFamilyIndex, bool aLoadCmaps)
      : Runnable("gfxPlatformFontList::InitializeFamilyRunnable"),
        mIndex(aFamilyIndex),
        mLoadCmaps(aLoadCmaps) {}

  NS_IMETHOD Run() override {
    auto list = gfxPlatformFontList::PlatformFontList()->SharedFontList();
    if (!list) {
      return NS_OK;
    }
    if (mIndex >= list->NumFamilies()) {
      // Out of range? Maybe the list got reinitialized since this request
      // was posted - just ignore it.
      return NS_OK;
    }
    dom::ContentChild::GetSingleton()->SendInitializeFamily(
        list->GetGeneration(), mIndex, mLoadCmaps);
    return NS_OK;
  }

 private:
  uint32_t mIndex;
  bool mLoadCmaps;
};

bool gfxPlatformFontList::InitializeFamily(fontlist::Family* aFamily,
                                           bool aLoadCmaps) {
  MOZ_ASSERT(SharedFontList());
  auto list = SharedFontList();
  if (!XRE_IsParentProcess()) {
    auto* families = list->Families();
    if (!families) {
      return false;
    }
    uint32_t index = aFamily - families;
    if (index >= list->NumFamilies()) {
      return false;
    }
    if (NS_IsMainThread()) {
      dom::ContentChild::GetSingleton()->SendInitializeFamily(
          list->GetGeneration(), index, aLoadCmaps);
    } else {
      NS_DispatchToMainThread(new InitializeFamilyRunnable(index, aLoadCmaps));
    }
    return aFamily->IsInitialized();
  }

  if (!aFamily->IsInitialized()) {
    // The usual case: we're being asked to populate the face list.
    AutoTArray<fontlist::Face::InitData, 16> faceList;
    GetFacesInitDataForFamily(aFamily, faceList, aLoadCmaps);
    aFamily->AddFaces(list, faceList);
  } else {
    // The family's face list was already initialized, but if aLoadCmaps is
    // true we also want to eagerly load character maps. This is used when a
    // child process is doing SearchAllFontsForChar, to have the parent load
    // all the cmaps at once and reduce IPC traffic (and content-process file
    // access overhead, which is crippling for DirectWrite on Windows).
    if (aLoadCmaps) {
      auto* faces = aFamily->Faces(list);
      if (faces) {
        for (size_t i = 0; i < aFamily->NumFaces(); i++) {
          auto* face = static_cast<fontlist::Face*>(faces[i].ToPtr(list));
          if (face && face->mCharacterMap.IsNull()) {
            // We don't want to cache this font entry, as the parent will most
            // likely never use it again; it's just to populate the charmap for
            // the benefit of the child process.
            RefPtr<gfxFontEntry> fe = CreateFontEntry(face, aFamily);
            if (fe) {
              fe->ReadCMAP();
            }
          }
        }
      }
    }
  }

  if (aLoadCmaps && aFamily->IsInitialized()) {
    aFamily->SetupFamilyCharMap(list);
  }

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
  return mFontEntries
      .LookupOrInsertWith(aFace,
                          [=] { return CreateFontEntry(aFace, aFamily); })
      .get();
}

void gfxPlatformFontList::AddOtherFamilyName(
    gfxFontFamily* aFamilyEntry, const nsCString& aOtherFamilyName) {
  nsAutoCString key;
  GenerateFontListKey(aOtherFamilyName, key);

  mOtherFamilyNames.LookupOrInsertWith(key, [&] {
    LOG_FONTLIST(
        ("(fontlist-otherfamily) canonical family: %s, "
         "other family: %s\n",
         aFamilyEntry->Name().get(), aOtherFamilyName.get()));
    if (mBadUnderlineFamilyNames.ContainsSorted(key)) {
      aFamilyEntry->SetBadUnderlineFamily();
    }
    return RefPtr{aFamilyEntry};
  });
}

void gfxPlatformFontList::AddFullname(gfxFontEntry* aFontEntry,
                                      const nsCString& aFullname) {
  mExtraNames->mFullnames.LookupOrInsertWith(aFullname, [&] {
    LOG_FONTLIST(("(fontlist-fullname) name: %s, fullname: %s\n",
                  aFontEntry->Name().get(), aFullname.get()));
    return RefPtr{aFontEntry};
  });
}

void gfxPlatformFontList::AddPostscriptName(gfxFontEntry* aFontEntry,
                                            const nsCString& aPostscriptName) {
  mExtraNames->mPostscriptNames.LookupOrInsertWith(aPostscriptName, [&] {
    LOG_FONTLIST(("(fontlist-postscript) name: %s, psname: %s\n",
                  aFontEntry->Name().get(), aPostscriptName.get()));
    return RefPtr{aFontEntry};
  });
}

bool gfxPlatformFontList::GetStandardFamilyName(const nsCString& aFontName,
                                                nsACString& aFamilyName) {
  FontFamily family = FindFamily(aFontName);
  if (family.IsNull()) {
    return false;
  }
  return GetLocalizedFamilyName(FindFamily(aFontName), aFamilyName);
}

bool gfxPlatformFontList::GetLocalizedFamilyName(const FontFamily& aFamily,
                                                 nsACString& aFamilyName) {
  if (aFamily.mIsShared) {
    if (aFamily.mShared) {
      aFamilyName = SharedFontList()->LocalizedFamilyName(aFamily.mShared);
      return true;
    }
  } else if (aFamily.mUnshared) {
    aFamily.mUnshared->LocalizedName(aFamilyName);
    return true;
  }
  return false;  // leaving the aFamilyName outparam untouched
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
    AutoTArray<FamilyAndGeneric, 10> families;
    FindAndAddFamilies(aGenericType, genericFamily, &families,
                       FindFamiliesFlags(0), nullptr, aLangGroup);
    for (const FamilyAndGeneric& f : families) {
      if (!aGenericFamilies->Contains(f.mFamily)) {
        aGenericFamilies->AppendElement(f.mFamily);
      }
    }
  }
}

gfxPlatformFontList::PrefFontList* gfxPlatformFontList::GetPrefFontsLangGroup(
    StyleGenericFontFamily aGenericType, eFontPrefLang aPrefLang) {
  if (aGenericType == StyleGenericFontFamily::MozEmoji ||
      aPrefLang == eFontPrefLang_Emoji) {
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
    case UBLOCK_MATHEMATICAL_OPERATORS:
    case UBLOCK_MATHEMATICAL_ALPHANUMERIC_SYMBOLS:
    case UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_A:
    case UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_B:
    case UBLOCK_SUPPLEMENTAL_MATHEMATICAL_OPERATORS:
      return eFontPrefLang_Mathematics;
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
    // order. We use gfxFontUtils::GetPrefsFontList to read the list even
    // though it's not actually a list of fonts but of lang codes; the format
    // is the same.
    AutoTArray<nsCString, 5> list;
    gfxFontUtils::GetPrefsFontList("intl.accept_languages", list, true);
    for (const auto& lang : list) {
      eFontPrefLang fpl = GetFontPrefLangFor(lang.get());
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
    }

    // Try using app's locale
    nsAutoCString localeStr;
    LocaleService::GetInstance()->GetAppLocaleAsBCP47(localeStr);

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
    prefLocales.AppendElement("ja"_ns);
    prefLocales.AppendElement("zh-CN"_ns);
    prefLocales.AppendElement("zh-TW"_ns);
    prefLocales.AppendElement("zh-HK"_ns);
    prefLocales.AppendElement("ko"_ns);

    AutoTArray<nsCString, 16> sysLocales;
    AutoTArray<nsCString, 16> negLocales;
    if (NS_SUCCEEDED(
            OSPreferences::GetInstance()->GetSystemLocales(sysLocales))) {
      LocaleService::GetInstance()->NegotiateLanguages(
          sysLocales, prefLocales, ""_ns,
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

    // Last resort... set up CJK font prefs in the order listed by the user-
    // configurable ordering pref.
    gfxFontUtils::GetPrefsFontList(kCJKFallbackOrderPref, list);
    for (const auto& item : list) {
      eFontPrefLang fpl = GetFontPrefLangFor(item.get());
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
    }

    // Truly-last resort... try Chinese font prefs before Japanese because
    // they tend to have more complete character coverage, and therefore less
    // risk of "ransom-note" effects.
    // (If the kCJKFallbackOrderPref was fully populated, as it is by default,
    // this will do nothing as all these values are already present.)
    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseCN);
    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseHK);
    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseTW);
    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Japanese);
    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Korean);

    // copy into the cached array
    for (const auto lang : Span<eFontPrefLang>(tempPrefLangs, tempLen)) {
      mCJKPrefLangs.AppendElement(lang);
    }
  }

  // append in cached CJK langs
  for (const auto lang : mCJKPrefLangs) {
    AppendPrefLang(aPrefLangs, aLen, eFontPrefLang(lang));
  }
}

void gfxPlatformFontList::AppendPrefLang(eFontPrefLang aPrefLangs[],
                                         uint32_t& aLen,
                                         eFontPrefLang aAddLang) {
  if (aLen >= kMaxLenPrefLangList) {
    return;
  }

  // If the lang is already present, just ignore the addition.
  for (const auto lang : Span<eFontPrefLang>(aPrefLangs, aLen)) {
    if (lang == aAddLang) {
      return;
    }
  }

  aPrefLangs[aLen++] = aAddLang;
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
  return FontFamily(mFontFamilies.ConstIter().Data());
}

void gfxPlatformFontList::GetFontFamilyNames(
    nsTArray<nsCString>& aFontFamilyNames) {
  if (SharedFontList()) {
    fontlist::FontList* list = SharedFontList();
    const fontlist::Family* families = list->Families();
    if (families) {
      for (uint32_t i = 0, n = list->NumFamilies(); i < n; i++) {
        const fontlist::Family& family = families[i];
        if (!family.IsHidden()) {
          aFontFamilyNames.AppendElement(family.DisplayName().AsString(list));
        }
      }
    }
  } else {
    for (const auto& entry : mFontFamilies) {
      const RefPtr<gfxFontFamily>& family = entry.GetData();
      if (!family->IsHidden()) {
        aFontFamilyNames.AppendElement(family->Name());
      }
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
  // type should be standard generic type at this point
  // map generic type to string
  switch (aGenericType) {
    case StyleGenericFontFamily::Serif:
      return "serif";
    case StyleGenericFontFamily::SansSerif:
      return "sans-serif";
    case StyleGenericFontFamily::Monospace:
      return "monospace";
    case StyleGenericFontFamily::Cursive:
      return "cursive";
    case StyleGenericFontFamily::Fantasy:
      return "fantasy";
    case StyleGenericFontFamily::MozEmoji:
      return "-moz-emoji";
    case StyleGenericFontFamily::None:
      break;
  }
  MOZ_ASSERT_UNREACHABLE("Unknown generic");
  return nullptr;
}

void gfxPlatformFontList::InitLoader() {
  GetFontFamilyNames(mFontInfo->mFontFamiliesToLoad);
  mStartIndex = 0;
  mNumFamilies = mFontInfo->mFontFamiliesToLoad.Length();
  memset(&(mFontInfo->mLoadStats), 0, sizeof(mFontInfo->mLoadStats));
}

#define FONT_LOADER_MAX_TIMESLICE \
  20  // max time for one pass through RunLoader = 20ms

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

    // Limit the time spent reading fonts in one pass, unless the font-loader
    // delay was set to zero, in which case we run to completion even if it
    // causes some jank.
    if (StaticPrefs::gfx_font_loader_delay_AtStartup() > 0) {
      TimeDuration elapsed = TimeStamp::Now() - start;
      if (elapsed.ToMilliseconds() > FONT_LOADER_MAX_TIMESLICE &&
          i + 1 != endIndex) {
        endIndex = i + 1;
        break;
      }
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
  uint32_t delay = std::max(1u, StaticPrefs::gfx_font_loader_delay_AtStartup());
  StartLoader(delay);
}

void gfxPlatformFontList::ForceGlobalReflow() {
  gfxPlatform::ForceGlobalReflow();
}

void gfxPlatformFontList::RebuildLocalFonts(bool aForgetLocalFaces) {
  for (auto it = mUserFontSetList.ConstIter(); !it.Done(); it.Next()) {
    auto* fontset = it.Get()->GetKey();
    if (aForgetLocalFaces) {
      fontset->ForgetLocalFaces();
    }
    fontset->RebuildLocalRules();
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
  for (const auto& entry : mFontFamilies) {
    aSizes->mFontListSize +=
        entry.GetKey().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    entry.GetData()->AddSizeOfIncludingThis(aMallocSizeOf, aSizes);
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
  for (const auto& entry : mSharedCmaps) {
    aSizes->mCharMapsSize += entry.GetKey()->SizeOfIncludingThis(aMallocSizeOf);
  }

  aSizes->mFontListSize +=
      mFontEntries.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& entry : mFontEntries) {
    if (entry.GetData()) {
      entry.GetData()->AddSizeOfIncludingThis(aMallocSizeOf, aSizes);
    }
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
      // If the gfxFontInfoLoader task is not yet running, kick it off now so
      // that it will load remaining names etc as soon as idle time permits.
      if (mState == stateInitial || mState == stateTimerOnDelay) {
        StartLoader(0);
        timedOut = true;
      }
    } else {
      for (const auto& entry : mFontFamilies) {
        const RefPtr<gfxFontFamily>& family = entry.GetData();
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
      for (const auto& entry : mFontFamilies) {
        const RefPtr<gfxFontFamily>& family = entry.GetData();
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
  if (list && XRE_IsParentProcess()) {
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
    uint32_t aGeneration, uint32_t aIndex, base::ProcessId aPid,
    base::SharedMemoryHandle* aOut) {
  auto list = SharedFontList();
  if (!list) {
    return;
  }
  if (!aGeneration || list->GetGeneration() == aGeneration) {
    list->ShareShmBlockToProcess(aIndex, aPid, aOut);
  } else {
    *aOut = base::SharedMemory::NULLHandle();
  }
}

void gfxPlatformFontList::ShareFontListToProcess(
    nsTArray<base::SharedMemoryHandle>* aBlocks, base::ProcessId aPid) {
  auto list = SharedFontList();
  if (list) {
    list->ShareBlocksToProcess(aBlocks, aPid);
  }
}

void gfxPlatformFontList::InitializeFamily(uint32_t aGeneration,
                                           uint32_t aFamilyIndex,
                                           bool aLoadCmaps) {
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
  if (!family->IsInitialized() || aLoadCmaps) {
    Unused << InitializeFamily(family, aLoadCmaps);
  }
}

void gfxPlatformFontList::SetCharacterMap(uint32_t aGeneration,
                                          const fontlist::Pointer& aFacePtr,
                                          const gfxSparseBitSet& aMap) {
  MOZ_ASSERT(XRE_IsParentProcess());
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
    face->mCharacterMap = GetShmemCharMap(&aMap);
  }
}

void gfxPlatformFontList::SetupFamilyCharMap(
    uint32_t aGeneration, const fontlist::Pointer& aFamilyPtr) {
  MOZ_ASSERT(XRE_IsParentProcess());
  auto list = SharedFontList();
  MOZ_ASSERT(list);
  if (!list) {
    return;
  }
  if (list->GetGeneration() != aGeneration) {
    return;
  }

  // aFamilyPtr was passed from a content process which may not be trusted,
  // so we cannot assume it is valid or safe to use. If the Pointer value is
  // bad, we must not crash or do anything bad, just bail out.
  // (In general, if the child process was trying to use an invalid pointer it
  // should have hit the MOZ_DIAGNOSTIC_ASSERT in FontList::ToSharedPointer
  // rather than passing a null or bad pointer to the parent.)

  auto* family = static_cast<fontlist::Family*>(aFamilyPtr.ToPtr(list));
  if (!family) {
    // Unable to resolve to a native pointer (or it was null).
    NS_WARNING("unexpected null Family pointer");
    return;
  }

  // Validate the pointer before trying to use it: check that it points to a
  // correctly-aligned offset within the Families() or AliasFamilies() array.
  // We just assert (in debug builds only) on failure, and return safely.
  // A misaligned pointer here would indicate a buggy (or compromised) child
  // process, but crashing the parent would be unnecessary and does not yield
  // any useful insight.
  if (family >= list->Families() &&
      family < list->Families() + list->NumFamilies()) {
    size_t offset = (char*)family - (char*)list->Families();
    if (offset % sizeof(fontlist::Family) != 0) {
      MOZ_ASSERT(false, "misaligned Family pointer");
      return;
    }
  } else if (family >= list->AliasFamilies() &&
             family < list->AliasFamilies() + list->NumAliases()) {
    size_t offset = (char*)family - (char*)list->AliasFamilies();
    if (offset % sizeof(fontlist::Family) != 0) {
      MOZ_ASSERT(false, "misaligned Family pointer");
      return;
    }
  } else {
    MOZ_ASSERT(false, "not a valid Family or AliasFamily pointer");
    return;
  }

  family->SetupFamilyCharMap(list);
}

bool gfxPlatformFontList::InitOtherFamilyNames(uint32_t aGeneration,
                                               bool aDefer) {
  auto list = SharedFontList();
  MOZ_ASSERT(list);
  if (!list) {
    return false;
  }
  if (list->GetGeneration() != aGeneration) {
    return false;
  }
  return InitOtherFamilyNames(aDefer);
}

uint32_t gfxPlatformFontList::GetGeneration() const {
  return SharedFontList() ? SharedFontList()->GetGeneration() : 0;
}

#undef LOG
#undef LOG_ENABLED
