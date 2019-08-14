/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHyphenationManager.h"
#include "nsHyphenator.h"
#include "nsAtom.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsIProperties.h"
#include "nsISimpleEnumerator.h"
#include "nsIDirectoryEnumerator.h"
#include "nsDirectoryServiceDefs.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "mozilla/CountingAllocatorBase.h"
#include "mozilla/Preferences.h"
#include "nsZipArchive.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsCRT.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsMemory.h"

using namespace mozilla;

static const char kIntlHyphenationAliasPrefix[] = "intl.hyphenation-alias.";
static const char kMemoryPressureNotification[] = "memory-pressure";

class HyphenReporter final : public nsIMemoryReporter,
                             public CountingAllocatorBase<HyphenReporter> {
 private:
  ~HyphenReporter() = default;

 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override {
    size_t total = MemoryAllocated();
    if (nsHyphenationManager::Instance()) {
      total += nsHyphenationManager::Instance()->SizeOfIncludingThis(
          moz_malloc_size_of);
    }
    MOZ_COLLECT_REPORT("explicit/hyphenation", KIND_HEAP, UNITS_BYTES, total,
                       "Memory used by hyphenation data.");
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(HyphenReporter, nsIMemoryReporter)

template <>
CountingAllocatorBase<HyphenReporter>::AmountType
    CountingAllocatorBase<HyphenReporter>::sAmount(0);

/**
 * Allocation wrappers to track the amount of memory allocated by libhyphen.
 * Note that libhyphen assumes its malloc/realloc functions are infallible!
 */
extern "C" {
void* hnj_malloc(size_t aSize);
void* hnj_realloc(void* aPtr, size_t aSize);
void hnj_free(void* aPtr);
};

void* hnj_malloc(size_t aSize) {
  return HyphenReporter::InfallibleCountingMalloc(aSize);
}

void* hnj_realloc(void* aPtr, size_t aSize) {
  return HyphenReporter::InfallibleCountingRealloc(aPtr, aSize);
}

void hnj_free(void* aPtr) { HyphenReporter::CountingFree(aPtr); }

nsHyphenationManager* nsHyphenationManager::sInstance = nullptr;

NS_IMPL_ISUPPORTS(nsHyphenationManager::MemoryPressureObserver, nsIObserver)

NS_IMETHODIMP
nsHyphenationManager::MemoryPressureObserver::Observe(nsISupports* aSubject,
                                                      const char* aTopic,
                                                      const char16_t* aData) {
  if (!nsCRT::strcmp(aTopic, kMemoryPressureNotification)) {
    // We don't call Instance() here, as we don't want to create a hyphenation
    // manager if there isn't already one in existence.
    // (This observer class is local to the hyphenation manager, so it can use
    // the protected members directly.)
    if (nsHyphenationManager::sInstance) {
      nsHyphenationManager::sInstance->mHyphenators.Clear();
    }
  }
  return NS_OK;
}

nsHyphenationManager* nsHyphenationManager::Instance() {
  if (sInstance == nullptr) {
    sInstance = new nsHyphenationManager();

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(new MemoryPressureObserver, kMemoryPressureNotification,
                       false);
    }

    RegisterStrongMemoryReporter(new HyphenReporter());
  }
  return sInstance;
}

void nsHyphenationManager::Shutdown() {
  delete sInstance;
  sInstance = nullptr;
}

nsHyphenationManager::nsHyphenationManager() {
  LoadPatternList();
  LoadAliases();
}

nsHyphenationManager::~nsHyphenationManager() { sInstance = nullptr; }

already_AddRefed<nsHyphenator> nsHyphenationManager::GetHyphenator(
    nsAtom* aLocale) {
  RefPtr<nsHyphenator> hyph;
  mHyphenators.Get(aLocale, getter_AddRefs(hyph));
  if (hyph) {
    return hyph.forget();
  }
  nsCOMPtr<nsIURI> uri = mPatternFiles.Get(aLocale);
  if (!uri) {
    RefPtr<nsAtom> alias = mHyphAliases.Get(aLocale);
    if (alias) {
      mHyphenators.Get(alias, getter_AddRefs(hyph));
      if (hyph) {
        return hyph.forget();
      }
      uri = mPatternFiles.Get(alias);
      if (uri) {
        aLocale = alias;
      }
    }
    if (!uri) {
      // In the case of a locale such as "de-DE-1996", we try replacing
      // successive trailing subtags with "-*" to find fallback patterns,
      // so "de-DE-1996" -> "de-DE-*" (and then recursively -> "de-*")
      nsAtomCString localeStr(aLocale);
      if (StringEndsWith(localeStr, NS_LITERAL_CSTRING("-*"))) {
        localeStr.Truncate(localeStr.Length() - 2);
      }
      int32_t i = localeStr.RFindChar('-');
      if (i > 1) {
        localeStr.ReplaceLiteral(i, localeStr.Length() - i, "-*");
        RefPtr<nsAtom> fuzzyLocale = NS_Atomize(localeStr);
        return GetHyphenator(fuzzyLocale);
      } else {
        return nullptr;
      }
    }
  }
  nsAutoCString hyphCapPref("intl.hyphenate-capitalized.");
  hyphCapPref.Append(nsAtomCString(aLocale));
  hyph = new nsHyphenator(uri, Preferences::GetBool(hyphCapPref.get()));
  if (hyph->IsValid()) {
    mHyphenators.Put(aLocale, hyph);
    return hyph.forget();
  }
#ifdef DEBUG
  nsCString msg("failed to load patterns from ");
  msg += uri->GetSpecOrDefault();
  NS_WARNING(msg.get());
#endif
  mPatternFiles.Remove(aLocale);
  return nullptr;
}

void nsHyphenationManager::LoadPatternList() {
  mPatternFiles.Clear();
  mHyphenators.Clear();

  LoadPatternListFromOmnijar(Omnijar::GRE);
  LoadPatternListFromOmnijar(Omnijar::APP);

  nsCOMPtr<nsIProperties> dirSvc =
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc) {
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIFile> greDir;
  rv = dirSvc->Get(NS_GRE_DIR, NS_GET_IID(nsIFile), getter_AddRefs(greDir));
  if (NS_SUCCEEDED(rv)) {
    greDir->AppendNative(NS_LITERAL_CSTRING("hyphenation"));
    LoadPatternListFromDir(greDir);
  }

  nsCOMPtr<nsIFile> appDir;
  rv = dirSvc->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile),
                   getter_AddRefs(appDir));
  if (NS_SUCCEEDED(rv)) {
    appDir->AppendNative(NS_LITERAL_CSTRING("hyphenation"));
    bool equals;
    if (NS_SUCCEEDED(appDir->Equals(greDir, &equals)) && !equals) {
      LoadPatternListFromDir(appDir);
    }
  }

  nsCOMPtr<nsIFile> profileDir;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                              getter_AddRefs(profileDir));
  if (NS_SUCCEEDED(rv)) {
    profileDir->AppendNative(NS_LITERAL_CSTRING("hyphenation"));
    LoadPatternListFromDir(profileDir);
  }
}

