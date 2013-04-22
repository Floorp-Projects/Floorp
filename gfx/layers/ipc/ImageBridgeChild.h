/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGEBRIDGECHILD_H
#define MOZILLA_GFX_IMAGEBRIDGECHILD_H

#include "mozilla/layers/PImageBridgeChild.h"
#include "nsAutoPtr.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/LayersTypes.h"

class gfxSharedImageSurface;

namespace base {
class Thread;
}

namespace mozilla {
namespace layers {

class ImageClient;
class ImageContainer;
class ImageBridgeParent;
class SurfaceDescriptor;
class CompositableClient;
class CompositableTransaction;
class ShadowableLayer;
class Image;


/**
 * Returns true if the current thread is the ImageBrdigeChild's thread.
 *
 * Can be called from any thread.
 */
bool InImageBridgeChildThread();

/**
 * The ImageBridge protocol is meant to allow ImageContainers to forward images
 * directly to the compositor thread/process without using the main thread.
 *
 * ImageBridgeChild is a CompositableForwarder just like ShadowLayerForwarder.
 * This means it also does transactions with the compositor thread/process,
 * except that the transactions are restricted to operations on the Compositables
 * and cannot contain messages affecting layers directly.
 *
 * ImageBridgeChild is also a ISurfaceAllocator. It can be used to allocate or
 * deallocate data that is shared with the compositor. The main differerence
 * with other ISurfaceAllocators is that some of its overriden methods can be
 * invoked from any thread.
 *
 * There are three important phases in the ImageBridge protocol. These three steps
 * can do different things depending if (A) the ImageContainer uses ImageBridge
 * or (B) it does not use ImageBridge:
 *
 * - When an ImageContainer calls its method SetCurrentImage:
 *   - (A) The image is sent directly to the compositor process through the
 *   ImageBridge IPDL protocol.
 *   On the compositor side the image is stored in a global table that associates
 *   the image with an ID corresponding to the ImageContainer, and a composition is
 *   triggered.
 *   - (B) Since it does not have an ImageBridge, the image is not sent yet.
 *   instead the will be sent to the compositor during the next layer transaction
 *   (on the main thread).
 *
 * - During a Layer transaction:
 *   - (A) The ImageContainer uses ImageBridge. The image is already available to the
 *   compositor process because it has been sent with SetCurrentImage. Yet, the
 *   CompositableHost on the compositor side will needs the ID referring to the
 *   ImageContainer to access the Image. So during the Swap operation that happens
 *   in the transaction, we swap the container ID rather than the image data.
 *   - (B) Since the ImageContainer does not use ImageBridge, the image data is swaped.
 *
 * - During composition:
 *   - (A) The CompositableHost has an AsyncID, it looks up the ID in the 
 *   global table to see if there is an image. If there is no image, nothing is rendered.
 *   - (B) The CompositableHost has image data rather than an ID (meaning it is not
 *   using ImageBridge), then it just composites the image data normally.
 *
 * This means that there might be a possibility for the ImageBridge to send the first
 * frame before the first layer transaction that will pass the container ID to the
 * CompositableHost happens. In this (unlikely) case the layer is not composited
 * until the layer transaction happens. This means this scenario is not harmful.
 *
 * Since sending an image through imageBridge triggers compositing, the main thread is
 * not used at all (except for the very first transaction that provides the
 * CompositableHost with an AsyncID).
 */
class ImageBridgeChild : public PImageBridgeChild
                       , public CompositableForwarder
{
  friend class ImageContainer;
public:

  /**
   * Creates the image bridge with a dedicated thread for ImageBridgeChild.
   *
   * We may want to use a specifi thread in the future. In this case, use
   * CreateWithThread instead.
   */
  static void StartUp();

  static PImageBridgeChild*
  StartUpInChildProcess(Transport* aTransport, ProcessId aOtherProcess);

  /**
   * Destroys the image bridge by calling DestroyBridge, and destroys the
   * ImageBridge's thread.
   *
   * If you don't want to destroy the thread, call DestroyBridge directly
   * instead.
   */
  static void ShutDown();

  /**
   * Creates the ImageBridgeChild manager protocol.
   */
  static bool StartUpOnThread(base::Thread* aThread);

  /**
   * Destroys The ImageBridge protcol.
   *
   * The actual destruction happens synchronously on the ImageBridgeChild thread
   * which means that if this function is called from another thread, the current
   * thread will be paused until the destruction is done.
   */
  static void DestroyBridge();

  /**
   * Returns true if the singleton has been created.
   *
   * Can be called from any thread.
   */
  static bool IsCreated();

  /**
   * returns the singleton instance.
   *
   * can be called from any thread.
   */
  static ImageBridgeChild* GetSingleton();


  /**
   * Dispatches a task to the ImageBridgeChild thread to do the connection
   */
  void ConnectAsync(ImageBridgeParent* aParent);

  void BeginTransaction();
  void EndTransaction();

  /**
   * Returns the ImageBridgeChild's thread.
   *
   * Can be called from any thread.
   */
  base::Thread * GetThread() const;

  /**
   * Returns the ImageBridgeChild's message loop.
   *
   * Can be called from any thread.
   */
  MessageLoop * GetMessageLoop() const;

  PCompositableChild* AllocPCompositable(const TextureInfo& aInfo, uint64_t* aID) MOZ_OVERRIDE;
  bool DeallocPCompositable(PCompositableChild* aActor) MOZ_OVERRIDE;

  /**
   * This must be called by the static function DeleteImageBridgeSync defined
   * in ImageBridgeChild.cpp ONLY.
   */
  ~ImageBridgeChild();

  virtual PGrallocBufferChild*
  AllocPGrallocBuffer(const gfxIntSize&, const uint32_t&, const uint32_t&,
                      MaybeMagicGrallocBufferHandle*) MOZ_OVERRIDE;

  virtual bool
  DeallocPGrallocBuffer(PGrallocBufferChild* actor) MOZ_OVERRIDE;

  /**
   * Allocate a gralloc SurfaceDescriptor remotely.
   */
  bool
  AllocSurfaceDescriptorGralloc(const gfxIntSize& aSize,
                                const uint32_t& aFormat,
                                const uint32_t& aUsage,
                                SurfaceDescriptor* aBuffer);

  /**
   * Part of the allocation of gralloc SurfaceDescriptor that is
   * executed on the ImageBridgeChild thread after invoking
   * AllocSurfaceDescriptorGralloc.
   *
   * Must be called from the ImageBridgeChild thread.
   */
  bool
  AllocSurfaceDescriptorGrallocNow(const gfxIntSize& aSize,
                                   const uint32_t& aContent,
                                   const uint32_t& aUsage,
                                   SurfaceDescriptor* aBuffer);

  /**
   * Deallocate a remotely allocated gralloc buffer.
   */
  bool
  DeallocSurfaceDescriptorGralloc(const SurfaceDescriptor& aBuffer);

  /**
   * Part of the deallocation of gralloc SurfaceDescriptor that is
   * executed on the ImageBridgeChild thread after invoking
   * DeallocSurfaceDescriptorGralloc.
   *
   * Must be called from the ImageBridgeChild thread.
   */
  bool
  DeallocSurfaceDescriptorGrallocNow(const SurfaceDescriptor& aBuffer);

  TemporaryRef<ImageClient> CreateImageClient(CompositableType aType);
  TemporaryRef<ImageClient> CreateImageClientNow(CompositableType aType);

  static void DispatchReleaseImageClient(ImageClient* aClient);
  static void DispatchImageClientUpdate(ImageClient* aClient, ImageContainer* aContainer);


  // CompositableForwarder

  virtual void Connect(CompositableClient* aCompositable) MOZ_OVERRIDE;

  virtual void PaintedTiledLayerBuffer(CompositableClient* aCompositable,
                                       BasicTiledLayerBuffer* aTiledLayerBuffer) MOZ_OVERRIDE
  {
    NS_RUNTIMEABORT("should not be called");
  }

  /**
   * Communicate to the compositor that the texture identified by aCompositable
   * and aTextureId has been updated to aDescriptor.
   */
  virtual void UpdateTexture(CompositableClient* aCompositable,
                             TextureIdentifier aTextureId,
                             SurfaceDescriptor* aDescriptor) MOZ_OVERRIDE;

  /**
   * Communicate the picture rect of a YUV image in aLayer to the compositor
   */
  virtual void UpdatePictureRect(CompositableClient* aCompositable,
                                 const nsIntRect& aRect) MOZ_OVERRIDE;


  // at the moment we don't need to implement these. They are only used for
  // thebes layers which don't support async updates.
  virtual void CreatedSingleBuffer(CompositableClient* aCompositable,
                                   const SurfaceDescriptor& aDescriptor,
                                   const TextureInfo& aTextureInfo,
                                   const SurfaceDescriptor* aDescriptorOnWhite = nullptr) MOZ_OVERRIDE {
    NS_RUNTIMEABORT("should not be called");
  }
  virtual void CreatedDoubleBuffer(CompositableClient* aCompositable,
                                   const SurfaceDescriptor& aFrontDescriptor,
                                   const SurfaceDescriptor& aBackDescriptor,
                                   const TextureInfo& aTextureInfo,
                                   const SurfaceDescriptor* aFrontDescriptorOnWhite = nullptr,
                                   const SurfaceDescriptor* aBackDescriptorOnWhite = nullptr) MOZ_OVERRIDE {
    NS_RUNTIMEABORT("should not be called");
  }
  virtual void DestroyThebesBuffer(CompositableClient* aCompositable) MOZ_OVERRIDE {
    NS_RUNTIMEABORT("should not be called");
  }
  virtual void UpdateTextureRegion(CompositableClient* aCompositable,
                                   const ThebesBufferData& aThebesBufferData,
                                   const nsIntRegion& aUpdatedRegion) MOZ_OVERRIDE {
    NS_RUNTIMEABORT("should not be called");
  }
  virtual void DestroyedThebesBuffer(const SurfaceDescriptor& aBackBufferToDestroy) MOZ_OVERRIDE
  {
    NS_RUNTIMEABORT("should not be called");
  }


  // ISurfaceAllocator

  /**
   * See ISurfaceAllocator.h
   * Can be used from any thread.
   * If used outside the ImageBridgeChild thread, it will proxy a synchronous
   * call on the ImageBridgeChild thread.
   */
  virtual bool AllocUnsafeShmem(size_t aSize,
                                ipc::SharedMemory::SharedMemoryType aType,
                                ipc::Shmem* aShmem) MOZ_OVERRIDE;
  /**
   * See ISurfaceAllocator.h
   * Can be used from any thread.
   * If used outside the ImageBridgeChild thread, it will proxy a synchronous
   * call on the ImageBridgeChild thread.
   */
  virtual bool AllocShmem(size_t aSize,
                          ipc::SharedMemory::SharedMemoryType aType,
                          ipc::Shmem* aShmem) MOZ_OVERRIDE;
  /**
   * See ISurfaceAllocator.h
   * Can be used from any thread.
   * If used outside the ImageBridgeChild thread, it will proxy a synchronous
   * call on the ImageBridgeChild thread.
   */
  virtual void DeallocShmem(ipc::Shmem& aShmem);

protected:
  ImageBridgeChild();
  bool DispatchAllocShmemInternal(size_t aSize,
                                  SharedMemory::SharedMemoryType aType,
                                  Shmem* aShmem,
                                  bool aUnsafe);

  CompositableTransaction* mTxn;

  virtual PGrallocBufferChild* AllocGrallocBuffer(const gfxIntSize& aSize,
                                                  gfxASurface::gfxContentType aContent,
                                                  MaybeMagicGrallocBufferHandle* aHandle) MOZ_OVERRIDE;
};

} // layers
} // mozilla

#endif
