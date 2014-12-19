/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SurfaceCache is a service for caching temporary surfaces in imagelib.
 */

#include "SurfaceCache.h"

#include <algorithm>
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"  // for MOZ_THIS_IN_INITIALIZER_LIST
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/Move.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsIMemoryReporter.h"
#include "gfx2DGlue.h"
#include "gfxPattern.h"  // Workaround for flaw in bug 921753 part 2.
#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "imgFrame.h"
#include "Image.h"
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

namespace mozilla {

using namespace gfx;

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

/**
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

/**
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

/**
 * A CachedSurface associates a surface with a key that uniquely identifies that
 * surface.
 */
class CachedSurface
{
  ~CachedSurface() {}
public:
  NS_INLINE_DECL_REFCOUNTING(CachedSurface)

  CachedSurface(imgFrame*          aSurface,
                const Cost         aCost,
                const ImageKey     aImageKey,
                const SurfaceKey&  aSurfaceKey,
                const Lifetime     aLifetime)
    : mSurface(aSurface)
    , mCost(aCost)
    , mImageKey(aImageKey)
    , mSurfaceKey(aSurfaceKey)
    , mLifetime(aLifetime)
  {
    MOZ_ASSERT(mSurface, "Must have a valid surface");
    MOZ_ASSERT(mImageKey, "Must have a valid image key");
  }

  DrawableFrameRef DrawableRef() const
  {
    return mSurface->DrawableRef();
  }

  void SetLocked(bool aLocked)
  {
    if (aLocked && mLifetime == Lifetime::Persistent) {
      // This may fail, and that's OK. We make no guarantees about whether
      // locking is successful if you call SurfaceCache::LockImage() after
      // SurfaceCache::Insert().
      mDrawableRef = mSurface->DrawableRef();
    } else {
      mDrawableRef.reset();
    }
  }

  bool IsLocked() const { return bool(mDrawableRef); }

  ImageKey GetImageKey() const { return mImageKey; }
  SurfaceKey GetSurfaceKey() const { return mSurfaceKey; }
  CostEntry GetCostEntry() { return image::CostEntry(this, mCost); }
  nsExpirationState* GetExpirationState() { return &mExpirationState; }
  Lifetime GetLifetime() const { return mLifetime; }

  // A helper type used by SurfaceCacheImpl::SizeOfSurfacesSum.
  struct SizeOfSurfacesSum
  {
    SizeOfSurfacesSum(gfxMemoryLocation aLocation,
                      MallocSizeOf      aMallocSizeOf)
      : mLocation(aLocation)
      , mMallocSizeOf(aMallocSizeOf)
      , mSum(0)
    { }

    void Add(CachedSurface* aCachedSurface)
    {
      MOZ_ASSERT(aCachedSurface, "Should have a CachedSurface");

      if (!aCachedSurface->mSurface) {
        return;
      }
      mSum += aCachedSurface->mSurface->SizeOfExcludingThis(mLocation,
                                                            mMallocSizeOf);
    }

    size_t Result() const { return mSum; }

  private:
    gfxMemoryLocation mLocation;
    MallocSizeOf      mMallocSizeOf;
    size_t            mSum;
  };

private:
  nsExpirationState  mExpirationState;
  nsRefPtr<imgFrame> mSurface;
  DrawableFrameRef   mDrawableRef;
  const Cost         mCost;
  const ImageKey     mImageKey;
  const SurfaceKey   mSurfaceKey;
  const Lifetime     mLifetime;
};

/**
 * An ImageSurfaceCache is a per-image surface cache. For correctness we must be
 * able to remove all surfaces associated with an image when the image is
 * destroyed or invalidated. Since this will happen frequently, it makes sense
 * to make it cheap by storing the surfaces for each image separately.
 *
 * ImageSurfaceCache also keeps track of whether its associated image is locked
 * or unlocked.
 */
class ImageSurfaceCache
{
  ~ImageSurfaceCache() { }
public:
  ImageSurfaceCache() : mLocked(false) { }

  NS_INLINE_DECL_REFCOUNTING(ImageSurfaceCache)

  typedef nsRefPtrHashtable<nsGenericHashKey<SurfaceKey>, CachedSurface> SurfaceTable;

  bool IsEmpty() const { return mSurfaces.Count() == 0; }
  
