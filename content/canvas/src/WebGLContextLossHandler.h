/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_CONTEXT_LOSS_HANDLER_H_
#define WEBGL_CONTEXT_LOSS_HANDLER_H_

#include "mozilla/DebugOnly.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"

class nsIThread;
class nsITimer;

namespace mozilla {
class WebGLContext;

class WebGLContextLossHandler
    : public RefCounted<WebGLContextLossHandler>
{
    WeakPtr<WebGLContext> mWeakWebGL;
    nsCOMPtr<nsITimer> mTimer;
    bool mIsTimerRunning;
    bool mShouldRunTimerAgain;
    bool mIsDisabled;
    DebugOnly<nsIThread*> mThread;

public:
    MOZ_DECLARE_REFCOUNTED_TYPENAME(WebGLContextLossHandler)

    WebGLContextLossHandler(WebGLContext* webgl);
    ~WebGLContextLossHandler();

    void RunTimer();
    void DisableTimer();

protected:
    void StartTimer(unsigned long delayMS);
    static void StaticTimerCallback(nsITimer*, void* tempRefForTimer);
    void TimerCallback();
};

} // namespace mozilla

#endif
