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
#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/Move.h"
#include "mozilla/Pair.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_image.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Tuple.h"
#include "nsIMemoryReporter.h"
#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "imgFrame.h"
#include "Image.h"
#include "ISurfaceProvider.h"
#include "LookupResult.h"
#include "nsExpirationTracker.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsSize.h"
#include "nsTArray.h"
#include "prsystem.h"
#include "ShutdownTracker.h"

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

// The mutex protecting the surface cache.
static StaticMutex sInstanceMutex;

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

static Cost ComputeCost(const IntSize& aSize, uint32_t aBytesPerPixel) {
  MOZ_ASSERT(aBytesPerPixel == 1 || aBytesPerPixel == 4);
  return aSize.width * aSize.height * aBytesPerPixel;
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
class CostEntry {
 public:
  CostEntry(NotNull<CachedSurface*> aSurface, Cost aCost)
      : mSurface(aSurface), mCost(aCost) {}

  NotNull<CachedSurface*> Surface() const { return mSurface; }
  Cost GetCost() const { return mCost; }

  bool operator==(const CostEntry& aOther) const {
    return mSurface == aOther.mSurface && mCost == aOther.mCost;
  }

  bool operator<(const CostEntry& aOther) const {
    return mCost < aOther.mCost ||
           (mCost == aOther.mCost &&
            recordreplay::RecordReplayValue(mSurface < aOther.mSurface));
  }

 private:
  NotNull<CachedSurface*> mSurface;
  Cost mCost;
};

/**
 * A CachedSurface associates a surface with a key that uniquely identifies that
 * surface.
 */
class CachedSurface {
  ~CachedSurface() {}

 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(CachedSurface)
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CachedSurface)

  explicit CachedSurface(NotNull<ISurfaceProvider*> aProvider)
      : mProvider(aProvider), mIsLocked(false) {}

  DrawableSurface GetDrawableSurface() const {
    if (MOZ_UNLIKELY(IsPlaceholder())) {
      MOZ_ASSERT_UNREACHABLE("Called GetDrawableSurface() on a placeholder");
      return DrawableSurface();
    }

    return mProvider->Surface();
  }

  void SetLocked(bool aLocked) {
    if (IsPlaceholder()) {
      return;  // Can't lock a placeholder.
    }

    // Update both our state and our provider's state. Some surface providers
    // are permanently locked; maintaining our own locking state enables us to
    // respect SetLocked() even when it's meaningless from the provider's
    // perspective.
    mIsLocked = aLocked;
    mProvider->SetLocked(aLocked);
  }

  bool IsLocked() const {
    return !IsPlaceholder() && mIsLocked && mProvider->IsLocked();
  }

  void SetCannotSubstitute() {
    mProvider->Availability().SetCannotSubstitute();
  }
  bool CannotSubstitute() const {
    return mProvider->Availability().CannotSubstitute();
  }

  bool IsPlaceholder() const {
    return mProvider->Availability().IsPlaceholder();
  }
  bool IsDecoded() const { return !IsPlaceholder() && mProvider->IsFinished(); }

  ImageKey GetImageKey() const { return mProvider->GetImageKey(); }
  const SurfaceKey& GetSurfaceKey() const { return mProvider->GetSurfaceKey(); }
  nsExpirationState* GetExpirationState() { return &mExpirationState; }

  CostEntry GetCostEntry() {
    return image::CostEntry(WrapNotNull(this), mProvider->LogicalSizeInBytes());
  }

  // A helper type used by SurfaceCacheImpl::CollectSizeOfSurfaces.
  struct MOZ_STACK_CLASS SurfaceMemoryReport {
    SurfaceMemoryReport(nsTArray<SurfaceMemoryCounter>& aCounters,
                        MallocSizeOf aMallocSizeOf)
        : mCounters(aCounters), mMallocSizeOf(aMallocSizeOf) {}

    void Add(NotNull<CachedSurface*> aCachedSurface, bool aIsFactor2) {
      if (aCachedSurface->IsPlaceholder()) {
        return;
      }

      // Record the memory used by the ISurfaceProvider. This may not have a
      // straightforward relationship to the size of the surface that
      // DrawableRef() returns if the surface is generated dynamically. (i.e.,
      // for surfaces with PlaybackType::eAnimated.)
      aCachedSurface->mProvider->AddSizeOfExcludingThis(
          mMallocSizeOf, [&](ISurfaceProvider::AddSizeOfCbData& aMetadata) {
            SurfaceMemoryCounter counter(
                aCachedSurface->GetSurfaceKey(), aCachedSurface->IsLocked(),
                aCachedSurface->CannotSubstitute(), aIsFactor2);

            counter.Values().SetDecodedHeap(aMetadata.heap);
            counter.Values().SetDecodedNonHeap(aMetadata.nonHeap);
            counter.Values().SetExternalHandles(aMetadata.handles);
            counter.Values().SetFrameIndex(aMetadata.index);
            counter.Values().SetExternalId(aMetadata.externalId);

            mCounters.AppendElement(counter);
          });
    }

   private:
    nsTArray<SurfaceMemoryCounter>& mCounters;
    MallocSizeOf mMallocSizeOf;
  };

 private:
  nsExpirationState mExpirationState;
  NotNull<RefPtr<ISurfaceProvider>> mProvider;
  bool mIsLocked;
};

static int64_t AreaOfIntSize(const IntSize& aSize) {
  return static_cast<int64_t>(aSize.width) * static_cast<int64_t>(aSize.height);
}

/**
 * An ImageSurfaceCache is a per-image surface cache. For correctness we must be
 * able to remove all surfaces associated with an image when the image is
 * destroyed or invalidated. Since this will happen frequently, it makes sense
 * to make it cheap by storing the surfaces for each image separately.
 *
 * ImageSurfaceCache also keeps track of whether its associated image is locked
 * or unlocked.
 *
 * The cache may also enter "factor of 2" mode which occurs when the number of
 * surfaces in the cache exceeds the "image.cache.factor2.threshold-surfaces"
 * pref plus the number of native sizes of the image. When in "factor of 2"
 * mode, the cache will strongly favour sizes which are a factor of 2 of the
 * largest native size. It accomplishes this by suggesting a factor of 2 size
 * when lookups fail and substituting the nearest factor of 2 surface to the
 * ideal size as the "best" available (as opposed to subsitution but not found).
 * This allows us to minimize memory consumption and CPU time spent decoding
 * when a website requires many variants of the same surface.
 */
class ImageSurfaceCache {
  ~ImageSurfaceCache() {}

 public:
  explicit ImageSurfaceCache(const ImageKey aImageKey)
      : mLocked(false),
        mFactor2Mode(false),
        mFactor2Pruned(false),
        mIsVectorImage(aImageKey->GetType() == imgIContainer::TYPE_VECTOR) {}

