/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CompositorThread.h"

#include "CompositorBridgeParent.h"
#include "gfxGradientCache.h"
#include "MainThreadUtils.h"
#include "VRManagerParent.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/layers/CanvasTranslator.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/media/MediaSystemResourceService.h"
#include "nsThread.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

static StaticRefPtr<CompositorThreadHolder> sCompositorThreadHolder;
static Atomic<bool> sFinishedCompositorShutDown(false);
static mozilla::BackgroundHangMonitor* sBackgroundHangMonitor;
static ProfilerThreadId sProfilerThreadId;

nsISerialEventTarget* CompositorThread() {
  return sCompositorThreadHolder
             ? sCompositorThreadHolder->GetCompositorThread()
             : nullptr;
}

CompositorThreadHolder* CompositorThreadHolder::GetSingleton() {
  return sCompositorThreadHolder;
}

CompositorThreadHolder::CompositorThreadHolder()
    : mCompositorThread(CreateCompositorThread()) {
  MOZ_ASSERT(NS_IsMainThread());
}

CompositorThreadHolder::~CompositorThreadHolder() {
  sFinishedCompositorShutDown = true;
}

/* static */ already_AddRefed<nsIThread>
CompositorThreadHolder::CreateCompositorThread() {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!sCompositorThreadHolder,
             "The compositor thread has already been started!");

  // This is 512K, which is higher than the default 256K.
  // Increased to accommodate Mesa in bug 1753340.
  //
  // Previously increased to 320K to avoid a stack overflow in the
  // Intel Vulkan driver initialization in bug 1716120.
  //
  // Note: we only override it if it's limited already.
  const uint32_t stackSize =
      nsIThreadManager::DEFAULT_STACK_SIZE ? 512 << 10 : 0;

  nsCOMPtr<nsIThread> compositorThread;
  nsresult rv = NS_NewNamedThread(
      "Compositor", getter_AddRefs(compositorThread),
      NS_NewRunnableFunction(
          "CompositorThreadHolder::CompositorThreadHolderSetup",
          []() {
            sProfilerThreadId = profiler_current_thread_id();
            sBackgroundHangMonitor = new mozilla::BackgroundHangMonitor(
                "Compositor",
                /* Timeout values are powers-of-two to enable us get better
                   data. 128ms is chosen for transient hangs because 8Hz should
                   be the minimally acceptable goal for Compositor
                   responsiveness (normal goal is 60Hz). */
                128,
                /* 2048ms is chosen for permanent hangs because it's longer than
                 * most Compositor hangs seen in the wild, but is short enough
                 * to not miss getting native hang stacks. */
                2048);
            nsCOMPtr<nsIThread> thread = NS_GetCurrentThread();
            static_cast<nsThread*>(thread.get())->SetUseHangMonitor(true);
          }),
      stackSize);

  if (NS_FAILED(rv)) {
    return nullptr;
  }

  CompositorBridgeParent::Setup();
  ImageBridgeParent::Setup();

  return compositorThread.forget();
}

void CompositorThreadHolder::Start() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main Thread!");
  MOZ_ASSERT(!sCompositorThreadHolder,
             "The compositor thread has already been started!");

  // We unset the holder instead of asserting because failing to start the
  // compositor thread may not be a fatal error. As long as this succeeds in
  // either the GPU process or the UI process, the user will have a usable
  // browser. If we get neither, it will crash as soon as we try to post to the
  // compositor thread for the first time.
  sProfilerThreadId = ProfilerThreadId();
  sCompositorThreadHolder = new CompositorThreadHolder();
  if (!sCompositorThreadHolder->GetCompositorThread()) {
    gfxCriticalNote << "Compositor thread not started ("
                    << XRE_IsParentProcess() << ")";
    sCompositorThreadHolder = nullptr;
  }
}

void CompositorThreadHolder::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main Thread!");
  if (!sCompositorThreadHolder) {
    // We've already shutdown or never started.
    return;
  }

  ImageBridgeParent::Shutdown();
  gfx::VRManagerParent::Shutdown();
  MediaSystemResourceService::Shutdown();
  CompositorManagerParent::Shutdown();
  CanvasTranslator::Shutdown();
  gfx::gfxGradientCache::Shutdown();

  // Ensure there are no pending tasks that would cause an access to the
  // thread's HangMonitor. APZ and Canvas can keep a reference to the compositor
  // thread and may continue to dispatch tasks on it as the system shuts down.
  CompositorThread()->Dispatch(NS_NewRunnableFunction(
      "CompositorThreadHolder::Shutdown",
      [compositorThreadHolder =
           RefPtr<CompositorThreadHolder>(sCompositorThreadHolder),
       backgroundHangMonitor = UniquePtr<mozilla::BackgroundHangMonitor>(
           sBackgroundHangMonitor)]() {
        nsCOMPtr<nsIThread> thread = NS_GetCurrentThread();
        static_cast<nsThread*>(thread.get())->SetUseHangMonitor(false);
      }));

  sCompositorThreadHolder = nullptr;
  sBackgroundHangMonitor = nullptr;

  SpinEventLoopUntil("CompositorThreadHolder::Shutdown"_ns, [&]() {
    bool finished = sFinishedCompositorShutDown;
    return finished;
  });

  // At this point, the CompositorThreadHolder instance will have been
  // destroyed, but the compositor thread itself may still be running due to
  // APZ/Canvas code holding a reference to the underlying
  // nsIThread/nsISerialEventTarget. Any tasks scheduled to run on the
  // compositor thread earlier in this function will have been run to
  // completion.
  CompositorBridgeParent::FinishShutdown();
}

/* static */
bool CompositorThreadHolder::IsInCompositorThread() {
  if (!CompositorThread()) {
    return false;
  }
  bool in = false;
  MOZ_ALWAYS_SUCCEEDS(CompositorThread()->IsOnCurrentThread(&in));
  return in;
}

/* static */
ProfilerThreadId CompositorThreadHolder::GetThreadId() {
  return sCompositorThreadHolder ? sProfilerThreadId : ProfilerThreadId();
}

}  // namespace layers
}  // namespace mozilla

bool NS_IsInCompositorThread() {
  return mozilla::layers::CompositorThreadHolder::IsInCompositorThread();
}
