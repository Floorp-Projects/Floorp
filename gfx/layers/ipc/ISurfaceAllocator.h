/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_ISURFACEDEALLOCATOR
#define GFX_LAYERS_ISURFACEDEALLOCATOR

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t
#include "gfxTypes.h"
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/RefPtr.h"
#include "nsIMemoryReporter.h"          // for nsIMemoryReporter
#include "mozilla/Atomics.h"            // for Atomic
#include "mozilla/layers/LayersMessages.h" // for ShmemSection
#include "LayersTypes.h"
#include <vector>
#include "mozilla/layers/AtomicRefCountedWithFinalize.h"

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

namespace mozilla {
namespace ipc {
class Shmem;
} // namespace ipc
namespace gfx {
class DataSourceSurface;
} // namespace gfx

namespace layers {

class MaybeMagicGrallocBufferHandle;

enum BufferCapabilities {
  DEFAULT_BUFFER_CAPS = 0,
  /**
   * The allocated buffer must be efficiently mappable as a DataSourceSurface.
   */
  MAP_AS_IMAGE_SURFACE = 1 << 0,
  /**
   * The allocated buffer will be used for GL rendering only
   */
  USING_GL_RENDERING_ONLY = 1 << 1
};

class SurfaceDescriptor;


mozilla::ipc::SharedMemory::SharedMemoryType OptimalShmemType();
bool IsSurfaceDescriptorValid(const SurfaceDescriptor& aSurface);
bool IsSurfaceDescriptorOwned(const SurfaceDescriptor& aDescriptor);
bool ReleaseOwnedSurfaceDescriptor(const SurfaceDescriptor& aDescriptor);

already_AddRefed<gfx::DrawTarget> GetDrawTargetForDescriptor(const SurfaceDescriptor& aDescriptor, gfx::BackendType aBackend);
already_AddRefed<gfx::DataSourceSurface> GetSurfaceForDescriptor(const SurfaceDescriptor& aDescriptor);
/**
 * An interface used to create and destroy surfaces that are shared with the
 * Compositor process (using shmem, or gralloc, or other platform specific memory)
 *
 * Most of the methods here correspond to methods that are implemented by IPDL
 * actors without a common polymorphic interface.
 * These methods should be only called in the ipdl implementor's thread, unless
 * specified otherwise in the implementing class.
 */
class ISurfaceAllocator : public AtomicRefCountedWithFinalize<ISurfaceAllocator>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(ISurfaceAllocator)
  ISurfaceAllocator()
    : mDefaultMessageLoop(MessageLoop::current())
  {}

  void Finalize();

  /**
   * Returns the type of backend that is used off the main thread.
   * We only don't allow changing the backend type at runtime so this value can
   * be queried once and will not change until Gecko is restarted.
   *
   * XXX - With e10s this may not be true anymore. we can have accelerated widgets
   * and non-accelerated widgets (small popups, etc.)
   */
  virtual LayersBackend GetCompositorBackendType() const = 0;

  /**
   * Allocate shared memory that can be accessed by only one process at a time.
   * Ownership of this memory is passed when the memory is sent in an IPDL
   * message.
   */
  virtual bool AllocShmem(size_t aSize,
                          mozilla::ipc::SharedMemory::SharedMemoryType aType,
                          mozilla::ipc::Shmem* aShmem) = 0;

  /**
   * Allocate shared memory that can be accessed by both processes at the
   * same time. Safety is left for the user of the memory to care about.
   */
  virtual bool AllocUnsafeShmem(size_t aSize,
                                mozilla::ipc::SharedMemory::SharedMemoryType aType,
                                mozilla::ipc::Shmem* aShmem) = 0;

  /**
   * Allocate memory in shared memory that can always be accessed by both
   * processes at a time. Safety is left for the user of the memory to care
   * about.
   */
  bool AllocShmemSection(size_t aSize,
                         mozilla::layers::ShmemSection* aShmemSection);

  /**
   * Deallocates a shmem section.
   */
  void FreeShmemSection(mozilla::layers::ShmemSection& aShmemSection);

  /**
   * Deallocate memory allocated by either AllocShmem or AllocUnsafeShmem.
   */
  virtual void DeallocShmem(mozilla::ipc::Shmem& aShmem) = 0;

  // was AllocBuffer
  virtual bool AllocSurfaceDescriptor(const gfx::IntSize& aSize,
                                      gfxContentType aContent,
                                      SurfaceDescriptor* aBuffer);

  // was AllocBufferWithCaps
  virtual bool AllocSurfaceDescriptorWithCaps(const gfx::IntSize& aSize,
                                              gfxContentType aContent,
                                              uint32_t aCaps,
                                              SurfaceDescriptor* aBuffer);

  /**
   * Returns the maximum texture size supported by the compositor.
   */
  virtual int32_t GetMaxTextureSize() const { return INT32_MAX; }

  virtual void DestroySharedSurface(SurfaceDescriptor* aSurface);

  // method that does the actual allocation work
  bool AllocGrallocBuffer(const gfx::IntSize& aSize,
                          uint32_t aFormat,
                          uint32_t aUsage,
                          MaybeMagicGrallocBufferHandle* aHandle);

  void DeallocGrallocBuffer(MaybeMagicGrallocBufferHandle* aHandle);

  void DropGrallocBuffer(MaybeMagicGrallocBufferHandle* aHandle);

  virtual bool IPCOpen() const { return true; }
  virtual bool IsSameProcess() const = 0;
  virtual base::ProcessId ParentPid() const { return base::ProcessId(); }

  virtual bool IsImageBridgeChild() const { return false; }

  virtual MessageLoop * GetMessageLoop() const
  {
    return mDefaultMessageLoop;
  }

  // Returns true if aSurface wraps a Shmem.
  static bool IsShmem(SurfaceDescriptor* aSurface);

protected:

  virtual bool IsOnCompositorSide() const = 0;

  virtual ~ISurfaceAllocator();

  void ShrinkShmemSectionHeap();

  // This is used to implement an extremely simple & naive heap allocator.
  std::vector<mozilla::ipc::Shmem> mUsedShmems;

  MessageLoop* mDefaultMessageLoop;

  friend class AtomicRefCountedWithFinalize<ISurfaceAllocator>;
};

class GfxMemoryImageReporter final : public nsIMemoryReporter
{
  ~GfxMemoryImageReporter() {}

public:
  NS_DECL_ISUPPORTS

  GfxMemoryImageReporter()
  {
#ifdef DEBUG
    // There must be only one instance of this class, due to |sAmount|
    // being static.
    static bool hasRun = false;
    MOZ_ASSERT(!hasRun);
    hasRun = true;
#endif
  }

  MOZ_DEFINE_MALLOC_SIZE_OF_ON_ALLOC(MallocSizeOfOnAlloc)
  MOZ_DEFINE_MALLOC_SIZE_OF_ON_FREE(MallocSizeOfOnFree)

  static void DidAlloc(void* aPointer)
  {
    sAmount += MallocSizeOfOnAlloc(aPointer);
  }

  static void WillFree(void* aPointer)
  {
    sAmount -= MallocSizeOfOnFree(aPointer);
  }

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    return MOZ_COLLECT_REPORT(
      "explicit/gfx/heap-textures", KIND_HEAP, UNITS_BYTES, sAmount,
      "Heap memory shared between threads by texture clients and hosts.");
  }

private:
  static mozilla::Atomic<size_t> sAmount;
};

} // namespace layers
} // namespace mozilla

#endif
