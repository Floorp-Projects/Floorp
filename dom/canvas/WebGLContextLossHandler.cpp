/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContextLossHandler.h"
#include "WebGLContext.h"

namespace mozilla {

void MaybeUpdateContextLoss(WeakPtr<WebGLContext> weakCxt) {
  RefPtr<WebGLContext> cxt = weakCxt.get();
  if (!cxt) {
    return;
  }
  cxt->UpdateContextLossStatus();
}

WebGLContextLossHandler::WebGLContextLossHandler(WebGLContext* webgl) {
  MOZ_ASSERT(webgl);
  WeakPtr<WebGLContext> weakCxt = webgl;
  mRunnable = NS_NewRunnableFunction("WebGLContextLossHandler", [weakCxt]() {
    MaybeUpdateContextLoss(weakCxt);
  });
  MOZ_ASSERT(mRunnable);
}

WebGLContextLossHandler::~WebGLContextLossHandler() {}

////////////////////

void WebGLContextLossHandler::RunTimer() {
  const uint32_t kDelayMS = 1000;
  MOZ_ASSERT(MessageLoop::current());
  // Only create a new task if one isn't already queued.
  if (mTimerIsScheduled.compareExchange(false, true)) {
    MessageLoop::current()->PostDelayedTask(do_AddRef(mRunnable), kDelayMS);
  }
}

void WebGLContextLossHandler::ClearTimer() { mTimerIsScheduled = false; }

}  // namespace mozilla
