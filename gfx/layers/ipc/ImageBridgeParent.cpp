/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBridgeParent.h"
#include <stdint.h>            // for uint64_t, uint32_t
#include "CompositableHost.h"  // for CompositableParent, Create
#include "base/process.h"      // for ProcessId
#include "base/task.h"         // for CancelableTask, DeleteTask, etc
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/gfx/Point.h"  // for IntSize
#include "mozilla/Hal.h"        // for hal::SetCurrentThreadPriority()
#include "mozilla/HalTypes.h"   // for hal::THREAD_PRIORITY_COMPOSITOR
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/MessageChannel.h"  // for MessageChannel, etc
#include "mozilla/ipc/Transport.h"       // for Transport
#include "mozilla/media/MediaSystemResourceManagerParent.h"  // for MediaSystemResourceManagerParent
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/CompositableTransactionParent.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply
#include "mozilla/layers/PImageBridgeParent.h"
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/layers/Compositor.h"
#include "mozilla/Monitor.h"
#include "mozilla/mozalloc.h"  // for operator new, etc
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/Unused.h"
#include "nsDebug.h"                 // for NS_ASSERTION, etc
#include "nsISupportsImpl.h"         // for ImageBridgeParent::Release, etc
#include "nsTArray.h"                // for nsTArray, nsTArray_Impl
#include "nsTArrayForwardDeclare.h"  // for nsTArray
#include "nsXULAppAPI.h"             // for XRE_GetIOMessageLoop
#include "mozilla/layers/TextureHost.h"
#include "nsThreadUtils.h"

#if defined(OS_WIN)
#  include "mozilla/layers/TextureD3D11.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/layers/AndroidHardwareBuffer.h"
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace mozilla::media;

ImageBridgeParent::ImageBridgeMap ImageBridgeParent::sImageBridges;

StaticAutoPtr<mozilla::Monitor> sImageBridgesLock;

static StaticRefPtr<ImageBridgeParent> sImageBridgeParentSingleton;

/* static */
void ImageBridgeParent::Setup() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sImageBridgesLock) {
    sImageBridgesLock = new Monitor("ImageBridges");
    mozilla::ClearOnShutdown(&sImageBridgesLock);
  }
}

ImageBridgeParent::ImageBridgeParent(nsISerialEventTarget* aThread,
                                     ProcessId aChildProcessId)
    : mThread(aThread),
      mClosed(false),
      mCompositorThreadHolder(CompositorThreadHolder::GetSingleton()) {
  MOZ_ASSERT(NS_IsMainThread());
  SetOtherProcessId(aChildProcessId);
}

ImageBridgeParent::~ImageBridgeParent() = default;

/* static */
ImageBridgeParent* ImageBridgeParent::CreateSameProcess() {
  base::ProcessId pid = base::GetCurrentProcId();
  RefPtr<ImageBridgeParent> parent =
      new ImageBridgeParent(CompositorThread(), pid);
  parent->mSelfRef = parent;

  {
    MonitorAutoLock lock(*sImageBridgesLock);
    MOZ_RELEASE_ASSERT(sImageBridges.count(pid) == 0);
    sImageBridges[pid] = parent;
  }

  sImageBridgeParentSingleton = parent;
  return parent;
}

/* static */
bool ImageBridgeParent::CreateForGPUProcess(
    Endpoint<PImageBridgeParent>&& aEndpoint) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);

  nsCOMPtr<nsISerialEventTarget> compositorThread = CompositorThread();
  if (!compositorThread) {
    return false;
  }

  RefPtr<ImageBridgeParent> parent =
      new ImageBridgeParent(compositorThread, aEndpoint.OtherPid());

  compositorThread->Dispatch(NewRunnableMethod<Endpoint<PImageBridgeParent>&&>(
      "layers::ImageBridgeParent::Bind", parent, &ImageBridgeParent::Bind,
      std::move(aEndpoint)));

  sImageBridgeParentSingleton = parent;
  return true;
}

/* static */
void ImageBridgeParent::ShutdownInternal() {
  // We make a copy because we don't want to hold the lock while closing and we
  // don't want the object to get freed underneath us.
  nsTArray<RefPtr<ImageBridgeParent>> actors;
  {
    MonitorAutoLock lock(*sImageBridgesLock);
    for (const auto& iter : sImageBridges) {
      actors.AppendElement(iter.second);
    }
  }

  for (auto const& actor : actors) {
    MOZ_RELEASE_ASSERT(!actor->mClosed);
    actor->Close();
  }

  sImageBridgeParentSingleton = nullptr;
}

