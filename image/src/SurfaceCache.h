/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SurfaceCache is a service for caching temporary surfaces and decoded image
 * data in imagelib.
 */

#ifndef MOZILLA_IMAGELIB_SURFACECACHE_H_
#define MOZILLA_IMAGELIB_SURFACECACHE_H_

#include "mozilla/Maybe.h"          // for Maybe
#include "mozilla/HashFunctions.h"  // for HashGeneric and AddToHash
#include "gfxPoint.h"               // for gfxSize
#include "nsCOMPtr.h"               // for already_AddRefed
#include "mozilla/gfx/Point.h"      // for mozilla::gfx::IntSize
#include "mozilla/gfx/2D.h"         // for SourceSurface
#include "SVGImageContext.h"        // for SVGImageContext

namespace mozilla {
namespace image {

class DrawableFrameRef;
class Image;
class imgFrame;

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

  friend SurfaceKey RasterSurfaceKey(const IntSize&, const uint32_t);
  friend SurfaceKey VectorSurfaceKey(const IntSize&,
                                     const Maybe<SVGImageContext>&,
                                     const float);

  IntSize                mSize;
  Maybe<SVGImageContext> mSVGContext;
  float                  mAnimationTime;
  uint32_t               mFlags;
};

inline SurfaceKey
RasterSurfaceKey(const gfx::IntSize& aSize,
                 const uint32_t aFlags)
{
  // We don't care about aAnimationTime for RasterImage because it's not
  // currently possible to store anything but the first frame in the
  // SurfaceCache.
  return SurfaceKey(aSize, Nothing(), 0.0f, aFlags);
}

inline SurfaceKey
VectorSurfaceKey(const gfx::IntSize& aSize,
                 const Maybe<SVGImageContext>& aSVGContext,
                 const float aAnimationTime)
{
  // We don't care about aFlags for VectorImage because none of the flags we
  // have right now influence VectorImage's rendering. If we add a new flag that
  // *does* affect how a VectorImage renders, we'll have to change this.
  return SurfaceKey(aSize, aSVGContext, aAnimationTime, 0);
}

/**
 * SurfaceCache is an imagelib-global service that allows caching of temporary
 * surfaces. Surfaces expire from the cache automatically if they go too long
 * without being accessed.
 *
 * SurfaceCache does not hold surfaces directly; instead, it holds imgFrame
 * objects, which hold surfaces but also layer on additional features specific
 * to imagelib's needs like animation, padding support, and transparent support
 * for volatile buffers.
 *
 * SurfaceCache is not thread-safe; it should only be accessed from the main
 * thread.
 */
struct SurfaceCache
{
  typedef gfx::IntSize IntSize;

  /*
   * Initialize static data. Called during imagelib module initialization.
   */
  static void Initialize();

  /*
   * Release static data. Called during imagelib module shutdown.
   */
  static void Shutdown();

  /*
   * Look up the imgFrame containing a surface in the cache and returns a
   * drawable reference to that imgFrame.
   *
   * If the imgFrame was found in the cache, but had stored its surface in a
   * volatile buffer which was discarded by the OS, then it is automatically
   * removed from the cache and an empty DrawableFrameRef is returned.
   *
   * @param aImageKey    Key data identifying which image the surface belongs to.
   * @param aSurfaceKey  Key data which uniquely identifies the requested surface.
   *
   * @return a DrawableFrameRef to the imgFrame wrapping the requested surface,
   *         or an empty DrawableFrameRef if not found.
   */
  static DrawableFrameRef Lookup(const ImageKey    aImageKey,
                                 const SurfaceKey& aSurfaceKey);

  /*
   * Insert a surface into the cache. It is an error to call this function
   * without first calling Lookup to verify that the surface is not already in
   * the cache.
   *
   * @param aTarget      The new surface (wrapped in an imgFrame) to insert into
   *                     the cache.
   * @param aImageKey    Key data identifying which image the surface belongs to.
   * @param aSurfaceKey  Key data which uniquely identifies the requested surface.
   */
  static void Insert(imgFrame*         aSurface,
                     const ImageKey    aImageKey,
                     const SurfaceKey& aSurfaceKey);

  /*
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
   *
   * @return false if the surface cache can't hold a surface of that size.
   */
  static bool CanHold(const IntSize& aSize);

  /*
   * Removes a surface from the cache, if it's present.
   *
   * Use this function to remove individual surfaces that have become invalid.
   * Prefer Discard() or DiscardAll() when they're applicable, as they have much
   * better performance than calling this function repeatedly.
   *
   * @param aImageKey    Key data identifying which image the surface belongs to.
   * @param aSurfaceKey  Key data which uniquely identifies the requested surface.
   */
  static void RemoveIfPresent(const ImageKey    aImageKey,
                              const SurfaceKey& aSurfaceKey);
  /*
   * Evicts any cached surfaces associated with the given image from the cache.
   * This MUST be called, at a minimum, when the image is destroyed. If
   * another image were allocated at the same address it could result in
   * subtle, difficult-to-reproduce bugs.
   *
   * @param aImageKey  The image which should be removed from the cache.
   */
  static void Discard(const ImageKey aImageKey);

  /*
   * Evicts all caches surfaces from ths cache.
   */
  static void DiscardAll();

private:
  virtual ~SurfaceCache() = 0;  // Forbid instantiation.
};

} // namespace image
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_SURFACECACHE_H_
