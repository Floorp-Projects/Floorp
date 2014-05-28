/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

using namespace mozilla;

/* static */ void
WebGLContext::ContextLossCallbackStatic(nsITimer* timer, void* thisPointer)
{
    (void)timer;
    WebGLContext* context = static_cast<WebGLContext*>(thisPointer);

    context->TerminateContextLossTimer();

    context->UpdateContextLossStatus();
}

void
WebGLContext::RunContextLossTimer()
{
    // If the timer was already running, don't restart it here. Instead,
    // wait until the previous call is done, then fire it one more time.
    // This is an optimization to prevent unnecessary
    // cross-communication between threads.
    if (mContextLossTimerRunning) {
        mRunContextLossTimerAgain = true;
        return;
    }
    mContextRestorer->InitWithFuncCallback(ContextLossCallbackStatic,
                                           static_cast<void*>(this),
                                           1000,
                                           nsITimer::TYPE_ONE_SHOT);
    mContextLossTimerRunning = true;
    mRunContextLossTimerAgain = false;
}

void
WebGLContext::TerminateContextLossTimer()
{
    if (!mContextLossTimerRunning)
        return;

    mContextRestorer->Cancel();
    mContextLossTimerRunning = false;

    if (mRunContextLossTimerAgain) {
        RunContextLossTimer();
    }
}
