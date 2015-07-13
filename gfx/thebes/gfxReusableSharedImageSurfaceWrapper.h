/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXSHMCOWSURFACEWRAPPER
#define GFXSHMCOWSURFACEWRAPPER

#include "gfxReusableSurfaceWrapper.h"
#include "mozilla/RefPtr.h"

class gfxSharedImageSurface;

namespace mozilla {
namespace ipc {
class Shmem;
} // namespace ipc
namespace layers {
class ISurfaceAllocator;
} // namespace layers
} // namespace mozilla

/**
 * A cross-process capable implementation of gfxReusableSurfaceWrapper based
 * on gfxSharedImageSurface.
 */
class gfxReusableSharedImageSurfaceWrapper : public gfxReusableSurfaceWrapper {
public:
  gfxReusableSharedImageSurfaceWrapper(mozilla::layers::ISurfaceAllocator* aAllocator,
                                       gfxSharedImageSurface* aSurface);
protected:
  ~gfxReusableSharedImageSurfaceWrapper();

public:
  const unsigned char* GetReadOnlyData() const override;
  gfxImageFormat Format() override;
  gfxReusableSurfaceWrapper* GetWritable(gfxImageSurface** aSurface) override;
  void ReadLock() override;
  void ReadUnlock() override;

  Type GetType() override
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
  mozilla::RefPtr<mozilla::layers::ISurfaceAllocator> mAllocator;
  nsRefPtr<gfxSharedImageSurface>         mSurface;
};

#endif // GFXSHMCOWSURFACEWRAPPER