  MOZ_DECLARE_REFCOUNTED_TYPENAME(ImageSurfaceCache)
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageSurfaceCache)

  typedef nsRefPtrHashtable<nsGenericHashKey<SurfaceKey>, CachedSurface>
      SurfaceTable;

  bool IsEmpty() const { return mSurfaces.Count() == 0; }

  MOZ_MUST_USE bool Insert(NotNull<CachedSurface*> aSurface) {
    MOZ_ASSERT(!mLocked || aSurface->IsPlaceholder() || aSurface->IsLocked(),
               "Inserting an unlocked surface for a locked image");
    return mSurfaces.Put(aSurface->GetSurfaceKey(), aSurface, fallible);
  }

  already_AddRefed<CachedSurface> Remove(NotNull<CachedSurface*> aSurface) {
    MOZ_ASSERT(mSurfaces.GetWeak(aSurface->GetSurfaceKey()),
               "Should not be removing a surface we don't have");

    RefPtr<CachedSurface> surface;
    mSurfaces.Remove(aSurface->GetSurfaceKey(), getter_AddRefs(surface));
    AfterMaybeRemove();
    return surface.forget();
  }

  already_AddRefed<CachedSurface> Lookup(const SurfaceKey& aSurfaceKey,
                                         bool aForAccess) {
    RefPtr<CachedSurface> surface;
    mSurfaces.Get(aSurfaceKey, getter_AddRefs(surface));

    if (aForAccess) {
      if (surface) {
        // We don't want to allow factor of 2 mode pruning to release surfaces
        // for which the callers will accept no substitute.
        surface->SetCannotSubstitute();
      } else if (!mFactor2Mode) {
        // If no exact match is found, and this is for use rather than internal
        // accounting (i.e. insert and removal), we know this will trigger a
        // decode. Make sure we switch now to factor of 2 mode if necessary.
        MaybeSetFactor2Mode();
      }
    }

    return surface.forget();
  }

  /**
   * @returns A tuple containing the best matching CachedSurface if available,
   *          a MatchType describing how the CachedSurface was selected, and
   *          an IntSize which is the size the caller should choose to decode
   *          at should it attempt to do so.
   */
  Tuple<already_AddRefed<CachedSurface>, MatchType, IntSize> LookupBestMatch(
      const SurfaceKey& aIdealKey) {
    // Try for an exact match first.
    RefPtr<CachedSurface> exactMatch;
    mSurfaces.Get(aIdealKey, getter_AddRefs(exactMatch));
    if (exactMatch) {
      if (exactMatch->IsDecoded()) {
        return MakeTuple(exactMatch.forget(), MatchType::EXACT, IntSize());
      }
    } else if (!mFactor2Mode) {
      // If no exact match is found, and we are not in factor of 2 mode, then
      // we know that we will trigger a decode because at best we will provide
      // a substitute. Make sure we switch now to factor of 2 mode if necessary.
      MaybeSetFactor2Mode();
    }

    // Try for a best match second, if using compact.
    IntSize suggestedSize = SuggestedSize(aIdealKey.Size());
    if (suggestedSize != aIdealKey.Size()) {
      if (!exactMatch) {
        SurfaceKey compactKey = aIdealKey.CloneWithSize(suggestedSize);
        mSurfaces.Get(compactKey, getter_AddRefs(exactMatch));
        if (exactMatch && exactMatch->IsDecoded()) {
          MOZ_ASSERT(suggestedSize != aIdealKey.Size());
          return MakeTuple(exactMatch.forget(),
                           MatchType::SUBSTITUTE_BECAUSE_BEST, suggestedSize);
        }
      }
    }

    // There's no perfect match, so find the best match we can.
    RefPtr<CachedSurface> bestMatch;
    for (auto iter = ConstIter(); !iter.Done(); iter.Next()) {
      NotNull<CachedSurface*> current = WrapNotNull(iter.UserData());
      const SurfaceKey& currentKey = current->GetSurfaceKey();

      // We never match a placeholder.
      if (current->IsPlaceholder()) {
        continue;
      }
      // Matching the playback type and SVG context is required.
      if (currentKey.Playback() != aIdealKey.Playback() ||
          currentKey.SVGContext() != aIdealKey.SVGContext()) {
        continue;
      }
      // Matching the flags is required.
      if (currentKey.Flags() != aIdealKey.Flags()) {
        continue;
      }
      // Anything is better than nothing! (Within the constraints we just
      // checked, of course.)
      if (!bestMatch) {
        bestMatch = current;
        continue;
      }

      MOZ_ASSERT(bestMatch, "Should have a current best match");

      // Always prefer completely decoded surfaces.
      bool bestMatchIsDecoded = bestMatch->IsDecoded();
      if (bestMatchIsDecoded && !current->IsDecoded()) {
        continue;
      }
      if (!bestMatchIsDecoded && current->IsDecoded()) {
        bestMatch = current;
        continue;
      }

      SurfaceKey bestMatchKey = bestMatch->GetSurfaceKey();
      if (CompareArea(aIdealKey.Size(), bestMatchKey.Size(),
                      currentKey.Size())) {
        bestMatch = current;
      }
    }

    MatchType matchType;
    if (bestMatch) {
      if (!exactMatch) {
        // No exact match, neither ideal nor factor of 2.
        MOZ_ASSERT(suggestedSize != bestMatch->GetSurfaceKey().Size(),
                   "No exact match despite the fact the sizes match!");
        matchType = MatchType::SUBSTITUTE_BECAUSE_NOT_FOUND;
      } else if (exactMatch != bestMatch) {
        // The exact match is still decoding, but we found a substitute.
        matchType = MatchType::SUBSTITUTE_BECAUSE_PENDING;
      } else if (aIdealKey.Size() != bestMatch->GetSurfaceKey().Size()) {
        // The best factor of 2 match is still decoding, but the best we've got.
        MOZ_ASSERT(suggestedSize != aIdealKey.Size());
        MOZ_ASSERT(mFactor2Mode || mIsVectorImage);
        matchType = MatchType::SUBSTITUTE_BECAUSE_BEST;
      } else {
        // The exact match is still decoding, but it's the best we've got.
        matchType = MatchType::EXACT;
      }
    } else {
      if (exactMatch) {
        // We found an "exact match"; it must have been a placeholder.
        MOZ_ASSERT(exactMatch->IsPlaceholder());
        matchType = MatchType::PENDING;
      } else {
        // We couldn't find an exact match *or* a substitute.
        matchType = MatchType::NOT_FOUND;
      }
    }

    return MakeTuple(bestMatch.forget(), matchType, suggestedSize);
  }

  void MaybeSetFactor2Mode() {
    MOZ_ASSERT(!mFactor2Mode);

    // Typically an image cache will not have too many size-varying surfaces, so
    // if we exceed the given threshold, we should consider using a subset.
    int32_t thresholdSurfaces =
        StaticPrefs::image_cache_factor2_threshold_surfaces();
    if (thresholdSurfaces < 0 ||
        mSurfaces.Count() <= static_cast<uint32_t>(thresholdSurfaces)) {
      return;
    }

    // Determine how many native surfaces this image has. If it is zero, and it
    // is a vector image, then we should impute a single native size. Otherwise,
    // it may be zero because we don't know yet, or the image has an error, or
    // it isn't supported.
    auto first = ConstIter();
    NotNull<CachedSurface*> current = WrapNotNull(first.UserData());
    Image* image = static_cast<Image*>(current->GetImageKey());
    size_t nativeSizes = image->GetNativeSizesLength();
    if (mIsVectorImage) {
      MOZ_ASSERT(nativeSizes == 0);
      nativeSizes = 1;
    } else if (nativeSizes == 0) {
      return;
    }

    // Increase the threshold by the number of native sizes. This ensures that
    // we do not prevent decoding of the image at all its native sizes. It does
    // not guarantee we will provide a surface at that size however (i.e. many
    // other sized surfaces are requested, in addition to the native sizes).
    thresholdSurfaces += nativeSizes;
    if (mSurfaces.Count() <= static_cast<uint32_t>(thresholdSurfaces)) {
      return;
    }

    // Get our native size. While we know the image should be fully decoded,
    // if it is an SVG, it is valid to have a zero size. We can't do compacting
    // in that case because we need to know the width/height ratio to define a
    // candidate set.
    IntSize nativeSize;
    if (NS_FAILED(image->GetWidth(&nativeSize.width)) ||
        NS_FAILED(image->GetHeight(&nativeSize.height)) ||
        nativeSize.IsEmpty()) {
      return;
    }

    // We have a valid size, we can change modes.
    mFactor2Mode = true;
  }

  template <typename Function>
  void Prune(Function&& aRemoveCallback) {
    if (!mFactor2Mode || mFactor2Pruned) {
      return;
    }

    // Attempt to discard any surfaces which are not factor of 2 and the best
    // factor of 2 match exists.
    bool hasNotFactorSize = false;
    for (auto iter = mSurfaces.Iter(); !iter.Done(); iter.Next()) {
      NotNull<CachedSurface*> current = WrapNotNull(iter.UserData());
      const SurfaceKey& currentKey = current->GetSurfaceKey();
      const IntSize& currentSize = currentKey.Size();

      // First we check if someone requested this size and would not accept
      // an alternatively sized surface.
      if (current->CannotSubstitute()) {
        continue;
      }

      // Next we find the best factor of 2 size for this surface. If this
      // surface is a factor of 2 size, then we want to keep it.
      IntSize bestSize = SuggestedSize(currentSize);
      if (bestSize == currentSize) {
        continue;
      }

      // Check the cache for a surface with the same parameters except for the
      // size which uses the closest factor of 2 size.
      SurfaceKey compactKey = currentKey.CloneWithSize(bestSize);
      RefPtr<CachedSurface> compactMatch;
      mSurfaces.Get(compactKey, getter_AddRefs(compactMatch));
      if (compactMatch && compactMatch->IsDecoded()) {
        aRemoveCallback(current);
        iter.Remove();
      } else {
        hasNotFactorSize = true;
      }
    }

    // We have no surfaces that are not factor of 2 sized, so we can stop
    // pruning henceforth, because we avoid the insertion of new surfaces that
    // don't match our sizing set (unless the caller won't accept a
    // substitution.)
    if (!hasNotFactorSize) {
      mFactor2Pruned = true;
    }

    // We should never leave factor of 2 mode due to pruning in of itself, but
    // if we discarded surfaces due to the volatile buffers getting released,
    // it is possible.
    AfterMaybeRemove();
  }

  IntSize SuggestedSize(const IntSize& aSize) const {
    IntSize suggestedSize = SuggestedSizeInternal(aSize);
    if (mIsVectorImage) {
      suggestedSize = SurfaceCache::ClampVectorSize(suggestedSize);
    }
    return suggestedSize;
  }

  IntSize SuggestedSizeInternal(const IntSize& aSize) const {
    // When not in factor of 2 mode, we can always decode at the given size.
    if (!mFactor2Mode) {
      return aSize;
    }

    // We cannot enter factor of 2 mode unless we have a minimum number of
    // surfaces, and we should have left it if the cache was emptied.
    if (MOZ_UNLIKELY(IsEmpty())) {
      MOZ_ASSERT_UNREACHABLE("Should not be empty and in factor of 2 mode!");
      return aSize;
    }

    // This bit of awkwardness gets the largest native size of the image.
    auto iter = ConstIter();
    NotNull<CachedSurface*> firstSurface = WrapNotNull(iter.UserData());
    Image* image = static_cast<Image*>(firstSurface->GetImageKey());
    IntSize factorSize;
    if (NS_FAILED(image->GetWidth(&factorSize.width)) ||
        NS_FAILED(image->GetHeight(&factorSize.height)) ||
        factorSize.IsEmpty()) {
      // We should not have entered factor of 2 mode without a valid size, and
      // several successfully decoded surfaces. Note that valid vector images
      // may have a default size of 0x0, and those are not yet supported.
      MOZ_ASSERT_UNREACHABLE("Expected valid native size!");
      return aSize;
    }

    if (mIsVectorImage) {
      // Ensure the aspect ratio matches the native size before forcing the
      // caller to accept a factor of 2 size. The difference between the aspect
      // ratios is:
      //
      //     delta = nativeWidth/nativeHeight - desiredWidth/desiredHeight
      //
      //     delta*nativeHeight*desiredHeight = nativeWidth*desiredHeight
      //                                      - desiredWidth*nativeHeight
      //
      // Using the maximum accepted delta as a constant, we can avoid the
      // floating point division and just compare after some integer ops.
      int32_t delta =
          factorSize.width * aSize.height - aSize.width * factorSize.height;
      int32_t maxDelta = (factorSize.height * aSize.height) >> 4;
      if (delta > maxDelta || delta < -maxDelta) {
        return aSize;
      }

      // If the requested size is bigger than the native size, we actually need
      // to grow the native size instead of shrinking it.
      if (factorSize.width < aSize.width) {
        do {
          IntSize candidate(factorSize.width * 2, factorSize.height * 2);
          if (!SurfaceCache::IsLegalSize(candidate)) {
            break;
          }

          factorSize = candidate;
        } while (factorSize.width < aSize.width);

        return factorSize;
      }

      // Otherwise we can find the best fit as normal.
    }

    // Start with the native size as the best first guess.
    IntSize bestSize = factorSize;
    factorSize.width /= 2;
    factorSize.height /= 2;

    while (!factorSize.IsEmpty()) {
      if (!CompareArea(aSize, bestSize, factorSize)) {
        // This size is not better than the last. Since we proceed from largest
        // to smallest, we know that the next size will not be better if the
        // previous size was rejected. Break early.
        break;
      }

      // The current factor of 2 size is better than the last selected size.
      bestSize = factorSize;
      factorSize.width /= 2;
      factorSize.height /= 2;
    }

    return bestSize;
  }

  bool CompareArea(const IntSize& aIdealSize, const IntSize& aBestSize,
                   const IntSize& aSize) const {
    // Compare sizes. We use an area-based heuristic here instead of computing a
    // truly optimal answer, since it seems very unlikely to make a difference
    // for realistic sizes.
    int64_t idealArea = AreaOfIntSize(aIdealSize);
    int64_t currentArea = AreaOfIntSize(aSize);
    int64_t bestMatchArea = AreaOfIntSize(aBestSize);

    // If the best match is smaller than the ideal size, prefer bigger sizes.
    if (bestMatchArea < idealArea) {
      if (currentArea > bestMatchArea) {
        return true;
      }
      return false;
    }

    // Other, prefer sizes closer to the ideal size, but still not smaller.
    if (idealArea <= currentArea && currentArea < bestMatchArea) {
      return true;
    }

    // This surface isn't an improvement over the current best match.
    return false;
  }

  template <typename Function>
  void CollectSizeOfSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                             MallocSizeOf aMallocSizeOf,
                             Function&& aRemoveCallback) {
    CachedSurface::SurfaceMemoryReport report(aCounters, aMallocSizeOf);
    for (auto iter = mSurfaces.Iter(); !iter.Done(); iter.Next()) {
      NotNull<CachedSurface*> surface = WrapNotNull(iter.UserData());

      // We don't need the drawable surface for ourselves, but adding a surface
      // to the report will trigger this indirectly. If the surface was
      // discarded by the OS because it was in volatile memory, we should remove
      // it from the cache immediately rather than include it in the report.
      DrawableSurface drawableSurface;
      if (!surface->IsPlaceholder()) {
        drawableSurface = surface->GetDrawableSurface();
        if (!drawableSurface) {
          aRemoveCallback(surface);
          iter.Remove();
          continue;
        }
      }

      const IntSize& size = surface->GetSurfaceKey().Size();
      bool factor2Size = false;
      if (mFactor2Mode) {
        factor2Size = (size == SuggestedSize(size));
      }
      report.Add(surface, factor2Size);
    }

    AfterMaybeRemove();
  }

  SurfaceTable::Iterator ConstIter() const { return mSurfaces.ConstIter(); }

  void SetLocked(bool aLocked) { mLocked = aLocked; }
  bool IsLocked() const { return mLocked; }

 private:
  void AfterMaybeRemove() {
    if (IsEmpty() && mFactor2Mode) {
      // The last surface for this cache was removed. This can happen if the
      // surface was stored in a volatile buffer and got purged, or the surface
      // expired from the cache. If the cache itself lingers for some reason
      // (e.g. in the process of performing a lookup, the cache itself is
      // locked), then we need to reset the factor of 2 state because it
      // requires at least one surface present to get the native size
      // information from the image.
      mFactor2Mode = mFactor2Pruned = false;
    }
  }

  SurfaceTable mSurfaces;

  bool mLocked;

  // True in "factor of 2" mode.
  bool mFactor2Mode;

  // True if all non-factor of 2 surfaces have been removed from the cache. Note
  // that this excludes unsubstitutable sizes.
  bool mFactor2Pruned;

  // True if the surfaces are produced from a vector image. If so, it must match
  // the aspect ratio when using factor of 2 mode.
  bool mIsVectorImage;
};

