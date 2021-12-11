/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_CONTEXT_LOSS_HANDLER_H_
#define WEBGL_CONTEXT_LOSS_HANDLER_H_

#include "mozilla/Atomics.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
class WebGLContext;

class WebGLContextLossHandler final : public SupportsWeakPtr {
  RefPtr<Runnable> mRunnable;
  Atomic<bool> mTimerIsScheduled;

 public:
  explicit WebGLContextLossHandler(WebGLContext* webgl);
  ~WebGLContextLossHandler();

  void RunTimer();
  // Allow future queueing of context loss timer.
  void ClearTimer();
};

}  // namespace mozilla

#endif  // WEBGL_CONTEXT_LOSS_HANDLER_H_
