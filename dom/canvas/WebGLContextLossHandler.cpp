/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContextLossHandler.h"

#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "WebGLContext.h"
#include "mozilla/dom/WorkerPrivate.h"

namespace mozilla {

// -------------------------------------------------------------------
// Begin worker specific code
// -------------------------------------------------------------------

// On workers we can only dispatch CancelableRunnables, so we have to wrap the
// timer's EventTarget to use our own cancelable runnable

class ContextLossWorkerEventTarget final : public nsIEventTarget
{
public:
    explicit ContextLossWorkerEventTarget(nsIEventTarget* aEventTarget)
        : mEventTarget(aEventTarget)
    {
        MOZ_ASSERT(aEventTarget);
    }

    NS_DECL_NSIEVENTTARGET
    NS_DECL_THREADSAFE_ISUPPORTS

protected:
    ~ContextLossWorkerEventTarget() {}

private:
    nsCOMPtr<nsIEventTarget> mEventTarget;
};

class ContextLossWorkerRunnable final : public CancelableRunnable
{
public:
    explicit ContextLossWorkerRunnable(nsIRunnable* aRunnable)
        : mRunnable(aRunnable)
    {
    }

    nsresult Cancel() override;

    NS_FORWARD_NSIRUNNABLE(mRunnable->)

protected:
    ~ContextLossWorkerRunnable() {}

private:
    nsCOMPtr<nsIRunnable> mRunnable;
};

NS_IMPL_ISUPPORTS(ContextLossWorkerEventTarget, nsIEventTarget,
                  nsISupports)

NS_IMETHODIMP
ContextLossWorkerEventTarget::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags)
{
    nsCOMPtr<nsIRunnable> event(aEvent);
    return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
ContextLossWorkerEventTarget::Dispatch(already_AddRefed<nsIRunnable> aEvent,
                                       uint32_t aFlags)
{
    nsCOMPtr<nsIRunnable> eventRef(aEvent);
    RefPtr<ContextLossWorkerRunnable> wrappedEvent =
        new ContextLossWorkerRunnable(eventRef);
    return mEventTarget->Dispatch(wrappedEvent, aFlags);
}

NS_IMETHODIMP
ContextLossWorkerEventTarget::DelayedDispatch(already_AddRefed<nsIRunnable>,
                                              uint32_t)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ContextLossWorkerEventTarget::IsOnCurrentThread(bool* aResult)
{
    return mEventTarget->IsOnCurrentThread(aResult);
}

nsresult
ContextLossWorkerRunnable::Cancel()
{
    mRunnable = nullptr;
    return NS_OK;
}

// -------------------------------------------------------------------
// End worker-specific code
// -------------------------------------------------------------------

WebGLContextLossHandler::WebGLContextLossHandler(WebGLContext* webgl)
    : mWeakWebGL(webgl)
    , mTimer(do_CreateInstance(NS_TIMER_CONTRACTID))
    , mIsTimerRunning(false)
    , mShouldRunTimerAgain(false)
    , mIsDisabled(false)
    , mFeatureAdded(false)
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
    // We can't pass an already_AddRefed through InitWithFuncCallback, so we
    // should do the AddRef/Release manually.
    this->AddRef();

    mTimer->InitWithFuncCallback(StaticTimerCallback,
                                 static_cast<void*>(this),
                                 delayMS,
                                 nsITimer::TYPE_ONE_SHOT);
}

/*static*/ void
WebGLContextLossHandler::StaticTimerCallback(nsITimer*, void* voidHandler)
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
    MOZ_ASSERT(mIsTimerRunning);
    mIsTimerRunning = false;

    if (mIsDisabled)
        return;

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

    if (!NS_IsMainThread()) {
        dom::workers::WorkerPrivate* workerPrivate =
            dom::workers::GetCurrentThreadWorkerPrivate();
        nsCOMPtr<nsIEventTarget> target = workerPrivate->GetEventTarget();
        mTimer->SetTarget(new ContextLossWorkerEventTarget(target));
        if (!mFeatureAdded) {
            workerPrivate->AddFeature(this);
            mFeatureAdded = true;
        }
    }

    StartTimer(1000);

    mIsTimerRunning = true;
    mShouldRunTimerAgain = false;
}

void
WebGLContextLossHandler::DisableTimer()
{
    if (mIsDisabled)
        return;

    mIsDisabled = true;

    if (mFeatureAdded) {
        dom::workers::WorkerPrivate* workerPrivate =
            dom::workers::GetCurrentThreadWorkerPrivate();
        MOZ_RELEASE_ASSERT(workerPrivate, "GFX: No private worker created.");
        workerPrivate->RemoveFeature(this);
        mFeatureAdded = false;
    }

    // We can't just Cancel() the timer, as sometimes we end up
    // receiving a callback after calling Cancel(). This could cause us
    // to receive the callback after object destruction.

    // Instead, we let the timer finish, but ignore it.

    if (!mIsTimerRunning)
        return;

    mTimer->SetDelay(0);
}

bool
WebGLContextLossHandler::Notify(dom::workers::Status aStatus)
{
    bool isWorkerRunning = aStatus < dom::workers::Closing;
    if (!isWorkerRunning && mIsTimerRunning) {
        mIsTimerRunning = false;
        this->Release();
    }

    return true;
}

} // namespace mozilla