  void Insert(const SurfaceKey& aKey, CachedSurface* aSurface)
  {
    MOZ_ASSERT(aSurface, "Should have a surface");
    MOZ_ASSERT(!mLocked || aSurface->GetLifetime() != Lifetime::Persistent ||
               aSurface->IsLocked(),
               "Inserting an unlocked persistent surface for a locked image");
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

  void SetLocked(bool aLocked) { mLocked = aLocked; }
  bool IsLocked() const { return mLocked; }

private:
  SurfaceTable mSurfaces;
  bool         mLocked;
};

/**
 * SurfaceCacheImpl is responsible for determining which surfaces will be cached
 * and managing the surface cache data structures. Rather than interact with
 * SurfaceCacheImpl directly, client code interacts with SurfaceCache, which
 * maintains high-level invariants and encapsulates the details of the surface
 * cache's implementation.
 */
class SurfaceCacheImpl MOZ_FINAL : public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS

  SurfaceCacheImpl(uint32_t aSurfaceCacheExpirationTimeMS,
                   uint32_t aSurfaceCacheDiscardFactor,
                   uint32_t aSurfaceCacheSize)
    : mExpirationTracker(MOZ_THIS_IN_INITIALIZER_LIST(),
                         aSurfaceCacheExpirationTimeMS)
    , mMemoryPressureObserver(new MemoryPressureObserver)
    , mDiscardFactor(aSurfaceCacheDiscardFactor)
    , mMaxCost(aSurfaceCacheSize)
    , mAvailableCost(aSurfaceCacheSize)
    , mLockedCost(0)
  {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os)
      os->AddObserver(mMemoryPressureObserver, "memory-pressure", false);
  }

private:
  virtual ~SurfaceCacheImpl()
  {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os)
      os->RemoveObserver(mMemoryPressureObserver, "memory-pressure");

    UnregisterWeakMemoryReporter(this);
  }

