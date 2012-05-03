/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "MessagePump.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStringGlue.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "pratom.h"
#include "prthread.h"

#include "base/logging.h"
#include "base/scoped_nsautorelease_pool.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

using mozilla::ipc::DoWorkRunnable;
using mozilla::ipc::MessagePump;
using mozilla::ipc::MessagePumpForChildProcess;
using base::Time;

NS_IMPL_THREADSAFE_ISUPPORTS2(DoWorkRunnable, nsIRunnable, nsITimerCallback)

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
: mThread(nsnull)
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

    did_work |= aDelegate->DoWork();
    if (!keep_running_)
      break;

#ifdef MOZ_WIDGET_ANDROID
    // This processes messages in the Android Looper. Note that we only
    // get here if the normal Gecko event loop has been awoken above.
    // Bug 750713
    AndroidBridge::Bridge()->PumpMessageLoop();
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
MessagePump::ScheduleDelayedWork(const base::Time& aDelayedTime)
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

  base::TimeDelta delay = aDelayedTime - base::Time::Now();
  PRUint32 delayMS = PRUint32(delay.InMilliseconds());
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
MessagePump::Delegate* gFirstDelegate = nsnull;
}
#endif

void
MessagePumpForChildProcess::Run(MessagePump::Delegate* aDelegate)
{
  if (mFirstRun) {
#ifdef DEBUG
    NS_ASSERTION(aDelegate && gFirstDelegate == nsnull, "Huh?!");
    gFirstDelegate = aDelegate;
#endif
    mFirstRun = false;
    if (NS_FAILED(XRE_RunAppShell())) {
        NS_WARNING("Failed to run app shell?!");
    }
#ifdef DEBUG
    NS_ASSERTION(aDelegate && aDelegate == gFirstDelegate, "Huh?!");
    gFirstDelegate = nsnull;
#endif
    return;
  }

#ifdef DEBUG
  NS_ASSERTION(aDelegate && aDelegate == gFirstDelegate, "Huh?!");
#endif
  // Really run.
  mozilla::ipc::MessagePump::Run(aDelegate);
}
