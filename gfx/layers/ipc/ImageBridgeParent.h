/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfx_layers_ipc_ImageBridgeParent_h_
#define gfx_layers_ipc_ImageBridgeParent_h_

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include "CompositableTransactionParent.h"
#include "ImageContainerParent.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for override
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/PImageBridgeParent.h"
#include "nsISupportsImpl.h"
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray

class MessageLoop;

namespace base {
class Thread;
} // namespace base

namespace mozilla {
namespace ipc {
class Shmem;
} // namespace ipc

namespace layers {

/**
 * ImageBridgeParent is the manager Protocol of ImageContainerParent.
 * It's purpose is mainly to setup the IPDL connection. Most of the
 * interesting stuff is in ImageContainerParent.
 */
class ImageBridgeParent final : public PImageBridgeParent,
                                public CompositableParentManager,
                                public ShmemAllocator
{
public:
  typedef InfallibleTArray<CompositableOperation> EditArray;
  typedef InfallibleTArray<OpDestroy> OpDestroyArray;
  typedef InfallibleTArray<EditReply> EditReplyArray;

protected:
  ImageBridgeParent(MessageLoop* aLoop, ProcessId aChildProcessId);

public:
  ~ImageBridgeParent();

  static ImageBridgeParent* CreateSameProcess();

  static bool CreateForContent(Endpoint<PImageBridgeParent>&& aEndpoint);

  virtual ShmemAllocator* AsShmemAllocator() override { return this; }

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  // CompositableParentManager
  virtual void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) override;

  virtual void NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId) override;

  virtual base::ProcessId GetChildProcessId() override
  {
    return OtherPid();
  }

  // PImageBridge
  virtual bool RecvImageBridgeThreadId(const PlatformThreadId& aThreadId) override;
  virtual bool RecvUpdate(EditArray&& aEdits, OpDestroyArray&& aToDestroy,
                          const uint64_t& aFwdTransactionId,
                          EditReplyArray* aReply) override;
  virtual bool RecvUpdateNoSwap(EditArray&& aEdits, OpDestroyArray&& aToDestroy,
                                const uint64_t& aFwdTransactionId) override;

  PCompositableParent* AllocPCompositableParent(const TextureInfo& aInfo,
                                                PImageContainerParent* aImageContainer,
                                                uint64_t*) override;
  bool DeallocPCompositableParent(PCompositableParent* aActor) override;

  virtual PTextureParent* AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                              const LayersBackend& aLayersBackend,
                                              const TextureFlags& aFlags,
                                              const uint64_t& aSerial) override;
  virtual bool DeallocPTextureParent(PTextureParent* actor) override;

  PMediaSystemResourceManagerParent* AllocPMediaSystemResourceManagerParent() override;
  bool DeallocPMediaSystemResourceManagerParent(PMediaSystemResourceManagerParent* aActor) override;
  virtual PImageContainerParent* AllocPImageContainerParent() override;
  virtual bool DeallocPImageContainerParent(PImageContainerParent* actor) override;

  // Shutdown step 1
  virtual bool RecvWillClose() override;

  MessageLoop* GetMessageLoop() const { return mMessageLoop; }

  // ShmemAllocator

  virtual bool AllocShmem(size_t aSize,
                          ipc::SharedMemory::SharedMemoryType aType,
                          ipc::Shmem* aShmem) override;

  virtual bool AllocUnsafeShmem(size_t aSize,
                                ipc::SharedMemory::SharedMemoryType aType,
                                ipc::Shmem* aShmem) override;

  virtual void DeallocShmem(ipc::Shmem& aShmem) override;

  virtual bool IsSameProcess() const override;

  virtual void ReplyRemoveTexture(const OpReplyRemoveTexture& aReply) override;

  void SendFenceHandleToNonRecycle(PTextureParent* aTexture);

  void NotifyNotUsedToNonRecycle(PTextureParent* aTexture,
                                 uint64_t aTransactionId);

  static void NotifyNotUsedToNonRecycle(base::ProcessId aChildProcessId,
                                        PTextureParent* aTexture,
                                        uint64_t aTransactionId);

  using CompositableParentManager::SetAboutToSendAsyncMessages;
  static void SetAboutToSendAsyncMessages(base::ProcessId aChildProcessId);

  using CompositableParentManager::SendPendingAsyncMessages;
  static void SendPendingAsyncMessages(base::ProcessId aChildProcessId);

  static ImageBridgeParent* GetInstance(ProcessId aId);

  static bool NotifyImageComposites(nsTArray<ImageCompositeNotification>& aNotifications);

  // Overriden from IToplevelProtocol
  IToplevelProtocol*
  CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                base::ProcessHandle aPeerProcess,
                mozilla::ipc::ProtocolCloneContext* aCtx) override;

  virtual bool UsesImageBridge() const override { return true; }

  virtual bool IPCOpen() const override { return !mClosed; }

protected:
  void OnChannelConnected(int32_t pid) override;

  void Bind(Endpoint<PImageBridgeParent>&& aEndpoint);

private:
  void DeferredDestroy();
  MessageLoop* mMessageLoop;
  // This keeps us alive until ActorDestroy(), at which point we do a
  // deferred destruction of ourselves.
  RefPtr<ImageBridgeParent> mSelfRef;

  bool mSetChildThreadPriority;
  bool mClosed;

  /**
   * Map of all living ImageBridgeParent instances
   */
  static std::map<base::ProcessId, ImageBridgeParent*> sImageBridges;

  static MessageLoop* sMainLoop;

  RefPtr<CompositorThreadHolder> mCompositorThreadHolder;
};

} // namespace layers
} // namespace mozilla

#endif // gfx_layers_ipc_ImageBridgeParent_h_
