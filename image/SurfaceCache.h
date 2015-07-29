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
#include "mozilla/MemoryReporting.h" // for MallocSizeOf
#include "mozilla/HashFunctions.h"   // for HashGeneric and AddToHash
#include "gfx2DGlue.h"
#include "gfxPoint.h"                // for gfxSize
#include "nsCOMPtr.h"                // for already_AddRefed
#include "mozilla/gfx/Point.h"       // for mozilla::gfx::IntSize
#include "mozilla/gfx/2D.h"          // for SourceSurface
#include "SVGImageContext.h"         // for SVGImageContext

namespace mozilla {
namespace image {

class Image;
class imgFrame;
class LookupResult;
struct SurfaceMemoryCounter;

/*
 * ImageKey contains the information we need to look up all cached surfaces for
 * a particular image.
 */
typedef Image* ImageKey;

/*
 * SurfaceKey contains the information we need to look up a specific cached
 * surface. Together with an ImageKey, this uniquely identifies the surface.
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
           aOther.mAnimationTime == mAnimationTime &&
           aOther.mFlags == mFlags;
  }

  uint32_t Hash() const
  {
    uint32_t hash = HashGeneric(mSize.width, mSize.height);
    hash = AddToHash(hash, mSVGContext.map(HashSIC).valueOr(0));
    hash = AddToHash(hash, mAnimationTime, mFlags);
    return hash;
  }

  IntSize Size() const { return mSize; }
  Maybe<SVGImageContext> SVGContext() const { return mSVGContext; }
  float AnimationTime() const { return mAnimationTime; }
  uint32_t Flags() const { return mFlags; }

  SurfaceKey WithNewFlags(uint32_t aFlags) const
  {
    return SurfaceKey(mSize, mSVGContext, mAnimationTime, aFlags);
  }

private:
  SurfaceKey(const IntSize& aSize,
             const Maybe<SVGImageContext>& aSVGContext,
             const float aAnimationTime,
             const uint32_t aFlags)
    : mSize(aSize)
    , mSVGContext(aSVGContext)
    , mAnimationTime(aAnimationTime)
    , mFlags(aFlags)
  { }

  static uint32_t HashSIC(const SVGImageContext& aSIC) {
    return aSIC.Hash();
  }

  friend SurfaceKey RasterSurfaceKey(const IntSize&, uint32_t, uint32_t);
  friend SurfaceKey VectorSurfaceKey(const IntSize&,
                                     const Maybe<SVGImageContext>&,
                                     float);

  IntSize                mSize;
  Maybe<SVGImageContext> mSVGContext;
  float                  mAnimationTime;
  uint32_t               mFlags;
};

inline SurfaceKey
RasterSurfaceKey(const gfx::IntSize& aSize,
                 uint32_t aFlags,
                 uint32_t aFrameNum)
{
  return SurfaceKey(aSize, Nothing(), float(aFrameNum), aFlags);
}

inline SurfaceKey
VectorSurfaceKey(const gfx::IntSize& aSize,
                 const Maybe<SVGImageContext>& aSVGContext,
                 float aAnimationTime)
{
  // We don't care about aFlags for VectorImage because none of the flags we
  // have right now influence VectorImage's rendering. If we add a new flag that
  // *does* affect how a VectorImage renders, we'll have to change this.
  return SurfaceKey(aSize, aSVGContext, aAnimationTime, 0);
}

enum class Lifetime : uint8_t {
  Transient,
  Persistent
};

enum class InsertOutcome : uint8_t {
  SUCCESS,                 // Success (but see Insert documentation).
  FAILURE,                 // Couldn't insert (e.g., for capacity reasons).
  FAILURE_ALREADY_PRESENT  // A surface with the same key is already present.
};

/**
 * SurfaceCache is an imagelib-global service that allows caching of temporary
 * surfaces. Surfaces normally expire from the cache automatically if they go
 * too long without being accessed.
 *
 * SurfaceCache does not hold surfaces directly; instead, it holds imgFrame
 * objects, which hold surfaces but also layer on additional features specific
 * to imagelib's needs like animation, padding support, and transparent support
 * for volatile buffers.
 *
 * Sometime it's useful to temporarily prevent surfaces from expiring from the
 * cache. This is most often because losing the data could harm the user
 * experience (for example, we often don't want to allow surfaces that are
 * currently visible to expire) or because it's not possible to rematerialize
 * the surface. SurfaceCache supports this through the use of image locking and
 * surface lifetimes; see the comments for Insert() and LockImage() for more
 * details.
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
   * Look up the imgFrame containing a surface in the cache and returns a
   * drawable reference to that imgFrame.
   *
   * If the image associated with the surface is locked, then the surface will
   * be locked before it is returned.
   *
   * If the imgFrame was found in the cache, but had stored its surface in a
   * volatile buffer which was discarded by the OS, then it is automatically
   * removed from the cache and an empty LookupResult is returned. Note that
   * this will never happen to persistent surfaces associated with a locked
   * image; the cache keeps a strong reference to such surfaces internally.
   *
   * @param aImageKey       Key data identifying which image the surface belongs
   *                        to.
   *
   * @param aSurfaceKey     Key data which uniquely identifies the requested
   *                        surface.
   *
   * @param aAlternateFlags If not Nothing(), a different set of flags than the
   *                        ones specified in @aSurfaceKey which are also
   *                        acceptable to the caller. This is more efficient
   *                        than calling Lookup() twice, which requires taking a
   *                        lock each time.
   *
   * @return                a LookupResult, which will either contain a
   *                        DrawableFrameRef to the requested surface, or an
   *                        empty DrawableFrameRef if the surface was not found.
   */
  static LookupResult Lookup(const ImageKey    aImageKey,
                             const SurfaceKey& aSurfaceKey,
                             const Maybe<uint32_t>& aAlternateFlags = Nothing());

