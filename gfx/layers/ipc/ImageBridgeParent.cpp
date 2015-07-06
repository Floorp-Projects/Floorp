/* vim: set ts=2 sw=2 et tw=80: */
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBridgeParent.h"
#include <stdint.h>                     // for uint64_t, uint32_t
#include "CompositableHost.h"           // for CompositableParent, Create
#include "base/message_loop.h"          // for MessageLoop
#include "base/process.h"               // for ProcessId
#include "base/task.h"                  // for CancelableTask, DeleteTask, etc
#include "base/tracked.h"               // for FROM_HERE
#include "mozilla/gfx/Point.h"                   // for IntSize
#include "mozilla/Hal.h"                // for hal::SetCurrentThreadPriority()
#include "mozilla/HalTypes.h"           // for hal::THREAD_PRIORITY_COMPOSITOR
#include "mozilla/ipc/MessageChannel.h" // for MessageChannel, etc
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Transport.h"      // for Transport
#include "mozilla/media/MediaSystemResourceManagerParent.h" // for MediaSystemResourceManagerParent
#include "mozilla/layers/CompositableTransactionParent.h"
#include "mozilla/layers/CompositorParent.h"  // for CompositorParent
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply
#include "mozilla/layers/LayersSurfaces.h"  // for PGrallocBufferParent
#include "mozilla/layers/PCompositableParent.h"
#include "mozilla/layers/PImageBridgeParent.h"
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/layers/Compositor.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "mozilla/unused.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_RUNTIMEABORT, etc
#include "nsISupportsImpl.h"            // for ImageBridgeParent::Release, etc
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray
#include "nsXULAppAPI.h"                // for XRE_GetIOMessageLoop
#include "mozilla/layers/TextureHost.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace mozilla::media;

std::map<base::ProcessId, ImageBridgeParent*> ImageBridgeParent::sImageBridges;

MessageLoop* ImageBridgeParent::sMainLoop = nullptr;

// defined in CompositorParent.cpp
CompositorThreadHolder* GetCompositorThreadHolder();

ImageBridgeParent::ImageBridgeParent(MessageLoop* aLoop,
                                     Transport* aTransport,
                                     ProcessId aChildProcessId)
  : mMessageLoop(aLoop)
  , mTransport(aTransport)
  , mSetChildThreadPriority(false)
  , mCompositorThreadHolder(GetCompositorThreadHolder())
{
  MOZ_ASSERT(NS_IsMainThread());
  sMainLoop = MessageLoop::current();

  // top-level actors must be destroyed on the main thread.
  SetMessageLoopToPostDestructionTo(sMainLoop);

  // creates the map only if it has not been created already, so it is safe
  // with several bridges
  CompositableMap::Create();
  sImageBridges[aChildProcessId] = this;
  SetOtherProcessId(aChildProcessId);
}

ImageBridgeParent::~ImageBridgeParent()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransport) {
    MOZ_ASSERT(XRE_GetIOMessageLoop());
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(mTransport));
  }

  sImageBridges.erase(OtherPid());
}

LayersBackend
ImageBridgeParent::GetCompositorBackendType() const
{
  return Compositor::GetBackend();
}

void
ImageBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &ImageBridgeParent::DeferredDestroy));
}

bool
ImageBridgeParent::RecvImageBridgeThreadId(const PlatformThreadId& aThreadId)
{
  MOZ_ASSERT(!mSetChildThreadPriority);
  if (mSetChildThreadPriority) {
    return false;
  }
  mSetChildThreadPriority = true;
#ifdef MOZ_WIDGET_GONK
  hal::SetThreadPriority(aThreadId, hal::THREAD_PRIORITY_COMPOSITOR);
#endif
  return true;
}

class MOZ_STACK_CLASS AutoImageBridgeParentAsyncMessageSender
{
public:
  explicit AutoImageBridgeParentAsyncMessageSender(ImageBridgeParent* aImageBridge)
    : mImageBridge(aImageBridge) {}

  ~AutoImageBridgeParentAsyncMessageSender()
  {
    mImageBridge->SendPendingAsyncMessages();
  }
private:
  ImageBridgeParent* mImageBridge;
};

