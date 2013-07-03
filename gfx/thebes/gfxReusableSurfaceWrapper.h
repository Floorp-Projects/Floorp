/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXCOWSURFACEWRAPPER
#define GFXCOWSURFACEWRAPPER

#include "gfxASurface.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"
#include "mozilla/Atomics.h"

class gfxImageSurface;

/**
 * Provides a cross thread wrapper for a gfxImageSurface
 * that has copy-on-write schemantics.
 *
 * Only the owner thread can write to the surface and aquire
 * read locks.
 *
 * OMTC Usage:
 * 1) Content creates a writable copy of this surface
 *    wrapper which be optimized to the same wrapper if there
 *    are no readers.
 * 2) The surface is sent from content to the compositor once
 *    or potentially many time, each increasing a read lock.
 * 3) When the compositor has processed the surface and uploaded
 *    the content it then releases the read lock.
 */
class gfxReusableSurfaceWrapper {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxReusableSurfaceWrapper)
public:
  /**
   * Pass the gfxASurface to the wrapper.
   * The wrapper should hold the only strong reference
   * to the surface and its memebers.
   */
  gfxReusableSurfaceWrapper(gfxImageSurface* aSurface);

  ~gfxReusableSurfaceWrapper();

  const unsigned char* GetReadOnlyData() const {
    NS_ABORT_IF_FALSE(mReadCount > 0, "Should have read lock");
    return mSurfaceData;
  }

  const gfxASurface::gfxImageFormat& Format() { return mFormat; }

  /**
   * Get a writable copy of the image.
   * If necessary this will copy the wrapper. If there are no contention
   * the same wrapper will be returned.
   */
  gfxReusableSurfaceWrapper* GetWritable(gfxImageSurface** aSurface);

  /**
   * A read only lock count is recorded, any attempts to
   * call GetWritable() while this count is positive will
   * create a new surface/wrapper pair.
   */
  void ReadLock();
  void ReadUnlock();

private:
  NS_DECL_OWNINGTHREAD
  nsRefPtr<gfxImageSurface>         mSurface;
  const gfxASurface::gfxImageFormat mFormat;
  const unsigned char*              mSurfaceData;
  mozilla::Atomic<int32_t>                           mReadCount;
};

#endif // GFXCOWSURFACEWRAPPER