public:
  void InitMemoryReporter() {
    RegisterWeakMemoryReporter(this);
  }

  bool Insert(imgFrame*         aSurface,
              const Cost        aCost,
              const ImageKey    aImageKey,
              const SurfaceKey& aSurfaceKey,
              Lifetime          aLifetime)
  {
    MOZ_ASSERT(!Lookup(aImageKey, aSurfaceKey),
               "Inserting a duplicate surface into the SurfaceCache");

    // If this is bigger than we can hold after discarding everything we can,
    // refuse to cache it.
    if (!CanHoldAfterDiscarding(aCost))
      return false;

    // Remove elements in order of cost until we can fit this in the cache. Note
    // that locked surfaces aren't in mCosts, so we never remove them here.
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

    nsRefPtr<CachedSurface> surface =
      new CachedSurface(aSurface, aCost, aImageKey, aSurfaceKey, aLifetime);

    // We require that locking succeed if the image is locked and the surface is
    // persistent; the caller may need to know this to handle errors correctly.
    if (cache->IsLocked() && aLifetime == Lifetime::Persistent) {
      surface->SetLocked(true);
      if (!surface->IsLocked()) {
        return false;
      }
    }

    // Insert.
    MOZ_ASSERT(aCost <= mAvailableCost, "Inserting despite too large a cost");
    cache->Insert(aSurfaceKey, surface);
    StartTracking(surface);

    return true;
  }

  void Remove(CachedSurface* aSurface)
  {
    MOZ_ASSERT(aSurface, "Should have a surface");
    ImageKey imageKey = aSurface->GetImageKey();

    nsRefPtr<ImageSurfaceCache> cache = GetImageCache(imageKey);
    MOZ_ASSERT(cache, "Shouldn't try to remove a surface with no image cache");

    // If the surface was persistent, tell its image that we discarded it.
    if (aSurface->GetLifetime() == Lifetime::Persistent) {
      static_cast<Image*>(imageKey)->OnSurfaceDiscarded();
    }

    StopTracking(aSurface);
    cache->Remove(aSurface);

    // Remove the per-image cache if it's unneeded now. (Keep it if the image is
    // locked, since the per-image cache is where we store that state.)
    if (cache->IsEmpty() && !cache->IsLocked()) {
      mImageCaches.Remove(imageKey);
    }
  }

  void StartTracking(CachedSurface* aSurface)
  {
    CostEntry costEntry = aSurface->GetCostEntry();
    MOZ_ASSERT(costEntry.GetCost() <= mAvailableCost,
               "Cost too large and the caller didn't catch it");

    mAvailableCost -= costEntry.GetCost();

    if (aSurface->IsLocked()) {
      mLockedCost += costEntry.GetCost();
      MOZ_ASSERT(mLockedCost <= mMaxCost, "Locked more than we can hold?");
    } else {
      mCosts.InsertElementSorted(costEntry);
      // This may fail during XPCOM shutdown, so we need to ensure the object is
      // tracked before calling RemoveObject in StopTracking.
      mExpirationTracker.AddObject(aSurface);
    }
  }

  void StopTracking(CachedSurface* aSurface)
  {
    MOZ_ASSERT(aSurface, "Should have a surface");
    CostEntry costEntry = aSurface->GetCostEntry();

    if (aSurface->IsLocked()) {
      MOZ_ASSERT(mLockedCost >= costEntry.GetCost(), "Costs don't balance");
      mLockedCost -= costEntry.GetCost();
      // XXX(seth): It'd be nice to use an O(log n) lookup here. This is O(n).
      MOZ_ASSERT(!mCosts.Contains(costEntry),
                 "Shouldn't have a cost entry for a locked surface");
    } else {
      if (MOZ_LIKELY(aSurface->GetExpirationState()->IsTracked())) {
        mExpirationTracker.RemoveObject(aSurface);
      } else {
        // Our call to AddObject must have failed in StartTracking; most likely
        // we're in XPCOM shutdown right now.
        NS_WARNING("Not expiration-tracking an unlocked surface!");
      }

      DebugOnly<bool> foundInCosts = mCosts.RemoveElementSorted(costEntry);
      MOZ_ASSERT(foundInCosts, "Lost track of costs for this surface");
    }

    mAvailableCost += costEntry.GetCost();
    MOZ_ASSERT(mAvailableCost <= mMaxCost,
               "More available cost than we started with");
  }

  DrawableFrameRef Lookup(const ImageKey    aImageKey,
                          const SurfaceKey& aSurfaceKey)
  {
    nsRefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache)
      return DrawableFrameRef();  // No cached surfaces for this image.

    nsRefPtr<CachedSurface> surface = cache->Lookup(aSurfaceKey);
    if (!surface)
      return DrawableFrameRef();  // Lookup in the per-image cache missed.

    DrawableFrameRef ref = surface->DrawableRef();
    if (!ref) {
      // The surface was released by the operating system. Remove the cache
      // entry as well.
      Remove(surface);
      return DrawableFrameRef();
    }

    if (!surface->IsLocked()) {
      mExpirationTracker.MarkUsed(surface);
    }

    return ref;
  }

  void RemoveSurface(const ImageKey    aImageKey,
                     const SurfaceKey& aSurfaceKey)
  {
    nsRefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache)
      return;  // No cached surfaces for this image.

    nsRefPtr<CachedSurface> surface = cache->Lookup(aSurfaceKey);
    if (!surface)
      return;  // Lookup in the per-image cache missed.

    Remove(surface);
  }

  bool CanHold(const Cost aCost) const
  {
    return aCost <= mMaxCost;
  }

  void LockImage(const ImageKey aImageKey)
  {
    nsRefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache) {
      cache = new ImageSurfaceCache;
      mImageCaches.Put(aImageKey, cache);
    }

    cache->SetLocked(true);

    // Try to lock all the surfaces the per-image cache is holding.
    cache->ForEach(DoLockSurface, this);
  }

  void UnlockImage(const ImageKey aImageKey)
  {
    nsRefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache)
      return;  // Already unlocked and removed.

    cache->SetLocked(false);

    // Unlock all the surfaces the per-image cache is holding.
    cache->ForEach(DoUnlockSurface, this);
  }

  void RemoveImage(const ImageKey aImageKey)
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
    // This implicitly unlocks the image if it was locked.
    mImageCaches.Remove(aImageKey);
  }

  void DiscardAll()
  {
    // Remove in order of cost because mCosts is an array and the other data
    // structures are all hash tables. Note that locked surfaces (persistent
    // surfaces belonging to locked images) are not removed, since they aren't
    // present in mCosts.
    while (!mCosts.IsEmpty()) {
      Remove(mCosts.LastElement().GetSurface());
    }
  }

  void DiscardForMemoryPressure()
  {
    // Compute our discardable cost. Since locked surfaces aren't discardable,
    // we exclude them.
    const Cost discardableCost = (mMaxCost - mAvailableCost) - mLockedCost;
    MOZ_ASSERT(discardableCost <= mMaxCost, "Discardable cost doesn't add up");

    // Our target is to raise our available cost by (1 / mDiscardFactor) of our
    // discardable cost - in other words, we want to end up with about
    // (discardableCost / mDiscardFactor) fewer bytes stored in the surface
    // cache after we're done.
    const Cost targetCost = mAvailableCost + (discardableCost / mDiscardFactor);

    if (targetCost > mMaxCost - mLockedCost) {
      MOZ_ASSERT_UNREACHABLE("Target cost is more than we can discard");
      DiscardAll();
      return;
    }

    // Discard surfaces until we've reduced our cost to our target cost.
    while (mAvailableCost < targetCost) {
      MOZ_ASSERT(!mCosts.IsEmpty(), "Removed everything and still not done");
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

  static PLDHashOperator DoLockSurface(const SurfaceKey&,
                                       CachedSurface*    aSurface,
                                       void*             aCache)
  {
    if (aSurface->GetLifetime() == Lifetime::Transient ||
        aSurface->IsLocked()) {
      return PL_DHASH_NEXT;
    }

    auto cache = static_cast<SurfaceCacheImpl*>(aCache);
    cache->StopTracking(aSurface);

    // Lock the surface. This can fail.
    aSurface->SetLocked(true);
    cache->StartTracking(aSurface);

    return PL_DHASH_NEXT;
  }

  static PLDHashOperator DoUnlockSurface(const SurfaceKey&,
                                         CachedSurface*    aSurface,
                                         void*             aCache)
  {
    if (aSurface->GetLifetime() == Lifetime::Transient ||
        !aSurface->IsLocked()) {
      return PL_DHASH_NEXT;
    }

    auto cache = static_cast<SurfaceCacheImpl*>(aCache);
    cache->StopTracking(aSurface);

    aSurface->SetLocked(false);
    cache->StartTracking(aSurface);

    return PL_DHASH_NEXT;
  }

  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport,
                 nsISupports*             aData,
                 bool                     aAnonymize) MOZ_OVERRIDE
  {
    // We have explicit memory reporting for the surface cache which is more
    // accurate than the cost metrics we report here, but these metrics are
    // still useful to report, since they control the cache's behavior.
    nsresult rv;

    rv = MOZ_COLLECT_REPORT("imagelib-surface-cache-estimated-total",
                            KIND_OTHER, UNITS_BYTES,
                            (mMaxCost - mAvailableCost),
                            "Estimated total memory used by the imagelib "
                            "surface cache.");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = MOZ_COLLECT_REPORT("imagelib-surface-cache-estimated-locked",
                            KIND_OTHER, UNITS_BYTES,
                            mLockedCost,
                            "Estimated memory used by locked surfaces in the "
                            "imagelib surface cache.");
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  size_t SizeOfSurfaces(const ImageKey    aImageKey,
                        gfxMemoryLocation aLocation,
                        MallocSizeOf      aMallocSizeOf)
  {
    nsRefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache) {
      return 0;  // No surfaces for this image.
    }

    // Sum the size of all surfaces in the per-image cache.
    CachedSurface::SizeOfSurfacesSum sum(aLocation, aMallocSizeOf);
    cache->ForEach(DoSizeOfSurfacesSum, &sum);

    return sum.Result();
  }

  static PLDHashOperator DoSizeOfSurfacesSum(const SurfaceKey&,
                                             CachedSurface*    aSurface,
                                             void*             aSum)
  {
    auto sum = static_cast<CachedSurface::SizeOfSurfacesSum*>(aSum);
    sum->Add(aSurface);
    return PL_DHASH_NEXT;
  }

private:
  already_AddRefed<ImageSurfaceCache> GetImageCache(const ImageKey aImageKey)
  {
    nsRefPtr<ImageSurfaceCache> imageCache;
    mImageCaches.Get(aImageKey, getter_AddRefs(imageCache));
    return imageCache.forget();
  }

  // This is similar to CanHold() except that it takes into account the costs of
  // locked surfaces. It's used internally in Insert(), but it's not exposed
  // publicly because if we start permitting multithreaded access to the surface
  // cache, which seems likely, then the result would be meaningless: another
  // thread could insert a persistent surface or lock an image at any time.
  bool CanHoldAfterDiscarding(const Cost aCost) const
  {
    return aCost <= mMaxCost - mLockedCost;
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

    NS_IMETHOD Observe(nsISupports*,
                       const char* aTopic,
                       const char16_t*) MOZ_OVERRIDE
    {
      if (sInstance && strcmp(aTopic, "memory-pressure") == 0) {
        sInstance->DiscardForMemoryPressure();
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
  const uint32_t                                            mDiscardFactor;
  const Cost                                                mMaxCost;
  Cost                                                      mAvailableCost;
  Cost                                                      mLockedCost;
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

  // See gfxPrefs for the default values of these preferences.

  // Length of time before an unused surface is removed from the cache, in
  // milliseconds.
  uint32_t surfaceCacheExpirationTimeMS =
    gfxPrefs::ImageMemSurfaceCacheMinExpirationMS();

  // What fraction of the memory used by the surface cache we should discard
  // when we get a memory pressure notification. This value is interpreted as
  // 1/N, so 1 means to discard everything, 2 means to discard about half of the
  // memory we're using, and so forth. We clamp it to avoid division by zero.
  uint32_t surfaceCacheDiscardFactor =
    max(gfxPrefs::ImageMemSurfaceCacheDiscardFactor(), 1u);

  // Maximum size of the surface cache, in kilobytes.
  uint64_t surfaceCacheMaxSizeKB = gfxPrefs::ImageMemSurfaceCacheMaxSizeKB();

  // A knob determining the actual size of the surface cache. Currently the
  // cache is (size of main memory) / (surface cache size factor) KB
  // or (surface cache max size) KB, whichever is smaller. The formula
  // may change in the future, though.
  // For example, a value of 4 would yield a 256MB cache on a 1GB machine.
  // The smallest machines we are likely to run this code on have 256MB
  // of memory, which would yield a 64MB cache on this setting.
  // We clamp this value to avoid division by zero.
  uint32_t surfaceCacheSizeFactor =
    max(gfxPrefs::ImageMemSurfaceCacheSizeFactor(), 1u);

  // Compute the size of the surface cache.
  uint64_t memorySize = PR_GetPhysicalMemorySize();
  if (memorySize == 0) {
    MOZ_ASSERT_UNREACHABLE("PR_GetPhysicalMemorySize not implemented here");
    memorySize = 256 * 1024 * 1024;  // Fall back to 256MB.
  }
  uint64_t proposedSize = memorySize / surfaceCacheSizeFactor;
  uint64_t surfaceCacheSizeBytes = min(proposedSize, surfaceCacheMaxSizeKB * 1024);
  uint32_t finalSurfaceCacheSizeBytes =
    min(surfaceCacheSizeBytes, uint64_t(UINT32_MAX));

  // Create the surface cache singleton with the requested settings.  Note that
  // the size is a limit that the cache may not grow beyond, but we do not
  // actually allocate any storage for surfaces at this time.
  sInstance = new SurfaceCacheImpl(surfaceCacheExpirationTimeMS,
                                   surfaceCacheDiscardFactor,
                                   finalSurfaceCacheSizeBytes);
  sInstance->InitMemoryReporter();
}

/* static */ void
SurfaceCache::Shutdown()
{
  MOZ_ASSERT(sInstance, "No singleton - was Shutdown() called twice?");
  sInstance = nullptr;
}

/* static */ DrawableFrameRef
SurfaceCache::Lookup(const ImageKey    aImageKey,
                     const SurfaceKey& aSurfaceKey)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return DrawableFrameRef();
  }

  return sInstance->Lookup(aImageKey, aSurfaceKey);
}

/* static */ bool
SurfaceCache::Insert(imgFrame*         aSurface,
                     const ImageKey    aImageKey,
                     const SurfaceKey& aSurfaceKey,
                     Lifetime          aLifetime)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return false;
  }

  Cost cost = ComputeCost(aSurfaceKey.Size());
  return sInstance->Insert(aSurface, cost, aImageKey, aSurfaceKey, aLifetime);
}

