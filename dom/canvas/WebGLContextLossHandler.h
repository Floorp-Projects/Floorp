/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_CONTEXT_LOSS_HANDLER_H_
#define WEBGL_CONTEXT_LOSS_HANDLER_H_

#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"

class nsIThread;
class nsITimer;

namespace mozilla {
class WebGLContext;

class WebGLContextLossHandler final : public SupportsWeakPtr<WebGLContextLossHandler>
{
    WebGLContext* const mWebGL;
    const nsCOMPtr<nsITimer> mTimer; // If we don't hold a ref to the timer, it will think
    bool mTimerPending;              // that it's been discarded, and be canceled 'for our
    bool mShouldRunTimerAgain;       // convenience'.
#ifdef DEBUG
    nsISerialEventTarget* const mEventTarget;
#endif

    friend class WatchdogTimerEvent;

public:
    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(WebGLContextLossHandler)

    explicit WebGLContextLossHandler(WebGLContext* webgl);
    ~WebGLContextLossHandler();

    void RunTimer();

private:
    void TimerCallback();
};

} // namespace mozilla

#endif // WEBGL_CONTEXT_LOSS_HANDLER_H_
