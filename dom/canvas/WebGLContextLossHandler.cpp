/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContextLossHandler.h"
#include "WebGLContext.h"

#include "base/message_loop.h"

namespace mozilla {

WebGLContextLossHandler::WebGLContextLossHandler(WebGLContext* const webgl)
    : mTimerIsScheduled(false) {
  const auto weak = WeakPtr<WebGLContext>(webgl);
  mRunnable = NS_NewRunnableFunction("WebGLContextLossHandler", [weak]() {
    const auto strong = RefPtr<WebGLContext>(weak);
    if (!strong) {
      return;
    }
    strong->CheckForContextLoss();
  });
}

WebGLContextLossHandler::~WebGLContextLossHandler() = default;

////////////////////

void WebGLContextLossHandler::RunTimer() {
  const uint32_t kDelayMS = 1000;
  MOZ_ASSERT(GetCurrentSerialEventTarget());
  // Only create a new task if one isn't already queued.
  if (mTimerIsScheduled.compareExchange(false, true)) {
    GetCurrentSerialEventTarget()->DelayedDispatch(do_AddRef(mRunnable),
                                                   kDelayMS);
  }
}

void WebGLContextLossHandler::ClearTimer() { mTimerIsScheduled = false; }

}  // namespace mozilla