bool
ImageBridgeParent::RecvUpdate(EditArray&& aEdits, EditReplyArray* aReply)
{
  AutoImageBridgeParentAsyncMessageSender autoAsyncMessageSender(this);

  // If we don't actually have a compositor, then don't bother
  // creating any textures.
  if (Compositor::GetBackend() == LayersBackend::LAYERS_NONE) {
    return true;
  }

  EditReplyVector replyv;
  for (EditArray::index_type i = 0; i < aEdits.Length(); ++i) {
    if (!ReceiveCompositableUpdate(aEdits[i], replyv)) {
      return false;
    }
  }

  aReply->SetCapacity(replyv.size());
  if (replyv.size() > 0) {
    aReply->AppendElements(&replyv.front(), replyv.size());
  }

  if (!IsSameProcess()) {
    // Ensure that any pending operations involving back and front
    // buffers have completed, so that neither process stomps on the
    // other's buffer contents.
    LayerManagerComposite::PlatformSyncBeforeReplyUpdate();
  }

  return true;
}

bool
ImageBridgeParent::RecvUpdateNoSwap(EditArray&& aEdits)
{
  InfallibleTArray<EditReply> noReplies;
  bool success = RecvUpdate(Move(aEdits), &noReplies);
  MOZ_ASSERT(noReplies.Length() == 0, "RecvUpdateNoSwap requires a sync Update to carry Edits");
  return success;
}

static void
ConnectImageBridgeInParentProcess(ImageBridgeParent* aBridge,
                                  Transport* aTransport,
                                  base::ProcessId aOtherPid)
{
  aBridge->Open(aTransport, aOtherPid, XRE_GetIOMessageLoop(), ipc::ParentSide);
}

/*static*/ PImageBridgeParent*
ImageBridgeParent::Create(Transport* aTransport, ProcessId aChildProcessId)
{
  MessageLoop* loop = CompositorParent::CompositorLoop();
  nsRefPtr<ImageBridgeParent> bridge = new ImageBridgeParent(loop, aTransport, aChildProcessId);
  bridge->mSelfRef = bridge;
  loop->PostTask(FROM_HERE,
                 NewRunnableFunction(ConnectImageBridgeInParentProcess,
                                     bridge.get(), aTransport, aChildProcessId));
  return bridge.get();
}

bool ImageBridgeParent::RecvWillStop()
{
  // If there is any texture still alive we have to force it to deallocate the
  // device data (GL textures, etc.) now because shortly after SenStop() returns
  // on the child side the widget will be destroyed along with it's associated
  // GL context.
  InfallibleTArray<PTextureParent*> textures;
  ManagedPTextureParent(textures);
  for (unsigned int i = 0; i < textures.Length(); ++i) {
    RefPtr<TextureHost> tex = TextureHost::AsTextureHost(textures[i]);
    tex->DeallocateDeviceData();
  }
  return true;
}

static void
ReleaseImageBridgeParent(ImageBridgeParent* aImageBridgeParent)
{
  RELEASE_MANUALLY(aImageBridgeParent);
}

bool ImageBridgeParent::RecvStop()
{
  // This message just serves as synchronization between the
  // child and parent threads during shutdown.

  // There is one thing that we need to do here: temporarily addref, so that
  // the handling of this sync message can't race with the destruction of
  // the ImageBridgeParent, which would trigger the dreaded "mismatched CxxStackFrames"
  // assertion of MessageChannel.
  ADDREF_MANUALLY(this);
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableFunction(&ReleaseImageBridgeParent, this));
  return true;
}

static  uint64_t GenImageContainerID() {
  static uint64_t sNextImageID = 1;

  ++sNextImageID;
  return sNextImageID;
}

PCompositableParent*
ImageBridgeParent::AllocPCompositableParent(const TextureInfo& aInfo,
                                            uint64_t* aID)
{
  uint64_t id = GenImageContainerID();
  *aID = id;
  return CompositableHost::CreateIPDLActor(this, aInfo, id);
}

bool ImageBridgeParent::DeallocPCompositableParent(PCompositableParent* aActor)
{
  return CompositableHost::DestroyIPDLActor(aActor);
}

PTextureParent*
ImageBridgeParent::AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                       const TextureFlags& aFlags)
{
  return TextureHost::CreateIPDLActor(this, aSharedData, aFlags);
}

bool
ImageBridgeParent::DeallocPTextureParent(PTextureParent* actor)
{
  return TextureHost::DestroyIPDLActor(actor);
}

PMediaSystemResourceManagerParent*
ImageBridgeParent::AllocPMediaSystemResourceManagerParent()
{
  return new mozilla::media::MediaSystemResourceManagerParent();
}

bool
ImageBridgeParent::DeallocPMediaSystemResourceManagerParent(PMediaSystemResourceManagerParent* aActor)
{
  MOZ_ASSERT(aActor);
  delete static_cast<mozilla::media::MediaSystemResourceManagerParent*>(aActor);
  return true;
}

void
ImageBridgeParent::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  mozilla::unused << SendParentAsyncMessages(aMessage);
}

bool
ImageBridgeParent::RecvChildAsyncMessages(InfallibleTArray<AsyncChildMessageData>&& aMessages)
{
  return true;
}

