/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContextLossHandler.h"

#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLContextLossHandler::WebGLContextLossHandler(WebGLContext* webgl)
    : mWeakWebGL(webgl)
    , mTimer(do_CreateInstance(NS_TIMER_CONTRACTID))
    , mIsTimerRunning(false)
    , mShouldRunTimerAgain(false)
    , mIsDisabled(false)
#ifdef DEBUG
    , mThread(NS_GetCurrentThread())
#endif
{
}

WebGLContextLossHandler::~WebGLContextLossHandler()
{
    MOZ_ASSERT(!mIsTimerRunning);
}

void
WebGLContextLossHandler::StartTimer(unsigned long delayMS)
{
    // We can't pass a TemporaryRef through InitWithFuncCallback, so we
    // should do the AddRef/Release manually.
    this->AddRef();

    mTimer->InitWithFuncCallback(StaticTimerCallback,
                                 static_cast<void*>(this),
                                 delayMS,
                                 nsITimer::TYPE_ONE_SHOT);
}

/* static */ void
WebGLContextLossHandler::StaticTimerCallback(nsITimer*,
                                             void* voidHandler)
{
    typedef WebGLContextLossHandler T;
    T* handler = static_cast<T*>(voidHandler);

    handler->TimerCallback();

    // Release the AddRef from StartTimer.
    handler->Release();
}

void
WebGLContextLossHandler::TimerCallback()
{
    MOZ_ASSERT(NS_GetCurrentThread() == mThread);

    if (mIsDisabled)
        return;

    MOZ_ASSERT(mIsTimerRunning);
    mIsTimerRunning = false;

    // If we need to run the timer again, restart it immediately.
    // Otherwise, the code we call into below might *also* try to
    // restart it.
    if (mShouldRunTimerAgain) {
        RunTimer();
        MOZ_ASSERT(mIsTimerRunning);
    }

    if (mWeakWebGL) {
        mWeakWebGL->UpdateContextLossStatus();
    }
}

void
WebGLContextLossHandler::RunTimer()
{
    MOZ_ASSERT(!mIsDisabled);

    // If the timer was already running, don't restart it here. Instead,
    // wait until the previous call is done, then fire it one more time.
    // This is an optimization to prevent unnecessary
    // cross-communication between threads.
    if (mIsTimerRunning) {
        mShouldRunTimerAgain = true;
        return;
    }

    StartTimer(1000);

    mIsTimerRunning = true;
    mShouldRunTimerAgain = false;
}

void
WebGLContextLossHandler::DisableTimer()
{
    if (!mIsDisabled)
        return;

    mIsDisabled = true;

    // We can't just Cancel() the timer, as sometimes we end up
    // receiving a callback after calling Cancel(). This could cause us
    // to receive the callback after object destruction.

    // Instead, we let the timer finish, but ignore it.

    if (!mIsTimerRunning)
        return;

    mTimer->SetDelay(0);
}

} // namespace mozilla