/* static */ bool
SurfaceCache::CanHold(const IntSize& aSize)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return false;
  }

  Cost cost = ComputeCost(aSize);
  return sInstance->CanHold(cost);
}

/* static */ void
SurfaceCache::LockImage(Image* aImageKey)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sInstance) {
    return sInstance->LockImage(aImageKey);
  }
}

/* static */ void
SurfaceCache::UnlockImage(Image* aImageKey)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sInstance) {
    return sInstance->UnlockImage(aImageKey);
  }
}

/* static */ void
SurfaceCache::RemoveSurface(const ImageKey    aImageKey,
                            const SurfaceKey& aSurfaceKey)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sInstance) {
    sInstance->RemoveSurface(aImageKey, aSurfaceKey);
  }
}

/* static */ void
SurfaceCache::RemoveImage(Image* aImageKey)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sInstance) {
    sInstance->RemoveImage(aImageKey);
  }
}

/* static */ void
SurfaceCache::DiscardAll()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sInstance) {
    sInstance->DiscardAll();
  }
}

/* static */ size_t
SurfaceCache::SizeOfSurfaces(const ImageKey    aImageKey,
                             gfxMemoryLocation aLocation,
                             MallocSizeOf      aMallocSizeOf)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return 0;
  }

  return sInstance->SizeOfSurfaces(aImageKey, aLocation, aMallocSizeOf);
}

} // namespace image
} // namespace mozilla
