/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/gfx/2D.h"
#include "nsTArray.h"
#include "PLDHashTable.h"
#include "nsExpirationTracker.h"
#include "nsClassHashtable.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Telemetry.h"
#include "gfxGradientCache.h"
#include <time.h>

namespace mozilla {
namespace gfx {

using namespace mozilla;

struct GradientCacheKey : public PLDHashEntryHdr {
  typedef const GradientCacheKey& KeyType;
  typedef const GradientCacheKey* KeyTypePointer;
  enum { ALLOW_MEMMOVE = true };
  const nsTArray<GradientStop> mStops;
  ExtendMode mExtend;
  BackendType mBackendType;

  GradientCacheKey(const nsTArray<GradientStop>& aStops, ExtendMode aExtend, BackendType aBackendType)
    : mStops(aStops), mExtend(aExtend), mBackendType(aBackendType)
  { }

  explicit GradientCacheKey(const GradientCacheKey* aOther)
    : mStops(aOther->mStops), mExtend(aOther->mExtend), mBackendType(aOther->mBackendType)
  { }

  union FloatUint32
  {
    float    f;
    uint32_t u;
  };

  static PLDHashNumber
  HashKey(const KeyTypePointer aKey)
  {
    PLDHashNumber hash = 0;
    FloatUint32 convert;
    hash = AddToHash(hash, int(aKey->mBackendType));
    hash = AddToHash(hash, int(aKey->mExtend));
    for (uint32_t i = 0; i < aKey->mStops.Length(); i++) {
      hash = AddToHash(hash, aKey->mStops[i].color.ToABGR());
      // Use the float bits as hash, except for the cases of 0.0 and -0.0 which both map to 0
      convert.f = aKey->mStops[i].offset;
      hash = AddToHash(hash, convert.f ? convert.u : 0);
    }
    return hash;
  }

  bool KeyEquals(KeyTypePointer aKey) const
  {
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

    return sameStops &&
           (aKey->mBackendType == mBackendType) &&
           (aKey->mExtend == mExtend);
  }
  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return &aKey;
  }
};

/**
 * This class is what is cached. It need to be allocated in an object separated
 * to the cache entry to be able to be tracked by the nsExpirationTracker.
 * */
struct GradientCacheData {
  GradientCacheData(GradientStops* aStops, const GradientCacheKey& aKey)
    : mStops(aStops),
      mKey(aKey)
  {}

  GradientCacheData(const GradientCacheData& aOther)
    : mStops(aOther.mStops),
      mKey(aOther.mKey)
  { }

  nsExpirationState *GetExpirationState() {
    return &mExpirationState;
  }

  nsExpirationState mExpirationState;
  const RefPtr<GradientStops> mStops;
  GradientCacheKey mKey;
};

/**
 * This class implements a cache with no maximum size, that retains the
 * gfxPatterns used to draw the gradients.
 *
 * The key is the nsStyleGradient that defines the gradient, and the size of the
 * gradient.
 *
 * The value is the gfxPattern, and whether or not we perform an optimization
 * based on the actual gradient property.
 *
 * An entry stays in the cache as long as it is used often. As long as a cache
 * entry is in the cache, all the references it has are guaranteed to be valid:
 * the nsStyleRect for the key, the gfxPattern for the value.
 */
class GradientCache final : public nsExpirationTracker<GradientCacheData,4>
{
  public:
    GradientCache()
      : nsExpirationTracker<GradientCacheData,4>(MAX_GENERATION_MS,
                                                 "GradientCache",
                                                 SystemGroup::EventTargetFor(TaskCategory::Other))
    {
      srand(time(nullptr));
      mTimerPeriod = rand() % MAX_GENERATION_MS + 1;
      Telemetry::Accumulate(Telemetry::GRADIENT_RETENTION_TIME, mTimerPeriod);
    }

    virtual void NotifyExpired(GradientCacheData* aObject)
    {
      // This will free the gfxPattern.
      RemoveObject(aObject);
      mHashEntries.Remove(aObject->mKey);
    }

    GradientCacheData* Lookup(const nsTArray<GradientStop>& aStops, ExtendMode aExtend, BackendType aBackendType)
    {
      GradientCacheData* gradient =
        mHashEntries.Get(GradientCacheKey(aStops, aExtend, aBackendType));

      if (gradient) {
        MarkUsed(gradient);
      }

      return gradient;
    }

    // Returns true if we successfully register the gradient in the cache, false
    // otherwise.
    bool RegisterEntry(GradientCacheData* aValue)
    {
      nsresult rv = AddObject(aValue);
      if (NS_FAILED(rv)) {
        // We are OOM, and we cannot track this object. We don't want stall
        // entries in the hash table (since the expiration tracker is responsible
        // for removing the cache entries), so we avoid putting that entry in the
        // table, which is a good things considering we are short on memory
        // anyway, we probably don't want to retain things.
        return false;
      }
      mHashEntries.Put(aValue->mKey, aValue);
      return true;
    }

  protected:
    uint32_t mTimerPeriod;
    static const uint32_t MAX_GENERATION_MS = 10000;
    /**
     * FIXME use nsTHashtable to avoid duplicating the GradientCacheKey.
     * https://bugzilla.mozilla.org/show_bug.cgi?id=761393#c47
     */
    nsClassHashtable<GradientCacheKey, GradientCacheData> mHashEntries;
};

static GradientCache* gGradientCache = nullptr;

GradientStops *
gfxGradientCache::GetGradientStops(const DrawTarget *aDT, nsTArray<GradientStop>& aStops, ExtendMode aExtend)
{
  if (!gGradientCache) {
    gGradientCache = new GradientCache();
  }
  GradientCacheData* cached =
    gGradientCache->Lookup(aStops, aExtend, aDT->GetBackendType());
  if (cached && cached->mStops) {
    if (!cached->mStops->IsValid()) {
      gGradientCache->NotifyExpired(cached);
    } else {
      return cached->mStops;
    }
  }

  return nullptr;
}

already_AddRefed<GradientStops>
gfxGradientCache::GetOrCreateGradientStops(const DrawTarget *aDT, nsTArray<GradientStop>& aStops, ExtendMode aExtend)
{
  if (aDT->IsRecording()) {
    return aDT->CreateGradientStops(aStops.Elements(), aStops.Length(), aExtend);
  }

  RefPtr<GradientStops> gs = GetGradientStops(aDT, aStops, aExtend);
  if (!gs) {
    gs = aDT->CreateGradientStops(aStops.Elements(), aStops.Length(), aExtend);
    if (!gs) {
      return nullptr;
    }
    GradientCacheData *cached =
      new GradientCacheData(gs, GradientCacheKey(aStops, aExtend,
                                                 aDT->GetBackendType()));
    if (!gGradientCache->RegisterEntry(cached)) {
      delete cached;
    }
  }
  return gs.forget();
}

void
gfxGradientCache::PurgeAllCaches()
{
  if (gGradientCache) {
    gGradientCache->AgeAllGenerations();
  }
}

void
gfxGradientCache::Shutdown()
{
  delete gGradientCache;
  gGradientCache = nullptr;
}

} // namespace gfx
} // namespace mozilla