void nsHyphenationManager::LoadPatternListFromOmnijar(Omnijar::Type aType) {
  nsCString base;
  nsresult rv = Omnijar::GetURIString(aType, base);
  if (NS_FAILED(rv)) {
    return;
  }

  RefPtr<nsZipArchive> zip = Omnijar::GetReader(aType);
  if (!zip) {
    return;
  }

  nsZipFind* find;
  zip->FindInit("hyphenation/hyph_*.dic", &find);
  if (!find) {
    return;
  }

  const char* result;
  uint16_t len;
  while (NS_SUCCEEDED(find->FindNext(&result, &len))) {
    nsCString uriString(base);
    uriString.Append(result, len);
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), uriString);
    if (NS_FAILED(rv)) {
      continue;
    }
    nsCString locale;
    rv = uri->GetPathQueryRef(locale);
    if (NS_FAILED(rv)) {
      continue;
    }
    ToLowerCase(locale);
    locale.SetLength(locale.Length() - 4);     // strip ".dic"
    locale.Cut(0, locale.RFindChar('/') + 1);  // strip directory
    if (StringBeginsWith(locale, NS_LITERAL_CSTRING("hyph_"))) {
      locale.Cut(0, 5);
    }
    for (uint32_t i = 0; i < locale.Length(); ++i) {
      if (locale[i] == '_') {
        locale.Replace(i, 1, '-');
      }
    }
    RefPtr<nsAtom> localeAtom = NS_Atomize(locale);
    if (NS_SUCCEEDED(rv)) {
      mPatternFiles.Put(localeAtom, uri);
    }
  }

  delete find;
}

