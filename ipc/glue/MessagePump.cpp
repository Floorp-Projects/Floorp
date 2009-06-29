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

#include "nsIAppShell.h"
#include "nsIThread.h"
#include "nsIThreadInternal.h"

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsStringGlue.h"
#include "nsThreadUtils.h"
#include "nsWidgetsCID.h"
#include "prthread.h"

#include "base/logging.h"
#include "base/scoped_nsautorelease_pool.h"

using mozilla::ipc::MessagePump;
using mozilla::ipc::MessagePumpForChildProcess;

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

namespace mozilla {
namespace ipc {

class UIThreadObserver : public nsIThreadObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADOBSERVER

  UIThreadObserver(MessagePump& aPump,
                   nsIThreadObserver* aRealObserver)
  : mPump(aPump),
    mRealObserver(aRealObserver),
    mPRThread(PR_GetCurrentThread())
  {
    NS_ASSERTION(aRealObserver, "This should never be null!");
  }

private:
  MessagePump& mPump;
  nsIThreadObserver* mRealObserver;
  PRThread* mPRThread;
};

} /* namespace ipc */
} /* namespace mozilla */

using mozilla::ipc::UIThreadObserver;

NS_IMETHODIMP_(nsrefcnt)
UIThreadObserver::AddRef()
{
  return 2;
}

NS_IMETHODIMP_(nsrefcnt)
UIThreadObserver::Release()
{
  return 1;
}

NS_IMPL_QUERY_INTERFACE1(UIThreadObserver, nsIThreadObserver)

NS_IMETHODIMP
UIThreadObserver::OnDispatchedEvent(nsIThreadInternal* aThread)
{
  // XXXbent See if we can figure out some faster way of doing this. On posix
  // the Signal() call grabs a lock and on windows it calls SetEvent. Seems
  // like a good idea to avoid calling Signal if we're on the main thead.
  if (PR_GetCurrentThread() != mPRThread) {
    mPump.event_.Signal();
  }
  return mRealObserver->OnDispatchedEvent(aThread);
}

NS_IMETHODIMP
UIThreadObserver::OnProcessNextEvent(nsIThreadInternal* aThread,
                                     PRBool aMayWait,
                                     PRUint32 aRecursionDepth)
{
  return mRealObserver->OnProcessNextEvent(aThread, aMayWait, aRecursionDepth);
}

NS_IMETHODIMP
UIThreadObserver::AfterProcessNextEvent(nsIThreadInternal* aThread,
                                        PRUint32 aRecursionDepth)
{
  return mRealObserver->AfterProcessNextEvent(aThread, aRecursionDepth);
}

void
MessagePump::Run(MessagePump::Delegate* aDelegate)
{
  NS_ASSERTION(keep_running_, "Quit must have been called outside of Run!");

  nsCOMPtr<nsIThread> thread(do_GetCurrentThread());
  NS_ASSERTION(thread, "This should never be null!");

  nsCOMPtr<nsIThreadInternal> threadInternal(do_QueryInterface(thread));
  NS_ASSERTION(threadInternal, "QI failed?!");

  nsCOMPtr<nsIThreadObserver> realObserver;
  threadInternal->GetObserver(getter_AddRefs(realObserver));
  NS_ASSERTION(realObserver, "This should never be null!");

  UIThreadObserver observer(*this, realObserver);
  threadInternal->SetObserver(&observer);

#ifdef DEBUG
  {
    nsCOMPtr<nsIAppShell> appShell(do_QueryInterface(realObserver));
    NS_ASSERTION(appShell, "Should be the app shell!");
  }
#endif


  for (;;) {
    // XXXbent This looks fishy... Maybe have one that calls Recycle each time
    // through the loop? Copied straight from message_pump_default.
    base::ScopedNSAutoreleasePool autorelease_pool;

    bool did_work = NS_ProcessNextEvent(thread, PR_FALSE) ? true : false;
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
      event_.Wait();
    } else {
      base::TimeDelta delay = delayed_work_time_ - base::Time::Now();
      if (delay > base::TimeDelta()) {
        event_.TimedWait(delay);
      } else {
        // It looks like delayed_work_time_ indicates a time in the past, so we
        // need to call DoDelayedWork now.
        delayed_work_time_ = base::Time();
      }
    }
    // Since event_ is auto-reset, we don't need to do anything special here
    // other than service each delegate method.
  }

  threadInternal->SetObserver(realObserver);

  keep_running_ = true;
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
    nsCOMPtr<nsIAppShell> appShell(do_GetService(kAppShellCID));
    NS_WARN_IF_FALSE(appShell, "Failed to get app shell?!");
    if (appShell) {
      nsresult rv = appShell->Run();
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to run app shell?!");
      }
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
  mozilla::ipc::MessagePump::Run(aDelegate);
}
