/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoDecoderManagerChild.h"
#include "VideoDecoderChild.h"
#include "mozilla/dom/ContentChild.h"
#include "MediaPrefs.h"
#include "nsThreadUtils.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/ISurfaceAllocator.h"

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

/* static */ void
VideoDecoderManagerChild::Initialize()
{
  MOZ_ASSERT(NS_IsMainThread());

  MediaPrefs::GetSingleton();

#ifdef XP_WIN
  if (!MediaPrefs::PDMUseGPUDecoder()) {
    return;
  }

  // Can't run remote video decoding in the parent process.
  if (!ContentChild::GetSingleton()) {
    return;
  }

  if (!sVideoDecoderChildThread) {
    RefPtr<nsIThread> childThread;
    nsresult rv = NS_NewNamedThread("VideoChild", getter_AddRefs(childThread));
    NS_ENSURE_SUCCESS_VOID(rv);
    sVideoDecoderChildThread = childThread;

    sVideoDecoderChildAbstractThread =
      AbstractThread::CreateXPCOMThreadWrapper(childThread, false);
  }
#else
  return;
#endif

}

/* static */ void
VideoDecoderManagerChild::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sVideoDecoderChildThread) {
    sVideoDecoderChildThread->Dispatch(NS_NewRunnableFunction([]() {
      if (sDecoderManager) {
        sDecoderManager->Close();
        sDecoderManager = nullptr;
      }
    }), NS_DISPATCH_NORMAL);

    sVideoDecoderChildAbstractThread = nullptr;
    sVideoDecoderChildThread->Shutdown();
    sVideoDecoderChildThread = nullptr;
  }
}

/* static */ VideoDecoderManagerChild*
VideoDecoderManagerChild::GetSingleton()
{
  MOZ_ASSERT(NS_GetCurrentThread() == GetManagerThread());

  if (!sDecoderManager || !sDecoderManager->mCanSend) {
    RefPtr<VideoDecoderManagerChild> manager;
      
    NS_DispatchToMainThread(NS_NewRunnableFunction([&]() {
      Endpoint<PVideoDecoderManagerChild> endpoint;
      if (!ContentChild::GetSingleton()->SendInitVideoDecoderManager(&endpoint)) {
        return;
      }

      if (!endpoint.IsValid()) {
        return;
      }

      manager = new VideoDecoderManagerChild();

      RefPtr<Runnable> task = NewRunnableMethod<Endpoint<PVideoDecoderManagerChild>&&>(
        manager, &VideoDecoderManagerChild::Open, Move(endpoint));
      sVideoDecoderChildThread->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
    }), NS_DISPATCH_SYNC);

    sDecoderManager = manager;
  }
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
VideoDecoderManagerChild::AllocPVideoDecoderChild()
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
  if (!aEndpoint.Bind(this)) {
    return;
  }
  AddRef();
  mCanSend = true;
}

void
VideoDecoderManagerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mCanSend = false;
}

void
VideoDecoderManagerChild::DeallocPVideoDecoderManagerChild()
{
  Release();
}

bool
VideoDecoderManagerChild::DeallocShmem(mozilla::ipc::Shmem& aShmem)
{
  if (NS_GetCurrentThread() != sVideoDecoderChildThread) {
    RefPtr<VideoDecoderManagerChild> self = this;
    mozilla::ipc::Shmem shmem = aShmem;
    sVideoDecoderChildThread->Dispatch(NS_NewRunnableFunction([self, shmem]() {
      if (self->mCanSend) {
        mozilla::ipc::Shmem shmemCopy = shmem;
        self->DeallocShmem(shmemCopy);
      }
    }), NS_DISPATCH_NORMAL);
    return true;
  }
  return PVideoDecoderManagerChild::DeallocShmem(aShmem);
}

void
VideoDecoderManagerChild::DeallocateSurfaceDescriptorGPUVideo(const SurfaceDescriptorGPUVideo& aSD)
{
  RefPtr<VideoDecoderManagerChild> ref = this;
  SurfaceDescriptorGPUVideo sd = Move(aSD);
  sVideoDecoderChildThread->Dispatch(NS_NewRunnableFunction([ref, sd]() {
    if (ref->mCanSend) {
      ref->SendDeallocateSurfaceDescriptorGPUVideo(sd);
    }
  }), NS_DISPATCH_NORMAL);
}

void
VideoDecoderManagerChild::FatalError(const char* const aName, const char* const aMsg) const
{
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aName, aMsg, OtherPid());
}

} // namespace dom
} // namespace mozilla
