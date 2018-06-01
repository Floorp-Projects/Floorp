/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoDecoderManagerChild.h"
#include "VideoDecoderChild.h"
#include "mozilla/dom/ContentChild.h"
#include "nsThreadUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/SynchronousTask.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "base/task.h"

namespace mozilla {
namespace dom {

using namespace ipc;
using namespace layers;
using namespace gfx;

// Only modified on the main-thread
StaticRefPtr<nsIThread> sVideoDecoderChildThread;
StaticRefPtr<AbstractThread> sVideoDecoderChildAbstractThread;

// Only accessed from sVideoDecoderChildThread
static StaticRefPtr<VideoDecoderManagerChild> sDecoderManager;
static UniquePtr<nsTArray<RefPtr<Runnable>>> sRecreateTasks;

/* static */ void
VideoDecoderManagerChild::InitializeThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sVideoDecoderChildThread) {
    RefPtr<nsIThread> childThread;
    nsresult rv = NS_NewNamedThread("VideoChild", getter_AddRefs(childThread));
    NS_ENSURE_SUCCESS_VOID(rv);
    sVideoDecoderChildThread = childThread;

    sVideoDecoderChildAbstractThread =
      AbstractThread::CreateXPCOMThreadWrapper(childThread, false);

    sRecreateTasks = MakeUnique<nsTArray<RefPtr<Runnable>>>();
  }
}

/* static */ void
VideoDecoderManagerChild::InitForContent(Endpoint<PVideoDecoderManagerChild>&& aVideoManager)
{
  InitializeThread();
  sVideoDecoderChildThread->Dispatch(NewRunnableFunction("InitForContentRunnable",
                                                         &Open, std::move(aVideoManager)), NS_DISPATCH_NORMAL);
}

/* static */ void
VideoDecoderManagerChild::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sVideoDecoderChildThread) {
    sVideoDecoderChildThread->Dispatch(
      NS_NewRunnableFunction("dom::VideoDecoderManagerChild::Shutdown",
                             []() {
                               if (sDecoderManager &&
                                   sDecoderManager->CanSend()) {
                                 sDecoderManager->Close();
                                 sDecoderManager = nullptr;
                               }
                             }),
      NS_DISPATCH_NORMAL);

    sVideoDecoderChildAbstractThread = nullptr;
    sVideoDecoderChildThread->Shutdown();
    sVideoDecoderChildThread = nullptr;

    sRecreateTasks = nullptr;
  }
}

void
VideoDecoderManagerChild::RunWhenRecreated(already_AddRefed<Runnable> aTask)
{
  MOZ_ASSERT(NS_GetCurrentThread() == GetManagerThread());

  // If we've already been recreated, then run the task immediately.
  if (sDecoderManager && sDecoderManager != this && sDecoderManager->CanSend()) {
    RefPtr<Runnable> task = aTask;
    task->Run();
  } else {
    sRecreateTasks->AppendElement(aTask);
  }
}


/* static */ VideoDecoderManagerChild*
VideoDecoderManagerChild::GetSingleton()
{
  MOZ_ASSERT(NS_GetCurrentThread() == GetManagerThread());
  return sDecoderManager;
}

/* static */ nsIThread*
VideoDecoderManagerChild::GetManagerThread()
{
  return sVideoDecoderChildThread;
}

/* static */ AbstractThread*
VideoDecoderManagerChild::GetManagerAbstractThread()
{
  return sVideoDecoderChildAbstractThread;
}

PVideoDecoderChild*
VideoDecoderManagerChild::AllocPVideoDecoderChild(const VideoInfo& aVideoInfo,
                                                  const float& aFramerate,
                                                  const layers::TextureFactoryIdentifier& aIdentifier,
                                                  bool* aSuccess,
                                                  nsCString* /* not used */,
                                                  nsCString* /* not used */,
                                                  nsCString* /* not used */)
{
  return new VideoDecoderChild();
}

bool
VideoDecoderManagerChild::DeallocPVideoDecoderChild(PVideoDecoderChild* actor)
{
  VideoDecoderChild* child = static_cast<VideoDecoderChild*>(actor);
  child->IPDLActorDestroyed();
  return true;
}

void
VideoDecoderManagerChild::Open(Endpoint<PVideoDecoderManagerChild>&& aEndpoint)
{
  // Make sure we always dispatch everything in sRecreateTasks, even if we
  // fail since this is as close to being recreated as we will ever be.
  sDecoderManager = nullptr;
  if (aEndpoint.IsValid()) {
    RefPtr<VideoDecoderManagerChild> manager = new VideoDecoderManagerChild();
    if (aEndpoint.Bind(manager)) {
      sDecoderManager = manager;
      manager->InitIPDL();
    }
  }
  for (Runnable* task : *sRecreateTasks) {
    task->Run();
  }
  sRecreateTasks->Clear();
}

