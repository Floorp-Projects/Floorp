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
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/PImageBridgeParent.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
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
                                public CompositableParentManager
{
public:
  typedef InfallibleTArray<CompositableOperation> EditArray;
  typedef InfallibleTArray<EditReply> EditReplyArray;
  typedef InfallibleTArray<AsyncChildMessageData> AsyncChildMessageArray;

  ImageBridgeParent(MessageLoop* aLoop, Transport* aTransport, ProcessId aChildProcessId);
  ~ImageBridgeParent();

  virtual LayersBackend GetCompositorBackendType() const override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  static PImageBridgeParent*
  Create(Transport* aTransport, ProcessId aChildProcessId);

  // CompositableParentManager
  virtual void SendFenceHandleIfPresent(PTextureParent* aTexture,
                                        CompositableHost* aCompositableHost) override;

  virtual void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) override;

  virtual base::ProcessId GetChildProcessId() override
  {
    return OtherPid();
  }

  // PImageBridge
  virtual bool RecvImageBridgeThreadId(const PlatformThreadId& aThreadId) override;
  virtual bool RecvUpdate(EditArray&& aEdits, EditReplyArray* aReply) override;
  virtual bool RecvUpdateNoSwap(EditArray&& aEdits) override;

  virtual bool IsAsync() const override { return true; }

  PCompositableParent* AllocPCompositableParent(const TextureInfo& aInfo,
                                                PImageContainerParent* aImageContainer,
                                                uint64_t*) override;
  bool DeallocPCompositableParent(PCompositableParent* aActor) override;

  virtual PTextureParent* AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                              const TextureFlags& aFlags) override;
  virtual bool DeallocPTextureParent(PTextureParent* actor) override;

  PMediaSystemResourceManagerParent* AllocPMediaSystemResourceManagerParent() override;
  bool DeallocPMediaSystemResourceManagerParent(PMediaSystemResourceManagerParent* aActor) override;
  virtual PImageContainerParent* AllocPImageContainerParent() override;
  virtual bool DeallocPImageContainerParent(PImageContainerParent* actor) override;

  virtual bool
  RecvChildAsyncMessages(InfallibleTArray<AsyncChildMessageData>&& aMessages) override;

  // Shutdown step 1
  virtual bool RecvWillStop() override;
  // Shutdown step 2
  virtual bool RecvStop() override;

  virtual MessageLoop* GetMessageLoop() const override;


  // ISurfaceAllocator

  bool AllocShmem(size_t aSize,
                  ipc::SharedMemory::SharedMemoryType aType,
                  ipc::Shmem* aShmem) override
  {
    return PImageBridgeParent::AllocShmem(aSize, aType, aShmem);
  }

  bool AllocUnsafeShmem(size_t aSize,
                        ipc::SharedMemory::SharedMemoryType aType,
                        ipc::Shmem* aShmem) override
  {
    return PImageBridgeParent::AllocUnsafeShmem(aSize, aType, aShmem);
  }

  void DeallocShmem(ipc::Shmem& aShmem) override
  {
    PImageBridgeParent::DeallocShmem(aShmem);
  }

  virtual bool IsSameProcess() const override;

  virtual void ReplyRemoveTexture(const OpReplyRemoveTexture& aReply) override;

  static void ReplyRemoveTexture(base::ProcessId aChildProcessId,
                                 const OpReplyRemoveTexture& aReply);

  void AppendDeliverFenceMessage(uint64_t aDestHolderId,
                                 uint64_t aTransactionId,
                                 PTextureParent* aTexture,
                                 CompositableHost* aCompositableHost);

  static void AppendDeliverFenceMessage(base::ProcessId aChildProcessId,
                                        uint64_t aDestHolderId,
                                        uint64_t aTransactionId,
                                        PTextureParent* aTexture,
                                        CompositableHost* aCompositableHost);

  using CompositableParentManager::SendPendingAsyncMessages;
  static void SendPendingAsyncMessages(base::ProcessId aChildProcessId);

  static ImageBridgeParent* GetInstance(ProcessId aId);

  static bool NotifyImageComposites(nsTArray<ImageCompositeNotification>& aNotifications);

  // Overriden from IToplevelProtocol
  IToplevelProtocol*
  CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                base::ProcessHandle aPeerProcess,
                mozilla::ipc::ProtocolCloneContext* aCtx) override;

private:
  void DeferredDestroy();
  MessageLoop* mMessageLoop;
  Transport* mTransport;
  // This keeps us alive until ActorDestroy(), at which point we do a
  // deferred destruction of ourselves.
  nsRefPtr<ImageBridgeParent> mSelfRef;

  bool mSetChildThreadPriority;

  /**
   * Map of all living ImageBridgeParent instances
   */
  static std::map<base::ProcessId, ImageBridgeParent*> sImageBridges;

  static MessageLoop* sMainLoop;

  nsRefPtr<CompositorThreadHolder> mCompositorThreadHolder;
};

} // namespace layers
} // namespace mozilla

#endif // gfx_layers_ipc_ImageBridgeParent_h_
