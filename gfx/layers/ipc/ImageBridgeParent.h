/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfx_layers_ipc_ImageBridgeParent_h_
#define gfx_layers_ipc_ImageBridgeParent_h_

#include <stddef.h>  // for size_t
#include <stdint.h>  // for uint32_t, uint64_t
#include "CompositableTransactionParent.h"
#include "mozilla/Assertions.h"  // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"  // for override
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/SharedMemory.h"  // for SharedMemory, etc
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/PImageBridgeParent.h"
#include "nsISupportsImpl.h"
#include "nsTArrayForwardDeclare.h"  // for nsTArray

namespace mozilla {
namespace ipc {
class Shmem;
}  // namespace ipc

namespace layers {

struct ImageCompositeNotificationInfo;
class RemoteTextureTxnScheduler;

/**
 * ImageBridgeParent is the manager Protocol of async Compositables.
 */
class ImageBridgeParent final : public PImageBridgeParent,
                                public CompositableParentManager,
                                public mozilla::ipc::IShmemAllocator {
 public:
  typedef nsTArray<CompositableOperation> EditArray;
  typedef nsTArray<OpDestroy> OpDestroyArray;

 protected:
  ImageBridgeParent(nsISerialEventTarget* aThread, ProcessId aChildProcessId,
                    dom::ContentParentId aContentId);

 public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef() override {
    return ISurfaceAllocator::AddRef();
  }
  NS_IMETHOD_(MozExternalRefCountType) Release() override {
    return ISurfaceAllocator::Release();
  }

  /**
   * Creates the globals of ImageBridgeParent.
   */
  static void Setup();

  static ImageBridgeParent* CreateSameProcess();
  static bool CreateForGPUProcess(Endpoint<PImageBridgeParent>&& aEndpoint);
  static bool CreateForContent(Endpoint<PImageBridgeParent>&& aEndpoint,
                               dom::ContentParentId aContentId);
  static void Shutdown();

  IShmemAllocator* AsShmemAllocator() override { return this; }

  void ActorDestroy(ActorDestroyReason aWhy) override;

  // CompositableParentManager
  void SendAsyncMessage(
      const nsTArray<AsyncParentMessageData>& aMessage) override;

  void NotifyNotUsed(PTextureParent* aTexture,
                     uint64_t aTransactionId) override;

  base::ProcessId GetChildProcessId() override { return OtherPid(); }
  dom::ContentParentId GetContentId() override { return mContentId; }

  // PImageBridge
  mozilla::ipc::IPCResult RecvUpdate(EditArray&& aEdits,
                                     OpDestroyArray&& aToDestroy,
                                     const uint64_t& aFwdTransactionId);

  PTextureParent* AllocPTextureParent(
      const SurfaceDescriptor& aSharedData, ReadLockDescriptor& aReadLock,
      const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
      const uint64_t& aSerial,
      const wr::MaybeExternalImageId& aExternalImageId);
  bool DeallocPTextureParent(PTextureParent* actor);

  mozilla::ipc::IPCResult RecvNewCompositable(const CompositableHandle& aHandle,
                                              const TextureInfo& aInfo);
  mozilla::ipc::IPCResult RecvReleaseCompositable(
      const CompositableHandle& aHandle);

  PMediaSystemResourceManagerParent* AllocPMediaSystemResourceManagerParent();
  bool DeallocPMediaSystemResourceManagerParent(
      PMediaSystemResourceManagerParent* aActor);

  // Shutdown step 1
  mozilla::ipc::IPCResult RecvWillClose();

  nsISerialEventTarget* GetThread() const { return mThread; }

  // IShmemAllocator

  bool AllocShmem(size_t aSize, ipc::Shmem* aShmem) override;

  bool AllocUnsafeShmem(size_t aSize, ipc::Shmem* aShmem) override;

  bool DeallocShmem(ipc::Shmem& aShmem) override;

  bool IsSameProcess() const override;

  static already_AddRefed<ImageBridgeParent> GetInstance(ProcessId aId);

  static bool NotifyImageComposites(
      nsTArray<ImageCompositeNotificationInfo>& aNotifications);

  bool UsesImageBridge() const override { return true; }

  bool IPCOpen() const override { return !mClosed; }

 protected:
  void Bind(Endpoint<PImageBridgeParent>&& aEndpoint);

 private:
  virtual ~ImageBridgeParent();

  static void ShutdownInternal();

  void DeferredDestroy();
  nsCOMPtr<nsISerialEventTarget> mThread;

  dom::ContentParentId mContentId;

  bool mClosed;

  /**
   * Map of all living ImageBridgeParent instances
   */
  typedef std::map<base::ProcessId, ImageBridgeParent*> ImageBridgeMap;
  static ImageBridgeMap sImageBridges;

  RefPtr<CompositorThreadHolder> mCompositorThreadHolder;

  RefPtr<RemoteTextureTxnScheduler> mRemoteTextureTxnScheduler;
};

}  // namespace layers
}  // namespace mozilla

#endif  // gfx_layers_ipc_ImageBridgeParent_h_
