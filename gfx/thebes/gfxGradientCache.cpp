/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxGradientCache.h"

#include "MainThreadUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/DataMutex.h"
#include "nsTArray.h"
#include "PLDHashTable.h"
#include "nsExpirationTracker.h"
#include "nsClassHashtable.h"
#include <time.h>

namespace mozilla {
namespace gfx {

using namespace mozilla;

struct GradientCacheKey : public PLDHashEntryHdr {
  typedef const GradientCacheKey& KeyType;
  typedef const GradientCacheKey* KeyTypePointer;
  enum { ALLOW_MEMMOVE = true };
  const CopyableTArray<GradientStop> mStops;
  ExtendMode mExtend;
  BackendType mBackendType;

  GradientCacheKey(const nsTArray<GradientStop>& aStops, ExtendMode aExtend,
                   BackendType aBackendType)
      : mStops(aStops), mExtend(aExtend), mBackendType(aBackendType) {}

  explicit GradientCacheKey(const GradientCacheKey* aOther)
      : mStops(aOther->mStops),
        mExtend(aOther->mExtend),
        mBackendType(aOther->mBackendType) {}

  GradientCacheKey(GradientCacheKey&& aOther) = default;

  union FloatUint32 {
    float f;
    uint32_t u;
  };

  static PLDHashNumber HashKey(const KeyTypePointer aKey) {
    PLDHashNumber hash = 0;
    FloatUint32 convert;
    hash = AddToHash(hash, int(aKey->mBackendType));
    hash = AddToHash(hash, int(aKey->mExtend));
    for (uint32_t i = 0; i < aKey->mStops.Length(); i++) {
      hash = AddToHash(hash, aKey->mStops[i].color.ToABGR());
      // Use the float bits as hash, except for the cases of 0.0 and -0.0 which
      // both map to 0
      convert.f = aKey->mStops[i].offset;
      hash = AddToHash(hash, convert.f ? convert.u : 0);
    }
    return hash;
  }

  bool KeyEquals(KeyTypePointer aKey) const {
    bool sameStops = true;
    if (aKey->mStops.Length() != mStops.Length()) {
      sameStops = false;
    } else {
      for (uint32_t i = 0; i < mStops.Length(); i++) {
        if (mStops[i].color.ToABGR() != aKey->mStops[i].color.ToABGR() ||
            mStops[i].offset != aKey->mStops[i].offset) {
          sameStops = false;
          break;
        }
      }
    }

    return sameStops && (aKey->mBackendType == mBackendType) &&
           (aKey->mExtend == mExtend);
  }
  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
};

/**
 * This class is what is cached. It need to be allocated in an object separated
 * to the cache entry to be able to be tracked by the nsExpirationTracker.
 * */
struct GradientCacheData {
  GradientCacheData(GradientStops* aStops, GradientCacheKey&& aKey)
      : mStops(aStops), mKey(std::move(aKey)) {}

  GradientCacheData(GradientCacheData&& aOther) = default;

  nsExpirationState* GetExpirationState() { return &mExpirationState; }

  nsExpirationState mExpirationState;
  const RefPtr<GradientStops> mStops;
  GradientCacheKey mKey;
};

/**
 * This class implements a cache, that retains the GradientStops used to draw
 * the gradients.
 *
 * An entry stays in the cache as long as it is used often and we don't exceed
 * the maximum, in which case the most recently used will be kept.
 */
class GradientCache;
using GradientCacheMutex = StaticDataMutex<UniquePtr<GradientCache>>;
class MOZ_RAII LockedInstance {
 public:
  explicit LockedInstance(GradientCacheMutex& aDataMutex)
      : mAutoLock(aDataMutex.Lock()) {}
  UniquePtr<GradientCache>& operator->() const& { return mAutoLock.ref(); }
  UniquePtr<GradientCache>& operator->() const&& = delete;
  UniquePtr<GradientCache>& operator*() const& { return mAutoLock.ref(); }
  UniquePtr<GradientCache>& operator*() const&& = delete;
  explicit operator bool() const { return !!mAutoLock.ref(); }

