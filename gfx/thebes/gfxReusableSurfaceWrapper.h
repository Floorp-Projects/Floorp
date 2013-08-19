/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXCOWSURFACEWRAPPER
#define GFXCOWSURFACEWRAPPER

#include "gfxASurface.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"
#include "mozilla/Atomics.h"
#include "mozilla/layers/ISurfaceAllocator.h"

class gfxSharedImageSurface;

/**
 * Provides a cross thread wrapper for a gfxSharedImageSurface
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
   * Pass the gfxSharedImageSurface to the wrapper. The wrapper will ReadLock
   * on creation and ReadUnlock on destruction.
   */
  gfxReusableSurfaceWrapper(mozilla::layers::ISurfaceAllocator* aAllocator, gfxSharedImageSurface* aSurface);

  ~gfxReusableSurfaceWrapper();

  const unsigned char* GetReadOnlyData() const;

  mozilla::ipc::Shmem& GetShmem();

  /**
   * Create a gfxReusableSurfaceWrapper from the shared memory segment of a
   * gfxSharedImageSurface. A ReadLock must be held, which will be adopted by
   * the returned gfxReusableSurfaceWrapper.
   */
  static already_AddRefed<gfxReusableSurfaceWrapper>
  Open(mozilla::layers::ISurfaceAllocator* aAllocator, const mozilla::ipc::Shmem& aShmem);

  gfxASurface::gfxImageFormat Format();

  /**
   * Get a writable copy of the image.
   * If necessary this will copy the wrapper. If there are no contention
   * the same wrapper will be returned. A ReadLock must be held when
   * calling this function, and calling it will give up this lock.
   */
  gfxReusableSurfaceWrapper* GetWritable(gfxImageSurface** aSurface);

  /**
   * A read only lock count is recorded, any attempts to
   * call GetWritable() while this count is greater than one will
   * create a new surface/wrapper pair.
   *
   * When a surface's read count falls to zero, its memory will be
   * deallocated. It is the responsibility of the user to make sure
   * that all locks are matched with an equal number of unlocks.
   */
  void ReadLock();
  void ReadUnlock();

private:
  NS_DECL_OWNINGTHREAD
  mozilla::layers::ISurfaceAllocator*     mAllocator;
  nsRefPtr<gfxSharedImageSurface>         mSurface;
};

#endif // GFXCOWSURFACEWRAPPER
