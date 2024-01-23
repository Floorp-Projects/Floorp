/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComplexBreaker.h"

#include <algorithm>

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsTHashMap.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

using namespace mozilla;

using CacheMap = nsTHashMap<nsString, nsTArray<uint8_t>>;

static StaticAutoPtr<CacheMap> sBreakCache;

// The underlying hash table extends capacity, when it hits .75 full and uses
// powers of 2 for sizing. This cache limit will hopefully mean most pages fit
// within the cache, while keeping it to a reasonable size. Also by holding the
// previous cache even if pages are bigger than the cache the most commonly used
// should still remain fast.
static const int kCacheLimit = 3072;

static StaticAutoPtr<CacheMap> sOldBreakCache;

// Simple runnable to delete caches off the main thread.
class CacheDeleter final : public Runnable {
 public:
  explicit CacheDeleter(CacheMap* aCacheToDelete)
      : Runnable("ComplexBreaker CacheDeleter"),
        mCacheToDelete(aCacheToDelete) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!NS_IsMainThread());
    mCacheToDelete = nullptr;
    return NS_OK;
  }

 private:
  UniquePtr<CacheMap> mCacheToDelete;
};

class ComplexBreakObserver final : public nsIObserver {
  ~ComplexBreakObserver() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS(ComplexBreakObserver, nsIObserver)

NS_IMETHODIMP ComplexBreakObserver::Observe(nsISupports* aSubject,
                                            const char* aTopic,
                                            const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (strcmp(aTopic, "memory-pressure") == 0) {
    if (sOldBreakCache) {
      // We have an old cache, so delete that one first.
      NS_DispatchBackgroundTask(
          MakeAndAddRef<CacheDeleter>(sOldBreakCache.forget()));
    } else if (sBreakCache) {
      NS_DispatchBackgroundTask(
          MakeAndAddRef<CacheDeleter>(sBreakCache.forget()));
    }
  }

  return NS_OK;
}

void ComplexBreaker::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->AddObserver(new ComplexBreakObserver(), "memory-pressure", false);
  }
}

void ComplexBreaker::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  sBreakCache = nullptr;
  sOldBreakCache = nullptr;
}

static void AddToCache(const char16_t* aText, uint32_t aLength,
                       nsTArray<uint8_t> aBreakBefore) {
  if (NS_WARN_IF(!sBreakCache->InsertOrUpdate(
          nsString(aText, aLength), std::move(aBreakBefore), fallible))) {
    return;
  }

  if (sBreakCache->Count() <= kCacheLimit) {
    return;
  }

  if (sOldBreakCache) {
    NS_DispatchBackgroundTask(
        MakeAndAddRef<CacheDeleter>(sOldBreakCache.forget()));
  }

  sOldBreakCache = sBreakCache.forget();
}

static void CopyAndFill(const nsTArray<uint8_t>& aCachedBreakBefore,
                        uint8_t* aBreakBefore, uint8_t* aEndBreakBefore) {
  auto* startFill = std::copy(aCachedBreakBefore.begin(),
                              aCachedBreakBefore.end(), aBreakBefore);
  std::fill(startFill, aEndBreakBefore, false);
}

void ComplexBreaker::GetBreaks(const char16_t* aText, uint32_t aLength,
                               uint8_t* aBreakBefore) {
  // It is believed that this is only called on the main thread, so we don't
  // need to lock the caching structures. A diagnostic assert is used in case
  // our tests don't exercise all code paths.
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aText, "aText shouldn't be null");
  MOZ_ASSERT(aLength, "aLength shouldn't be zero");
  MOZ_ASSERT(aBreakBefore, "aBreakBefore shouldn't be null");

  // If we have a cache then check it, if not then create it.
  if (sBreakCache) {
    if (auto entry =
            sBreakCache->Lookup(nsDependentSubstring(aText, aLength))) {
      auto& breakBefore = entry.Data();
      CopyAndFill(breakBefore, aBreakBefore, aBreakBefore + aLength);
      return;
    }
  } else {
    sBreakCache = new CacheMap();
  }

  // We keep the previous cache when we hit our limit, so that the most recently
  // used fragments remain fast.
  if (sOldBreakCache) {
    auto breakBefore =
        sOldBreakCache->Extract(nsDependentSubstring(aText, aLength));
    if (breakBefore) {
      CopyAndFill(*breakBefore, aBreakBefore, aBreakBefore + aLength);
      // Move the entry to the latest cache.
      AddToCache(aText, aLength, std::move(*breakBefore));
      return;
    }
  }

  NS_GetComplexLineBreaks(aText, aLength, aBreakBefore);

  // As a very simple memory saving measure we trim off trailing elements that
  // are false before caching.
  auto* afterLastTrue = aBreakBefore + aLength;
  while (!*(afterLastTrue - 1)) {
    if (--afterLastTrue == aBreakBefore) {
      break;
    }
  }

  AddToCache(aText, aLength,
             nsTArray<uint8_t>(aBreakBefore, afterLastTrue - aBreakBefore));
}
