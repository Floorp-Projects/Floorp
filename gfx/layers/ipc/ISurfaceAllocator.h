/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_ISURFACEDEALLOCATOR
#define GFX_LAYERS_ISURFACEDEALLOCATOR

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t
#include "gfxTypes.h"
#include "gfxPoint.h"                   // for gfxIntSize
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "nsIMemoryReporter.h"          // for MemoryUniReporter
#include "mozilla/Atomics.h"            // for Atomic

/*
 * FIXME [bjacob] *** PURE CRAZYNESS WARNING ***
 *
 * This #define is actually needed here, because subclasses of ISurfaceAllocator,
 * namely ShadowLayerForwarder, will or will not override AllocGrallocBuffer
 * depending on whether MOZ_HAVE_SURFACEDESCRIPTORGRALLOC is defined.
 */
#ifdef MOZ_WIDGET_GONK
#define MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
#endif

class gfxSharedImageSurface;
class MemoryTextureClient;
class MemoryTextureHost;

namespace base {
class Thread;
}

namespace mozilla {
namespace ipc {
class Shmem;
}

namespace layers {

class PGrallocBufferChild;
class MaybeMagicGrallocBufferHandle;

enum BufferCapabilities {
  DEFAULT_BUFFER_CAPS = 0,
  /**
   * The allocated buffer must be efficiently mappable as a
   * gfxImageSurface.
   */
  MAP_AS_IMAGE_SURFACE = 1 << 0,
  /**
   * The allocated buffer will be used for GL rendering only
   */
  USING_GL_RENDERING_ONLY = 1 << 1
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
                                       gfxContentType aContent,
                                       gfxSharedImageSurface** aBuffer);
  virtual bool AllocSurfaceDescriptor(const gfxIntSize& aSize,
                                      gfxContentType aContent,
                                      SurfaceDescriptor* aBuffer);

  // was AllocBufferWithCaps
  virtual bool AllocSurfaceDescriptorWithCaps(const gfxIntSize& aSize,
                                              gfxContentType aContent,
                                              uint32_t aCaps,
                                              SurfaceDescriptor* aBuffer);

  virtual void DestroySharedSurface(SurfaceDescriptor* aSurface);

  // method that does the actual allocation work
  virtual PGrallocBufferChild* AllocGrallocBuffer(const gfxIntSize& aSize,
                                                  uint32_t aFormat,
                                                  uint32_t aUsage,
                                                  MaybeMagicGrallocBufferHandle* aHandle)
  {
    return nullptr;
  }
protected:
  // this method is needed for a temporary fix, will be removed after
  // DeprecatedTextureClient/Host rework.
  virtual bool IsOnCompositorSide() const = 0;
  static bool PlatformDestroySharedSurface(SurfaceDescriptor* aSurface);
  virtual bool PlatformAllocSurfaceDescriptor(const gfxIntSize& aSize,
                                              gfxContentType aContent,
                                              uint32_t aCaps,
                                              SurfaceDescriptor* aBuffer);


  ~ISurfaceAllocator() {}
};

class GfxHeapTexturesReporter MOZ_FINAL : public mozilla::MemoryUniReporter
{
public:
  GfxHeapTexturesReporter()
    : MemoryUniReporter("explicit/gfx/heap-textures", KIND_HEAP, UNITS_BYTES,
                        "Heap memory shared between threads by texture clients and hosts.")
  {
#ifdef DEBUG
    // There must be only one instance of this class, due to |sAmount|
    // being static.
    static bool hasRun = false;
    MOZ_ASSERT(!hasRun);
    hasRun = true;
#endif
  }

  static void OnAlloc(void* aPointer)
  {
    sAmount += MallocSizeOfOnAlloc(aPointer);
  }

  static void OnFree(void* aPointer)
  {
    sAmount -= MallocSizeOfOnFree(aPointer);
  }

private:
  int64_t Amount() MOZ_OVERRIDE { return sAmount; }

  static mozilla::Atomic<int32_t> sAmount;
};

} // namespace
} // namespace

#endif
