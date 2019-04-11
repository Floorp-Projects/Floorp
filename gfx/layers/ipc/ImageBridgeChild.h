/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGEBRIDGECHILD_H
#define MOZILLA_GFX_IMAGEBRIDGECHILD_H

#include <stddef.h>  // for size_t
#include <stdint.h>  // for uint32_t, uint64_t
#include <unordered_map>

#include "mozilla/Attributes.h"  // for override
#include "mozilla/Atomics.h"
#include "mozilla/RefPtr.h"            // for already_AddRefed
#include "mozilla/ipc/SharedMemory.h"  // for SharedMemory, etc
#include "mozilla/layers/CanvasClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/PImageBridgeChild.h"
#include "mozilla/Mutex.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsIObserver.h"
#include "nsRegion.h"  // for nsIntRegion
#include "mozilla/gfx/Rect.h"
#include "mozilla/ReentrantMonitor.h"  // for ReentrantMonitor, etc

class MessageLoop;

namespace base {
class Thread;
}  // namespace base

namespace mozilla {
namespace ipc {
class Shmem;
}  // namespace ipc

namespace layers {

class AsyncCanvasRenderer;
class ImageClient;
class ImageContainer;
class ImageContainerListener;
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
 * except that the transactions are restricted to operations on the
 * Compositables and cannot contain messages affecting layers directly.
 *
 * ImageBridgeChild is also a ISurfaceAllocator. It can be used to allocate or
 * deallocate data that is shared with the compositor. The main differerence
 * with other ISurfaceAllocators is that some of its overriden methods can be
 * invoked from any thread.
 *
 * There are three important phases in the ImageBridge protocol. These three
 * steps can do different things depending if (A) the ImageContainer uses
 * ImageBridge or (B) it does not use ImageBridge:
 *
 * - When an ImageContainer calls its method SetCurrentImage:
 *   - (A) The image is sent directly to the compositor process through the
 *   ImageBridge IPDL protocol.
 *   On the compositor side the image is stored in a global table that
 *   associates the image with an ID corresponding to the ImageContainer, and a
 *   composition is triggered.
 *   - (B) Since it does not have an ImageBridge, the image is not sent yet.
 *   instead the will be sent to the compositor during the next layer
 *   transaction (on the main thread).
 *
 * - During a Layer transaction:
 *   - (A) The ImageContainer uses ImageBridge. The image is already available
 *   to the compositor process because it has been sent with SetCurrentImage.
 *   Yet, the CompositableHost on the compositor side will needs the ID
 *   referring to the ImageContainer to access the Image. So during the Swap
 *   operation that happens in the transaction, we swap the container ID rather
 *   than the image data.
 *   - (B) Since the ImageContainer does not use ImageBridge, the image data is
 *   swaped.
 *
 * - During composition:
 *   - (A) The CompositableHost has an AsyncID, it looks up the ID in the
 *   global table to see if there is an image. If there is no image, nothing is
 *   rendered.
 *   - (B) The CompositableHost has image data rather than an ID (meaning it is
 *   not using ImageBridge), then it just composites the image data normally.
 *
 * This means that there might be a possibility for the ImageBridge to send the
 * first frame before the first layer transaction that will pass the container
 * ID to the CompositableHost happens. In this (unlikely) case the layer is not
 * composited until the layer transaction happens. This means this scenario is
 * not harmful.
 *
 * Since sending an image through imageBridge triggers compositing, the main
 * thread is not used at all (except for the very first transaction that
 * provides the CompositableHost with an AsyncID).
 */
class ImageBridgeChild final : public PImageBridgeChild,
                               public CompositableForwarder,
                               public TextureForwarder {
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
  static void InitSameProcess(uint32_t aNamespace);

  static void InitWithGPUProcess(Endpoint<PImageBridgeChild>&& aEndpoint,
                                 uint32_t aNamespace);
  static bool InitForContent(Endpoint<PImageBridgeChild>&& aEndpoint,
                             uint32_t aNamespace);
  static bool ReinitForContent(Endpoint<PImageBridgeChild>&& aEndpoint,
                               uint32_t aNamespace);

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

  static void IdentifyCompositorTextureHost(
      const TextureFactoryIdentifier& aIdentifier);

  void BeginTransaction();
  void EndTransaction();

  /**
   * Returns the ImageBridgeChild's thread.
   *
   * Can be called from any thread.
   */
  base::Thread* GetThread() const;

  /**
   * Returns the ImageBridgeChild's message loop.
   *
   * Can be called from any thread.
   */
  MessageLoop* GetMessageLoop() const override;

  base::ProcessId GetParentPid() const override { return OtherPid(); }

  PTextureChild* AllocPTextureChild(
      const SurfaceDescriptor& aSharedData, const ReadLockDescriptor& aReadLock,
      const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
      const uint64_t& aSerial,
      const wr::MaybeExternalImageId& aExternalImageId);

  bool DeallocPTextureChild(PTextureChild* actor);

  PMediaSystemResourceManagerChild* AllocPMediaSystemResourceManagerChild();
  bool DeallocPMediaSystemResourceManagerChild(
      PMediaSystemResourceManagerChild* aActor);

  mozilla::ipc::IPCResult RecvParentAsyncMessages(
      InfallibleTArray<AsyncParentMessageData>&& aMessages);

  mozilla::ipc::IPCResult RecvDidComposite(
      InfallibleTArray<ImageCompositeNotification>&& aNotifications);

  mozilla::ipc::IPCResult RecvReportFramesDropped(
      const CompositableHandle& aHandle, const uint32_t& aFrames);

  // Create an ImageClient from any thread.
  RefPtr<ImageClient> CreateImageClient(CompositableType aType,
                                        ImageContainer* aImageContainer);

  // Create an ImageClient from the ImageBridge thread.
  RefPtr<ImageClient> CreateImageClientNow(CompositableType aType,
                                           ImageContainer* aImageContainer);

  already_AddRefed<CanvasClient> CreateCanvasClient(
      CanvasClient::CanvasClientType aType, TextureFlags aFlag);
  void UpdateAsyncCanvasRenderer(AsyncCanvasRenderer* aClient);
  void UpdateImageClient(RefPtr<ImageContainer> aContainer);

  /**
   * Flush all Images sent to CompositableHost.
   */
  void FlushAllImages(ImageClient* aClient, ImageContainer* aContainer);

  bool IPCOpen() const override { return mCanSend; }

 private:
  /**
   * This must be called by the static function DeleteImageBridgeSync defined
   * in ImageBridgeChild.cpp ONLY.
   */
  virtual ~ImageBridgeChild();

  // Helpers for dispatching.
  already_AddRefed<CanvasClient> CreateCanvasClientNow(
      CanvasClient::CanvasClientType aType, TextureFlags aFlags);
  void CreateCanvasClientSync(SynchronousTask* aTask,
                              CanvasClient::CanvasClientType aType,
                              TextureFlags aFlags,
                              RefPtr<CanvasClient>* const outResult);

  void CreateImageClientSync(SynchronousTask* aTask,
                             RefPtr<ImageClient>* result,
                             CompositableType aType,
                             ImageContainer* aImageContainer);

  void UpdateAsyncCanvasRendererNow(AsyncCanvasRenderer* aClient);
  void UpdateAsyncCanvasRendererSync(SynchronousTask* aTask,
                                     AsyncCanvasRenderer* aWrapper);

  void FlushAllImagesSync(SynchronousTask* aTask, ImageClient* aClient,
                          ImageContainer* aContainer);

  void ProxyAllocShmemNow(SynchronousTask* aTask, AllocShmemParams* aParams);
  void ProxyDeallocShmemNow(SynchronousTask* aTask, Shmem* aShmem,
                            bool* aResult);

  void UpdateTextureFactoryIdentifier(
      const TextureFactoryIdentifier& aIdentifier);

 public:
  // CompositableForwarder

  void Connect(CompositableClient* aCompositable,
               ImageContainer* aImageContainer) override;

  bool UsesImageBridge() const override { return true; }

  /**
   * See CompositableForwarder::UseTextures
   */
  void UseTextures(CompositableClient* aCompositable,
                   const nsTArray<TimedTextureClient>& aTextures,
                   const Maybe<wr::RenderRoot>& aRenderRoot) override;
  void UseComponentAlphaTextures(CompositableClient* aCompositable,
                                 TextureClient* aClientOnBlack,
                                 TextureClient* aClientOnWhite) override;

  void ReleaseCompositable(const CompositableHandle& aHandle) override;

  void ForgetImageContainer(const CompositableHandle& aHandle);

  /**
   * Hold TextureClient ref until end of usage on host side if
   * TextureFlags::RECYCLE is set. Host side's usage is checked via
   * CompositableRef.
   */
  void HoldUntilCompositableRefReleasedIfNecessary(TextureClient* aClient);

  /**
   * Notify id of Texture When host side end its use. Transaction id is used to
   * make sure if there is no newer usage.
   */
  void NotifyNotUsed(uint64_t aTextureId, uint64_t aFwdTransactionId);

  void CancelWaitForRecycle(uint64_t aTextureId) override;

  bool DestroyInTransaction(PTextureChild* aTexture) override;
  bool DestroyInTransaction(const CompositableHandle& aHandle);

  void RemoveTextureFromCompositable(
      CompositableClient* aCompositable, TextureClient* aTexture,
      const Maybe<wr::RenderRoot>& aRenderRoot) override;

  void UseTiledLayerBuffer(
      CompositableClient* aCompositable,
      const SurfaceDescriptorTiles& aTileLayerDescriptor) override {
    MOZ_CRASH("should not be called");
  }

  void UpdateTextureRegion(CompositableClient* aCompositable,
                           const ThebesBufferData& aThebesBufferData,
                           const nsIntRegion& aUpdatedRegion) override {
    MOZ_CRASH("should not be called");
  }

  // ISurfaceAllocator

  /**
   * See ISurfaceAllocator.h
   * Can be used from any thread.
   * If used outside the ImageBridgeChild thread, it will proxy a synchronous
   * call on the ImageBridgeChild thread.
   */
  bool AllocUnsafeShmem(size_t aSize,
                        mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                        mozilla::ipc::Shmem* aShmem) override;
  bool AllocShmem(size_t aSize,
                  mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                  mozilla::ipc::Shmem* aShmem) override;

  /**
   * See ISurfaceAllocator.h
   * Can be used from any thread.
   * If used outside the ImageBridgeChild thread, it will proxy a synchronous
   * call on the ImageBridgeChild thread.
   */
  bool DeallocShmem(mozilla::ipc::Shmem& aShmem) override;

  PTextureChild* CreateTexture(const SurfaceDescriptor& aSharedData,
                               const ReadLockDescriptor& aReadLock,
                               LayersBackend aLayersBackend,
                               TextureFlags aFlags, uint64_t aSerial,
                               wr::MaybeExternalImageId& aExternalImageId,
                               nsIEventTarget* aTarget = nullptr) override;

  bool IsSameProcess() const override;

  void UpdateFwdTransactionId() override { ++mFwdTransactionId; }
  uint64_t GetFwdTransactionId() override { return mFwdTransactionId; }

  bool InForwarderThread() override { return InImageBridgeChildThread(); }

  void HandleFatalError(const char* aMsg) const override;

  wr::MaybeExternalImageId GetNextExternalImageId() override;

 protected:
  explicit ImageBridgeChild(uint32_t aNamespace);
  bool DispatchAllocShmemInternal(size_t aSize,
                                  SharedMemory::SharedMemoryType aType,
                                  Shmem* aShmem, bool aUnsafe);

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
  bool CanPostTask() const;

  static void ShutdownSingleton();

 private:
  uint32_t mNamespace;

  CompositableTransaction* mTxn;

  bool mCanSend;
  mozilla::Atomic<bool> mDestroyed;

  /**
   * Transaction id of CompositableForwarder.
   * It is incrementaed by UpdateFwdTransactionId() in each BeginTransaction()
   * call.
   */
  uint64_t mFwdTransactionId;

  /**
   * Hold TextureClients refs until end of their usages on host side.
   * It defer calling of TextureClient recycle callback.
   */
  std::unordered_map<uint64_t, RefPtr<TextureClient>> mTexturesWaitingRecycled;

  /**
   * Mapping from async compositable IDs to image containers.
   */
  Mutex mContainerMapLock;
  std::unordered_map<uint64_t, RefPtr<ImageContainerListener>>
      mImageContainerListeners;
  RefPtr<ImageContainerListener> FindListener(
      const CompositableHandle& aHandle);

#if defined(XP_WIN)
  /**
   * Used for checking if D3D11Device is updated.
   */
  RefPtr<ID3D11Device> mImageDevice;
#endif
};

}  // namespace layers
}  // namespace mozilla

#endif
