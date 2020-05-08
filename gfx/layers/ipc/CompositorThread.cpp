/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CompositorThread.h"
#include "MainThreadUtils.h"
#include "nsThreadUtils.h"
#include "CompositorBridgeParent.h"
#include "mozilla/layers/CanvasTranslator.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/media/MediaSystemResourceService.h"
#include "VRManagerParent.h"

namespace mozilla {
namespace layers {

static StaticRefPtr<CompositorThreadHolder> sCompositorThreadHolder;
static bool sFinishedCompositorShutDown = false;

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
  MOZ_ASSERT(NS_IsMainThread());
  sFinishedCompositorShutDown = true;
}

/* static */ already_AddRefed<nsIThread>
CompositorThreadHolder::CreateCompositorThread() {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!sCompositorThreadHolder,
             "The compositor thread has already been started!");

  nsCOMPtr<nsIThread> compositorThread;
  nsresult rv =
      NS_NewNamedThread("Compositor", getter_AddRefs(compositorThread));

  // TODO re-enable hangout monitor. Will be done in a follow-up thread
  //  base::Thread::Options options;
  //  /* Timeout values are powers-of-two to enable us get better data.
  //     128ms is chosen for transient hangs because 8Hz should be the minimally
  //     acceptable goal for Compositor responsiveness (normal goal is 60Hz). */
  //  options.transient_hang_timeout = 128;  // milliseconds
  //  /* 2048ms is chosen for permanent hangs because it's longer than most
  //   * Compositor hangs seen in the wild, but is short enough to not miss
  //   getting
  //   * native hang stacks. */
  //  options.permanent_hang_timeout = 2048;  // milliseconds

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

  sCompositorThreadHolder = nullptr;

  // No locking is needed around sFinishedCompositorShutDown because it is only
  // ever accessed on the main thread.
  SpinEventLoopUntil([&]() { return sFinishedCompositorShutDown; });

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

}  // namespace layers
}  // namespace mozilla

bool NS_IsInCompositorThread() {
  return mozilla::layers::CompositorThreadHolder::IsInCompositorThread();
}
