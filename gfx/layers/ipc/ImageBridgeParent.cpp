/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/thread.h"

#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "CompositableHost.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"

using namespace base;
using namespace mozilla::ipc;

namespace mozilla {
namespace layers {


ImageBridgeParent::ImageBridgeParent(MessageLoop* aLoop, Transport* aTransport)
  : mMessageLoop(aLoop)
  , mTransport(aTransport)
{
  // creates the map only if it has not been created already, so it is safe
  // with several bridges
  CompositableMap::Create();
}

ImageBridgeParent::~ImageBridgeParent()
{
  if (mTransport) {
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(mTransport));
  }
}

void
ImageBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &ImageBridgeParent::DeferredDestroy));
}

bool
ImageBridgeParent::RecvUpdate(const EditArray& aEdits, EditReplyArray* aReply)
{
  EditReplyVector replyv;
  for (EditArray::index_type i = 0; i < aEdits.Length(); ++i) {
    ReceiveCompositableUpdate(aEdits[i], replyv);
  }

  aReply->SetCapacity(replyv.size());
  if (replyv.size() > 0) {
    aReply->AppendElements(&replyv.front(), replyv.size());
  }

  // Ensure that any pending operations involving back and front
  // buffers have completed, so that neither process stomps on the
  // other's buffer contents.
  ShadowLayerManager::PlatformSyncBeforeReplyUpdate();

  return true;
}

bool
ImageBridgeParent::RecvUpdateNoSwap(const EditArray& aEdits)
{
  InfallibleTArray<EditReply> noReplies;
  bool success = RecvUpdate(aEdits, &noReplies);
  NS_ABORT_IF_FALSE(noReplies.Length() == 0, "RecvUpdateNoSwap requires a sync Update to carry Edits");
  return success;
}


static void
ConnectImageBridgeInParentProcess(ImageBridgeParent* aBridge,
                                  Transport* aTransport,
                                  ProcessHandle aOtherProcess)
{
  aBridge->Open(aTransport, aOtherProcess,
                XRE_GetIOMessageLoop(), AsyncChannel::Parent);
}

/*static*/ PImageBridgeParent*
ImageBridgeParent::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  ProcessHandle processHandle;
  if (!base::OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

  MessageLoop* loop = CompositorParent::CompositorLoop();
  nsRefPtr<ImageBridgeParent> bridge = new ImageBridgeParent(loop, aTransport);
  bridge->mSelfRef = bridge;
  loop->PostTask(FROM_HERE,
                 NewRunnableFunction(ConnectImageBridgeInParentProcess,
                                     bridge.get(), aTransport, processHandle));
  return bridge.get();
}

bool ImageBridgeParent::RecvStop()
{
  return true;
}

static  uint64_t GenImageContainerID() {
  static uint64_t sNextImageID = 1;

  ++sNextImageID;
  return sNextImageID;
}

PGrallocBufferParent*
ImageBridgeParent::AllocPGrallocBuffer(const gfxIntSize& aSize,
                                       const uint32_t& aFormat,
                                       const uint32_t& aUsage,
                                       MaybeMagicGrallocBufferHandle* aOutHandle)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  return GrallocBufferActor::Create(aSize, aFormat, aUsage, aOutHandle);
#else
  NS_RUNTIMEABORT("No gralloc buffers for you");
  return nullptr;
#endif
}

bool
ImageBridgeParent::DeallocPGrallocBuffer(PGrallocBufferParent* actor)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  delete actor;
  return true;
#else
  NS_RUNTIMEABORT("Um, how did we get here?");
  return false;
#endif
}

PCompositableParent*
ImageBridgeParent::AllocPCompositable(const TextureInfo& aInfo,
                                      uint64_t* aID)
{
  uint64_t id = GenImageContainerID();
  *aID = id;
  return new CompositableParent(this, aInfo, id);
}

bool ImageBridgeParent::DeallocPCompositable(PCompositableParent* aActor)
{
  delete aActor;
  return true;
}



MessageLoop * ImageBridgeParent::GetMessageLoop() {
  return mMessageLoop;
}

void
ImageBridgeParent::DeferredDestroy()
{
  mSelfRef = nullptr;
  // |this| was just destroyed, hands off
}

} // layers
} // mozilla
