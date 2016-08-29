/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SurfaceCache is a service for caching temporary surfaces and decoded image
 * data in imagelib.
 */

#ifndef mozilla_image_SurfaceCache_h
#define mozilla_image_SurfaceCache_h

#include "mozilla/Maybe.h"           // for Maybe
#include "mozilla/NotNull.h"
#include "mozilla/MemoryReporting.h" // for MallocSizeOf
#include "mozilla/HashFunctions.h"   // for HashGeneric and AddToHash
#include "gfx2DGlue.h"
#include "gfxPoint.h"                // for gfxSize
#include "nsCOMPtr.h"                // for already_AddRefed
#include "mozilla/gfx/Point.h"       // for mozilla::gfx::IntSize
#include "mozilla/gfx/2D.h"          // for SourceSurface
#include "PlaybackType.h"
#include "SurfaceFlags.h"
#include "SVGImageContext.h"         // for SVGImageContext

namespace mozilla {
namespace image {

class Image;
class ISurfaceProvider;
class LookupResult;
class SurfaceCacheImpl;
struct SurfaceMemoryCounter;

/*
 * ImageKey contains the information we need to look up all SurfaceCache entries
 * for a particular image.
 */
typedef Image* ImageKey;

/*
 * SurfaceKey contains the information we need to look up a specific
 * SurfaceCache entry. Together with an ImageKey, this uniquely identifies the
 * surface.
 *
 * Callers should construct a SurfaceKey using the appropriate helper function
 * for their image type - either RasterSurfaceKey or VectorSurfaceKey.
 */
class SurfaceKey
{
  typedef gfx::IntSize IntSize;

public:
  bool operator==(const SurfaceKey& aOther) const
  {
    return aOther.mSize == mSize &&
           aOther.mSVGContext == mSVGContext &&
           aOther.mPlayback == mPlayback &&
           aOther.mFlags == mFlags;
  }

  uint32_t Hash() const
  {
    uint32_t hash = HashGeneric(mSize.width, mSize.height);
    hash = AddToHash(hash, mSVGContext.map(HashSIC).valueOr(0));
    hash = AddToHash(hash, uint8_t(mPlayback), uint32_t(mFlags));
    return hash;
  }

  const IntSize& Size() const { return mSize; }
  Maybe<SVGImageContext> SVGContext() const { return mSVGContext; }
  PlaybackType Playback() const { return mPlayback; }
  SurfaceFlags Flags() const { return mFlags; }

private:
  SurfaceKey(const IntSize& aSize,
             const Maybe<SVGImageContext>& aSVGContext,
             PlaybackType aPlayback,
             SurfaceFlags aFlags)
    : mSize(aSize)
    , mSVGContext(aSVGContext)
    , mPlayback(aPlayback)
    , mFlags(aFlags)
  { }

  static uint32_t HashSIC(const SVGImageContext& aSIC) {
    return aSIC.Hash();
  }

  friend SurfaceKey RasterSurfaceKey(const IntSize&, SurfaceFlags, PlaybackType);
  friend SurfaceKey VectorSurfaceKey(const IntSize&,
                                     const Maybe<SVGImageContext>&);

  IntSize                mSize;
  Maybe<SVGImageContext> mSVGContext;
  PlaybackType           mPlayback;
  SurfaceFlags           mFlags;
};

inline SurfaceKey
RasterSurfaceKey(const gfx::IntSize& aSize,
                 SurfaceFlags aFlags,
                 PlaybackType aPlayback)
{
  return SurfaceKey(aSize, Nothing(), aPlayback, aFlags);
}

inline SurfaceKey
VectorSurfaceKey(const gfx::IntSize& aSize,
                 const Maybe<SVGImageContext>& aSVGContext)
{
  // We don't care about aFlags for VectorImage because none of the flags we
  // have right now influence VectorImage's rendering. If we add a new flag that
  // *does* affect how a VectorImage renders, we'll have to change this.
  // Similarly, we don't accept a PlaybackType parameter because we don't
  // currently cache frames of animated SVG images.
  return SurfaceKey(aSize, aSVGContext, PlaybackType::eStatic,
                    DefaultSurfaceFlags());
}


/**
 * AvailabilityState is used to track whether an ISurfaceProvider has a surface
 * available or is just a placeholder.
 *
 * To ensure that availability changes are atomic (and especially that internal
 * SurfaceCache code doesn't have to deal with asynchronous availability
 * changes), an ISurfaceProvider which starts as a placeholder can only reveal
 * the fact that it now has a surface available via a call to
 * SurfaceCache::SurfaceAvailable().
 */
class AvailabilityState
{
public:
  static AvailabilityState StartAvailable() { return AvailabilityState(true); }
  static AvailabilityState StartAsPlaceholder() { return AvailabilityState(false); }

