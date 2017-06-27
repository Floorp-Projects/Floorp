/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoDecoderManagerParent.h"
#include "VideoDecoderParent.h"
#include "base/thread.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Services.h"
#include "mozilla/Observer.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIEventTarget.h"
#include "nsThreadUtils.h"
#include "ImageContainer.h"
#include "mozilla/layers/VideoBridgeChild.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/SyncRunnable.h"

#if XP_WIN
#include <objbase.h>
#endif

namespace mozilla {
namespace dom {

using namespace ipc;
using namespace layers;
using namespace gfx;

SurfaceDescriptorGPUVideo
VideoDecoderManagerParent::StoreImage(Image* aImage, TextureClient* aTexture)
{
  mImageMap[aTexture->GetSerial()] = aImage;
  mTextureMap[aTexture->GetSerial()] = aTexture;
  return SurfaceDescriptorGPUVideo(aTexture->GetSerial());
}

StaticRefPtr<nsIThread> sVideoDecoderManagerThread;
StaticRefPtr<TaskQueue> sManagerTaskQueue;

class VideoDecoderManagerThreadHolder
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoDecoderManagerThreadHolder)

public:
  VideoDecoderManagerThreadHolder() {}

private:
  ~VideoDecoderManagerThreadHolder() {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
      "dom::VideoDecoderManagerThreadHolder::~VideoDecoderManagerThreadHolder",
      []() -> void {
        sVideoDecoderManagerThread->Shutdown();
        sVideoDecoderManagerThread = nullptr;
      }));
  }
};
StaticRefPtr<VideoDecoderManagerThreadHolder> sVideoDecoderManagerThreadHolder;

class ManagerThreadShutdownObserver : public nsIObserver
{
  virtual ~ManagerThreadShutdownObserver() = default;
public:
  ManagerThreadShutdownObserver() {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override
  {
    MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);

    VideoDecoderManagerParent::ShutdownThreads();
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(ManagerThreadShutdownObserver, nsIObserver);

void
VideoDecoderManagerParent::StartupThreads()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sVideoDecoderManagerThread) {
    return;
  }

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    return;
  }

  RefPtr<nsIThread> managerThread;
  nsresult rv = NS_NewNamedThread("VideoParent", getter_AddRefs(managerThread));
  if (NS_FAILED(rv)) {
    return;
  }
  sVideoDecoderManagerThread = managerThread;
  sVideoDecoderManagerThreadHolder = new VideoDecoderManagerThreadHolder();
#if XP_WIN
  sVideoDecoderManagerThread->Dispatch(NS_NewRunnableFunction("VideoDecoderManagerParent::StartupThreads",
  []() {
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    MOZ_ASSERT(hr == S_OK);
  }), NS_DISPATCH_NORMAL);
#endif
  sVideoDecoderManagerThread->Dispatch(
    NS_NewRunnableFunction("dom::VideoDecoderManagerParent::StartupThreads",
                           []() { layers::VideoBridgeChild::Startup(); }),
    NS_DISPATCH_NORMAL);

  sManagerTaskQueue = new TaskQueue(
    managerThread.forget(), "VideoDecoderManagerParent::sManagerTaskQueue");

  auto* obs = new ManagerThreadShutdownObserver();
  observerService->AddObserver(obs, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

void
VideoDecoderManagerParent::ShutdownThreads()
{
  sManagerTaskQueue->BeginShutdown();
  sManagerTaskQueue->AwaitShutdownAndIdle();
  sManagerTaskQueue = nullptr;

  sVideoDecoderManagerThreadHolder = nullptr;
  while (sVideoDecoderManagerThread) {
    NS_ProcessNextEvent(nullptr, true);
  }
}

void
VideoDecoderManagerParent::ShutdownVideoBridge()
{
  if (sVideoDecoderManagerThread) {
    RefPtr<Runnable> task = NS_NewRunnableFunction(
      "dom::VideoDecoderManagerParent::ShutdownVideoBridge",
      []() { VideoBridgeChild::Shutdown(); });
    SyncRunnable::DispatchToThread(sVideoDecoderManagerThread, task);
  }
}

bool
VideoDecoderManagerParent::OnManagerThread()
{
  return NS_GetCurrentThread() == sVideoDecoderManagerThread;
}

bool
VideoDecoderManagerParent::CreateForContent(Endpoint<PVideoDecoderManagerParent>&& aEndpoint)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
  MOZ_ASSERT(NS_IsMainThread());

  StartupThreads();
  if (!sVideoDecoderManagerThread) {
    return false;
  }

  RefPtr<VideoDecoderManagerParent> parent =
    new VideoDecoderManagerParent(sVideoDecoderManagerThreadHolder);

  RefPtr<Runnable> task =
    NewRunnableMethod<Endpoint<PVideoDecoderManagerParent>&&>(
      "dom::VideoDecoderManagerParent::Open",
      parent,
      &VideoDecoderManagerParent::Open,
      Move(aEndpoint));
  sVideoDecoderManagerThread->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
  return true;
}