/**
 * SurfaceCacheImpl is responsible for determining which surfaces will be cached
 * and managing the surface cache data structures. Rather than interact with
 * SurfaceCacheImpl directly, client code interacts with SurfaceCache, which
 * maintains high-level invariants and encapsulates the details of the surface
 * cache's implementation.
 */
class SurfaceCacheImpl final : public nsIMemoryReporter {
 public:
  NS_DECL_ISUPPORTS

  SurfaceCacheImpl(uint32_t aSurfaceCacheExpirationTimeMS,
                   uint32_t aSurfaceCacheDiscardFactor,
                   uint32_t aSurfaceCacheSize)
      : mExpirationTracker(aSurfaceCacheExpirationTimeMS),
        mMemoryPressureObserver(new MemoryPressureObserver),
        mDiscardFactor(aSurfaceCacheDiscardFactor),
        mMaxCost(aSurfaceCacheSize),
        mAvailableCost(aSurfaceCacheSize),
        mLockedCost(0),
        mOverflowCount(0) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->AddObserver(mMemoryPressureObserver, "memory-pressure", false);
    }
  }

 private:
  virtual ~SurfaceCacheImpl() {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->RemoveObserver(mMemoryPressureObserver, "memory-pressure");
    }

    UnregisterWeakMemoryReporter(this);
  }

 public:
  void InitMemoryReporter() { RegisterWeakMemoryReporter(this); }

  InsertOutcome Insert(NotNull<ISurfaceProvider*> aProvider, bool aSetAvailable,
                       const StaticMutexAutoLock& aAutoLock) {
    // If this is a duplicate surface, refuse to replace the original.
    // XXX(seth): Calling Lookup() and then RemoveEntry() does the lookup
    // twice. We'll make this more efficient in bug 1185137.
    LookupResult result =
        Lookup(aProvider->GetImageKey(), aProvider->GetSurfaceKey(), aAutoLock,
               /* aMarkUsed = */ false);
    if (MOZ_UNLIKELY(result)) {
      return InsertOutcome::FAILURE_ALREADY_PRESENT;
    }

    if (result.Type() == MatchType::PENDING) {
      RemoveEntry(aProvider->GetImageKey(), aProvider->GetSurfaceKey(),
                  aAutoLock);
    }

    MOZ_ASSERT(result.Type() == MatchType::NOT_FOUND ||
                   result.Type() == MatchType::PENDING,
               "A LookupResult with no surface should be NOT_FOUND or PENDING");

    // If this is bigger than we can hold after discarding everything we can,
    // refuse to cache it.
    Cost cost = aProvider->LogicalSizeInBytes();
    if (MOZ_UNLIKELY(!CanHoldAfterDiscarding(cost))) {
      mOverflowCount++;
      return InsertOutcome::FAILURE;
    }

    // Remove elements in order of cost until we can fit this in the cache. Note
    // that locked surfaces aren't in mCosts, so we never remove them here.
    while (cost > mAvailableCost) {
      MOZ_ASSERT(!mCosts.IsEmpty(),
                 "Removed everything and it still won't fit");
      Remove(mCosts.LastElement().Surface(), /* aStopTracking */ true,
             aAutoLock);
    }

    // Locate the appropriate per-image cache. If there's not an existing cache
    // for this image, create it.
    const ImageKey imageKey = aProvider->GetImageKey();
    RefPtr<ImageSurfaceCache> cache = GetImageCache(imageKey);
    if (!cache) {
      cache = new ImageSurfaceCache(imageKey);
      mImageCaches.Put(aProvider->GetImageKey(), cache);
    }

    // If we were asked to mark the cache entry available, do so.
    if (aSetAvailable) {
      aProvider->Availability().SetAvailable();
    }

    auto surface = MakeNotNull<RefPtr<CachedSurface>>(aProvider);

    // We require that locking succeed if the image is locked and we're not
    // inserting a placeholder; the caller may need to know this to handle
    // errors correctly.
    bool mustLock = cache->IsLocked() && !surface->IsPlaceholder();
    if (mustLock) {
      surface->SetLocked(true);
      if (!surface->IsLocked()) {
        return InsertOutcome::FAILURE;
      }
    }

    // Insert.
    MOZ_ASSERT(cost <= mAvailableCost, "Inserting despite too large a cost");
    if (!cache->Insert(surface)) {
      if (mustLock) {
        surface->SetLocked(false);
      }
      return InsertOutcome::FAILURE;
    }

    if (MOZ_UNLIKELY(!StartTracking(surface, aAutoLock))) {
      MOZ_ASSERT(!mustLock);
      Remove(surface, /* aStopTracking */ false, aAutoLock);
      return InsertOutcome::FAILURE;
    }

    return InsertOutcome::SUCCESS;
  }

  void Remove(NotNull<CachedSurface*> aSurface, bool aStopTracking,
              const StaticMutexAutoLock& aAutoLock) {
    ImageKey imageKey = aSurface->GetImageKey();

    RefPtr<ImageSurfaceCache> cache = GetImageCache(imageKey);
    MOZ_ASSERT(cache, "Shouldn't try to remove a surface with no image cache");

    // If the surface was not a placeholder, tell its image that we discarded
    // it.
    if (!aSurface->IsPlaceholder()) {
      static_cast<Image*>(imageKey)->OnSurfaceDiscarded(
          aSurface->GetSurfaceKey());
    }

    // If we failed during StartTracking, we can skip this step.
    if (aStopTracking) {
      StopTracking(aSurface, /* aIsTracked */ true, aAutoLock);
    }

    // Individual surfaces must be freed outside the lock.
    mCachedSurfacesDiscard.AppendElement(cache->Remove(aSurface));

    MaybeRemoveEmptyCache(imageKey, cache);
  }

  bool StartTracking(NotNull<CachedSurface*> aSurface,
                     const StaticMutexAutoLock& aAutoLock) {
    CostEntry costEntry = aSurface->GetCostEntry();
    MOZ_ASSERT(costEntry.GetCost() <= mAvailableCost,
               "Cost too large and the caller didn't catch it");

    if (aSurface->IsLocked()) {
      mLockedCost += costEntry.GetCost();
      MOZ_ASSERT(mLockedCost <= mMaxCost, "Locked more than we can hold?");
    } else {
      if (NS_WARN_IF(!mCosts.InsertElementSorted(costEntry, fallible))) {
        return false;
      }

      // This may fail during XPCOM shutdown, so we need to ensure the object is
      // tracked before calling RemoveObject in StopTracking.
      nsresult rv = mExpirationTracker.AddObjectLocked(aSurface, aAutoLock);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        DebugOnly<bool> foundInCosts = mCosts.RemoveElementSorted(costEntry);
        MOZ_ASSERT(foundInCosts, "Lost track of costs for this surface");
        return false;
      }
    }

    mAvailableCost -= costEntry.GetCost();
    return true;
  }

  void StopTracking(NotNull<CachedSurface*> aSurface, bool aIsTracked,
                    const StaticMutexAutoLock& aAutoLock) {
    CostEntry costEntry = aSurface->GetCostEntry();

    if (aSurface->IsLocked()) {
      MOZ_ASSERT(mLockedCost >= costEntry.GetCost(), "Costs don't balance");
      mLockedCost -= costEntry.GetCost();
      // XXX(seth): It'd be nice to use an O(log n) lookup here. This is O(n).
      MOZ_ASSERT(!mCosts.Contains(costEntry),
                 "Shouldn't have a cost entry for a locked surface");
    } else {
      if (MOZ_LIKELY(aSurface->GetExpirationState()->IsTracked())) {
        MOZ_ASSERT(aIsTracked, "Expiration-tracking a surface unexpectedly!");
        mExpirationTracker.RemoveObjectLocked(aSurface, aAutoLock);
      } else {
        // Our call to AddObject must have failed in StartTracking; most likely
        // we're in XPCOM shutdown right now.
        MOZ_ASSERT(!aIsTracked, "Not expiration-tracking an unlocked surface!");
      }

      DebugOnly<bool> foundInCosts = mCosts.RemoveElementSorted(costEntry);
      MOZ_ASSERT(foundInCosts, "Lost track of costs for this surface");
    }

    mAvailableCost += costEntry.GetCost();
    MOZ_ASSERT(mAvailableCost <= mMaxCost,
               "More available cost than we started with");
  }

  LookupResult Lookup(const ImageKey aImageKey, const SurfaceKey& aSurfaceKey,
                      const StaticMutexAutoLock& aAutoLock, bool aMarkUsed) {
    RefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache) {
      // No cached surfaces for this image.
      return LookupResult(MatchType::NOT_FOUND);
    }

    RefPtr<CachedSurface> surface = cache->Lookup(aSurfaceKey, aMarkUsed);
    if (!surface) {
      // Lookup in the per-image cache missed.
      return LookupResult(MatchType::NOT_FOUND);
    }

    if (surface->IsPlaceholder()) {
      return LookupResult(MatchType::PENDING);
    }

    DrawableSurface drawableSurface = surface->GetDrawableSurface();
    if (!drawableSurface) {
      // The surface was released by the operating system. Remove the cache
      // entry as well.
      Remove(WrapNotNull(surface), /* aStopTracking */ true, aAutoLock);
      return LookupResult(MatchType::NOT_FOUND);
    }

    if (aMarkUsed &&
        !MarkUsed(WrapNotNull(surface), WrapNotNull(cache), aAutoLock)) {
      Remove(WrapNotNull(surface), /* aStopTracking */ false, aAutoLock);
      return LookupResult(MatchType::NOT_FOUND);
    }

    MOZ_ASSERT(surface->GetSurfaceKey() == aSurfaceKey,
               "Lookup() not returning an exact match?");
    return LookupResult(std::move(drawableSurface), MatchType::EXACT);
  }

  LookupResult LookupBestMatch(const ImageKey aImageKey,
                               const SurfaceKey& aSurfaceKey,
                               const StaticMutexAutoLock& aAutoLock,
                               bool aMarkUsed) {
    RefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache) {
      // No cached surfaces for this image.
      return LookupResult(
          MatchType::NOT_FOUND,
          SurfaceCache::ClampSize(aImageKey, aSurfaceKey.Size()));
    }

    // Repeatedly look up the best match, trying again if the resulting surface
    // has been freed by the operating system, until we can either lock a
    // surface for drawing or there are no matching surfaces left.
    // XXX(seth): This is O(N^2), but N is expected to be very small. If we
    // encounter a performance problem here we can revisit this.

    RefPtr<CachedSurface> surface;
    DrawableSurface drawableSurface;
    MatchType matchType = MatchType::NOT_FOUND;
    IntSize suggestedSize;
    while (true) {
      Tie(surface, matchType, suggestedSize) =
          cache->LookupBestMatch(aSurfaceKey);

      if (!surface) {
        return LookupResult(
            matchType, suggestedSize);  // Lookup in the per-image cache missed.
      }

      drawableSurface = surface->GetDrawableSurface();
      if (drawableSurface) {
        break;
      }

      // The surface was released by the operating system. Remove the cache
      // entry as well.
      Remove(WrapNotNull(surface), /* aStopTracking */ true, aAutoLock);
    }

    MOZ_ASSERT_IF(matchType == MatchType::EXACT,
                  surface->GetSurfaceKey() == aSurfaceKey);
    MOZ_ASSERT_IF(
        matchType == MatchType::SUBSTITUTE_BECAUSE_NOT_FOUND ||
            matchType == MatchType::SUBSTITUTE_BECAUSE_PENDING,
        surface->GetSurfaceKey().SVGContext() == aSurfaceKey.SVGContext() &&
            surface->GetSurfaceKey().Playback() == aSurfaceKey.Playback() &&
            surface->GetSurfaceKey().Flags() == aSurfaceKey.Flags());

    if (matchType == MatchType::EXACT ||
        matchType == MatchType::SUBSTITUTE_BECAUSE_BEST) {
      if (aMarkUsed &&
          !MarkUsed(WrapNotNull(surface), WrapNotNull(cache), aAutoLock)) {
        Remove(WrapNotNull(surface), /* aStopTracking */ false, aAutoLock);
      }
    }

    return LookupResult(std::move(drawableSurface), matchType, suggestedSize);
  }

  bool CanHold(const Cost aCost) const { return aCost <= mMaxCost; }

  size_t MaximumCapacity() const { return size_t(mMaxCost); }

  void SurfaceAvailable(NotNull<ISurfaceProvider*> aProvider,
                        const StaticMutexAutoLock& aAutoLock) {
    if (!aProvider->Availability().IsPlaceholder()) {
      MOZ_ASSERT_UNREACHABLE("Calling SurfaceAvailable on non-placeholder");
      return;
    }

    // Reinsert the provider, requesting that Insert() mark it available. This
    // may or may not succeed, depending on whether some other decoder has
    // beaten us to the punch and inserted a non-placeholder version of this
    // surface first, but it's fine either way.
    // XXX(seth): This could be implemented more efficiently; we should be able
    // to just update our data structures without reinserting.
    Insert(aProvider, /* aSetAvailable = */ true, aAutoLock);
  }

  void LockImage(const ImageKey aImageKey) {
    RefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache) {
      cache = new ImageSurfaceCache(aImageKey);
      mImageCaches.Put(aImageKey, cache);
    }

    cache->SetLocked(true);

    // We don't relock this image's existing surfaces right away; instead, the
    // image should arrange for Lookup() to touch them if they are still useful.
  }

  void UnlockImage(const ImageKey aImageKey,
                   const StaticMutexAutoLock& aAutoLock) {
    RefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache || !cache->IsLocked()) {
      return;  // Already unlocked.
    }

    cache->SetLocked(false);
    DoUnlockSurfaces(WrapNotNull(cache), /* aStaticOnly = */ false, aAutoLock);
  }

  void UnlockEntries(const ImageKey aImageKey,
                     const StaticMutexAutoLock& aAutoLock) {
    RefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache || !cache->IsLocked()) {
      return;  // Already unlocked.
    }

    // (Note that we *don't* unlock the per-image cache here; that's the
    // difference between this and UnlockImage.)
    DoUnlockSurfaces(WrapNotNull(cache),
                     /* aStaticOnly = */
                     !StaticPrefs::image_mem_animated_discardable_AtStartup(),
                     aAutoLock);
  }

  already_AddRefed<ImageSurfaceCache> RemoveImage(
      const ImageKey aImageKey, const StaticMutexAutoLock& aAutoLock) {
    RefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache) {
      return nullptr;  // No cached surfaces for this image, so nothing to do.
    }

    // Discard all of the cached surfaces for this image.
    // XXX(seth): This is O(n^2) since for each item in the cache we are
    // removing an element from the costs array. Since n is expected to be
    // small, performance should be good, but if usage patterns change we should
    // change the data structure used for mCosts.
    for (auto iter = cache->ConstIter(); !iter.Done(); iter.Next()) {
      StopTracking(WrapNotNull(iter.UserData()),
                   /* aIsTracked */ true, aAutoLock);
    }

    // The per-image cache isn't needed anymore, so remove it as well.
    // This implicitly unlocks the image if it was locked.
    mImageCaches.Remove(aImageKey);

    // Since we did not actually remove any of the surfaces from the cache
    // itself, only stopped tracking them, we should free it outside the lock.
    return cache.forget();
  }

  void PruneImage(const ImageKey aImageKey,
                  const StaticMutexAutoLock& aAutoLock) {
    RefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache) {
      return;  // No cached surfaces for this image, so nothing to do.
    }

    cache->Prune([this, &aAutoLock](NotNull<CachedSurface*> aSurface) -> void {
      StopTracking(aSurface, /* aIsTracked */ true, aAutoLock);
      // Individual surfaces must be freed outside the lock.
      mCachedSurfacesDiscard.AppendElement(aSurface);
    });

    MaybeRemoveEmptyCache(aImageKey, cache);
  }

  void DiscardAll(const StaticMutexAutoLock& aAutoLock) {
    // Remove in order of cost because mCosts is an array and the other data
    // structures are all hash tables. Note that locked surfaces are not
    // removed, since they aren't present in mCosts.
    while (!mCosts.IsEmpty()) {
      Remove(mCosts.LastElement().Surface(), /* aStopTracking */ true,
             aAutoLock);
    }
  }

  void DiscardForMemoryPressure(const StaticMutexAutoLock& aAutoLock) {
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
      DiscardAll(aAutoLock);
      return;
    }

    // Discard surfaces until we've reduced our cost to our target cost.
    while (mAvailableCost < targetCost) {
      MOZ_ASSERT(!mCosts.IsEmpty(), "Removed everything and still not done");
      Remove(mCosts.LastElement().Surface(), /* aStopTracking */ true,
             aAutoLock);
    }
  }

  void TakeDiscard(nsTArray<RefPtr<CachedSurface>>& aDiscard,
                   const StaticMutexAutoLock& aAutoLock) {
    MOZ_ASSERT(aDiscard.IsEmpty());
    aDiscard = std::move(mCachedSurfacesDiscard);
  }

  void LockSurface(NotNull<CachedSurface*> aSurface,
                   const StaticMutexAutoLock& aAutoLock) {
    if (aSurface->IsPlaceholder() || aSurface->IsLocked()) {
      return;
    }

    StopTracking(aSurface, /* aIsTracked */ true, aAutoLock);

    // Lock the surface. This can fail.
    aSurface->SetLocked(true);
    DebugOnly<bool> tracking = StartTracking(aSurface, aAutoLock);
    MOZ_ASSERT(tracking);
  }

  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize) override {
    StaticMutexAutoLock lock(sInstanceMutex);

    // clang-format off
    // We have explicit memory reporting for the surface cache which is more
    // accurate than the cost metrics we report here, but these metrics are
    // still useful to report, since they control the cache's behavior.
    MOZ_COLLECT_REPORT(
      "imagelib-surface-cache-estimated-total",
      KIND_OTHER, UNITS_BYTES, (mMaxCost - mAvailableCost),
"Estimated total memory used by the imagelib surface cache.");

    MOZ_COLLECT_REPORT(
      "imagelib-surface-cache-estimated-locked",
      KIND_OTHER, UNITS_BYTES, mLockedCost,
"Estimated memory used by locked surfaces in the imagelib surface cache.");

    MOZ_COLLECT_REPORT(
      "imagelib-surface-cache-overflow-count",
      KIND_OTHER, UNITS_COUNT, mOverflowCount,
"Count of how many times the surface cache has hit its capacity and been "
"unable to insert a new surface.");
    // clang-format on

    return NS_OK;
  }

  void CollectSizeOfSurfaces(const ImageKey aImageKey,
                             nsTArray<SurfaceMemoryCounter>& aCounters,
                             MallocSizeOf aMallocSizeOf,
                             const StaticMutexAutoLock& aAutoLock) {
    RefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache) {
      return;  // No surfaces for this image.
    }

    // Report all surfaces in the per-image cache.
    cache->CollectSizeOfSurfaces(
        aCounters, aMallocSizeOf,
        [this, &aAutoLock](NotNull<CachedSurface*> aSurface) -> void {
          StopTracking(aSurface, /* aIsTracked */ true, aAutoLock);
          // Individual surfaces must be freed outside the lock.
          mCachedSurfacesDiscard.AppendElement(aSurface);
        });

    MaybeRemoveEmptyCache(aImageKey, cache);
  }

 private:
  already_AddRefed<ImageSurfaceCache> GetImageCache(const ImageKey aImageKey) {
    RefPtr<ImageSurfaceCache> imageCache;
    mImageCaches.Get(aImageKey, getter_AddRefs(imageCache));
    return imageCache.forget();
  }

  void MaybeRemoveEmptyCache(const ImageKey aImageKey,
                             ImageSurfaceCache* aCache) {
    // Remove the per-image cache if it's unneeded now. Keep it if the image is
    // locked, since the per-image cache is where we store that state. Note that
    // we don't push it into mImageCachesDiscard because all of its surfaces
    // have been removed, so it is safe to free while holding the lock.
    if (aCache->IsEmpty() && !aCache->IsLocked()) {
      mImageCaches.Remove(aImageKey);
    }
  }

  // This is similar to CanHold() except that it takes into account the costs of
  // locked surfaces. It's used internally in Insert(), but it's not exposed
  // publicly because we permit multithreaded access to the surface cache, which
  // means that the result would be meaningless: another thread could insert a
  // surface or lock an image at any time.
  bool CanHoldAfterDiscarding(const Cost aCost) const {
    return aCost <= mMaxCost - mLockedCost;
  }

  bool MarkUsed(NotNull<CachedSurface*> aSurface,
                NotNull<ImageSurfaceCache*> aCache,
                const StaticMutexAutoLock& aAutoLock) {
    if (aCache->IsLocked()) {
      LockSurface(aSurface, aAutoLock);
      return true;
    }

    nsresult rv = mExpirationTracker.MarkUsedLocked(aSurface, aAutoLock);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // If mark used fails, it is because it failed to reinsert the surface
      // after removing it from the tracker. Thus we need to update our
      // own accounting but otherwise expect it to be untracked.
      StopTracking(aSurface, /* aIsTracked */ false, aAutoLock);
      return false;
    }
    return true;
  }

  void DoUnlockSurfaces(NotNull<ImageSurfaceCache*> aCache, bool aStaticOnly,
                        const StaticMutexAutoLock& aAutoLock) {
    AutoTArray<NotNull<CachedSurface*>, 8> discard;

    // Unlock all the surfaces the per-image cache is holding.
    for (auto iter = aCache->ConstIter(); !iter.Done(); iter.Next()) {
      NotNull<CachedSurface*> surface = WrapNotNull(iter.UserData());
      if (surface->IsPlaceholder() || !surface->IsLocked()) {
        continue;
      }
      if (aStaticOnly &&
          surface->GetSurfaceKey().Playback() != PlaybackType::eStatic) {
        continue;
      }
      StopTracking(surface, /* aIsTracked */ true, aAutoLock);
      surface->SetLocked(false);
      if (MOZ_UNLIKELY(!StartTracking(surface, aAutoLock))) {
        discard.AppendElement(surface);
      }
    }

    // Discard any that we failed to track.
    for (auto iter = discard.begin(); iter != discard.end(); ++iter) {
      Remove(*iter, /* aStopTracking */ false, aAutoLock);
    }
  }

  void RemoveEntry(const ImageKey aImageKey, const SurfaceKey& aSurfaceKey,
                   const StaticMutexAutoLock& aAutoLock) {
    RefPtr<ImageSurfaceCache> cache = GetImageCache(aImageKey);
    if (!cache) {
      return;  // No cached surfaces for this image.
    }

    RefPtr<CachedSurface> surface =
        cache->Lookup(aSurfaceKey, /* aForAccess = */ false);
    if (!surface) {
      return;  // Lookup in the per-image cache missed.
    }

    Remove(WrapNotNull(surface), /* aStopTracking */ true, aAutoLock);
  }

  class SurfaceTracker final
      : public ExpirationTrackerImpl<CachedSurface, 2, StaticMutex,
                                     StaticMutexAutoLock> {
   public:
    explicit SurfaceTracker(uint32_t aSurfaceCacheExpirationTimeMS)
        : ExpirationTrackerImpl<CachedSurface, 2, StaticMutex,
                                StaticMutexAutoLock>(
              aSurfaceCacheExpirationTimeMS, "SurfaceTracker",
              SystemGroup::EventTargetFor(TaskCategory::Other)) {}

   protected:
    void NotifyExpiredLocked(CachedSurface* aSurface,
                             const StaticMutexAutoLock& aAutoLock) override {
      sInstance->Remove(WrapNotNull(aSurface), /* aStopTracking */ true,
                        aAutoLock);
    }

    void NotifyHandlerEndLocked(const StaticMutexAutoLock& aAutoLock) override {
      sInstance->TakeDiscard(mDiscard, aAutoLock);
    }

    void NotifyHandlerEnd() override {
      nsTArray<RefPtr<CachedSurface>> discard(std::move(mDiscard));
    }

    StaticMutex& GetMutex() override { return sInstanceMutex; }

    nsTArray<RefPtr<CachedSurface>> mDiscard;
  };

  class MemoryPressureObserver final : public nsIObserver {
   public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Observe(nsISupports*, const char* aTopic,
                       const char16_t*) override {
      nsTArray<RefPtr<CachedSurface>> discard;
      {
        StaticMutexAutoLock lock(sInstanceMutex);
        if (sInstance && strcmp(aTopic, "memory-pressure") == 0) {
          sInstance->DiscardForMemoryPressure(lock);
          sInstance->TakeDiscard(discard, lock);
        }
      }
      return NS_OK;
    }

   private:
    virtual ~MemoryPressureObserver() {}
  };

  nsTArray<CostEntry> mCosts;
  nsRefPtrHashtable<nsPtrHashKey<Image>, ImageSurfaceCache> mImageCaches;
  nsTArray<RefPtr<CachedSurface>> mCachedSurfacesDiscard;
  SurfaceTracker mExpirationTracker;
  RefPtr<MemoryPressureObserver> mMemoryPressureObserver;
  const uint32_t mDiscardFactor;
  const Cost mMaxCost;
  Cost mAvailableCost;
  Cost mLockedCost;
  size_t mOverflowCount;
};

