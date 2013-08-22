/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXSHMCOWSURFACEWRAPPER
#define GFXSHMCOWSURFACEWRAPPER

#include "gfxReusableSurfaceWrapper.h"
#include "mozilla/layers/ISurfaceAllocator.h"

class gfxSharedImageSurface;

/**
 * A cross-process capable implementation of gfxReusableSurfaceWrapper based
 * on gfxSharedImageSurface.
 */
class gfxReusableSharedImageSurfaceWrapper : public gfxReusableSurfaceWrapper {
public:
  gfxReusableSharedImageSurfaceWrapper(mozilla::layers::ISurfaceAllocator* aAllocator,
                                       gfxSharedImageSurface* aSurface);
  ~gfxReusableSharedImageSurfaceWrapper();

  const unsigned char* GetReadOnlyData() const MOZ_OVERRIDE;
  gfxASurface::gfxImageFormat Format() MOZ_OVERRIDE;
  gfxReusableSurfaceWrapper* GetWritable(gfxImageSurface** aSurface) MOZ_OVERRIDE;
  void ReadLock() MOZ_OVERRIDE;
  void ReadUnlock() MOZ_OVERRIDE;

  Type GetType()
  {
    return TYPE_SHARED_IMAGE;
  }

  /**
   * Returns the shared memory segment that backs the shared image surface.
   */
  mozilla::ipc::Shmem& GetShmem();

  /**
   * Create a gfxReusableSurfaceWrapper from the shared memory segment of a
   * gfxSharedImageSurface. A ReadLock must be held, which will be adopted by
   * the returned gfxReusableSurfaceWrapper.
   */
  static already_AddRefed<gfxReusableSharedImageSurfaceWrapper>
  Open(mozilla::layers::ISurfaceAllocator* aAllocator, const mozilla::ipc::Shmem& aShmem);

private:
  mozilla::layers::ISurfaceAllocator*     mAllocator;
  nsRefPtr<gfxSharedImageSurface>         mSurface;
};

#endif // GFXSHMCOWSURFACEWRAPPER
