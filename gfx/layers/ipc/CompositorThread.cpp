/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CompositorThread.h"
#include "MainThreadUtils.h"
#include "nsThreadUtils.h"
#include "CompositorBridgeParent.h"
#include "mozilla/media/MediaSystemResourceService.h"

namespace mozilla {

namespace gfx {
// See VRManagerChild.cpp
void ReleaseVRManagerParentSingleton();
} // namespace gfx

namespace layers {

static StaticRefPtr<CompositorThreadHolder> sCompositorThreadHolder;
static bool sFinishedCompositorShutDown = false;

// See ImageBridgeChild.cpp
void ReleaseImageBridgeParentSingleton();

CompositorThreadHolder* GetCompositorThreadHolder()
{
  return sCompositorThreadHolder;
}

base::Thread*
CompositorThread()
{
  return sCompositorThreadHolder
         ? sCompositorThreadHolder->GetCompositorThread()
         : nullptr;
}

/* static */ MessageLoop*
CompositorThreadHolder::Loop()
{
  return CompositorThread() ? CompositorThread()->message_loop() : nullptr;
}

CompositorThreadHolder*
CompositorThreadHolder::GetSingleton()
{
  return sCompositorThreadHolder;
}

CompositorThreadHolder::CompositorThreadHolder()
  : mCompositorThread(CreateCompositorThread())
{
  MOZ_ASSERT(NS_IsMainThread());
}

CompositorThreadHolder::~CompositorThreadHolder()
{
  MOZ_ASSERT(NS_IsMainThread());

  DestroyCompositorThread(mCompositorThread);
}

/* static */ void
CompositorThreadHolder::DestroyCompositorThread(base::Thread* aCompositorThread)
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!sCompositorThreadHolder, "We shouldn't be destroying the compositor thread yet.");

  CompositorBridgeParent::Shutdown();
  delete aCompositorThread;
  sFinishedCompositorShutDown = true;
}

/* static */ base::Thread*
CompositorThreadHolder::CreateCompositorThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!sCompositorThreadHolder, "The compositor thread has already been started!");

  base::Thread* compositorThread = new base::Thread("Compositor");

  base::Thread::Options options;
  /* Timeout values are powers-of-two to enable us get better data.
     128ms is chosen for transient hangs because 8Hz should be the minimally
     acceptable goal for Compositor responsiveness (normal goal is 60Hz). */
  options.transient_hang_timeout = 128; // milliseconds
  /* 2048ms is chosen for permanent hangs because it's longer than most
   * Compositor hangs seen in the wild, but is short enough to not miss getting
   * native hang stacks. */
  options.permanent_hang_timeout = 2048; // milliseconds
#if defined(_WIN32)
  /* With d3d9 the compositor thread creates native ui, see DeviceManagerD3D9. As
   * such the thread is a gui thread, and must process a windows message queue or
   * risk deadlocks. Chromium message loop TYPE_UI does exactly what we need. */
  options.message_loop_type = MessageLoop::TYPE_UI;
#endif

  if (!compositorThread->StartWithOptions(options)) {
    delete compositorThread;
    return nullptr;
  }

  CompositorBridgeParent::Setup();

  return compositorThread;
}

void
CompositorThreadHolder::Start()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main Thread!");
  MOZ_ASSERT(!sCompositorThreadHolder, "The compositor thread has already been started!");

  sCompositorThreadHolder = new CompositorThreadHolder();
}

void
CompositorThreadHolder::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main Thread!");
  MOZ_ASSERT(sCompositorThreadHolder, "The compositor thread has already been shut down!");

  ReleaseImageBridgeParentSingleton();
  gfx::ReleaseVRManagerParentSingleton();
  MediaSystemResourceService::Shutdown();

  sCompositorThreadHolder = nullptr;

  // No locking is needed around sFinishedCompositorShutDown because it is only
  // ever accessed on the main thread.
  while (!sFinishedCompositorShutDown) {
    NS_ProcessNextEvent(nullptr, true);
  }

  CompositorBridgeParent::FinishShutdown();
}

/* static */ bool
CompositorThreadHolder::IsInCompositorThread()
{
  return CompositorThread() &&
         CompositorThread()->thread_id() == PlatformThread::CurrentId();
}

} // namespace mozilla
} // namespace layers