  bool IsAvailable() const { return mIsAvailable; }
  bool IsPlaceholder() const { return !mIsAvailable; }

private:
  friend class SurfaceCacheImpl;

  explicit AvailabilityState(bool aIsAvailable) : mIsAvailable(aIsAvailable) { }

  void SetAvailable() { mIsAvailable = true; }

  bool mIsAvailable;
};

enum class InsertOutcome : uint8_t {
  SUCCESS,                 // Success (but see Insert documentation).
  FAILURE,                 // Couldn't insert (e.g., for capacity reasons).
  FAILURE_ALREADY_PRESENT  // A surface with the same key is already present.
};

/**
 * SurfaceCache is an ImageLib-global service that allows caching of decoded
 * image surfaces, temporary surfaces (e.g. for caching rotated or clipped
 * versions of images), or dynamically generated surfaces (e.g. for animations).
 * SurfaceCache entries normally expire from the cache automatically if they go
 * too long without being accessed.
 *
 * Because SurfaceCache must support both normal surfaces and dynamically
 * generated surfaces, it does not actually hold surfaces directly. Instead, it
 * holds ISurfaceProvider objects which can provide access to a surface when
 * requested; SurfaceCache doesn't care about the details of how this is
 * accomplished.
 *
 * Sometime it's useful to temporarily prevent entries from expiring from the
 * cache. This is most often because losing the data could harm the user
 * experience (for example, we often don't want to allow surfaces that are
 * currently visible to expire) or because it's not possible to rematerialize
 * the surface. SurfaceCache supports this through the use of image locking; see
 * the comments for Insert() and LockImage() for more details.
 *
 * Any image which stores surfaces in the SurfaceCache *must* ensure that it
 * calls RemoveImage() before it is destroyed. See the comments for
 * RemoveImage() for more details.
 */
struct SurfaceCache
{
  typedef gfx::IntSize IntSize;

  /**
   * Initialize static data. Called during imagelib module initialization.
   */
  static void Initialize();

  /**
   * Release static data. Called during imagelib module shutdown.
   */
  static void Shutdown();

  /**
   * Looks up the requested cache entry and returns a drawable reference to its
   * associated surface.
   *
   * If the image associated with the cache entry is locked, then the entry will
   * be locked before it is returned.
   *
   * If a matching ISurfaceProvider was found in the cache, but SurfaceCache
   * couldn't obtain a surface from it (e.g. because it had stored its surface
   * in a volatile buffer which was discarded by the OS) then it is
   * automatically removed from the cache and an empty LookupResult is returned.
   * Note that this will never happen to ISurfaceProviders associated with a
   * locked image; SurfaceCache tells such ISurfaceProviders to keep a strong
   * references to their data internally.
   *
   * @param aImageKey       Key data identifying which image the cache entry
   *                        belongs to.
   * @param aSurfaceKey     Key data which uniquely identifies the requested
   *                        cache entry.
   * @return                a LookupResult which will contain a DrawableSurface
   *                        if the cache entry was found.
   */
  static LookupResult Lookup(const ImageKey    aImageKey,
                             const SurfaceKey& aSurfaceKey);

