/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SurfaceCache is a service for caching temporary surfaces in imagelib.
 */

#include "SurfaceCache.h"

#include <algorithm>
#include "mozilla/Attributes.h"  // for MOZ_THIS_IN_INITIALIZER_LIST
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsIMemoryReporter.h"
#include "gfx2DGlue.h"
#include "gfxPattern.h"  // Workaround for flaw in bug 921753 part 2.
#include "gfxDrawable.h"
#include "gfxPlatform.h"
#include "nsAutoPtr.h"
#include "nsExpirationTracker.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsSize.h"
#include "nsTArray.h"
#include "prsystem.h"
#include "SVGImageContext.h"

using std::max;
using std::min;
using namespace mozilla::gfx;

namespace mozilla {
namespace image {

class CachedSurface;
class SurfaceCacheImpl;

///////////////////////////////////////////////////////////////////////////////
// Static Data
///////////////////////////////////////////////////////////////////////////////

// The single surface cache instance.
static StaticRefPtr<SurfaceCacheImpl> sInstance;


///////////////////////////////////////////////////////////////////////////////
// SurfaceCache Implementation
///////////////////////////////////////////////////////////////////////////////

/*
 * Cost models the cost of storing a surface in the cache. Right now, this is
 * simply an estimate of the size of the surface in bytes, but in the future it
 * may be worth taking into account the cost of rematerializing the surface as
 * well.
 */
typedef size_t Cost;

static Cost ComputeCost(const IntSize& aSize)
{
  return aSize.width * aSize.height * 4;  // width * height * 4 bytes (32bpp)
}

/*
 * Since we want to be able to make eviction decisions based on cost, we need to
 * be able to look up the CachedSurface which has a certain cost as well as the
 * cost associated with a certain CachedSurface. To make this possible, in data
 * structures we actually store a CostEntry, which contains a weak pointer to
 * its associated surface.
 *
 * To make usage of the weak pointer safe, SurfaceCacheImpl always calls
 * StartTracking after a surface is stored in the cache and StopTracking before
 * it is removed.
 */
class CostEntry
{
public:
  CostEntry(CachedSurface* aSurface, Cost aCost)
    : mSurface(aSurface)
    , mCost(aCost)
  {
    MOZ_ASSERT(aSurface, "Must have a surface");
  }

  CachedSurface* GetSurface() const { return mSurface; }
  Cost GetCost() const { return mCost; }

  bool operator==(const CostEntry& aOther) const
  {
    return mSurface == aOther.mSurface &&
           mCost == aOther.mCost;
  }

  bool operator<(const CostEntry& aOther) const
  {
    return mCost < aOther.mCost ||
           (mCost == aOther.mCost && mSurface < aOther.mSurface);
  }

private:
  CachedSurface* mSurface;
  Cost           mCost;
};

/*
 * A CachedSurface associates a surface with a key that uniquely identifies that
 * surface.
 */
class CachedSurface
{
  ~CachedSurface() {}
public:
  NS_INLINE_DECL_REFCOUNTING(CachedSurface)

  CachedSurface(DrawTarget*       aTarget,
                const IntSize     aTargetSize,
                const Cost        aCost,
                const ImageKey    aImageKey,
                const SurfaceKey& aSurfaceKey)
    : mTarget(aTarget)
    , mTargetSize(aTargetSize)
    , mCost(aCost)
    , mImageKey(aImageKey)
    , mSurfaceKey(aSurfaceKey)
  {
    MOZ_ASSERT(mTarget, "Must have a valid DrawTarget");
    MOZ_ASSERT(mImageKey, "Must have a valid image key");
  }

  already_AddRefed<gfxDrawable> Drawable() const
  {
    nsRefPtr<gfxDrawable> drawable =
      new gfxSurfaceDrawable(mTarget, ThebesIntSize(mTargetSize));
    return drawable.forget();
  }

  ImageKey GetImageKey() const { return mImageKey; }
  SurfaceKey GetSurfaceKey() const { return mSurfaceKey; }
  CostEntry GetCostEntry() { return image::CostEntry(this, mCost); }
  nsExpirationState* GetExpirationState() { return &mExpirationState; }

private:
  nsExpirationState       mExpirationState;
  nsRefPtr<DrawTarget>    mTarget;
  const IntSize           mTargetSize;
  const Cost              mCost;
  const ImageKey          mImageKey;
  const SurfaceKey        mSurfaceKey;
};

/*
 * An ImageSurfaceCache is a per-image surface cache. For correctness we must be
 * able to remove all surfaces associated with an image when the image is
 * destroyed or invalidated. Since this will happen frequently, it makes sense
 * to make it cheap by storing the surfaces for each image separately.
 */
class ImageSurfaceCache
{
  ~ImageSurfaceCache() {}
public:
  NS_INLINE_DECL_REFCOUNTING(ImageSurfaceCache)

