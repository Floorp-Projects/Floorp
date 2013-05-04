/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_ISURFACEDEALLOCATOR
#define GFX_LAYERS_ISURFACEDEALLOCATOR

#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/RefPtr.h"
#include "gfxPoint.h"
#include "gfxASurface.h"

class gfxSharedImageSurface;
class gfxASurface;

namespace base {
class Thread;
} // namespace

namespace mozilla {
namespace ipc {
class Shmem;
} // namespace
namespace layers {

class PGrallocBufferChild;
class MaybeMagicGrallocBufferHandle;

enum BufferCapabilities {
  DEFAULT_BUFFER_CAPS = 0,
  /**
   * The allocated buffer must be efficiently mappable as a
   * gfxImageSurface.
   */
  MAP_AS_IMAGE_SURFACE = 1 << 0
};

class SurfaceDescriptor;


ipc::SharedMemory::SharedMemoryType OptimalShmemType();
bool IsSurfaceDescriptorValid(const SurfaceDescriptor& aSurface);
bool IsSurfaceDescriptorOwned(const SurfaceDescriptor& aDescriptor);
bool ReleaseOwnedSurfaceDescriptor(const SurfaceDescriptor& aDescriptor);
/**
 * An interface used to create and destroy surfaces that are shared with the
 * Compositor process (using shmem, or gralloc, or other platform specific memory)
 *
 * Most of the methods here correspond to methods that are implemented by IPDL
 * actors without a common polymorphic interface.
 * These methods should be only called in the ipdl implementor's thread, unless
 * specified otherwise in the implementing class.
 */
class ISurfaceAllocator
{
public:
ISurfaceAllocator() {}

  /**
   * Allocate shared memory that can be accessed by only one process at a time.
   * Ownership of this memory is passed when the memory is sent in an IPDL
   * message.
   */
  virtual bool AllocShmem(size_t aSize,
                          ipc::SharedMemory::SharedMemoryType aType,
                          ipc::Shmem* aShmem) = 0;

  /**
   * Allocate shared memory that can be accessed by both processes at the
   * same time. Safety is left for the user of the memory to care about.
   */
  virtual bool AllocUnsafeShmem(size_t aSize,
                                ipc::SharedMemory::SharedMemoryType aType,
                                ipc::Shmem* aShmem) = 0;
  /**
   * Deallocate memory allocated by either AllocShmem or AllocUnsafeShmem.
   */
  virtual void DeallocShmem(ipc::Shmem& aShmem) = 0;

  // was AllocBuffer
  virtual bool AllocSharedImageSurface(const gfxIntSize& aSize,
                                       gfxASurface::gfxContentType aContent,
                                       gfxSharedImageSurface** aBuffer);
  virtual bool AllocSurfaceDescriptor(const gfxIntSize& aSize,
                                      gfxASurface::gfxContentType aContent,
                                      SurfaceDescriptor* aBuffer);

  // was AllocBufferWithCaps
  virtual bool AllocSurfaceDescriptorWithCaps(const gfxIntSize& aSize,
                                              gfxASurface::gfxContentType aContent,
                                              uint32_t aCaps,
                                              SurfaceDescriptor* aBuffer);

  virtual void DestroySharedSurface(SurfaceDescriptor* aSurface);

protected:
  // this method is needed for a temporary fix, will be removed after
  // TextureClient/Host rework.
  virtual bool IsOnCompositorSide() const = 0;
  static bool PlatformDestroySharedSurface(SurfaceDescriptor* aSurface);
  virtual bool PlatformAllocSurfaceDescriptor(const gfxIntSize& aSize,
                                              gfxASurface::gfxContentType aContent,
                                              uint32_t aCaps,
                                              SurfaceDescriptor* aBuffer);
  virtual PGrallocBufferChild* AllocGrallocBuffer(const gfxIntSize& aSize,
                                                  gfxASurface::gfxContentType aContent,
                                                  MaybeMagicGrallocBufferHandle* aHandle)
  {
    return nullptr;
  }

  ~ISurfaceAllocator() {}
};

} // namespace
} // namespace

#endif