/* static */
void ImageBridgeParent::Shutdown() {
  CompositorThread()->Dispatch(NS_NewRunnableFunction(
      "ImageBridgeParent::Shutdown",
      []() -> void { ImageBridgeParent::ShutdownInternal(); }));
}

void ImageBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Can't alloc/dealloc shmems from now on.
  mClosed = true;
  mCompositables.clear();
  {
    MonitorAutoLock lock(*sImageBridgesLock);
    sImageBridges.erase(OtherPid());
  }
  GetThread()->Dispatch(
      NewRunnableMethod("layers::ImageBridgeParent::DeferredDestroy", this,
                        &ImageBridgeParent::DeferredDestroy));

  // It is very important that this method gets called at shutdown (be it a
  // clean or an abnormal shutdown), because DeferredDestroy is what clears
  // mSelfRef. If mSelfRef is not null and ActorDestroy is not called, the
  // ImageBridgeParent is leaked which causes the CompositorThreadHolder to be
  // leaked and CompsoitorParent's shutdown ends up spinning the event loop
  // forever, waiting for the compositor thread to terminate.
}

class MOZ_STACK_CLASS AutoImageBridgeParentAsyncMessageSender final {
 public:
  explicit AutoImageBridgeParentAsyncMessageSender(
      ImageBridgeParent* aImageBridge,
      nsTArray<OpDestroy>* aToDestroy = nullptr)
      : mImageBridge(aImageBridge), mToDestroy(aToDestroy) {
    mImageBridge->SetAboutToSendAsyncMessages();
  }

  ~AutoImageBridgeParentAsyncMessageSender() {
    mImageBridge->SendPendingAsyncMessages();
    if (mToDestroy) {
      for (const auto& op : *mToDestroy) {
        mImageBridge->DestroyActor(op);
      }
    }
  }

 private:
  ImageBridgeParent* mImageBridge;
  nsTArray<OpDestroy>* mToDestroy;
};

mozilla::ipc::IPCResult ImageBridgeParent::RecvUpdate(
    EditArray&& aEdits, OpDestroyArray&& aToDestroy,
    const uint64_t& aFwdTransactionId) {
  AUTO_PROFILER_TRACING_MARKER("Paint", "ImageBridgeTransaction", GRAPHICS);
  AUTO_PROFILER_LABEL("ImageBridgeParent::RecvUpdate", GRAPHICS);

  // This ensures that destroy operations are always processed. It is not safe
  // to early-return from RecvUpdate without doing so.
  AutoImageBridgeParentAsyncMessageSender autoAsyncMessageSender(this,
                                                                 &aToDestroy);
  UpdateFwdTransactionId(aFwdTransactionId);

  for (const auto& edit : aEdits) {
    RefPtr<CompositableHost> compositable =
        FindCompositable(edit.compositable());
    if (!compositable ||
        !ReceiveCompositableUpdate(edit.detail(), WrapNotNull(compositable))) {
      return IPC_FAIL_NO_REASON(this);
    }
    uint32_t dropped = compositable->GetDroppedFrames();
    if (dropped) {
      Unused << SendReportFramesDropped(edit.compositable(), dropped);
    }
  }

  return IPC_OK();
}

/* static */
bool ImageBridgeParent::CreateForContent(
    Endpoint<PImageBridgeParent>&& aEndpoint) {
  nsCOMPtr<nsISerialEventTarget> compositorThread = CompositorThread();
  if (!compositorThread) {
    return false;
  }

  RefPtr<ImageBridgeParent> bridge =
      new ImageBridgeParent(compositorThread, aEndpoint.OtherPid());
  compositorThread->Dispatch(NewRunnableMethod<Endpoint<PImageBridgeParent>&&>(
      "layers::ImageBridgeParent::Bind", bridge, &ImageBridgeParent::Bind,
      std::move(aEndpoint)));

  return true;
}

void ImageBridgeParent::Bind(Endpoint<PImageBridgeParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) return;
  mSelfRef = this;

  // If the child process ID was reused by the OS before the ImageBridgeParent
  // object was destroyed, we need to clean it up first.
  RefPtr<ImageBridgeParent> oldActor;
  {
    MonitorAutoLock lock(*sImageBridgesLock);
    ImageBridgeMap::const_iterator i = sImageBridges.find(OtherPid());
    if (i != sImageBridges.end()) {
      oldActor = i->second;
    }
  }

  // We can't hold the lock during Close because it erases itself from the map.
  if (oldActor) {
    MOZ_RELEASE_ASSERT(!oldActor->mClosed);
    oldActor->Close();
  }

  {
    MonitorAutoLock lock(*sImageBridgesLock);
    sImageBridges[OtherPid()] = this;
  }
}

