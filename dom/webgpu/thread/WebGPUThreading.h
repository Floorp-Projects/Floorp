/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_WEBGPU_THREADING_H
#define MOZILLA_WEBGPU_THREADING_H

#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "base/thread.h"           // for Thread
#include "mozilla/layers/SynchronousTask.h"

namespace mozilla {
namespace webgpu {

class WebGPUThreading final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(WebGPUThreading)

public:
  /// Can only be called from the main thread.
  static void Start();

  /// Can only be called from the main thread.
  static void ShutDown();

  /// Can be called from any thread. Returns `nullptr` if
  /// the threading is not initialized.
  static MessageLoop* GetLoop();

private:
  explicit WebGPUThreading(base::Thread* aThread);
  ~WebGPUThreading();

  base::Thread* const mThread;
};

}
}

#endif // MOZILLA_WEBGPU_THREADING_H
