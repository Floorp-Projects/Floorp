/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePump.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStringGlue.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "prthread.h"

#include "base/logging.h"
#include "base/scoped_nsautorelease_pool.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

using mozilla::ipc::DoWorkRunnable;
using mozilla::ipc::MessagePump;
using mozilla::ipc::MessagePumpForChildProcess;
using base::TimeTicks;

NS_IMPL_ISUPPORTS2(DoWorkRunnable, nsIRunnable, nsITimerCallback)

NS_IMETHODIMP
DoWorkRunnable::Run()
{
  MessageLoop* loop = MessageLoop::current();
  NS_ASSERTION(loop, "Shouldn't be null!");
  if (loop) {
    bool nestableTasksAllowed = loop->NestableTasksAllowed();

    // MessageLoop::RunTask() disallows nesting, but our Frankenventloop
    // will always dispatch DoWork() below from what looks to
    // MessageLoop like a nested context.  So we unconditionally allow
    // nesting here.
    loop->SetNestableTasksAllowed(true);
    loop->DoWork();
    loop->SetNestableTasksAllowed(nestableTasksAllowed);
  }
  return NS_OK;
}

NS_IMETHODIMP
DoWorkRunnable::Notify(nsITimer* aTimer)
{
  MessageLoop* loop = MessageLoop::current();
  NS_ASSERTION(loop, "Shouldn't be null!");
  if (loop) {
    mPump->DoDelayedWork(loop);
  }
  return NS_OK;
}

MessagePump::MessagePump()
: mThread(nullptr)
{
  mDoWorkEvent = new DoWorkRunnable(this);
}

void
MessagePump::Run(MessagePump::Delegate* aDelegate)
{
  NS_ASSERTION(keep_running_, "Quit must have been called outside of Run!");
  NS_ASSERTION(NS_IsMainThread(), "Called Run on the wrong thread!");

  mThread = NS_GetCurrentThread();
  NS_ASSERTION(mThread, "This should never be null!");

  mDelayedWorkTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  NS_ASSERTION(mDelayedWorkTimer, "Failed to create timer!");

  base::ScopedNSAutoreleasePool autoReleasePool;

  for (;;) {
    autoReleasePool.Recycle();

    bool did_work = NS_ProcessNextEvent(mThread, false) ? true : false;
    if (!keep_running_)
      break;

    // NB: it is crucial *not* to directly call |aDelegate->DoWork()|
    // here.  To ensure that MessageLoop tasks and XPCOM events have
    // equal priority, we sensitively rely on processing exactly one
    // Task per DoWorkRunnable XPCOM event.

#ifdef MOZ_WIDGET_ANDROID
    // This processes messages in the Android Looper. Note that we only
    // get here if the normal Gecko event loop has been awoken above.
    // Bug 750713
    did_work |= AndroidBridge::Bridge()->PumpMessageLoop();
#endif

    did_work |= aDelegate->DoDelayedWork(&delayed_work_time_);

    if (did_work && delayed_work_time_.is_null())
      mDelayedWorkTimer->Cancel();

    if (!keep_running_)
      break;

    if (did_work)
      continue;

    did_work = aDelegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    // This will either sleep or process an event.
    NS_ProcessNextEvent(mThread, true);
  }

  mDelayedWorkTimer->Cancel();

  keep_running_ = true;
}

void
MessagePump::ScheduleWork()
{
  // Make sure the event loop wakes up.
  if (mThread) {
    mThread->Dispatch(mDoWorkEvent, NS_DISPATCH_NORMAL);
  }
  else {
    // Some things (like xpcshell) don't use the app shell and so Run hasn't
    // been called. We still need to wake up the main thread.
    NS_DispatchToMainThread(mDoWorkEvent, NS_DISPATCH_NORMAL);
  }
  event_.Signal();
}

void
MessagePump::ScheduleWorkForNestedLoop()
{
  // This method is called when our MessageLoop has just allowed
  // nested tasks.  In our setup, whenever that happens we know that
  // DoWork() will be called "soon", so there's no need to pay the
  // cost of what will be a no-op nsThread::Dispatch(mDoWorkEvent).
}

void
MessagePump::ScheduleDelayedWork(const base::TimeTicks& aDelayedTime)
{
  if (!mDelayedWorkTimer) {
    mDelayedWorkTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (!mDelayedWorkTimer) {
        // Called before XPCOM has started up? We can't do this correctly.
        NS_WARNING("Delayed task might not run!");
        delayed_work_time_ = aDelayedTime;
        return;
    }
  }

  if (!delayed_work_time_.is_null()) {
    mDelayedWorkTimer->Cancel();
  }

  delayed_work_time_ = aDelayedTime;

  // TimeDelta's constructor initializes to 0
  base::TimeDelta delay;
  if (aDelayedTime > base::TimeTicks::Now())
    delay = aDelayedTime - base::TimeTicks::Now();

  uint32_t delayMS = uint32_t(delay.InMilliseconds());
  mDelayedWorkTimer->InitWithCallback(mDoWorkEvent, delayMS,
                                      nsITimer::TYPE_ONE_SHOT);
}

void
MessagePump::DoDelayedWork(base::MessagePump::Delegate* aDelegate)
{
  aDelegate->DoDelayedWork(&delayed_work_time_);
  if (!delayed_work_time_.is_null()) {
    ScheduleDelayedWork(delayed_work_time_);
  }
}

#ifdef DEBUG
namespace {
MessagePump::Delegate* gFirstDelegate = nullptr;
}
#endif

void
MessagePumpForChildProcess::Run(MessagePump::Delegate* aDelegate)
{
  if (mFirstRun) {
#ifdef DEBUG
    NS_ASSERTION(aDelegate && gFirstDelegate == nullptr, "Huh?!");
    gFirstDelegate = aDelegate;
#endif
    mFirstRun = false;
    if (NS_FAILED(XRE_RunAppShell())) {
        NS_WARNING("Failed to run app shell?!");
    }
#ifdef DEBUG
    NS_ASSERTION(aDelegate && aDelegate == gFirstDelegate, "Huh?!");
    gFirstDelegate = nullptr;
#endif
    return;
  }

#ifdef DEBUG
  NS_ASSERTION(aDelegate && aDelegate == gFirstDelegate, "Huh?!");
#endif

  // We can get to this point in startup with Tasks in our loop's
  // incoming_queue_ or pending_queue_, but without a matching
  // DoWorkRunnable().  In MessagePump::Run() above, we sensitively
  // depend on *not* directly calling delegate->DoWork(), because that
  // prioritizes Tasks above XPCOM events.  However, from this point
  // forward, any Task posted to our loop is guaranteed to have a
  // DoWorkRunnable enqueued for it.
  //
  // So we just flush the pending work here and move on.
  MessageLoop* loop = MessageLoop::current();
  bool nestableTasksAllowed = loop->NestableTasksAllowed();
  loop->SetNestableTasksAllowed(true);

  while (aDelegate->DoWork());

  loop->SetNestableTasksAllowed(nestableTasksAllowed);


  // Really run.
  mozilla::ipc::MessagePump::Run(aDelegate);
}