  /**
   * Looks up the best matching surface in the cache and returns a drawable
   * reference to the imgFrame containing it.
   *
   * Returned surfaces may vary from the requested surface only in terms of
   * size, unless @aAlternateFlags is specified.
   *
   * @param aImageKey    Key data identifying which image the surface belongs
   *                     to.
   *
   * @param aSurfaceKey  Key data which identifies the ideal surface to return.
   *
   * @param aAlternateFlags If not Nothing(), a different set of flags than the
   *                        ones specified in @aSurfaceKey which are also
   *                        acceptable to the caller. This is much more
   *                        efficient than calling LookupBestMatch() twice.
   *
   * @return                a LookupResult, which will either contain a
   *                        DrawableFrameRef to a surface similar to the
   *                        requested surface, or an empty DrawableFrameRef if
   *                        the surface was not found. Callers can use
   *                        LookupResult::IsExactMatch() to check whether the
   *                        returned surface exactly matches @aSurfaceKey.
   */
  static LookupResult LookupBestMatch(const ImageKey    aImageKey,
                                      const SurfaceKey& aSurfaceKey,
                                      const Maybe<uint32_t>& aAlternateFlags
                                        = Nothing());

  /**
   * Insert a surface into the cache. If a surface with the same ImageKey and
   * SurfaceKey is already in the cache, Insert returns FAILURE_ALREADY_PRESENT.
   * If a matching placeholder is already present, the placeholder is removed.
   *
   * Each surface in the cache has a lifetime, either Transient or Persistent.
   * Transient surfaces can expire from the cache at any time. Persistent
   * surfaces, on the other hand, will never expire as long as they remain
   * locked, but if they become unlocked, can expire just like transient
   * surfaces. When it is first inserted, a persistent surface is locked if its
   * associated image is locked. When that image is later unlocked, the surface
   * becomes unlocked too. To become locked again at that point, two things must
   * happen: the image must become locked again (via LockImage()), and the
   * surface must be touched again (via one of the Lookup() functions).
   *
   * All of this means that a very particular procedure has to be followed for
   * surfaces which cannot be rematerialized. First, they must be inserted
   * with a persistent lifetime *after* the image is locked with LockImage(); if
   * you use the other order, the surfaces might expire before LockImage() gets
   * called or before the surface is touched again by Lookup(). Second, the
   * image they are associated with must never be unlocked.
   *
   * If a surface cannot be rematerialized, it may be important to know whether
   * it was inserted into the cache successfully. Insert() returns FAILURE if it
   * failed to insert the surface, which could happen because of capacity
   * reasons, or because it was already freed by the OS. If you aren't inserting
   * a surface with persistent lifetime, or if the surface isn't associated with
   * a locked image, checking for SUCCESS or FAILURE is useless: the surface
   * might expire immediately after being inserted, even though Insert()
   * returned SUCCESS. Thus, many callers do not need to check the result of
   * Insert() at all.
   *
   * @param aTarget      The new surface (wrapped in an imgFrame) to insert into
   *                     the cache.
   * @param aImageKey    Key data identifying which image the surface belongs
   *                     to.
   * @param aSurfaceKey  Key data which uniquely identifies the requested
   *                     surface.
   * @param aLifetime    Whether this is a transient surface that can always be
   *                     allowed to expire, or a persistent surface that
   *                     shouldn't expire if the image is locked.
   * @return SUCCESS if the surface was inserted successfully. (But see above
   *           for more information about when you should check this.)
   *         FAILURE if the surface could not be inserted, e.g. for capacity
   *           reasons. (But see above for more information about when you
   *           should check this.)
   *         FAILURE_ALREADY_PRESENT if a surface with the same ImageKey and
   *           SurfaceKey already exists in the cache.
   */
  static InsertOutcome Insert(imgFrame*         aSurface,
                              const ImageKey    aImageKey,
                              const SurfaceKey& aSurfaceKey,
                              Lifetime          aLifetime);