void
VideoDecoderManagerChild::InitIPDL()
{
  mCanSend = true;
  mIPDLSelfRef = this;
}

void
VideoDecoderManagerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mCanSend = false;
}

void
VideoDecoderManagerChild::DeallocPVideoDecoderManagerChild()
{
  mIPDLSelfRef = nullptr;
}

bool
VideoDecoderManagerChild::CanSend()
{
  MOZ_ASSERT(NS_GetCurrentThread() == GetManagerThread());
  return mCanSend;
}

bool
VideoDecoderManagerChild::DeallocShmem(mozilla::ipc::Shmem& aShmem)
{
  if (NS_GetCurrentThread() != sVideoDecoderChildThread) {
    RefPtr<VideoDecoderManagerChild> self = this;
    mozilla::ipc::Shmem shmem = aShmem;
    sVideoDecoderChildThread->Dispatch(
      NS_NewRunnableFunction("dom::VideoDecoderManagerChild::DeallocShmem",
                             [self, shmem]() {
                               if (self->CanSend()) {
                                 mozilla::ipc::Shmem shmemCopy = shmem;
                                 self->DeallocShmem(shmemCopy);
                               }
                             }),
      NS_DISPATCH_NORMAL);
    return true;
  }
  return PVideoDecoderManagerChild::DeallocShmem(aShmem);
}

struct SurfaceDescriptorUserData
{
  SurfaceDescriptorUserData(VideoDecoderManagerChild* aAllocator, SurfaceDescriptor& aSD)
    : mAllocator(aAllocator)
    , mSD(aSD)
  {}
  ~SurfaceDescriptorUserData()
  {
    DestroySurfaceDescriptor(mAllocator, &mSD);
  }

  RefPtr<VideoDecoderManagerChild> mAllocator;
  SurfaceDescriptor mSD;
};

void DeleteSurfaceDescriptorUserData(void* aClosure)
{
  SurfaceDescriptorUserData* sd = reinterpret_cast<SurfaceDescriptorUserData*>(aClosure);
  delete sd;
}

already_AddRefed<SourceSurface>
VideoDecoderManagerChild::Readback(const SurfaceDescriptorGPUVideo& aSD)
{
  // We can't use NS_DISPATCH_SYNC here since that can spin the event
  // loop while it waits. This function can be called from JS and we
  // don't want that to happen.
  SynchronousTask task("Readback sync");

  RefPtr<VideoDecoderManagerChild> ref = this;
  SurfaceDescriptor sd;
  if (NS_FAILED(sVideoDecoderChildThread->Dispatch(NS_NewRunnableFunction("VideoDecoderManagerChild::Readback",
                                                                          [&]() {
    AutoCompleteTask complete(&task);
    if (ref->CanSend()) {
      ref->SendReadback(aSD, &sd);
    }
  }), NS_DISPATCH_NORMAL))) {
    return nullptr;
  }

  task.Wait();

  if (!IsSurfaceDescriptorValid(sd)) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> source = GetSurfaceForDescriptor(sd);
  if (!source) {
    DestroySurfaceDescriptor(this, &sd);
    NS_WARNING("Failed to map SurfaceDescriptor in Readback");
    return nullptr;
  }

  static UserDataKey sSurfaceDescriptor;
  source->AddUserData(&sSurfaceDescriptor,
                      new SurfaceDescriptorUserData(this, sd),
                      DeleteSurfaceDescriptorUserData);

  return source.forget();
}

void
VideoDecoderManagerChild::DeallocateSurfaceDescriptorGPUVideo(const SurfaceDescriptorGPUVideo& aSD)
{
  RefPtr<VideoDecoderManagerChild> ref = this;
  SurfaceDescriptorGPUVideo sd = std::move(aSD);
  sVideoDecoderChildThread->Dispatch(
    NS_NewRunnableFunction(
      "dom::VideoDecoderManagerChild::DeallocateSurfaceDescriptorGPUVideo",
      [ref, sd]() {
        if (ref->CanSend()) {
          ref->SendDeallocateSurfaceDescriptorGPUVideo(sd);
        }
      }),
    NS_DISPATCH_NORMAL);
}

void
VideoDecoderManagerChild::HandleFatalError(const char* aMsg) const
{
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherPid());
}

} // namespace dom
} // namespace mozilla
