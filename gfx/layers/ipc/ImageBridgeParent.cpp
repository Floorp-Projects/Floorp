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
#include "mozilla/gfx/Point.h"                   // for IntSize
#include "mozilla/Hal.h"                // for hal::SetCurrentThreadPriority()
#include "mozilla/HalTypes.h"           // for hal::THREAD_PRIORITY_COMPOSITOR
#include "mozilla/ipc/MessageChannel.h" // for MessageChannel, etc
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Transport.h"      // for Transport
#include "mozilla/media/MediaSystemResourceManagerParent.h" // for MediaSystemResourceManagerParent
#include "mozilla/layers/CompositableTransactionParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply
#include "mozilla/layers/LayersSurfaces.h"  // for PGrallocBufferParent
#include "mozilla/layers/PCompositableParent.h"
#include "mozilla/layers/PImageBridgeParent.h"
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/layers/Compositor.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "mozilla/Unused.h"
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

// defined in CompositorBridgeParent.cpp
CompositorThreadHolder* GetCompositorThreadHolder();

ImageBridgeParent::ImageBridgeParent(MessageLoop* aLoop,
                                     ProcessId aChildProcessId)
  : mMessageLoop(aLoop)
  , mSetChildThreadPriority(false)
  , mClosed(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  sMainLoop = MessageLoop::current();

  // creates the map only if it has not been created already, so it is safe
  // with several bridges
  CompositableMap::Create();
  sImageBridges[aChildProcessId] = this;
  SetOtherProcessId(aChildProcessId);
}

ImageBridgeParent::~ImageBridgeParent()
{
  nsTArray<PImageContainerParent*> parents;
  ManagedPImageContainerParent(parents);
  for (PImageContainerParent* p : parents) {
    delete p;
  }

  sImageBridges.erase(OtherPid());
}

static StaticRefPtr<ImageBridgeParent> sImageBridgeParentSingleton;

void ReleaseImageBridgeParentSingleton() {
  sImageBridgeParentSingleton = nullptr;
}

/* static */ ImageBridgeParent*
ImageBridgeParent::CreateSameProcess()
{
  RefPtr<ImageBridgeParent> parent =
    new ImageBridgeParent(CompositorThreadHolder::Loop(), base::GetCurrentProcId());
  parent->mSelfRef = parent;

  sImageBridgeParentSingleton = parent;
  return parent;
}

/* static */ bool
ImageBridgeParent::CreateForGPUProcess(Endpoint<PImageBridgeParent>&& aEndpoint)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);

  MessageLoop* loop = CompositorThreadHolder::Loop();
  RefPtr<ImageBridgeParent> parent = new ImageBridgeParent(loop, aEndpoint.OtherPid());

  loop->PostTask(NewRunnableMethod<Endpoint<PImageBridgeParent>&&>(
    parent, &ImageBridgeParent::Bind, Move(aEndpoint)));

  sImageBridgeParentSingleton = parent;
  return true;
}

void
ImageBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Can't alloc/dealloc shmems from now on.
  mClosed = true;

  MessageLoop::current()->PostTask(NewRunnableMethod(this, &ImageBridgeParent::DeferredDestroy));

  // It is very important that this method gets called at shutdown (be it a clean
  // or an abnormal shutdown), because DeferredDestroy is what clears mSelfRef.
  // If mSelfRef is not null and ActorDestroy is not called, the ImageBridgeParent
  // is leaked which causes the CompositorThreadHolder to be leaked and
  // CompsoitorParent's shutdown ends up spinning the event loop forever, waiting
  // for the compositor thread to terminate.
}

mozilla::ipc::IPCResult
ImageBridgeParent::RecvImageBridgeThreadId(const PlatformThreadId& aThreadId)
{
  MOZ_ASSERT(!mSetChildThreadPriority);
  if (mSetChildThreadPriority) {
    return IPC_FAIL_NO_REASON(this);
  }
  mSetChildThreadPriority = true;
  return IPC_OK();
}