  /**
   * Looks up the best matching cache entry and returns a drawable reference to
   * its associated surface.
   *
   * The result may vary from the requested cache entry only in terms of size.
   *
   * @param aImageKey       Key data identifying which image the cache entry
   *                        belongs to.
   * @param aSurfaceKey     Key data which uniquely identifies the requested
   *                        cache entry.
   * @return                a LookupResult which will contain a DrawableSurface
   *                        if a cache entry similar to the one the caller
   *                        requested could be found. Callers can use
   *                        LookupResult::IsExactMatch() to check whether the
   *                        returned surface exactly matches @aSurfaceKey.
   */
  static LookupResult LookupBestMatch(const ImageKey    aImageKey,
                                      const SurfaceKey& aSurfaceKey);

  /**
   * Insert an ISurfaceProvider into the cache. If an entry with the same
   * ImageKey and SurfaceKey is already in the cache, Insert returns
   * FAILURE_ALREADY_PRESENT. If a matching placeholder is already present, it
   * is replaced.
   *
   * Cache entries will never expire as long as they remain locked, but if they
   * become unlocked, they can expire either because the SurfaceCache runs out
   * of capacity or because they've gone too long without being used.  When it
   * is first inserted, a cache entry is locked if its associated image is
   * locked.  When that image is later unlocked, the cache entry becomes
   * unlocked too. To become locked again at that point, two things must happen:
   * the image must become locked again (via LockImage()), and the cache entry
   * must be touched again (via one of the Lookup() functions).
   *
   * All of this means that a very particular procedure has to be followed for
   * cache entries which cannot be rematerialized. First, they must be inserted
   * *after* the image is locked with LockImage(); if you use the other order,
   * the cache entry might expire before LockImage() gets called or before the
   * entry is touched again by Lookup(). Second, the image they are associated
   * with must never be unlocked.
   *
   * If a cache entry cannot be rematerialized, it may be important to know
   * whether it was inserted into the cache successfully. Insert() returns
   * FAILURE if it failed to insert the cache entry, which could happen because
   * of capacity reasons, or because it was already freed by the OS. If the
   * cache entry isn't associated with a locked image, checking for SUCCESS or
   * FAILURE is useless: the entry might expire immediately after being
   * inserted, even though Insert() returned SUCCESS. Thus, many callers do not
   * need to check the result of Insert() at all.
   *
   * @param aProvider    The new cache entry to insert into the cache.
   * @return SUCCESS if the cache entry was inserted successfully. (But see above
   *           for more information about when you should check this.)
   *         FAILURE if the cache entry could not be inserted, e.g. for capacity
   *           reasons. (But see above for more information about when you
   *           should check this.)
   *         FAILURE_ALREADY_PRESENT if an entry with the same ImageKey and
   *           SurfaceKey already exists in the cache.
   */
  static InsertOutcome Insert(NotNull<ISurfaceProvider*> aProvider);

  /**
   * Mark the cache entry @aProvider as having an available surface. This turns
   * a placeholder cache entry into a normal cache entry. The cache entry
   * becomes locked if the associated image is locked; otherwise, it starts in
   * the unlocked state.
   *
   * If the cache entry containing @aProvider has already been evicted from the
   * surface cache, this function has no effect.
   *
   * It's illegal to call this function if @aProvider is not a placeholder; by
   * definition, non-placeholder ISurfaceProviders should have a surface
   * available already.
   *
   * @param aProvider       The cache entry that now has a surface available.
   */
  static void SurfaceAvailable(NotNull<ISurfaceProvider*> aProvider);

  /**
   * Checks if a surface of a given size could possibly be stored in the cache.
   * If CanHold() returns false, Insert() will always fail to insert the
   * surface, but the inverse is not true: Insert() may take more information
   * into account than just image size when deciding whether to cache the
   * surface, so Insert() may still fail even if CanHold() returns true.
   *
   * Use CanHold() to avoid the need to create a temporary surface when we know
   * for sure the cache can't hold it.
   *
   * @param aSize  The dimensions of a surface in pixels.
   * @param aBytesPerPixel  How many bytes each pixel of the surface requires.
   *                        Defaults to 4, which is appropriate for RGBA or RGBX
   *                        images.
   *
   * @return false if the surface cache can't hold a surface of that size.
   */
  static bool CanHold(const IntSize& aSize, uint32_t aBytesPerPixel = 4);
  static bool CanHold(size_t aSize);

