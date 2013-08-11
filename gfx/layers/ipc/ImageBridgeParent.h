/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include "CompositableTransactionParent.h"
#include "gfxPoint.h"                   // for gfxIntSize
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/PImageBridgeParent.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray

class MessageLoop;

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
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageBridgeParent)

public:
  typedef InfallibleTArray<CompositableOperation> EditArray;
  typedef InfallibleTArray<EditReply> EditReplyArray;

  ImageBridgeParent(MessageLoop* aLoop, Transport* aTransport);
  ~ImageBridgeParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  static PImageBridgeParent*
  Create(Transport* aTransport, ProcessId aOtherProcess);

  virtual PGrallocBufferParent*
  AllocPGrallocBufferParent(const gfxIntSize&, const uint32_t&, const uint32_t&,
                            MaybeMagicGrallocBufferHandle*) MOZ_OVERRIDE;

  virtual bool
  DeallocPGrallocBufferParent(PGrallocBufferParent* actor) MOZ_OVERRIDE;

  // PImageBridge
  virtual bool RecvUpdate(const EditArray& aEdits, EditReplyArray* aReply);
  virtual bool RecvUpdateNoSwap(const EditArray& aEdits);

  PCompositableParent* AllocPCompositableParent(const TextureInfo& aInfo,
                                                uint64_t*) MOZ_OVERRIDE;
  bool DeallocPCompositableParent(PCompositableParent* aActor) MOZ_OVERRIDE;

  bool RecvStop() MOZ_OVERRIDE;

  MessageLoop * GetMessageLoop();


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

private:
  void DeferredDestroy();

  MessageLoop* mMessageLoop;
  Transport* mTransport;
  // This keeps us alive until ActorDestroy(), at which point we do a
  // deferred destruction of ourselves.
  nsRefPtr<ImageBridgeParent> mSelfRef;
};

} // layers
} // mozilla