  /**
   * Insert a placeholder for a surface into the cache. If a surface with the
   * same ImageKey and SurfaceKey is already in the cache, InsertPlaceholder()
   * returns FAILURE_ALREADY_PRESENT.
   *
   * Placeholders exist to allow lazy allocation of surfaces. The Lookup*()
   * methods will report whether a placeholder for an exactly matching surface
   * existed by returning a MatchType of PENDING or SUBSTITUTE_BECAUSE_PENDING,
   * but they will never return a placeholder directly. (They couldn't, since
   * placeholders don't have an associated surface.)
   *
   * Once inserted, placeholders can be removed using RemoveSurface() or
   * RemoveImage(), just like a surface.  They're automatically removed when a
   * real surface that matches the placeholder is inserted with Insert().
   *
   * @param aImageKey    Key data identifying which image the placeholder
   *                     belongs to.
   * @param aSurfaceKey  Key data which uniquely identifies the surface the
   *                     placeholder stands in for.
   * @return SUCCESS if the placeholder was inserted successfully.
   *         FAILURE if the placeholder could not be inserted for some reason.
   *         FAILURE_ALREADY_PRESENT if a surface with the same ImageKey and
   *           SurfaceKey already exists in the cache.
   */
  static InsertOutcome InsertPlaceholder(const ImageKey    aImageKey,
                                         const SurfaceKey& aSurfaceKey);

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
   * Locks an image. Any of the image's persistent surfaces which are either
   * inserted or accessed while the image is locked will not expire.
   *
   * Locking an image does not automatically lock that image's existing
   * surfaces. A call to LockImage() guarantees that persistent surfaces which
   * are inserted afterward will not expire before the next call to
   * UnlockImage() or UnlockSurfaces() for that image. Surfaces that are
   * accessed via Lookup() or LookupBestMatch() after a LockImage() call will
   * also not expire until the next UnlockImage() or UnlockSurfaces() call for
   * that image. Any other surfaces owned by the image may expire at any time,
   * whether they are persistent or transient.
   *
   * Regardless of locking, any of an image's surfaces may be removed using
   * RemoveSurface(), and all of an image's surfaces are removed by
   * RemoveImage(), whether the image is locked or not.
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
   * Unlocks an image, allowing any of its surfaces to expire at any time.
   *
   * It's OK to call UnlockImage() on an image that's already unlocked; this has
   * no effect.
   *
   * @param aImageKey    The image to unlock.
   */
  static void UnlockImage(const ImageKey aImageKey);

  /**
   * Unlocks the existing surfaces of an image, allowing them to expire at any
   * time.
   *
   * This does not unlock the image itself, so accessing the surfaces via
   * Lookup() or LookupBestMatch() will lock them again, and prevent them from
   * expiring.
   *
   * This is intended to be used in situations where it's no longer clear that
   * all of the persistent surfaces owned by an image are needed. Calling
   * UnlockSurfaces() and then taking some action that will cause Lookup() to
   * touch any surfaces that are still useful will permit the remaining surfaces
   * to expire from the cache.
   *
   * If the image is unlocked, this has no effect.
   *
   * @param aImageKey    The image which should have its existing surfaces
   *                     unlocked.
   */
  static void UnlockSurfaces(const ImageKey aImageKey);

  /**
   * Removes a surface or placeholder from the cache, if it's present. If it's
   * not present, RemoveSurface() has no effect.
   *
   * Use this function to remove individual surfaces that have become invalid.
   * Prefer RemoveImage() or DiscardAll() when they're applicable, as they have
   * much better performance than calling this function repeatedly.
   *
   * @param aImageKey    Key data identifying which image the surface belongs
                         to.
   * @param aSurfaceKey  Key data which uniquely identifies the requested
                         surface.
   */
  static void RemoveSurface(const ImageKey    aImageKey,
                            const SurfaceKey& aSurfaceKey);

  /**
   * Removes all cached surfaces and placeholders associated with the given
   * image from the cache.  If the image is locked, it is automatically
   * unlocked.
   *
   * This MUST be called, at a minimum, when an Image which could be storing
   * surfaces in the surface cache is destroyed. If another image were allocated
   * at the same address it could result in subtle, difficult-to-reproduce bugs.
   *
   * @param aImageKey  The image which should be removed from the cache.
   */
  static void RemoveImage(const ImageKey aImageKey);

  /**
   * Evicts all evictable surfaces from the cache.
   *
   * All surfaces are evictable except for persistent surfaces associated with
   * locked images. Non-evictable surfaces can only be removed by
   * RemoveSurface() or RemoveImage().
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

private:
  virtual ~SurfaceCache() = 0;  // Forbid instantiation.
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_SurfaceCache_h