VideoDecoderManagerParent::VideoDecoderManagerParent(VideoDecoderManagerThreadHolder* aHolder)
 : mThreadHolder(aHolder)
{
  MOZ_COUNT_CTOR(VideoDecoderManagerParent);
}

VideoDecoderManagerParent::~VideoDecoderManagerParent()
{
  MOZ_COUNT_DTOR(VideoDecoderManagerParent);
}

void
VideoDecoderManagerParent::ActorDestroy(mozilla::ipc::IProtocol::ActorDestroyReason)
{
  mThreadHolder = nullptr;
}

PVideoDecoderParent*
VideoDecoderManagerParent::AllocPVideoDecoderParent(const VideoInfo& aVideoInfo,
                                                    const layers::TextureFactoryIdentifier& aIdentifier,
                                                    bool* aSuccess)
{
  RefPtr<TaskQueue> decodeTaskQueue = new TaskQueue(
    SharedThreadPool::Get(NS_LITERAL_CSTRING("VideoDecoderParent"), 4),
    "VideoDecoderParent::mDecodeTaskQueue");

  return new VideoDecoderParent(
    this, aVideoInfo, aIdentifier,
    sManagerTaskQueue, decodeTaskQueue, aSuccess);
}

bool
VideoDecoderManagerParent::DeallocPVideoDecoderParent(PVideoDecoderParent* actor)
{
  VideoDecoderParent* parent = static_cast<VideoDecoderParent*>(actor);
  parent->Destroy();
  return true;
}

void
VideoDecoderManagerParent::Open(Endpoint<PVideoDecoderManagerParent>&& aEndpoint)
{
  if (!aEndpoint.Bind(this)) {
    // We can't recover from this.
    MOZ_CRASH("Failed to bind VideoDecoderManagerParent to endpoint");
  }
  AddRef();
}

void
VideoDecoderManagerParent::DeallocPVideoDecoderManagerParent()
{
  Release();
}

mozilla::ipc::IPCResult
VideoDecoderManagerParent::RecvReadback(const SurfaceDescriptorGPUVideo& aSD, SurfaceDescriptor* aResult)
{
  RefPtr<Image> image = mImageMap[aSD.handle()];
  if (!image) {
    *aResult = null_t();
    return IPC_OK();
  }

  RefPtr<SourceSurface> source = image->GetAsSourceSurface();
  if (!source) {
    *aResult = null_t();
    return IPC_OK();
  }

  SurfaceFormat format = source->GetFormat();
  IntSize size = source->GetSize();
  size_t length = ImageDataSerializer::ComputeRGBBufferSize(size, format);

  Shmem buffer;
  if (!length || !AllocShmem(length, Shmem::SharedMemory::TYPE_BASIC, &buffer)) {
    *aResult = null_t();
    return IPC_OK();
  }

  RefPtr<DrawTarget> dt = Factory::CreateDrawTargetForData(gfx::BackendType::CAIRO,
                                                           buffer.get<uint8_t>(), size,
                                                           ImageDataSerializer::ComputeRGBStride(format, size.width),
                                                           format);
  if (!dt) {
    DeallocShmem(buffer);
    *aResult = null_t();
    return IPC_OK();
  }

  dt->CopySurface(source, IntRect(0, 0, size.width, size.height), IntPoint());
  dt->Flush();

  *aResult = SurfaceDescriptorBuffer(RGBDescriptor(size, format, true), MemoryOrShmem(buffer));
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderManagerParent::RecvDeallocateSurfaceDescriptorGPUVideo(const SurfaceDescriptorGPUVideo& aSD)
{
  mImageMap.erase(aSD.handle());
  mTextureMap.erase(aSD.handle());
  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
