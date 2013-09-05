/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

using namespace mozilla;

/* static */ void
WebGLContext::RobustnessTimerCallbackStatic(nsITimer* timer, void *thisPointer) {
    static_cast<WebGLContext*>(thisPointer)->RobustnessTimerCallback(timer);
}

void
WebGLContext::SetupContextLossTimer() {
    // If the timer was already running, don't restart it here. Instead,
    // wait until the previous call is done, then fire it one more time.
    // This is an optimization to prevent unnecessary cross-communication
    // between threads.
    if (mContextLossTimerRunning) {
        mDrawSinceContextLossTimerSet = true;
        return;
    }

    mContextRestorer->InitWithFuncCallback(RobustnessTimerCallbackStatic,
                                            static_cast<void*>(this),
                                            1000,
                                            nsITimer::TYPE_ONE_SHOT);
    mContextLossTimerRunning = true;
    mDrawSinceContextLossTimerSet = false;
}

void
WebGLContext::TerminateContextLossTimer() {
    if (mContextLossTimerRunning) {
        mContextRestorer->Cancel();
        mContextLossTimerRunning = false;
    }
}
