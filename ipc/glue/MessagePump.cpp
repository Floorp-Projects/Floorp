/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePump.h"

#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsITimer.h"
#include "nsICancelableRunnable.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_nsautorelease_pool.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsTimerImpl.h"
#include "nsXULAppAPI.h"
#include "prthread.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

using base::TimeTicks;
using namespace mozilla::ipc;

NS_DEFINE_NAMED_CID(NS_TIMER_CID);

static mozilla::DebugOnly<MessagePump::Delegate*> gFirstDelegate;

namespace mozilla {
namespace ipc {

class DoWorkRunnable MOZ_FINAL : public nsICancelableRunnable,
                                 public nsITimerCallback
{
public:
  explicit DoWorkRunnable(MessagePump* aPump)
  : mPump(aPump)
  {
    MOZ_ASSERT(aPump);
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSICANCELABLERUNNABLE

private:
  ~DoWorkRunnable()
  { }

  MessagePump* mPump;
  // DoWorkRunnable is designed as a stateless singleton.  Do not add stateful
  // members here!
};

} /* namespace ipc */
} /* namespace mozilla */

MessagePump::MessagePump()
: mThread(nullptr)
{
  mDoWorkEvent = new DoWorkRunnable(this);
}

MessagePump::~MessagePump()
{
}

void
MessagePump::Run(MessagePump::Delegate* aDelegate)
{
  MOZ_ASSERT(keep_running_);
  MOZ_ASSERT(NS_IsMainThread(),
             "Use mozilla::ipc::MessagePumpForNonMainThreads instead!");

  mThread = NS_GetCurrentThread();
  MOZ_ASSERT(mThread);

  mDelayedWorkTimer = do_CreateInstance(kNS_TIMER_CID);
  MOZ_ASSERT(mDelayedWorkTimer);

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
    if (MOZ_LIKELY(AndroidBridge::HasEnv())) {
        did_work |= mozilla::widget::android::GeckoAppShell::PumpMessageLoop();
    }
#endif

    did_work |= aDelegate->DoDelayedWork(&delayed_work_time_);

if (did_work && delayed_work_time_.is_null()
#ifdef MOZ_NUWA_PROCESS
    && (!IsNuwaReady() || !IsNuwaProcess())
#endif
   )
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

#ifdef MOZ_NUWA_PROCESS
  if (!IsNuwaReady() || !IsNuwaProcess())
#endif
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
    NS_DispatchToMainThread(mDoWorkEvent);
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
#ifdef MOZ_NUWA_PROCESS
  if (IsNuwaReady() && IsNuwaProcess())
    return;
#endif

  if (!mDelayedWorkTimer) {
    mDelayedWorkTimer = do_CreateInstance(kNS_TIMER_CID);
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

NS_IMPL_ISUPPORTS(DoWorkRunnable, nsIRunnable, nsITimerCallback,
                                  nsICancelableRunnable)

NS_IMETHODIMP
DoWorkRunnable::Run()
{
  MessageLoop* loop = MessageLoop::current();
  MOZ_ASSERT(loop);

  bool nestableTasksAllowed = loop->NestableTasksAllowed();

  // MessageLoop::RunTask() disallows nesting, but our Frankenventloop will
  // always dispatch DoWork() below from what looks to MessageLoop like a nested
  // context.  So we unconditionally allow nesting here.
  loop->SetNestableTasksAllowed(true);
  loop->DoWork();
  loop->SetNestableTasksAllowed(nestableTasksAllowed);

  return NS_OK;
}

NS_IMETHODIMP
DoWorkRunnable::Notify(nsITimer* aTimer)
{
  MessageLoop* loop = MessageLoop::current();
  MOZ_ASSERT(loop);

  mPump->DoDelayedWork(loop);

  return NS_OK;
}

NS_IMETHODIMP
DoWorkRunnable::Cancel()
{
  // Workers require cancelable runnables, but we can't really cancel cleanly
  // here.  If we don't process this runnable then we will leave something
  // unprocessed in the message_loop.  Therefore, eagerly complete our work
  // instead by immediately calling Run().  Run() should be called separately
  // after this.  Unfortunately we cannot use flags to verify this because
  // DoWorkRunnable is a stateless singleton that can be in the event queue
  // multiple times simultaneously.
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(Run()));
  return NS_OK;
}

void
MessagePumpForChildProcess::Run(base::MessagePump::Delegate* aDelegate)
{
  if (mFirstRun) {
    MOZ_ASSERT(aDelegate && !gFirstDelegate);
    gFirstDelegate = aDelegate;

    mFirstRun = false;
    if (NS_FAILED(XRE_RunAppShell())) {
        NS_WARNING("Failed to run app shell?!");
    }

    MOZ_ASSERT(aDelegate && aDelegate == gFirstDelegate);
    gFirstDelegate = nullptr;

    return;
  }

  MOZ_ASSERT(aDelegate && aDelegate == gFirstDelegate);

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

void
MessagePumpForNonMainThreads::Run(base::MessagePump::Delegate* aDelegate)
{
  MOZ_ASSERT(keep_running_);
  MOZ_ASSERT(!NS_IsMainThread(), "Use mozilla::ipc::MessagePump instead!");

  mThread = NS_GetCurrentThread();
  MOZ_ASSERT(mThread);

  mDelayedWorkTimer = do_CreateInstance(kNS_TIMER_CID);
  MOZ_ASSERT(mDelayedWorkTimer);

  if (NS_FAILED(mDelayedWorkTimer->SetTarget(mThread))) {
    MOZ_CRASH("Failed to set timer target!");
  }

  // Chromium event notifications to be processed will be received by this
  // event loop as a DoWorkRunnables via ScheduleWork. Chromium events that
  // were received before our mThread is valid, however, will not generate
  // runnable wrappers. We must process any of these before we enter this
  // loop, or we will forever have unprocessed chromium messages in our queue.
  //
  // Note we would like to request a flush of the chromium event queue
  // using a runnable on the xpcom side, but some thread implementations
  // (dom workers) get cranky if we call ScheduleWork here (ScheduleWork
  // calls dispatch on mThread) before the thread processes an event. As
  // such, clear the queue manually.
  while (aDelegate->DoWork()) {
  }

  base::ScopedNSAutoreleasePool autoReleasePool;
  for (;;) {
    autoReleasePool.Recycle();

    bool didWork = NS_ProcessNextEvent(mThread, false) ? true : false;
    if (!keep_running_) {
      break;
    }

    didWork |= aDelegate->DoDelayedWork(&delayed_work_time_);

    if (didWork && delayed_work_time_.is_null()) {
      mDelayedWorkTimer->Cancel();
    }

    if (!keep_running_) {
      break;
    }

    if (didWork) {
      continue;
    }

    didWork = aDelegate->DoIdleWork();
    if (!keep_running_) {
      break;
    }

    if (didWork) {
      continue;
    }

    // This will either sleep or process an event.
    NS_ProcessNextEvent(mThread, true);
  }

  mDelayedWorkTimer->Cancel();

  keep_running_ = true;
}

#if defined(XP_WIN)

NS_IMPL_QUERY_INTERFACE(MessagePumpForNonMainUIThreads, nsIThreadObserver)

#define CHECK_QUIT_STATE { if (state_->should_quit) { break; } }

void MessagePumpForNonMainUIThreads::DoRunLoop()
{
  // If this is a chromium thread and no nsThread is associated
  // with it, this call will create a new nsThread.
  mThread = NS_GetCurrentThread();
  MOZ_ASSERT(mThread);

  // Set the main thread observer so we can wake up when
  // xpcom events need to get processed.
  nsCOMPtr<nsIThreadInternal> ti(do_QueryInterface(mThread));
  MOZ_ASSERT(ti);
  ti->SetObserver(this);

  base::ScopedNSAutoreleasePool autoReleasePool;
  for (;;) {
    autoReleasePool.Recycle();

    bool didWork = NS_ProcessNextEvent(mThread, false);

    didWork |= ProcessNextWindowsMessage();
    CHECK_QUIT_STATE

    didWork |= state_->delegate->DoWork();
    CHECK_QUIT_STATE

    didWork |= state_->delegate->DoDelayedWork(&delayed_work_time_);
    if (didWork && delayed_work_time_.is_null()) {
      KillTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this));
    }
    CHECK_QUIT_STATE

    if (didWork) {
      continue;
    }

    didWork = state_->delegate->DoIdleWork();
    CHECK_QUIT_STATE

    SetInWait();
    bool hasWork = NS_HasPendingEvents(mThread);
    if (didWork || hasWork) {
      ClearInWait();
      continue;
    }
    WaitForWork(); // Calls MsgWaitForMultipleObjectsEx(QS_ALLINPUT)
    ClearInWait();
  }

  ClearInWait();

  ti->SetObserver(nullptr);
}

NS_IMETHODIMP
MessagePumpForNonMainUIThreads::OnDispatchedEvent(nsIThreadInternal *thread)
{
  // If our thread is sleeping in DoRunLoop's call to WaitForWork() and an
  // event posts to the nsIThread event queue - break our thread out of
  // chromium's WaitForWork.
  if (GetInWait()) {
    ScheduleWork();
  }
  return NS_OK;
}

NS_IMETHODIMP
MessagePumpForNonMainUIThreads::OnProcessNextEvent(nsIThreadInternal *thread,
                                                   bool mayWait,
                                                   uint32_t recursionDepth)
{
  return NS_OK;
}

NS_IMETHODIMP
MessagePumpForNonMainUIThreads::AfterProcessNextEvent(nsIThreadInternal *thread,
                                                      uint32_t recursionDepth,
                                                      bool eventWasProcessed)
{
  return NS_OK;
}

#endif // XP_WIN
