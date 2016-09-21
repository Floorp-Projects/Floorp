/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoDecoderManagerParent.h"
#include "base/thread.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Services.h"
#include "mozilla/Observer.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIEventTarget.h"
#include "nsThreadUtils.h"
#include "ImageContainer.h"

#if XP_WIN
#include <objbase.h>
#endif

namespace mozilla {
namespace dom {

using base::Thread;
using namespace ipc;
using namespace layers;
using namespace gfx;


struct ImageMapEntry {
  ImageMapEntry()
    : mOwner(nullptr)
  {}
  ImageMapEntry(layers::Image* aImage, VideoDecoderManagerParent* aOwner)
    : mImage(aImage)
    , mOwner(aOwner)
  {}
  ~ImageMapEntry() {}

  RefPtr<layers::Image> mImage;
  VideoDecoderManagerParent* mOwner;
};
std::map<uint64_t, ImageMapEntry> sImageMap;
StaticMutex sImageMutex;

/* static */ layers::Image*
VideoDecoderManagerParent::LookupImage(const SurfaceDescriptorGPUVideo& aSD)
{
  StaticMutexAutoLock lock(sImageMutex);
  return sImageMap[aSD.handle()].mImage;
}

SurfaceDescriptorGPUVideo
VideoDecoderManagerParent::StoreImage(Image* aImage)
{
  StaticMutexAutoLock lock(sImageMutex);

  static uint64_t sImageCount = 0;
  sImageMap[++sImageCount] = ImageMapEntry(aImage, this);

  return SurfaceDescriptorGPUVideo(sImageCount);
}

void
VideoDecoderManagerParent::ClearAllOwnedImages()
{
  StaticMutexAutoLock lock(sImageMutex);
  for (auto it = sImageMap.begin(); it != sImageMap.end();)
  {
    if ((*it).second.mOwner == this) {
      it = sImageMap.erase(it);
    } else {
      ++it;
    }
  }
}

StaticRefPtr<nsIThread> sVideoDecoderManagerThread;
StaticRefPtr<nsIThread> sVideoDecoderTaskThread;
StaticRefPtr<TaskQueue> sManagerTaskQueue;

class ManagerThreadShutdownObserver : public nsIObserver
{
  virtual ~ManagerThreadShutdownObserver() {}
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
#if XP_WIN
  sVideoDecoderManagerThread->Dispatch(NS_NewRunnableFunction([]() {
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    MOZ_ASSERT(hr == S_OK);
  }), NS_DISPATCH_NORMAL);
#endif

  sManagerTaskQueue = new TaskQueue(managerThread.forget());

  RefPtr<nsIThread> taskThread;
  rv = NS_NewNamedThread("VideoTaskQueue", getter_AddRefs(taskThread));
  if (NS_FAILED(rv)) {
    sVideoDecoderManagerThread->Shutdown();
    sVideoDecoderManagerThread = nullptr;
    return;
  }
  sVideoDecoderTaskThread = taskThread;

#ifdef XP_WIN
  sVideoDecoderTaskThread->Dispatch(NS_NewRunnableFunction([]() {
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    MOZ_ASSERT(hr == S_OK);
  }), NS_DISPATCH_NORMAL);
#endif

  ManagerThreadShutdownObserver* obs = new ManagerThreadShutdownObserver();
  observerService->AddObserver(obs, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

void
VideoDecoderManagerParent::ShutdownThreads()
{
  sManagerTaskQueue->BeginShutdown();
  sManagerTaskQueue->AwaitShutdownAndIdle();
  sVideoDecoderTaskThread->Shutdown();
  sVideoDecoderTaskThread = nullptr;
  sVideoDecoderManagerThread->Shutdown();
  sVideoDecoderManagerThread = nullptr;
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

  RefPtr<VideoDecoderManagerParent> parent = new VideoDecoderManagerParent();

  RefPtr<Runnable> task = NewRunnableMethod<Endpoint<PVideoDecoderManagerParent>&&>(
    parent, &VideoDecoderManagerParent::Open, Move(aEndpoint));
  sVideoDecoderManagerThread->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
  return true;
}

VideoDecoderManagerParent::VideoDecoderManagerParent()
{
  MOZ_COUNT_CTOR(VideoDecoderManagerParent);
}

VideoDecoderManagerParent::~VideoDecoderManagerParent()
{
  MOZ_COUNT_DTOR(VideoDecoderManagerParent);

  ClearAllOwnedImages();
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

bool
VideoDecoderManagerParent::RecvDeallocateSurfaceDescriptorGPUVideo(const SurfaceDescriptorGPUVideo& aSD)
{
  StaticMutexAutoLock lock(sImageMutex);
  sImageMap.erase(aSD.handle());
  return true;
}

} // namespace dom
} // namespace mozilla