  typedef nsRefPtrHashtable<nsGenericHashKey<SurfaceKey>, CachedSurface> SurfaceTable;

  bool IsEmpty() const { return mSurfaces.Count() == 0; }
  
  void Insert(const SurfaceKey& aKey, CachedSurface* aSurface)
  {
    MOZ_ASSERT(aSurface, "Should have a surface");
    mSurfaces.Put(aKey, aSurface);
  }

  void Remove(CachedSurface* aSurface)
  {
    MOZ_ASSERT(aSurface, "Should have a surface");
    MOZ_ASSERT(mSurfaces.GetWeak(aSurface->GetSurfaceKey()),
        "Should not be removing a surface we don't have");

    mSurfaces.Remove(aSurface->GetSurfaceKey());
  }

  already_AddRefed<CachedSurface> Lookup(const SurfaceKey& aSurfaceKey)
  {
    nsRefPtr<CachedSurface> surface;
    mSurfaces.Get(aSurfaceKey, getter_AddRefs(surface));
    return surface.forget();
  }

  void ForEach(SurfaceTable::EnumReadFunction aFunction, void* aData)
  {
    mSurfaces.EnumerateRead(aFunction, aData);
  }

private:
  SurfaceTable mSurfaces;
};

/*
 * SurfaceCacheImpl is responsible for determining which surfaces will be cached
 * and managing the surface cache data structures. Rather than interact with
 * SurfaceCacheImpl directly, client code interacts with SurfaceCache, which
 * maintains high-level invariants and encapsulates the details of the surface
 * cache's implementation.
 */
class SurfaceCacheImpl : public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS

  SurfaceCacheImpl(uint32_t aSurfaceCacheExpirationTimeMS,
                   uint32_t aSurfaceCacheSize)
    : mExpirationTracker(MOZ_THIS_IN_INITIALIZER_LIST(),
                         aSurfaceCacheExpirationTimeMS)
    , mMemoryPressureObserver(new MemoryPressureObserver)
    , mMaxCost(aSurfaceCacheSize)
    , mAvailableCost(aSurfaceCacheSize)
  {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os)
      os->AddObserver(mMemoryPressureObserver, "memory-pressure", false);
  }

private:
  virtual ~SurfaceCacheImpl()
  {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os)
      os->RemoveObserver(mMemoryPressureObserver, "memory-pressure");

    UnregisterWeakMemoryReporter(this);
  }

