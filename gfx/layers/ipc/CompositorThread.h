/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_layers_CompositorThread_h
#define mozilla_layers_CompositorThread_h

#include "nsISupportsImpl.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

class nsISerialEventTarget;
class nsIThread;

namespace mozilla {
namespace layers {

class CompositorThreadHolder final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(
      CompositorThreadHolder)

 public:
  CompositorThreadHolder();

  nsISerialEventTarget* GetCompositorThread() const {
    return mCompositorThread;
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

 private:
  ~CompositorThreadHolder();

  nsCOMPtr<nsIThread> mCompositorThread;

  static already_AddRefed<nsIThread> CreateCompositorThread();

  friend class CompositorBridgeParent;
};

nsISerialEventTarget* CompositorThread();

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CompositorThread_h
