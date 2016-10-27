/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGEBRIDGECHILD_H
#define MOZILLA_GFX_IMAGEBRIDGECHILD_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include "mozilla/Attributes.h"         // for override
#include "mozilla/Atomics.h"
#include "mozilla/RefPtr.h"             // for already_AddRefed
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/AsyncTransactionTracker.h" // for AsyncTransactionTrackerHolder
#include "mozilla/layers/CanvasClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/PImageBridgeChild.h"
#include "mozilla/Mutex.h"
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsIObserver.h"
#include "nsRegion.h"                   // for nsIntRegion
#include "mozilla/gfx/Rect.h"

class MessageLoop;

namespace base {
class Thread;
} // namespace base

namespace mozilla {
namespace ipc {
class Shmem;
} // namespace ipc

namespace layers {

class AsyncCanvasRenderer;
class AsyncTransactionTracker;
class ImageClient;
class ImageContainer;
class ImageContainerChild;
class ImageBridgeParent;
class CompositableClient;
struct CompositableTransaction;
class Image;
class TextureClient;
class SynchronousTask;
struct AllocShmemParams;

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
class ImageBridgeChild final : public PImageBridgeChild
                             , public CompositableForwarder
                             , public TextureForwarder
{
  friend class ImageContainer;

  typedef InfallibleTArray<AsyncParentMessageData> AsyncParentMessageArray;
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageBridgeChild, override);

  TextureForwarder* GetTextureForwarder() override { return this; }
  LayersIPCActor* GetLayersIPCActor() override { return this; }

  /**
   * Creates the image bridge with a dedicated thread for ImageBridgeChild.
   *
   * We may want to use a specifi thread in the future. In this case, use
   * CreateWithThread instead.
   */
  static void InitSameProcess();

  static void InitWithGPUProcess(Endpoint<PImageBridgeChild>&& aEndpoint);
  static bool InitForContent(Endpoint<PImageBridgeChild>&& aEndpoint);
  static bool ReinitForContent(Endpoint<PImageBridgeChild>&& aEndpoint);

  /**
   * Destroys the image bridge by calling DestroyBridge, and destroys the
   * ImageBridge's thread.
   *
   * If you don't want to destroy the thread, call DestroyBridge directly
   * instead.
   */
  static void ShutDown();

  /**
   * returns the singleton instance.
   *
   * can be called from any thread.
   */
  static RefPtr<ImageBridgeChild> GetSingleton();


  static void IdentifyCompositorTextureHost(const TextureFactoryIdentifier& aIdentifier);

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
  virtual MessageLoop * GetMessageLoop() const override;

  virtual base::ProcessId GetParentPid() const override { return OtherPid(); }

  PCompositableChild* AllocPCompositableChild(const TextureInfo& aInfo,
                                              PImageContainerChild* aChild, uint64_t* aID) override;
  bool DeallocPCompositableChild(PCompositableChild* aActor) override;

  virtual PTextureChild*
  AllocPTextureChild(const SurfaceDescriptor& aSharedData, const LayersBackend& aLayersBackend, const TextureFlags& aFlags, const uint64_t& aSerial) override;

  virtual bool
  DeallocPTextureChild(PTextureChild* actor) override;

  PMediaSystemResourceManagerChild*
  AllocPMediaSystemResourceManagerChild() override;
  bool
  DeallocPMediaSystemResourceManagerChild(PMediaSystemResourceManagerChild* aActor) override;

  virtual PImageContainerChild*
  AllocPImageContainerChild() override;
  virtual bool
  DeallocPImageContainerChild(PImageContainerChild* actor) override;

  virtual bool
  RecvParentAsyncMessages(InfallibleTArray<AsyncParentMessageData>&& aMessages) override;

  virtual bool
  RecvDidComposite(InfallibleTArray<ImageCompositeNotification>&& aNotifications) override;

