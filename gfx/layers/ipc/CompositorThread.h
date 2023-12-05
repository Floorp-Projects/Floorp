/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_layers_CompositorThread_h
#define mozilla_layers_CompositorThread_h

#include "nsISupportsImpl.h"
#include "nsIThread.h"

namespace mozilla::baseprofiler {
class BaseProfilerThreadId;
}
using ProfilerThreadId = mozilla::baseprofiler::BaseProfilerThreadId;
class nsIThread;

namespace mozilla {
namespace layers {

class CompositorThreadHolder final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_MAIN_THREAD(
      CompositorThreadHolder)

 public:
  CompositorThreadHolder();

  nsIThread* GetCompositorThread() const { return mCompositorThread; }

  /**
   * Returns true if the calling thread is the compositor thread. This works
   * even if the CompositorThread has begun to shutdown.
   */
  bool IsInThread() {
    bool rv = false;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mCompositorThread->IsOnCurrentThread(&rv)));
    return rv;
  }

  nsresult Dispatch(already_AddRefed<nsIRunnable> event,
                    uint32_t flags = nsIEventTarget::DISPATCH_NORMAL) {
    return mCompositorThread->Dispatch(std::move(event), flags);
  }

  static CompositorThreadHolder* GetSingleton();

  static bool IsActive() { return !!GetSingleton(); }

  /**
   * Creates the compositor thread and the global compositor map.
   */
  static void Start();

  /*
   * Waits for all [CrossProcess]CompositorBridgeParents to shutdown and
   * releases compositor-thread owned resources.
   */
  static void Shutdown();

  // Returns true if the calling thread is the compositor thread.
  static bool IsInCompositorThread();

  // Thread id to use as a MarkerThreadId option for profiler markers.
  static ProfilerThreadId GetThreadId();

 private:
  ~CompositorThreadHolder();

  const nsCOMPtr<nsIThread> mCompositorThread;

  static already_AddRefed<nsIThread> CreateCompositorThread();

  friend class CompositorBridgeParent;
};

nsIThread* CompositorThread();

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CompositorThread_h