class MOZ_STACK_CLASS AutoImageBridgeParentAsyncMessageSender
{
public:
  explicit AutoImageBridgeParentAsyncMessageSender(ImageBridgeParent* aImageBridge,
                                                   InfallibleTArray<OpDestroy>* aToDestroy = nullptr)
    : mImageBridge(aImageBridge)
    , mToDestroy(aToDestroy)
  {
    mImageBridge->SetAboutToSendAsyncMessages();
  }

  ~AutoImageBridgeParentAsyncMessageSender()
  {
    mImageBridge->SendPendingAsyncMessages();
    if (mToDestroy) {
      for (const auto& op : *mToDestroy) {
        mImageBridge->DestroyActor(op);
      }
    }
  }
private:
  ImageBridgeParent* mImageBridge;
  InfallibleTArray<OpDestroy>* mToDestroy;
};

mozilla::ipc::IPCResult
ImageBridgeParent::RecvUpdate(EditArray&& aEdits, OpDestroyArray&& aToDestroy,
                              const uint64_t& aFwdTransactionId,
                              EditReplyArray* aReply)
{
  // This ensures that destroy operations are always processed. It is not safe
  // to early-return from RecvUpdate without doing so.
  AutoImageBridgeParentAsyncMessageSender autoAsyncMessageSender(this, &aToDestroy);
  UpdateFwdTransactionId(aFwdTransactionId);

  EditReplyVector replyv;
  for (EditArray::index_type i = 0; i < aEdits.Length(); ++i) {
    if (!ReceiveCompositableUpdate(aEdits[i], replyv)) {
      return IPC_FAIL_NO_REASON(this);
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

  return IPC_OK();
}

mozilla::ipc::IPCResult
ImageBridgeParent::RecvUpdateNoSwap(EditArray&& aEdits, OpDestroyArray&& aToDestroy,
                                    const uint64_t& aFwdTransactionId)
{
  InfallibleTArray<EditReply> noReplies;
  bool success = RecvUpdate(Move(aEdits), Move(aToDestroy), aFwdTransactionId, &noReplies);
  MOZ_ASSERT(noReplies.Length() == 0, "RecvUpdateNoSwap requires a sync Update to carry Edits");
  if (!success) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

/* static */ bool
ImageBridgeParent::CreateForContent(Endpoint<PImageBridgeParent>&& aEndpoint)
{
  MessageLoop* loop = CompositorThreadHolder::Loop();

  RefPtr<ImageBridgeParent> bridge = new ImageBridgeParent(loop, aEndpoint.OtherPid());
  loop->PostTask(NewRunnableMethod<Endpoint<PImageBridgeParent>&&>(
    bridge, &ImageBridgeParent::Bind, Move(aEndpoint)));

  return true;
}

void
ImageBridgeParent::Bind(Endpoint<PImageBridgeParent>&& aEndpoint)
{
  if (!aEndpoint.Bind(this))
    return;
  mSelfRef = this;
}

mozilla::ipc::IPCResult ImageBridgeParent::RecvWillClose()
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
  return IPC_OK();
}

static  uint64_t GenImageContainerID() {
  static uint64_t sNextImageID = 1;

  ++sNextImageID;
  return sNextImageID;
}

PCompositableParent*
ImageBridgeParent::AllocPCompositableParent(const TextureInfo& aInfo,
                                            PImageContainerParent* aImageContainer,
                                            uint64_t* aID)
{
  uint64_t id = GenImageContainerID();
  *aID = id;
  return CompositableHost::CreateIPDLActor(this, aInfo, id, aImageContainer);
}

bool ImageBridgeParent::DeallocPCompositableParent(PCompositableParent* aActor)
{
  return CompositableHost::DestroyIPDLActor(aActor);
}

PTextureParent*
ImageBridgeParent::AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                       const LayersBackend& aLayersBackend,
                                       const TextureFlags& aFlags,
                                       const uint64_t& aSerial)
{
  return TextureHost::CreateIPDLActor(this, aSharedData, aLayersBackend, aFlags, aSerial);
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

PImageContainerParent*
ImageBridgeParent::AllocPImageContainerParent()
{
  return new ImageContainerParent();
}

bool
ImageBridgeParent::DeallocPImageContainerParent(PImageContainerParent* actor)
{
  delete actor;
  return true;
}

void
ImageBridgeParent::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  mozilla::Unused << SendParentAsyncMessages(aMessage);
}

class ProcessIdComparator
{
public:
  bool Equals(const ImageCompositeNotification& aA,
              const ImageCompositeNotification& aB) const
  {
    return aA.imageContainerParent()->OtherPid() == aB.imageContainerParent()->OtherPid();
  }
  bool LessThan(const ImageCompositeNotification& aA,
                const ImageCompositeNotification& aB) const
  {
    return aA.imageContainerParent()->OtherPid() < aB.imageContainerParent()->OtherPid();
  }
};

/* static */ bool
ImageBridgeParent::NotifyImageComposites(nsTArray<ImageCompositeNotification>& aNotifications)
{
  // Group the notifications by destination process ID and then send the
  // notifications in one message per group.
  aNotifications.Sort(ProcessIdComparator());
  uint32_t i = 0;
  bool ok = true;
  while (i < aNotifications.Length()) {
    AutoTArray<ImageCompositeNotification,1> notifications;
    notifications.AppendElement(aNotifications[i]);
    uint32_t end = i + 1;
    MOZ_ASSERT(aNotifications[i].imageContainerParent());
    ProcessId pid = aNotifications[i].imageContainerParent()->OtherPid();
    while (end < aNotifications.Length() &&
           aNotifications[end].imageContainerParent()->OtherPid() == pid) {
      notifications.AppendElement(aNotifications[end]);
      ++end;
    }
    GetInstance(pid)->SendPendingAsyncMessages();
    if (!GetInstance(pid)->SendDidComposite(notifications)) {
      ok = false;
    }
    i = end;
  }
  return ok;
}

void
ImageBridgeParent::DeferredDestroy()
{
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr; // "this" ImageBridge may get deleted here.
}

ImageBridgeParent*
ImageBridgeParent::GetInstance(ProcessId aId)
{
  NS_ASSERTION(sImageBridges.count(aId) == 1, "ImageBridgeParent for the process");
  return sImageBridges[aId];
}

void
ImageBridgeParent::OnChannelConnected(int32_t aPid)
{
  mCompositorThreadHolder = GetCompositorThreadHolder();
}


bool
ImageBridgeParent::AllocShmem(size_t aSize,
                      ipc::SharedMemory::SharedMemoryType aType,
                      ipc::Shmem* aShmem)
{
  if (mClosed) {
    return false;
  }
  return PImageBridgeParent::AllocShmem(aSize, aType, aShmem);
}

bool
ImageBridgeParent::AllocUnsafeShmem(size_t aSize,
                      ipc::SharedMemory::SharedMemoryType aType,
                      ipc::Shmem* aShmem)
{
  if (mClosed) {
    return false;
  }
  return PImageBridgeParent::AllocUnsafeShmem(aSize, aType, aShmem);
}

void
ImageBridgeParent::DeallocShmem(ipc::Shmem& aShmem)
{
  if (mClosed) {
    return;
  }
  PImageBridgeParent::DeallocShmem(aShmem);
}

bool ImageBridgeParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

/*static*/ void
ImageBridgeParent::SetAboutToSendAsyncMessages(base::ProcessId aChildProcessId)
{
  ImageBridgeParent* imageBridge = ImageBridgeParent::GetInstance(aChildProcessId);
  if (!imageBridge) {
    return;
  }
  imageBridge->SetAboutToSendAsyncMessages();
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

void
ImageBridgeParent::NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId)
{
  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    return;
  }

  if (!(texture->GetFlags() & TextureFlags::RECYCLE)) {
    return;
  }

  uint64_t textureId = TextureHost::GetTextureSerial(aTexture);
  mPendingAsyncMessage.push_back(
    OpNotifyNotUsed(textureId, aTransactionId));

  if (!IsAboutToSendAsyncMessages()) {
    SendPendingAsyncMessages();
  }
}

} // namespace layers
} // namespace mozilla