  // Create an ImageClient from any thread.
  RefPtr<ImageClient> CreateImageClient(
    CompositableType aType,
    ImageContainer* aImageContainer,
    ImageContainerChild* aContainerChild);

  // Create an ImageClient from the ImageBridge thread.
  RefPtr<ImageClient> CreateImageClientNow(
    CompositableType aType,
    ImageContainer* aImageContainer,
    ImageContainerChild* aContainerChild);

  already_AddRefed<CanvasClient> CreateCanvasClient(CanvasClient::CanvasClientType aType,
                                                    TextureFlags aFlag);
  void ReleaseImageContainer(RefPtr<ImageContainerChild> aChild);
  void UpdateAsyncCanvasRenderer(AsyncCanvasRenderer* aClient);
  void UpdateImageClient(RefPtr<ImageClient> aClient, RefPtr<ImageContainer> aContainer);
  static void DispatchReleaseTextureClient(TextureClient* aClient);

  /**
   * Flush all Images sent to CompositableHost.
   */
  void FlushAllImages(ImageClient* aClient, ImageContainer* aContainer);

  virtual bool IPCOpen() const override { return mCanSend; }

private:

  /**
   * This must be called by the static function DeleteImageBridgeSync defined
   * in ImageBridgeChild.cpp ONLY.
   */
  ~ImageBridgeChild();

  // Helpers for dispatching.
  already_AddRefed<CanvasClient> CreateCanvasClientNow(
    CanvasClient::CanvasClientType aType,
    TextureFlags aFlags);
  void CreateCanvasClientSync(
    SynchronousTask* aTask,
    CanvasClient::CanvasClientType aType,
    TextureFlags aFlags,
    RefPtr<CanvasClient>* const outResult);

  void CreateImageClientSync(
    SynchronousTask* aTask,
    RefPtr<ImageClient>* result,
    CompositableType aType,
    ImageContainer* aImageContainer,
    ImageContainerChild* aContainerChild);

  void ReleaseTextureClientNow(TextureClient* aClient);

  void UpdateAsyncCanvasRendererNow(AsyncCanvasRenderer* aClient);
  void UpdateAsyncCanvasRendererSync(
    SynchronousTask* aTask,
    AsyncCanvasRenderer* aWrapper);

  void FlushAllImagesSync(
    SynchronousTask* aTask,
    ImageClient* aClient,
    ImageContainer* aContainer,
    RefPtr<AsyncTransactionWaiter> aWaiter);

  void ProxyAllocShmemNow(SynchronousTask* aTask, AllocShmemParams* aParams);
  void ProxyDeallocShmemNow(SynchronousTask* aTask, Shmem* aShmem);

public:
  // CompositableForwarder

  virtual void Connect(CompositableClient* aCompositable,
                       ImageContainer* aImageContainer) override;

  virtual bool UsesImageBridge() const override { return true; }

  /**
   * See CompositableForwarder::UseTextures
   */
  virtual void UseTextures(CompositableClient* aCompositable,
                           const nsTArray<TimedTextureClient>& aTextures) override;
  virtual void UseComponentAlphaTextures(CompositableClient* aCompositable,
                                         TextureClient* aClientOnBlack,
                                         TextureClient* aClientOnWhite) override;

  void Destroy(CompositableChild* aCompositable) override;

  /**
   * Hold TextureClient ref until end of usage on host side if TextureFlags::RECYCLE is set.
   * Host side's usage is checked via CompositableRef.
   */
  void HoldUntilCompositableRefReleasedIfNecessary(TextureClient* aClient);

  /**
   * Notify id of Texture When host side end its use. Transaction id is used to
   * make sure if there is no newer usage.
   */
  void NotifyNotUsed(uint64_t aTextureId, uint64_t aFwdTransactionId);

  void DeliverFence(uint64_t aTextureId, FenceHandle& aReleaseFenceHandle);

