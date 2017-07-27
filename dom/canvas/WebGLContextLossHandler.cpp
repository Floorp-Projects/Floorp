/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContextLossHandler.h"

#include "mozilla/DebugOnly.h"
#include "nsINamed.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "WebGLContext.h"

namespace mozilla {

class WatchdogTimerEvent final : public nsITimerCallback
                               , public nsINamed
{
    const WeakPtr<WebGLContextLossHandler> mHandler;

public:
    NS_DECL_ISUPPORTS

    explicit WatchdogTimerEvent(WebGLContextLossHandler* handler)
        : mHandler(handler)
    { }

    NS_IMETHOD GetName(nsACString& aName) override
    {
      aName.AssignLiteral("WatchdogTimerEvent");
      return NS_OK;
    }

private:
    virtual ~WatchdogTimerEvent() { }

    NS_IMETHOD Notify(nsITimer*) override {
        if (mHandler) {
            mHandler->TimerCallback();
        }
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(WatchdogTimerEvent, nsITimerCallback, nsINamed)

////////////////////////////////////////

WebGLContextLossHandler::WebGLContextLossHandler(WebGLContext* webgl)
    : mWebGL(webgl)
    , mTimer(do_CreateInstance(NS_TIMER_CONTRACTID))
    , mTimerPending(false)
    , mShouldRunTimerAgain(false)
#ifdef DEBUG
    , mEventTarget(GetCurrentThreadSerialEventTarget())
#endif
{
    MOZ_ASSERT(mEventTarget);
}

WebGLContextLossHandler::~WebGLContextLossHandler()
{
    const DebugOnly<nsISerialEventTarget*> callingThread = GetCurrentThreadSerialEventTarget();
    MOZ_ASSERT(!callingThread || mEventTarget->IsOnCurrentThread());
}

////////////////////

void
WebGLContextLossHandler::RunTimer()
{
    MOZ_ASSERT(mEventTarget->IsOnCurrentThread());

    // If the timer was already running, don't restart it here. Instead,
    // wait until the previous call is done, then fire it one more time.
    // This is also an optimization to prevent unnecessary
    // cross-communication between threads.
    if (mTimerPending) {
        mShouldRunTimerAgain = true;
        return;
    }

    const RefPtr<WatchdogTimerEvent> event = new WatchdogTimerEvent(this);
    const uint32_t kDelayMS = 1000;
    mTimer->InitWithCallback(event, kDelayMS, nsITimer::TYPE_ONE_SHOT);

    mTimerPending = true;
}

////////////////////

void
WebGLContextLossHandler::TimerCallback()
{
    MOZ_ASSERT(mEventTarget->IsOnCurrentThread());

    mTimerPending = false;

    const bool runOnceMore = mShouldRunTimerAgain;
    mShouldRunTimerAgain = false;

    mWebGL->UpdateContextLossStatus();

    if (runOnceMore && !mTimerPending) {
        RunTimer();
    }
}

} // namespace mozilla