 private:
  GradientCacheMutex::AutoLock mAutoLock;
};

class GradientCache final
    : public ExpirationTrackerImpl<GradientCacheData, 4, GradientCacheMutex,
                                   LockedInstance> {
 public:
  GradientCache()
      : ExpirationTrackerImpl<GradientCacheData, 4, GradientCacheMutex,
                              LockedInstance>(MAX_GENERATION_MS,
                                              "GradientCache") {}
  static bool EnsureInstance() {
    LockedInstance lockedInstance(sInstanceMutex);
    return EnsureInstanceLocked(lockedInstance);
  }

  static void DestroyInstance() {
    LockedInstance lockedInstance(sInstanceMutex);
    if (lockedInstance) {
      *lockedInstance = nullptr;
    }
  }

  static void AgeAllGenerations() {
    LockedInstance lockedInstance(sInstanceMutex);
    if (!lockedInstance) {
      return;
    }
    lockedInstance->AgeAllGenerationsLocked(lockedInstance);
    lockedInstance->NotifyHandlerEndLocked(lockedInstance);
  }

  template <typename CreateFunc>
  static already_AddRefed<GradientStops> LookupOrInsert(
      const GradientCacheKey& aKey, CreateFunc aCreateFunc) {
    uint32_t numberOfEntries;
    RefPtr<GradientStops> stops;
    {
      LockedInstance lockedInstance(sInstanceMutex);
      if (!EnsureInstanceLocked(lockedInstance)) {
        return aCreateFunc();
      }

      GradientCacheData* gradientData = lockedInstance->mHashEntries.Get(aKey);
      if (gradientData) {
        if (gradientData->mStops && gradientData->mStops->IsValid()) {
          lockedInstance->MarkUsedLocked(gradientData, lockedInstance);
          return do_AddRef(gradientData->mStops);
        }

        lockedInstance->NotifyExpiredLocked(gradientData, lockedInstance);
        lockedInstance->NotifyHandlerEndLocked(lockedInstance);
      }

      stops = aCreateFunc();
      if (!stops) {
        return nullptr;
      }

      auto data = MakeUnique<GradientCacheData>(stops, GradientCacheKey(&aKey));
      nsresult rv = lockedInstance->AddObjectLocked(data.get(), lockedInstance);
      if (NS_FAILED(rv)) {
        // We are OOM, and we cannot track this object. We don't want to store
        // entries in the hash table (since the expiration tracker is
        // responsible for removing the cache entries), so we avoid putting that
        // entry in the table, which is a good thing considering we are short on
        // memory anyway, we probably don't want to retain things.
        return stops.forget();
      }
      lockedInstance->mHashEntries.InsertOrUpdate(aKey, std::move(data));
      numberOfEntries = lockedInstance->mHashEntries.Count();
    }

    if (numberOfEntries > MAX_ENTRIES) {
      // We have too many entries force the cache to age a generation.
      NS_DispatchToMainThread(
          NS_NewRunnableFunction("GradientCache::OnMaxEntriesBreached", [] {
            LockedInstance lockedInstance(sInstanceMutex);
            if (!lockedInstance) {
              return;
            }
            lockedInstance->AgeOneGenerationLocked(lockedInstance);
            lockedInstance->NotifyHandlerEndLocked(lockedInstance);
          }));
    }

    return stops.forget();
  }

  GradientCacheMutex& GetMutex() final { return sInstanceMutex; }

  void NotifyExpiredLocked(GradientCacheData* aObject,
                           const LockedInstance& aLockedInstance) final {
    // Remove the gradient from the tracker.
    RemoveObjectLocked(aObject, aLockedInstance);

    // If entry exists move the data to mRemovedGradientData because we want to
    // drop it outside of the lock.
    Maybe<UniquePtr<GradientCacheData>> gradientData =
        mHashEntries.Extract(aObject->mKey);
    if (gradientData.isSome()) {
      mRemovedGradientData.AppendElement(std::move(*gradientData));
    }
  }

  void NotifyHandlerEndLocked(const LockedInstance&) final {
    NS_DispatchToCurrentThread(
        NS_NewRunnableFunction("GradientCache::DestroyRemovedGradientStops",
                               [stops = std::move(mRemovedGradientData)] {}));
  }

 private:
  static const uint32_t MAX_GENERATION_MS = 10000;

  // On Windows some of the Direct2D objects associated with the gradient stops
  // can be quite large, so we limit the number of cache entries.
  static const uint32_t MAX_ENTRIES = 4000;
  static GradientCacheMutex sInstanceMutex;

  [[nodiscard]] static bool EnsureInstanceLocked(
      LockedInstance& aLockedInstance) {
    if (!aLockedInstance) {
      // GradientCache must be created on the main thread.
      if (!NS_IsMainThread()) {
        // This should only happen at shutdown, we fall back to not caching.
        return false;
      }
      *aLockedInstance = MakeUnique<GradientCache>();
    }
    return true;
  }

  /**
   * FIXME use nsTHashtable to avoid duplicating the GradientCacheKey.
   * https://bugzilla.mozilla.org/show_bug.cgi?id=761393#c47
   */
  nsClassHashtable<GradientCacheKey, GradientCacheData> mHashEntries;
  nsTArray<UniquePtr<GradientCacheData>> mRemovedGradientData;
};

GradientCacheMutex GradientCache::sInstanceMutex("GradientCache");

void gfxGradientCache::Init() {
  MOZ_RELEASE_ASSERT(GradientCache::EnsureInstance(),
                     "First call must be on main thread.");
}

already_AddRefed<GradientStops> gfxGradientCache::GetOrCreateGradientStops(
    const DrawTarget* aDT, nsTArray<GradientStop>& aStops, ExtendMode aExtend) {
  if (aDT->IsRecording()) {
    return aDT->CreateGradientStops(aStops.Elements(), aStops.Length(),
                                    aExtend);
  }

  return GradientCache::LookupOrInsert(
      GradientCacheKey(aStops, aExtend, aDT->GetBackendType()),
      [&]() -> already_AddRefed<GradientStops> {
        return aDT->CreateGradientStops(aStops.Elements(), aStops.Length(),
                                        aExtend);
      });
}

void gfxGradientCache::PurgeAllCaches() { GradientCache::AgeAllGenerations(); }

void gfxGradientCache::Shutdown() { GradientCache::DestroyInstance(); }

}  // namespace gfx
}  // namespace mozilla