void nsHyphenationManager::LoadPatternListFromDir(nsIFile* aDir) {
  nsresult rv;

  bool check = false;
  rv = aDir->Exists(&check);
  if (NS_FAILED(rv) || !check) {
    return;
  }

  rv = aDir->IsDirectory(&check);
  if (NS_FAILED(rv) || !check) {
    return;
  }

  nsCOMPtr<nsIDirectoryEnumerator> files;
  rv = aDir->GetDirectoryEntries(getter_AddRefs(files));
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(files->GetNextFile(getter_AddRefs(file))) && file) {
    nsAutoString dictName;
    file->GetLeafName(dictName);
    NS_ConvertUTF16toUTF8 locale(dictName);
    ToLowerCase(locale);
    if (!StringEndsWith(locale, NS_LITERAL_CSTRING(".dic"))) {
      continue;
    }
    if (StringBeginsWith(locale, NS_LITERAL_CSTRING("hyph_"))) {
      locale.Cut(0, 5);
    }
    locale.SetLength(locale.Length() - 4);  // strip ".dic"
    for (uint32_t i = 0; i < locale.Length(); ++i) {
      if (locale[i] == '_') {
        locale.Replace(i, 1, '-');
      }
    }
#ifdef DEBUG_hyph
    printf("adding hyphenation patterns for %s: %s\n", locale.get(),
           NS_ConvertUTF16toUTF8(dictName).get());
#endif
    RefPtr<nsAtom> localeAtom = NS_Atomize(locale);
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewFileURI(getter_AddRefs(uri), file);
    if (NS_SUCCEEDED(rv)) {
      mPatternFiles.Put(localeAtom, uri);
    }
  }
}

void nsHyphenationManager::LoadAliases() {
  nsIPrefBranch* prefRootBranch = Preferences::GetRootBranch();
  if (!prefRootBranch) {
    return;
  }
  nsTArray<nsCString> prefNames;
  nsresult rv =
      prefRootBranch->GetChildList(kIntlHyphenationAliasPrefix, prefNames);
  if (NS_SUCCEEDED(rv)) {
    for (auto& prefName : prefNames) {
      nsAutoCString value;
      rv = Preferences::GetCString(prefName.get(), value);
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString alias(prefName);
        alias.Cut(0, sizeof(kIntlHyphenationAliasPrefix) - 1);
        ToLowerCase(alias);
        ToLowerCase(value);
        RefPtr<nsAtom> aliasAtom = NS_Atomize(alias);
        RefPtr<nsAtom> valueAtom = NS_Atomize(value);
        mHyphAliases.Put(aliasAtom, valueAtom);
      }
    }
  }
}

size_t nsHyphenationManager::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) {
  size_t result = aMallocSizeOf(this);

  result += mHyphAliases.ShallowSizeOfExcludingThis(aMallocSizeOf);

  result += mPatternFiles.ShallowSizeOfExcludingThis(aMallocSizeOf);
  // Measurement of the URIs stored in mPatternFiles may be added later if DMD
  // finds it is worthwhile.

  result += mHyphenators.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto i = mHyphenators.ConstIter(); !i.Done(); i.Next()) {
    result += aMallocSizeOf(i.Data().get());
  }

  return result;
}
