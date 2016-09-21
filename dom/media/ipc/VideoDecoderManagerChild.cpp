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

namespace mozilla {
namespace dom {

using namespace ipc;
using namespace layers;
using namespace gfx;

StaticRefPtr<nsIThread> sVideoDecoderChildThread;
StaticRefPtr<AbstractThread> sVideoDecoderChildAbstractThread;
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

  Endpoint<PVideoDecoderManagerChild> endpoint;
  if (!ContentChild::GetSingleton()->SendInitVideoDecoderManager(&endpoint)) {
    return;
  }

  // TODO: The above message should return an empty endpoint if there wasn't a GPU
  // process. Unfortunately IPDL will assert in this case, so it can't actually
  // happen. Bug 1302009 is filed for fixing this.

  sDecoderManager = new VideoDecoderManagerChild();

  RefPtr<Runnable> task = NewRunnableMethod<Endpoint<PVideoDecoderManagerChild>&&>(
    sDecoderManager, &VideoDecoderManagerChild::Open, Move(endpoint));
  sVideoDecoderChildThread->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
#else
  return;
#endif

}

/* static */ void
VideoDecoderManagerChild::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sVideoDecoderChildThread) {
    MOZ_ASSERT(sDecoderManager);

    sVideoDecoderChildThread->Dispatch(NS_NewRunnableFunction([]() {
      sDecoderManager->Close();
    }), NS_DISPATCH_SYNC);

    sDecoderManager = nullptr;

    sVideoDecoderChildAbstractThread = nullptr;
    sVideoDecoderChildThread->Shutdown();
    sVideoDecoderChildThread = nullptr;
  }
}

/* static */ VideoDecoderManagerChild*
VideoDecoderManagerChild::GetSingleton()
{
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
    // We can't recover from this.
    MOZ_CRASH("Failed to bind VideoDecoderChild to endpoint");
  }
  AddRef();
}

void
VideoDecoderManagerChild::DeallocPVideoDecoderManagerChild()
{
  Release();
}

void
VideoDecoderManagerChild::DeallocateSurfaceDescriptorGPUVideo(const SurfaceDescriptorGPUVideo& aSD)
{
  RefPtr<VideoDecoderManagerChild> ref = this;
  SurfaceDescriptorGPUVideo sd = Move(aSD);
  sVideoDecoderChildThread->Dispatch(NS_NewRunnableFunction([ref, sd]() {
    ref->SendDeallocateSurfaceDescriptorGPUVideo(sd);
  }), NS_DISPATCH_NORMAL);
}

} // namespace dom
} // namespace mozilla
