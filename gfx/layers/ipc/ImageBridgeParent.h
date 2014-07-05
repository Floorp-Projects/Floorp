/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfx_layers_ipc_ImageBridgeParent_h_
#define gfx_layers_ipc_ImageBridgeParent_h_

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include "CompositableTransactionParent.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/PImageBridgeParent.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray

class MessageLoop;

namespace base {
class Thread;
}

namespace mozilla {
namespace ipc {
class Shmem;
}

namespace layers {

/**
 * ImageBridgeParent is the manager Protocol of ImageContainerParent.
 * It's purpose is mainly to setup the IPDL connection. Most of the
 * interesting stuff is in ImageContainerParent.
 */
class ImageBridgeParent : public PImageBridgeParent,
                          public CompositableParentManager
{
public:
  typedef InfallibleTArray<CompositableOperation> EditArray;
  typedef InfallibleTArray<EditReply> EditReplyArray;
  typedef InfallibleTArray<AsyncChildMessageData> AsyncChildMessageArray;

  ImageBridgeParent(MessageLoop* aLoop, Transport* aTransport, ProcessId aChildProcessId);
  ~ImageBridgeParent();

  virtual LayersBackend GetCompositorBackendType() const MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  static PImageBridgeParent*
  Create(Transport* aTransport, ProcessId aChildProcessId);

  // CompositableParentManager
  virtual void SendFenceHandle(AsyncTransactionTracker* aTracker,
                               PTextureParent* aTexture,
                               const FenceHandle& aFence) MOZ_OVERRIDE;

  virtual void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) MOZ_OVERRIDE;

  virtual base::ProcessId GetChildProcessId() MOZ_OVERRIDE
  {
    return mChildProcessId;
  }

  // PImageBridge
  virtual bool RecvUpdate(const EditArray& aEdits, EditReplyArray* aReply) MOZ_OVERRIDE;
  virtual bool RecvUpdateNoSwap(const EditArray& aEdits) MOZ_OVERRIDE;

  virtual bool IsAsync() const MOZ_OVERRIDE { return true; }

  PCompositableParent* AllocPCompositableParent(const TextureInfo& aInfo,
                                                uint64_t*) MOZ_OVERRIDE;
  bool DeallocPCompositableParent(PCompositableParent* aActor) MOZ_OVERRIDE;

  virtual PTextureParent* AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                              const TextureFlags& aFlags) MOZ_OVERRIDE;
  virtual bool DeallocPTextureParent(PTextureParent* actor) MOZ_OVERRIDE;

  virtual bool
  RecvChildAsyncMessages(const InfallibleTArray<AsyncChildMessageData>& aMessages) MOZ_OVERRIDE;

  // Shutdown step 1
  virtual bool RecvWillStop() MOZ_OVERRIDE;
  // Shutdown step 2
  virtual bool RecvStop() MOZ_OVERRIDE;

  virtual MessageLoop* GetMessageLoop() const MOZ_OVERRIDE;


  // ISurfaceAllocator

  bool AllocShmem(size_t aSize,
                  ipc::SharedMemory::SharedMemoryType aType,
                  ipc::Shmem* aShmem) MOZ_OVERRIDE
  {
    return AllocShmem(aSize, aType, aShmem);
  }

  bool AllocUnsafeShmem(size_t aSize,
                        ipc::SharedMemory::SharedMemoryType aType,
                        ipc::Shmem* aShmem) MOZ_OVERRIDE
  {
    return AllocUnsafeShmem(aSize, aType, aShmem);
  }

  void DeallocShmem(ipc::Shmem& aShmem) MOZ_OVERRIDE
  {
    PImageBridgeParent::DeallocShmem(aShmem);
  }

  virtual bool IsSameProcess() const MOZ_OVERRIDE;

  virtual void ReplyRemoveTexture(const OpReplyRemoveTexture& aReply) MOZ_OVERRIDE;

  static void ReplyRemoveTexture(base::ProcessId aChildProcessId,
                                 const OpReplyRemoveTexture& aReply);

  void SendFenceHandleToTrackerIfPresent(uint64_t aDestHolderId,
                                         uint64_t aTransactionId,
                                         PTextureParent* aTexture);

  static void SendFenceHandleToTrackerIfPresent(base::ProcessId aChildProcessId,
                                                uint64_t aDestHolderId,
                                                uint64_t aTransactionId,
                                                PTextureParent* aTexture);

  static ImageBridgeParent* GetInstance(ProcessId aId);

  // Overriden from IToplevelProtocol
  IToplevelProtocol*
  CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                base::ProcessHandle aPeerProcess,
                mozilla::ipc::ProtocolCloneContext* aCtx) MOZ_OVERRIDE;

private:
  void DeferredDestroy();

  MessageLoop* mMessageLoop;
  Transport* mTransport;
  // Child side's process id.
  base::ProcessId mChildProcessId;
  // This keeps us alive until ActorDestroy(), at which point we do a
  // deferred destruction of ourselves.
  nsRefPtr<ImageBridgeParent> mSelfRef;

  /**
   * Map of all living ImageBridgeParent instances
   */
  static std::map<base::ProcessId, ImageBridgeParent*> sImageBridges;
};

} // layers
} // mozilla

#endif // gfx_layers_ipc_ImageBridgeParent_h_