MessageLoop * ImageBridgeParent::GetMessageLoop() const {
  return mMessageLoop;
}

void
ImageBridgeParent::DeferredDestroy()
{
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr;
}

ImageBridgeParent*
ImageBridgeParent::GetInstance(ProcessId aId)
{
  NS_ASSERTION(sImageBridges.count(aId) == 1, "ImageBridgeParent for the process");
  return sImageBridges[aId];
}

IToplevelProtocol*
ImageBridgeParent::CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                                 base::ProcessHandle aPeerProcess,
                                 mozilla::ipc::ProtocolCloneContext* aCtx)
{
  for (unsigned int i = 0; i < aFds.Length(); i++) {
    if (aFds[i].protocolId() == unsigned(GetProtocolId())) {
      Transport* transport = OpenDescriptor(aFds[i].fd(),
                                            Transport::MODE_SERVER);
      PImageBridgeParent* bridge = Create(transport, base::GetProcId(aPeerProcess));
      bridge->CloneManagees(this, aCtx);
      bridge->IToplevelProtocol::SetTransport(transport);
      return bridge;
    }
  }
  return nullptr;
}

bool ImageBridgeParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

void
ImageBridgeParent::ReplyRemoveTexture(const OpReplyRemoveTexture& aReply)
{
  mPendingAsyncMessage.push_back(aReply);
}

/*static*/ void
ImageBridgeParent::ReplyRemoveTexture(base::ProcessId aChildProcessId,
                                      const OpReplyRemoveTexture& aReply)
{
  ImageBridgeParent* imageBridge = ImageBridgeParent::GetInstance(aChildProcessId);
  if (!imageBridge) {
    return;
  }
  imageBridge->ReplyRemoveTexture(aReply);
}

void
ImageBridgeParent::SendFenceHandleIfPresent(PTextureParent* aTexture,
                                            CompositableHost* aCompositableHost)
{
  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    return;
  }

  // Send a ReleaseFence of CompositorOGL.
  if (aCompositableHost && aCompositableHost->GetCompositor()) {
    FenceHandle fence = aCompositableHost->GetCompositor()->GetReleaseFence();
    if (fence.IsValid()) {
      mPendingAsyncMessage.push_back(OpDeliverFence(aTexture, nullptr,
                                                    fence));
    }
  }

  // Send a ReleaseFence that is set by HwcComposer2D.
  FenceHandle fence = texture->GetAndResetReleaseFenceHandle();
  if (fence.IsValid()) {
    mPendingAsyncMessage.push_back(OpDeliverFence(aTexture, nullptr,
                                                  fence));
  }
}

void
ImageBridgeParent::AppendDeliverFenceMessage(uint64_t aDestHolderId,
                                             uint64_t aTransactionId,
                                             PTextureParent* aTexture,
                                             CompositableHost* aCompositableHost)
{
  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    return;
  }

  // Send a ReleaseFence of CompositorOGL.
  if (aCompositableHost && aCompositableHost->GetCompositor()) {
    FenceHandle fence = aCompositableHost->GetCompositor()->GetReleaseFence();
    if (fence.IsValid()) {
      mPendingAsyncMessage.push_back(OpDeliverFenceToTracker(aDestHolderId,
                                                             aTransactionId,
                                                             fence));
    }
  }

  // Send a ReleaseFence that is set by HwcComposer2D.
  FenceHandle fence = texture->GetAndResetReleaseFenceHandle();
  if (fence.IsValid()) {
    mPendingAsyncMessage.push_back(OpDeliverFenceToTracker(aDestHolderId,
                                                           aTransactionId,
                                                           fence));
  }
}

/*static*/ void
ImageBridgeParent::AppendDeliverFenceMessage(base::ProcessId aChildProcessId,
                                             uint64_t aDestHolderId,
                                             uint64_t aTransactionId,
                                             PTextureParent* aTexture,
                                             CompositableHost* aCompositableHost)
{
  ImageBridgeParent* imageBridge = ImageBridgeParent::GetInstance(aChildProcessId);
  if (!imageBridge) {
    return;
  }
  imageBridge->AppendDeliverFenceMessage(aDestHolderId,
                                         aTransactionId,
                                         aTexture,
                                         aCompositableHost);
}

/*static*/ void
ImageBridgeParent::SendPendingAsyncMessages(base::ProcessId aChildProcessId)
{
  ImageBridgeParent* imageBridge = ImageBridgeParent::GetInstance(aChildProcessId);
  if (!imageBridge) {
    return;
  }
  imageBridge->SendPendingAsyncMessages();
}

} // layers
} // mozilla