NS_IMPL_ISUPPORTS(SurfaceCacheImpl, nsIMemoryReporter)
NS_IMPL_ISUPPORTS(SurfaceCacheImpl::MemoryPressureObserver, nsIObserver)

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

/* static */
void SurfaceCache::Initialize() {
  // Initialize preferences.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sInstance, "Shouldn't initialize more than once");

  // See StaticPrefs for the default values of these preferences.

  // Length of time before an unused surface is removed from the cache, in
  // milliseconds.
  uint32_t surfaceCacheExpirationTimeMS =
      StaticPrefs::image_mem_surfacecache_min_expiration_ms_AtStartup();

  // What fraction of the memory used by the surface cache we should discard
  // when we get a memory pressure notification. This value is interpreted as
  // 1/N, so 1 means to discard everything, 2 means to discard about half of the
  // memory we're using, and so forth. We clamp it to avoid division by zero.
  uint32_t surfaceCacheDiscardFactor =
      max(StaticPrefs::image_mem_surfacecache_discard_factor_AtStartup(), 1u);

  // Maximum size of the surface cache, in kilobytes.
  uint64_t surfaceCacheMaxSizeKB =
      StaticPrefs::image_mem_surfacecache_max_size_kb_AtStartup();

  // A knob determining the actual size of the surface cache. Currently the
  // cache is (size of main memory) / (surface cache size factor) KB
  // or (surface cache max size) KB, whichever is smaller. The formula
  // may change in the future, though.
  // For example, a value of 4 would yield a 256MB cache on a 1GB machine.
  // The smallest machines we are likely to run this code on have 256MB
  // of memory, which would yield a 64MB cache on this setting.
  // We clamp this value to avoid division by zero.
  uint32_t surfaceCacheSizeFactor =
      max(StaticPrefs::image_mem_surfacecache_size_factor_AtStartup(), 1u);

  // Compute the size of the surface cache.
  uint64_t memorySize = PR_GetPhysicalMemorySize();
  if (memorySize == 0) {
    MOZ_ASSERT_UNREACHABLE("PR_GetPhysicalMemorySize not implemented here");
    memorySize = 256 * 1024 * 1024;  // Fall back to 256MB.
  }
  uint64_t proposedSize = memorySize / surfaceCacheSizeFactor;
  uint64_t surfaceCacheSizeBytes =
      min(proposedSize, surfaceCacheMaxSizeKB * 1024);
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

/* static */
void SurfaceCache::Shutdown() {
  RefPtr<SurfaceCacheImpl> cache;
  {
    StaticMutexAutoLock lock(sInstanceMutex);
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(sInstance, "No singleton - was Shutdown() called twice?");
    cache = sInstance.forget();
  }
}

/* static */
LookupResult SurfaceCache::Lookup(const ImageKey aImageKey,
                                  const SurfaceKey& aSurfaceKey,
                                  bool aMarkUsed) {
  nsTArray<RefPtr<CachedSurface>> discard;
  LookupResult rv(MatchType::NOT_FOUND);

  {
    StaticMutexAutoLock lock(sInstanceMutex);
    if (!sInstance) {
      return rv;
    }

    rv = sInstance->Lookup(aImageKey, aSurfaceKey, lock, aMarkUsed);
    sInstance->TakeDiscard(discard, lock);
  }

  return rv;
}

/* static */
LookupResult SurfaceCache::LookupBestMatch(const ImageKey aImageKey,
                                           const SurfaceKey& aSurfaceKey,
                                           bool aMarkUsed) {
  nsTArray<RefPtr<CachedSurface>> discard;
  LookupResult rv(MatchType::NOT_FOUND);

  {
    StaticMutexAutoLock lock(sInstanceMutex);
    if (!sInstance) {
      return rv;
    }

    rv = sInstance->LookupBestMatch(aImageKey, aSurfaceKey, lock, aMarkUsed);
    sInstance->TakeDiscard(discard, lock);
  }

  return rv;
}

/* static */
InsertOutcome SurfaceCache::Insert(NotNull<ISurfaceProvider*> aProvider) {
  nsTArray<RefPtr<CachedSurface>> discard;
  InsertOutcome rv(InsertOutcome::FAILURE);

  {
    StaticMutexAutoLock lock(sInstanceMutex);
    if (!sInstance) {
      return rv;
    }

    rv = sInstance->Insert(aProvider, /* aSetAvailable = */ false, lock);
    sInstance->TakeDiscard(discard, lock);
  }

  return rv;
}

/* static */
bool SurfaceCache::CanHold(const IntSize& aSize,
                           uint32_t aBytesPerPixel /* = 4 */) {
  StaticMutexAutoLock lock(sInstanceMutex);
  if (!sInstance) {
    return false;
  }

  Cost cost = ComputeCost(aSize, aBytesPerPixel);
  return sInstance->CanHold(cost);
}

/* static */
bool SurfaceCache::CanHold(size_t aSize) {
  StaticMutexAutoLock lock(sInstanceMutex);
  if (!sInstance) {
    return false;
  }

  return sInstance->CanHold(aSize);
}

/* static */
void SurfaceCache::SurfaceAvailable(NotNull<ISurfaceProvider*> aProvider) {
  StaticMutexAutoLock lock(sInstanceMutex);
  if (!sInstance) {
    return;
  }

  sInstance->SurfaceAvailable(aProvider, lock);
}

/* static */
void SurfaceCache::LockImage(const ImageKey aImageKey) {
  StaticMutexAutoLock lock(sInstanceMutex);
  if (sInstance) {
    return sInstance->LockImage(aImageKey);
  }
}

/* static */
void SurfaceCache::UnlockImage(const ImageKey aImageKey) {
  StaticMutexAutoLock lock(sInstanceMutex);
  if (sInstance) {
    return sInstance->UnlockImage(aImageKey, lock);
  }
}

/* static */
void SurfaceCache::UnlockEntries(const ImageKey aImageKey) {
  StaticMutexAutoLock lock(sInstanceMutex);
  if (sInstance) {
    return sInstance->UnlockEntries(aImageKey, lock);
  }
}

/* static */
void SurfaceCache::RemoveImage(const ImageKey aImageKey) {
  RefPtr<ImageSurfaceCache> discard;
  {
    StaticMutexAutoLock lock(sInstanceMutex);
    if (sInstance) {
      discard = sInstance->RemoveImage(aImageKey, lock);
    }
  }
}

/* static */
void SurfaceCache::PruneImage(const ImageKey aImageKey) {
  nsTArray<RefPtr<CachedSurface>> discard;
  {
    StaticMutexAutoLock lock(sInstanceMutex);
    if (sInstance) {
      sInstance->PruneImage(aImageKey, lock);
      sInstance->TakeDiscard(discard, lock);
    }
  }
}

/* static */
void SurfaceCache::DiscardAll() {
  nsTArray<RefPtr<CachedSurface>> discard;
  {
    StaticMutexAutoLock lock(sInstanceMutex);
    if (sInstance) {
      sInstance->DiscardAll(lock);
      sInstance->TakeDiscard(discard, lock);
    }
  }
}

/* static */
void SurfaceCache::CollectSizeOfSurfaces(
    const ImageKey aImageKey, nsTArray<SurfaceMemoryCounter>& aCounters,
    MallocSizeOf aMallocSizeOf) {
  nsTArray<RefPtr<CachedSurface>> discard;
  {
    StaticMutexAutoLock lock(sInstanceMutex);
    if (!sInstance) {
      return;
    }

    sInstance->CollectSizeOfSurfaces(aImageKey, aCounters, aMallocSizeOf, lock);
    sInstance->TakeDiscard(discard, lock);
  }
}

/* static */
size_t SurfaceCache::MaximumCapacity() {
  StaticMutexAutoLock lock(sInstanceMutex);
  if (!sInstance) {
    return 0;
  }

  return sInstance->MaximumCapacity();
}

/* static */
bool SurfaceCache::IsLegalSize(const IntSize& aSize) {
  // reject over-wide or over-tall images
  const int32_t k64KLimit = 0x0000FFFF;
  if (MOZ_UNLIKELY(aSize.width > k64KLimit || aSize.height > k64KLimit)) {
    NS_WARNING("image too big");
    return false;
  }

  // protect against invalid sizes
  if (MOZ_UNLIKELY(aSize.height <= 0 || aSize.width <= 0)) {
    return false;
  }

  // check to make sure we don't overflow a 32-bit
  CheckedInt32 requiredBytes =
      CheckedInt32(aSize.width) * CheckedInt32(aSize.height) * 4;
  if (MOZ_UNLIKELY(!requiredBytes.isValid())) {
    NS_WARNING("width or height too large");
    return false;
  }
  return true;
}

IntSize SurfaceCache::ClampVectorSize(const IntSize& aSize) {
  // If we exceed the maximum, we need to scale the size downwards to fit.
  // It shouldn't get here if it is significantly larger because
  // VectorImage::UseSurfaceCacheForSize should prevent us from requesting
  // a rasterized version of a surface greater than 4x the maximum.
  int32_t maxSizeKB =
      StaticPrefs::image_cache_max_rasterized_svg_threshold_kb();
  if (maxSizeKB <= 0) {
    return aSize;
  }

  int64_t proposedKB = int64_t(aSize.width) * aSize.height / 256;
  if (maxSizeKB >= proposedKB) {
    return aSize;
  }

  double scale = sqrt(double(maxSizeKB) / proposedKB);
  return IntSize(int32_t(scale * aSize.width), int32_t(scale * aSize.height));
}

IntSize SurfaceCache::ClampSize(ImageKey aImageKey, const IntSize& aSize) {
  if (aImageKey->GetType() != imgIContainer::TYPE_VECTOR) {
    return aSize;
  }

  return ClampVectorSize(aSize);
}

}  // namespace image
}  // namespace mozilla
