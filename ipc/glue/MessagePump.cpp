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

#include "nsIThread.h"
#include "nsITimer.h"

#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsStringGlue.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "pratom.h"
#include "prthread.h"

#include "base/logging.h"
#include "base/scoped_nsautorelease_pool.h"

using mozilla::ipc::MessagePump;
using mozilla::ipc::MessagePumpForChildProcess;

namespace {

void
TimerCallback(nsITimer* aTimer,
              void* aClosure)
{
  MessagePump* messagePump = reinterpret_cast<MessagePump*>(aClosure);
  messagePump->ScheduleWork();
}

} /* anonymous namespace */

class DoWorkRunnable : public nsRunnable
{
public:
  NS_IMETHOD Run() {
    MessageLoop* loop = MessageLoop::current();
    NS_ASSERTION(loop, "Shouldn't be null!");
    if (loop) {
      loop->DoWork();
    }
    return NS_OK;
  }
};

MessagePump::MessagePump()
: mThread(nsnull)
{
  mDummyEvent = new DoWorkRunnable();
  // I'm tired of adding OOM checks.
  NS_ADDREF(mDummyEvent);
}

MessagePump::~MessagePump()
{
  NS_RELEASE(mDummyEvent);
}

void
MessagePump::Run(MessagePump::Delegate* aDelegate)
{
  NS_ASSERTION(keep_running_, "Quit must have been called outside of Run!");

  NS_ASSERTION(NS_IsMainThread(),
               "This should only ever happen on Gecko's main thread!");

  mThread = NS_GetCurrentThread();
  NS_ASSERTION(mThread, "This should never be null!");

  nsCOMPtr<nsITimer> timer(do_CreateInstance(NS_TIMER_CONTRACTID));
  NS_ASSERTION(timer, "Failed to create timer!");

  base::ScopedNSAutoreleasePool autoReleasePool;

  for (;;) {
    autoReleasePool.Recycle();
    timer->Cancel();

    bool did_work = NS_ProcessNextEvent(mThread, PR_FALSE) ? true : false;
    if (!keep_running_)
      break;

    did_work |= aDelegate->DoWork();
    if (!keep_running_)
      break;

    did_work |= aDelegate->DoDelayedWork(&delayed_work_time_);
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    did_work = aDelegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    if (delayed_work_time_.is_null()) {
      // This will sleep or process native events.
      NS_ProcessNextEvent(mThread, PR_TRUE);
      continue;
    }

    base::TimeDelta delay = delayed_work_time_ - base::Time::Now();
    if (delay > base::TimeDelta()) {
      PRUint32 delayMS = PRUint32(delay.InMilliseconds());
      timer->InitWithFuncCallback(TimerCallback, this, delayMS,
                                  nsITimer::TYPE_ONE_SHOT);
      // This will sleep or process native events. The timer should wake us up
      // if nothing else does.
      NS_ProcessNextEvent(mThread, PR_TRUE);
      continue;
    }

    // It looks like delayed_work_time_ indicates a time in the past, so we
    // need to call DoDelayedWork now.
    delayed_work_time_ = base::Time();
  }

  timer->Cancel();

  keep_running_ = true;
}

void
MessagePump::ScheduleWork()
{
  // Make sure the event loop wakes up.
  if (mThread) {
    mThread->Dispatch(mDummyEvent, NS_DISPATCH_NORMAL);
    event_.Signal();
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
