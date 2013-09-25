/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CACHED_TEMP_SURFACE_H
#define GFX_CACHED_TEMP_SURFACE_H

#include "gfxASurface.h"
#include "nsExpirationTracker.h"
#include "nsSize.h"

class gfxContext;

/**
 * This class can be used to cache double-buffering back surfaces.
 *
 * Large resource allocations may have an overhead that can be avoided by
 * caching.  Caching also alows the system to use history in deciding whether
 * to manage the surfaces in video or system memory.
 *
 * However, because we don't want to set aside megabytes of unused resources
 * unncessarily, these surfaces are released on a timer.
 */

class gfxCachedTempSurface {
public:
  /**
   * Returns a context for a surface that can be efficiently copied to
   * |aSimilarTo|.
   *
   * When |aContentType| has an alpha component, the surface will be cleared.
   * For opaque surfaces, the initial surface contents are undefined.
   * When |aContentType| differs in different invocations this is handled
   * appropriately, creating a new surface if necessary.
   * 
   * Because the cached surface may have been created during a previous
   * invocation, this will not be efficient if the new |aSimilarTo| has a
   * different format, size, or gfxSurfaceType.
   */
  already_AddRefed<gfxContext> Get(gfxContentType aContentType,
                                   const gfxRect& aRect,
                                   gfxASurface* aSimilarTo);

  void Expire() { mSurface = nullptr; }
  nsExpirationState* GetExpirationState() { return &mExpirationState; }
  ~gfxCachedTempSurface();

  bool IsSurface(gfxASurface* aSurface) { return mSurface == aSurface; }

private:
  nsRefPtr<gfxASurface> mSurface;
  gfxIntSize mSize;
  nsExpirationState mExpirationState;
  gfxSurfaceType mType;
};

#endif /* GFX_CACHED_TEMP_SURFACE_H */
