/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_layers_CompositorThread_h
#define mozilla_layers_CompositorThread_h

#include "base/basictypes.h"            // for DISALLOW_EVIL_CONSTRUCTORS
#include "base/platform_thread.h"       // for PlatformThreadId
#include "base/thread.h"                // for Thread
#include "base/message_loop.h"
#include "nsISupportsImpl.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

namespace mozilla {
namespace layers {

class CompositorThreadHolder final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(CompositorThreadHolder)

public:
  CompositorThreadHolder();

  base::Thread* GetCompositorThread() const {
    return mCompositorThread;
  }

  static CompositorThreadHolder* GetSingleton();

  static bool IsActive() {
    return !!GetSingleton();
  }

  /**
   * Creates the compositor thread and the global compositor map.
   */
  static void Start();

  /*
   * Waits for all [CrossProcess]CompositorBridgeParents to shutdown and
   * releases compositor-thread owned resources.
   */
  static void Shutdown();

  static MessageLoop* Loop();

private:
  ~CompositorThreadHolder();

  base::Thread* const mCompositorThread;

  static base::Thread* CreateCompositorThread();
  static void DestroyCompositorThread(base::Thread* aCompositorThread);

  friend class CompositorBridgeParent;
};

base::Thread* CompositorThread();

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CompositorThread_h