public:
  void InitMemoryReporter() {
    RegisterWeakMemoryReporter(this);
  }

  void Insert(DrawTarget*       aTarget,
              IntSize           aTargetSize,
              const Cost        aCost,
              const ImageKey    aImageKey,
              const SurfaceKey& aSurfaceKey)
  {
    MOZ_ASSERT(!Lookup(aImageKey, aSurfaceKey).take(),
               "Inserting a duplicate drawable into the SurfaceCache");

    // If this is bigger than the maximum cache size, refuse to cache it.
    if (!CanHold(aCost))
      return;

    nsRefPtr<CachedSurface> surface =
      new CachedSurface(aTarget, aTargetSize, aCost, aImageKey, aSurfaceKey);

    // Remove elements in order of cost until we can fit this in the cache.
    while (aCost > mAvailableCost) {
      MOZ_ASSERT(!mCosts.IsEmpty(), "Removed everything and it still won't fit");
      Remove(mCosts.LastElement().GetSurface());
    }

    // Locate the appropriate per-image cache. If there's not an existing cache
    // for this image, create it.
    nsRefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache) {
      cache = new ImageSurfaceCache;
      mImageCaches.Put(aImageKey, cache);
    }

    // Insert.
    MOZ_ASSERT(aCost <= mAvailableCost, "Inserting despite too large a cost");
    cache->Insert(aSurfaceKey, surface);
    StartTracking(surface);
  }

  void Remove(CachedSurface* aSurface)
  {
    MOZ_ASSERT(aSurface, "Should have a surface");
    const ImageKey imageKey = aSurface->GetImageKey();

    nsRefPtr<ImageSurfaceCache> cache = GetImageCache(imageKey);
    MOZ_ASSERT(cache, "Shouldn't try to remove a surface with no image cache");

    StopTracking(aSurface);
    cache->Remove(aSurface);

    // Remove the per-image cache if it's unneeded now.
    if (cache->IsEmpty()) {
      mImageCaches.Remove(imageKey);
    }
  }

  void StartTracking(CachedSurface* aSurface)
  {
    CostEntry costEntry = aSurface->GetCostEntry();
    MOZ_ASSERT(costEntry.GetCost() <= mAvailableCost,
               "Cost too large and the caller didn't catch it");

    mAvailableCost -= costEntry.GetCost();
    mCosts.InsertElementSorted(costEntry);
    mExpirationTracker.AddObject(aSurface);
  }

  void StopTracking(CachedSurface* aSurface)
  {
    MOZ_ASSERT(aSurface, "Should have a surface");
    CostEntry costEntry = aSurface->GetCostEntry();

    mExpirationTracker.RemoveObject(aSurface);
    DebugOnly<bool> foundInCosts = mCosts.RemoveElementSorted(costEntry);
    mAvailableCost += costEntry.GetCost();

    MOZ_ASSERT(foundInCosts, "Lost track of costs for this surface");
    MOZ_ASSERT(mAvailableCost <= mMaxCost, "More available cost than we started with");
  }

  already_AddRefed<gfxDrawable> Lookup(const ImageKey    aImageKey,
                                       const SurfaceKey& aSurfaceKey)
  {
    nsRefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache)
      return nullptr;  // No cached surfaces for this image.
    
    nsRefPtr<CachedSurface> surface = cache->Lookup(aSurfaceKey);
    if (!surface)
      return nullptr;  // Lookup in the per-image cache missed.
    
    mExpirationTracker.MarkUsed(surface);
    return surface->Drawable();
  }

  bool CanHold(const Cost aCost) const
  {
    return aCost <= mMaxCost;
  }

  void Discard(const ImageKey aImageKey)
  {
    nsRefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache)
      return;  // No cached surfaces for this image, so nothing to do.

    // Discard all of the cached surfaces for this image.
    // XXX(seth): This is O(n^2) since for each item in the cache we are
    // removing an element from the costs array. Since n is expected to be
    // small, performance should be good, but if usage patterns change we should
    // change the data structure used for mCosts.
    cache->ForEach(DoStopTracking, this);

    // The per-image cache isn't needed anymore, so remove it as well.
    mImageCaches.Remove(aImageKey);
  }

  void DiscardAll()
  {
    // Remove in order of cost because mCosts is an array and the other data
    // structures are all hash tables.
    while (!mCosts.IsEmpty()) {
      Remove(mCosts.LastElement().GetSurface());
    }
  }

  static PLDHashOperator DoStopTracking(const SurfaceKey&,
                                        CachedSurface*    aSurface,
                                        void*             aCache)
  {
    static_cast<SurfaceCacheImpl*>(aCache)->StopTracking(aSurface);
    return PL_DHASH_NEXT;
  }

  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize)
  {
    return MOZ_COLLECT_REPORT(
      "imagelib-surface-cache", KIND_OTHER, UNITS_BYTES,
      SizeOfSurfacesEstimate(),
      "Memory used by the imagelib temporary surface cache.");
  }

  // XXX(seth): This is currently only an estimate and, since we don't know
  // which surfaces are in GPU memory and which aren't, it's reported as
  // KIND_OTHER and will also show up in heap-unclassified. Bug 923302 will
  // make this nicer.
  Cost SizeOfSurfacesEstimate() const
  {
    return mMaxCost - mAvailableCost;
  }

private:
  already_AddRefed<ImageSurfaceCache> GetImageCache(const ImageKey aImageKey)
  {
    nsRefPtr<ImageSurfaceCache> imageCache;
    mImageCaches.Get(aImageKey, getter_AddRefs(imageCache));
    return imageCache.forget();
  }

  struct SurfaceTracker : public nsExpirationTracker<CachedSurface, 2>
  {
    SurfaceTracker(SurfaceCacheImpl* aCache, uint32_t aSurfaceCacheExpirationTimeMS)
      : nsExpirationTracker<CachedSurface, 2>(aSurfaceCacheExpirationTimeMS)
      , mCache(aCache)
    { }

  protected:
    virtual void NotifyExpired(CachedSurface* aSurface) MOZ_OVERRIDE
    {
      if (mCache) {
        mCache->Remove(aSurface);
      }
    }

  private:
    SurfaceCacheImpl* const mCache;  // Weak pointer to owner.
  };

  struct MemoryPressureObserver : public nsIObserver
  {
    NS_DECL_ISUPPORTS

    NS_IMETHOD Observe(nsISupports*, const char* aTopic, const char16_t*)
    {
      if (sInstance && strcmp(aTopic, "memory-pressure") == 0) {
        sInstance->DiscardAll();
      }
      return NS_OK;
    }

  private:
    virtual ~MemoryPressureObserver() { }
  };


  nsTArray<CostEntry>                                       mCosts;
  nsRefPtrHashtable<nsPtrHashKey<Image>, ImageSurfaceCache> mImageCaches;
  SurfaceTracker                                            mExpirationTracker;
  nsRefPtr<MemoryPressureObserver>                          mMemoryPressureObserver;
  const Cost                                                mMaxCost;
  Cost                                                      mAvailableCost;
};