  void HoldUntilFenceHandleDelivery(TextureClient* aClient, uint64_t aTransactionId);

  void DeliverFenceToNonRecycle(uint64_t aTextureId, FenceHandle& aReleaseFenceHandle);

  void NotifyNotUsedToNonRecycle(uint64_t aTextureId, uint64_t aTransactionId);

  void CancelWaitFenceHandle(TextureClient* aClient);

  virtual void CancelWaitForRecycle(uint64_t aTextureId) override;

  virtual bool DestroyInTransaction(PTextureChild* aTexture, bool synchronously) override;
  virtual bool DestroyInTransaction(PCompositableChild* aCompositable, bool synchronously) override;

  virtual void RemoveTextureFromCompositable(CompositableClient* aCompositable,
                                             TextureClient* aTexture) override;

  virtual void RemoveTextureFromCompositableAsync(AsyncTransactionTracker* aAsyncTransactionTracker,
                                                  CompositableClient* aCompositable,
                                                  TextureClient* aTexture) override;

  virtual void UseTiledLayerBuffer(CompositableClient* aCompositable,
                                   const SurfaceDescriptorTiles& aTileLayerDescriptor) override
  {
    NS_RUNTIMEABORT("should not be called");
  }

  virtual void UpdateTextureRegion(CompositableClient* aCompositable,
                                   const ThebesBufferData& aThebesBufferData,
                                   const nsIntRegion& aUpdatedRegion) override {
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
                                mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                                mozilla::ipc::Shmem* aShmem) override;
  virtual bool AllocShmem(size_t aSize,
                          mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                          mozilla::ipc::Shmem* aShmem) override;

  /**
   * See ISurfaceAllocator.h
   * Can be used from any thread.
   * If used outside the ImageBridgeChild thread, it will proxy a synchronous
   * call on the ImageBridgeChild thread.
   */
  virtual void DeallocShmem(mozilla::ipc::Shmem& aShmem) override;

  virtual PTextureChild* CreateTexture(const SurfaceDescriptor& aSharedData,
                                       LayersBackend aLayersBackend,
                                       TextureFlags aFlags,
                                       uint64_t aSerial) override;

  virtual bool IsSameProcess() const override;

  virtual void UpdateFwdTransactionId() override { ++mFwdTransactionId; }
  virtual uint64_t GetFwdTransactionId() override { return mFwdTransactionId; }

  bool InForwarderThread() override {
    return InImageBridgeChildThread();
  }

  virtual void FatalError(const char* const aName, const char* const aMsg) const override;

protected:
  ImageBridgeChild();
  bool DispatchAllocShmemInternal(size_t aSize,
                                  SharedMemory::SharedMemoryType aType,
                                  Shmem* aShmem,
                                  bool aUnsafe);

  void Bind(Endpoint<PImageBridgeChild>&& aEndpoint);
  void BindSameProcess(RefPtr<ImageBridgeParent> aParent);

  void SendImageBridgeThreadId();

  void WillShutdown();
  void ShutdownStep1(SynchronousTask* aTask);
  void ShutdownStep2(SynchronousTask* aTask);
  void MarkShutDown();

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeallocPImageBridgeChild() override;

  bool CanSend() const;

  static void ShutdownSingleton();

private:
  CompositableTransaction* mTxn;

  bool mCanSend;
  bool mCalledClose;

  /**
   * Transaction id of CompositableForwarder.
   * It is incrementaed by UpdateFwdTransactionId() in each BeginTransaction() call.
   */
  uint64_t mFwdTransactionId;

  /**
   * Hold TextureClients refs until end of their usages on host side.
   * It defer calling of TextureClient recycle callback.
   */
  nsDataHashtable<nsUint64HashKey, RefPtr<TextureClient> > mTexturesWaitingRecycled;

  AsyncTransactionTrackersHolder mTrackersHolder;
};

} // namespace layers
} // namespace mozilla

#endif