  /**
   * Locks an image. Any of the image's cache entries which are either inserted
   * or accessed while the image is locked will not expire.
   *
   * Locking an image does not automatically lock that image's existing cache
   * entries. A call to LockImage() guarantees that entries which are inserted
   * afterward will not expire before the next call to UnlockImage() or
   * UnlockSurfaces() for that image. Cache entries that are accessed via
   * Lookup() or LookupBestMatch() after a LockImage() call will also not expire
   * until the next UnlockImage() or UnlockSurfaces() call for that image. Any
   * other cache entries owned by the image may expire at any time.
   *
   * All of an image's cache entries are removed by RemoveImage(), whether the
   * image is locked or not.
   *
   * It's safe to call LockImage() on an image that's already locked; this has
   * no effect.
   *
   * You must always unlock any image you lock. You may do this explicitly by
   * calling UnlockImage(), or implicitly by calling RemoveImage(). Since you're
   * required to call RemoveImage() when you destroy an image, this doesn't
   * impose any additional requirements, but it's preferable to call
   * UnlockImage() earlier if it's possible.
   *
   * @param aImageKey    The image to lock.
   */
  static void LockImage(const ImageKey aImageKey);

  /**
   * Unlocks an image, allowing any of its cache entries to expire at any time.
   *
   * It's OK to call UnlockImage() on an image that's already unlocked; this has
   * no effect.
   *
   * @param aImageKey    The image to unlock.
   */
  static void UnlockImage(const ImageKey aImageKey);

  /**
   * Unlocks the existing cache entries of an image, allowing them to expire at
   * any time.
   *
   * This does not unlock the image itself, so accessing the cache entries via
   * Lookup() or LookupBestMatch() will lock them again, and prevent them from
   * expiring.
   *
   * This is intended to be used in situations where it's no longer clear that
   * all of the cache entries owned by an image are needed. Calling
   * UnlockSurfaces() and then taking some action that will cause Lookup() to
   * touch any cache entries that are still useful will permit the remaining
   * entries to expire from the cache.
   *
   * If the image is unlocked, this has no effect.
   *
   * @param aImageKey    The image which should have its existing cache entries
   *                     unlocked.
   */
  static void UnlockEntries(const ImageKey aImageKey);

  /**
   * Removes all cache entries (including placeholders) associated with the
   * given image from the cache.  If the image is locked, it is automatically
   * unlocked.
   *
   * This MUST be called, at a minimum, when an Image which could be storing
   * entries in the surface cache is destroyed. If another image were allocated
   * at the same address it could result in subtle, difficult-to-reproduce bugs.
   *
   * @param aImageKey  The image which should be removed from the cache.
   */
  static void RemoveImage(const ImageKey aImageKey);

  /**
   * Evicts all evictable entries from the cache.
   *
   * All entries are evictable except for entries associated with locked images.
   * Non-evictable entries can only be removed by RemoveImage().
   */
  static void DiscardAll();

  /**
   * Collects an accounting of the surfaces contained in the SurfaceCache for
   * the given image, along with their size and various other metadata.
   *
   * This is intended for use with memory reporting.
   *
   * @param aImageKey     The image to report memory usage for.
   * @param aCounters     An array into which the report for each surface will
   *                      be written.
   * @param aMallocSizeOf A fallback malloc memory reporting function.
   */
  static void CollectSizeOfSurfaces(const ImageKey    aImageKey,
                                    nsTArray<SurfaceMemoryCounter>& aCounters,
                                    MallocSizeOf      aMallocSizeOf);

  /**
   * @return maximum capacity of the SurfaceCache in bytes. This is only exposed
   * for use by tests; normal code should use CanHold() instead.
   */
  static size_t MaximumCapacity();

private:
  virtual ~SurfaceCache() = 0;  // Forbid instantiation.
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_SurfaceCache_h