NS_IMPL_ISUPPORTS(SurfaceCacheImpl, nsIMemoryReporter)
NS_IMPL_ISUPPORTS(SurfaceCacheImpl::MemoryPressureObserver, nsIObserver)

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

/* static */ void
SurfaceCache::Initialize()
{
  // Initialize preferences.
  MOZ_ASSERT(!sInstance, "Shouldn't initialize more than once");

  // Length of time before an unused surface is removed from the cache, in milliseconds.
  // The default value gives an expiration time of 1 minute.
  uint32_t surfaceCacheExpirationTimeMS =
    Preferences::GetUint("image.mem.surfacecache.min_expiration_ms", 60 * 1000);

  // Maximum size of the surface cache, in kilobytes.
  // The default is 100MB. (But we may override this for e.g. B2G.)
  uint32_t surfaceCacheMaxSizeKB =
    Preferences::GetUint("image.mem.surfacecache.max_size_kb", 100 * 1024);

  // A knob determining the actual size of the surface cache. Currently the
  // cache is (size of main memory) / (surface cache size factor) KB
  // or (surface cache max size) KB, whichever is smaller. The formula
  // may change in the future, though.
  // The default value is 64, which yields a 64MB cache on a 4GB machine.
  // The smallest machines we are likely to run this code on have 256MB
  // of memory, which would yield a 4MB cache on the default setting.
  uint32_t surfaceCacheSizeFactor =
    Preferences::GetUint("image.mem.surfacecache.size_factor", 64);

  // Clamp to avoid division by zero below.
  surfaceCacheSizeFactor = max(surfaceCacheSizeFactor, 1u);

  // Compute the size of the surface cache.
  uint32_t proposedSize = PR_GetPhysicalMemorySize() / surfaceCacheSizeFactor;
  uint32_t surfaceCacheSizeBytes = min(proposedSize, surfaceCacheMaxSizeKB * 1024);

  // Create the surface cache singleton with the requested expiration time and
  // size. Note that the size is a limit that the cache may not grow beyond, but
  // we do not actually allocate any storage for surfaces at this time.
  sInstance = new SurfaceCacheImpl(surfaceCacheExpirationTimeMS,
                                   surfaceCacheSizeBytes);
  sInstance->InitMemoryReporter();
}

/* static */ void
SurfaceCache::Shutdown()
{
  MOZ_ASSERT(sInstance, "No singleton - was Shutdown() called twice?");
  sInstance = nullptr;
}

/* static */ already_AddRefed<gfxDrawable>
SurfaceCache::Lookup(const ImageKey    aImageKey,
                     const SurfaceKey& aSurfaceKey)
{
  MOZ_ASSERT(sInstance, "Should be initialized");
  MOZ_ASSERT(NS_IsMainThread());

  return sInstance->Lookup(aImageKey, aSurfaceKey);
}

/* static */ void
SurfaceCache::Insert(DrawTarget*       aTarget,
                     const ImageKey    aImageKey,
                     const SurfaceKey& aSurfaceKey)
{
  MOZ_ASSERT(sInstance, "Should be initialized");
  MOZ_ASSERT(NS_IsMainThread());

  Cost cost = ComputeCost(aSurfaceKey.Size());
  return sInstance->Insert(aTarget, aSurfaceKey.Size(), cost, aImageKey,
                           aSurfaceKey);
}

/* static */ bool
SurfaceCache::CanHold(const IntSize& aSize)
{
  MOZ_ASSERT(sInstance, "Should be initialized");
  MOZ_ASSERT(NS_IsMainThread());

  Cost cost = ComputeCost(aSize);
  return sInstance->CanHold(cost);
}

/* static */ void
SurfaceCache::Discard(Image* aImageKey)
{
  MOZ_ASSERT(sInstance, "Should be initialized");
  MOZ_ASSERT(NS_IsMainThread());

  return sInstance->Discard(aImageKey);
}

/* static */ void
SurfaceCache::DiscardAll()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sInstance) {
    sInstance->DiscardAll();
  }
  // nothing to discard
}

} // namespace image
} // namespace mozilla
