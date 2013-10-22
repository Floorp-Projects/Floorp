/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SurfaceCache is a service for caching temporary surfaces in imagelib.
 */

#ifndef MOZILLA_IMAGELIB_SURFACECACHE_H_
#define MOZILLA_IMAGELIB_SURFACECACHE_H_

#include "mozilla/HashFunctions.h"  // for HashGeneric and AddToHash
#include "gfxPoint.h"               // for gfxSize
#include "nsCOMPtr.h"               // for already_AddRefed
#include "nsSize.h"                 // for nsIntSize
#include "SVGImageContext.h"        // for SVGImageContext

class gfxDrawable;

namespace mozilla {

namespace gfx {
class DrawTarget;
} // namespace gfx

namespace image {

class Image;

/*
 * ImageKey contains the information we need to look up all cached surfaces for
 * a particular image.
 */
typedef Image* ImageKey;

/*
 * SurfaceKey contains the information we need to look up a specific cached
 * surface. Together with an ImageKey, this uniquely identifies the surface.
 *
 * XXX(seth): Right now this is specialized to the needs of VectorImage. We'll
 * generalize it in bug 919071.
 */
class SurfaceKey
{
public:
  SurfaceKey(const nsIntSize aSize,
             const gfxSize aScale,
             const SVGImageContext* aSVGContext,
             const float aAnimationTime,
             const uint32_t aFlags)
    : mSize(aSize)
    , mScale(aScale)
    , mSVGContextIsValid(aSVGContext != nullptr)
    , mAnimationTime(aAnimationTime)
    , mFlags(aFlags)
  {
    // XXX(seth): Would love to use Maybe<T> here, but see bug 913586.
    if (mSVGContextIsValid)
      mSVGContext = *aSVGContext;
  }

  bool operator==(const SurfaceKey& aOther) const
  {
    bool matchesSVGContext = aOther.mSVGContextIsValid == mSVGContextIsValid &&
                             (!mSVGContextIsValid || aOther.mSVGContext == mSVGContext);
    return aOther.mSize == mSize &&
           aOther.mScale == mScale &&
           matchesSVGContext &&
           aOther.mAnimationTime == mAnimationTime &&
           aOther.mFlags == mFlags;
  }

  uint32_t Hash() const
  {
    uint32_t hash = HashGeneric(mSize.width, mSize.height);
    hash = AddToHash(hash, mScale.width, mScale.height);
    hash = AddToHash(hash, mSVGContextIsValid, mSVGContext.Hash());
    hash = AddToHash(hash, mAnimationTime, mFlags);
    return hash;
  }

  nsIntSize Size() const { return mSize; }

private:
  nsIntSize       mSize;
  gfxSize         mScale;
  SVGImageContext mSVGContext;
  bool            mSVGContextIsValid;
  float           mAnimationTime;
  uint32_t        mFlags;
};

/**
 * SurfaceCache is an imagelib-global service that allows caching of temporary
 * surfaces. Surfaces expire from the cache automatically if they go too long
 * without being accessed.
 *
 * SurfaceCache is not thread-safe; it should only be accessed from the main
 * thread.
 */
struct SurfaceCache
{
  /*
   * Initialize static data. Called during imagelib module initialization.
   */
  static void Initialize();

  /*
   * Release static data. Called during imagelib module shutdown.
   */
  static void Shutdown();

  /*
   * Look up a surface in the cache.
   *
   * @param aImageKey    Key data identifying which image the surface belongs to.
   * @param aSurfaceKey  Key data which uniquely identifies the requested surface.
   *
   * @return the requested surface, or nullptr if not found.
   */
  static already_AddRefed<gfxDrawable> Lookup(const ImageKey    aImageKey,
                                              const SurfaceKey& aSurfaceKey);

  /*
   * Insert a surface into the cache. It is an error to call this function
   * without first calling Lookup to verify that the surface is not already in
   * the cache.
   *
   * @param aTarget      The new surface (in the form of a DrawTarget) to insert
   *                     into the cache.
   * @param aImageKey    Key data identifying which image the surface belongs to.
   * @param aSurfaceKey  Key data which uniquely identifies the requested surface.
   */
  static void Insert(mozilla::gfx::DrawTarget* aTarget,
                     const ImageKey            aImageKey,
                     const SurfaceKey&         aSurfaceKey);

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
  static bool CanHold(const nsIntSize& aSize);

  /*
   * Evicts any cached surfaces associated with the given image from the cache.
   * This MUST be called, at a minimum, when the image is destroyed. If
   * another image were allocated at the same address it could result in
   * subtle, difficult-to-reproduce bugs.
   *
   * @param aImageKey  The image which should be removed from the cache.
   */
  static void Discard(const ImageKey aImageKey);

private:
  virtual ~SurfaceCache() = 0;  // Forbid instantiation.
};

} // namespace image
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_SURFACECACHE_H_