mozilla::ipc::IPCResult ImageBridgeParent::RecvWillClose() {
  // If there is any texture still alive we have to force it to deallocate the
  // device data (GL textures, etc.) now because shortly after SenStop() returns
  // on the child side the widget will be destroyed along with it's associated
  // GL context.
  nsTArray<PTextureParent*> textures;
  ManagedPTextureParent(textures);
  for (unsigned int i = 0; i < textures.Length(); ++i) {
    RefPtr<TextureHost> tex = TextureHost::AsTextureHost(textures[i]);
    tex->DeallocateDeviceData();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ImageBridgeParent::RecvNewCompositable(
    const CompositableHandle& aHandle, const TextureInfo& aInfo) {
  RefPtr<CompositableHost> host = AddCompositable(aHandle, aInfo);
  if (!host) {
    return IPC_FAIL_NO_REASON(this);
  }

  host->SetAsyncRef(AsyncCompositableRef(OtherPid(), aHandle));
  return IPC_OK();
}

mozilla::ipc::IPCResult ImageBridgeParent::RecvReleaseCompositable(
    const CompositableHandle& aHandle) {
  ReleaseCompositable(aHandle);
  return IPC_OK();
}

PTextureParent* ImageBridgeParent::AllocPTextureParent(
    const SurfaceDescriptor& aSharedData, ReadLockDescriptor& aReadLock,
    const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
    const uint64_t& aSerial, const wr::MaybeExternalImageId& aExternalImageId) {
  return TextureHost::CreateIPDLActor(this, aSharedData, std::move(aReadLock),
                                      aLayersBackend, aFlags, aSerial,
                                      aExternalImageId);
}

bool ImageBridgeParent::DeallocPTextureParent(PTextureParent* actor) {
  return TextureHost::DestroyIPDLActor(actor);
}

PMediaSystemResourceManagerParent*
ImageBridgeParent::AllocPMediaSystemResourceManagerParent() {
  return new mozilla::media::MediaSystemResourceManagerParent();
}

bool ImageBridgeParent::DeallocPMediaSystemResourceManagerParent(
    PMediaSystemResourceManagerParent* aActor) {
  MOZ_ASSERT(aActor);
  delete static_cast<mozilla::media::MediaSystemResourceManagerParent*>(aActor);
  return true;
}

void ImageBridgeParent::SendAsyncMessage(
    const nsTArray<AsyncParentMessageData>& aMessage) {
  mozilla::Unused << SendParentAsyncMessages(aMessage);
}

class ProcessIdComparator {
 public:
  bool Equals(const ImageCompositeNotificationInfo& aA,
              const ImageCompositeNotificationInfo& aB) const {
    return aA.mImageBridgeProcessId == aB.mImageBridgeProcessId;
  }
  bool LessThan(const ImageCompositeNotificationInfo& aA,
                const ImageCompositeNotificationInfo& aB) const {
    return aA.mImageBridgeProcessId < aB.mImageBridgeProcessId;
  }
};

/* static */
bool ImageBridgeParent::NotifyImageComposites(
    nsTArray<ImageCompositeNotificationInfo>& aNotifications) {
  // Group the notifications by destination process ID and then send the
  // notifications in one message per group.
  aNotifications.Sort(ProcessIdComparator());
  uint32_t i = 0;
  bool ok = true;
  while (i < aNotifications.Length()) {
    AutoTArray<ImageCompositeNotification, 1> notifications;
    notifications.AppendElement(aNotifications[i].mNotification);
    uint32_t end = i + 1;
    MOZ_ASSERT(aNotifications[i].mNotification.compositable());
    ProcessId pid = aNotifications[i].mImageBridgeProcessId;
    while (end < aNotifications.Length() &&
           aNotifications[end].mImageBridgeProcessId == pid) {
      notifications.AppendElement(aNotifications[end].mNotification);
      ++end;
    }
    RefPtr<ImageBridgeParent> bridge = GetInstance(pid);
    if (!bridge || bridge->mClosed) {
      i = end;
      continue;
    }
    bridge->SendPendingAsyncMessages();
    if (!bridge->SendDidComposite(notifications)) {
      ok = false;
    }
    i = end;
  }
  return ok;
}

void ImageBridgeParent::DeferredDestroy() {
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr;  // "this" ImageBridge may get deleted here.
}

already_AddRefed<ImageBridgeParent> ImageBridgeParent::GetInstance(
    ProcessId aId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MonitorAutoLock lock(*sImageBridgesLock);
  ImageBridgeMap::const_iterator i = sImageBridges.find(aId);
  if (i == sImageBridges.end()) {
    NS_WARNING("Cannot find image bridge for process!");
    return nullptr;
  }
  RefPtr<ImageBridgeParent> bridge = i->second;
  return bridge.forget();
}

bool ImageBridgeParent::AllocShmem(size_t aSize,
                                   ipc::SharedMemory::SharedMemoryType aType,
                                   ipc::Shmem* aShmem) {
  if (mClosed) {
    return false;
  }
  return PImageBridgeParent::AllocShmem(aSize, aType, aShmem);
}

bool ImageBridgeParent::AllocUnsafeShmem(
    size_t aSize, ipc::SharedMemory::SharedMemoryType aType,
    ipc::Shmem* aShmem) {
  if (mClosed) {
    return false;
  }
  return PImageBridgeParent::AllocUnsafeShmem(aSize, aType, aShmem);
}

bool ImageBridgeParent::DeallocShmem(ipc::Shmem& aShmem) {
  if (mClosed) {
    return false;
  }
  return PImageBridgeParent::DeallocShmem(aShmem);
}

bool ImageBridgeParent::IsSameProcess() const {
  return OtherPid() == base::GetCurrentProcId();
}

void ImageBridgeParent::NotifyNotUsed(PTextureParent* aTexture,
                                      uint64_t aTransactionId) {
  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    return;
  }

#ifdef MOZ_WIDGET_ANDROID
  if (auto hardwareBuffer = texture->GetAndroidHardwareBuffer()) {
    MOZ_ASSERT(texture->GetFlags() & TextureFlags::RECYCLE);

    Maybe<FileDescriptor> fenceFd = Some(FileDescriptor());

    auto* wrTexture = texture->AsWebRenderTextureHost();
    if (wrTexture) {
      MOZ_ASSERT(!fenceFd->IsValid());
      fenceFd = Some(texture->GetAndResetReleaseFence());
    }

    // Invalid file descriptor could not be sent via IPC, but
    // OpDeliverReleaseFence message needs to be sent to child side.
    if (!fenceFd->IsValid()) {
      fenceFd = Nothing();
    }
    mPendingAsyncMessage.push_back(OpDeliverReleaseFence(
        std::move(fenceFd), hardwareBuffer->mId, aTransactionId,
        /* usesImageBridge */ true));
  }
#endif

  if (!(texture->GetFlags() & TextureFlags::RECYCLE) &&
      !(texture->GetFlags() & TextureFlags::WAIT_HOST_USAGE_END)) {
    return;
  }

  uint64_t textureId = TextureHost::GetTextureSerial(aTexture);
  mPendingAsyncMessage.push_back(OpNotifyNotUsed(textureId, aTransactionId));

  if (!IsAboutToSendAsyncMessages()) {
    SendPendingAsyncMessages();
  }
}

/* static */
void ImageBridgeParent::NotifyBufferNotUsedOfCompositorBridge(
    base::ProcessId aChildProcessId, TextureHost* aTexture,
    uint64_t aTransactionId) {
  RefPtr<ImageBridgeParent> bridge = GetInstance(aChildProcessId);
  if (!bridge || bridge->mClosed) {
    return;
  }
  bridge->NotifyBufferNotUsedOfCompositorBridge(aTexture, aTransactionId);
}

void ImageBridgeParent::NotifyBufferNotUsedOfCompositorBridge(
    TextureHost* aTexture, uint64_t aTransactionId) {
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aTexture->GetAndroidHardwareBuffer());

#ifdef MOZ_WIDGET_ANDROID
  Maybe<FileDescriptor> fenceFd = Some(FileDescriptor());

  auto* wrTexture = aTexture->AsWebRenderTextureHost();
  if (wrTexture) {
    MOZ_ASSERT(!fenceFd->IsValid());
    fenceFd = Some(aTexture->GetAndResetReleaseFence());
  }

  // Invalid file descriptor could not be sent via IPC, but
  // OpDeliverReleaseFence message needs to be sent to child side.
  if (!fenceFd->IsValid()) {
    fenceFd = Nothing();
  }
  mPendingAsyncMessage.push_back(
      OpDeliverReleaseFence(fenceFd, aTexture->GetAndroidHardwareBuffer()->mId,
                            aTransactionId, /* usesImageBridge */ false));
  SendPendingAsyncMessages();
#else
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
#endif
}

}  // namespace layers
}  // namespace mozilla
