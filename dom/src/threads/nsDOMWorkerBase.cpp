/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is worker threads.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#include "nsDOMWorkerBase.h"

struct JSContext;

// Interfaces
#include "nsIDOMThreads.h"
#include "nsIJSContextStack.h"
#include "nsIXPConnect.h"

// Other includes
#include "jsapi.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsThreadUtils.h"
#include "prlog.h"

#include "nsDOMWorkerThread.h"
#include "nsDOMWorkerPool.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gDOMThreadsLog;
#endif
#define LOG(_args) PR_LOG(gDOMThreadsLog, PR_LOG_DEBUG, _args)

/**
 * Calls the message handler function of the target worker on the correct
 * thread.
 */
class nsDOMPostMessageRunnable : public nsRunnable
{
public:
  nsDOMPostMessageRunnable(const nsAString& aMessage,
                           nsDOMWorkerBase* aSource,
                           nsDOMWorkerBase* aTarget)
  : mMessage(aMessage), mSource(aSource), mTarget(aTarget) {
    NS_ASSERTION(aSource && aTarget, "Must specify both!");
  }

  NS_IMETHOD Run() {
#ifdef PR_LOGGING
    nsCAutoString utf8Message;
    utf8Message.AssignWithConversion(mMessage);

    static const char* poolStr = "pool";
    static const char* workerStr = "worker";

    nsCOMPtr<nsIDOMWorkerPool> sourceIsPool;
    mSource->QueryInterface(NS_GET_IID(nsIDOMWorkerPool),
                            getter_AddRefs(sourceIsPool));

    nsCOMPtr<nsIDOMWorkerPool> targetIsPool;
    mTarget->QueryInterface(NS_GET_IID(nsIDOMWorkerPool),
                            getter_AddRefs(targetIsPool));
#endif

    if (!(mTarget->IsCanceled() || mSource->IsCanceled())) {
      LOG(("Posting message '%s' from %s [0x%p] to %s [0x%p]",
           utf8Message.get(),
           sourceIsPool ? poolStr : workerStr,
           static_cast<void*>(mSource.get()),
           targetIsPool ? poolStr : workerStr,
           static_cast<void*>(mTarget.get())));

      mTarget->HandleMessage(mMessage, mSource);
    }

    return NS_OK;
  }

protected:
  nsString mMessage;
  nsRefPtr<nsDOMWorkerBase> mSource;
  nsRefPtr<nsDOMWorkerBase> mTarget;
};

nsresult
nsDOMWorkerBase::PostMessageInternal(const nsAString& aMessage,
                                     nsDOMWorkerBase* aSource)
{
  if (IsCanceled() || aSource->IsCanceled()) {
    return NS_OK;
  }

  nsRefPtr<nsDOMPostMessageRunnable> runnable =
    new nsDOMPostMessageRunnable(aMessage, aSource, this);
  NS_ENSURE_TRUE(runnable, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = DispatchMessage(runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMWorkerBase::PostMessageInternal(const nsAString& aMessage)
{
  nsRefPtr<nsDOMWorkerBase> source;
  JSContext *cx = nsContentUtils::GetCurrentJSContext();

  if (cx) {
    if (NS_IsMainThread()) {
      // Must be a normal DOM context, use the pool as the source.
      source = Pool();
    }
    else {
      // Must be a worker context, get the worker from the context private.
      nsRefPtr<nsDOMWorkerThread> worker =
        (nsDOMWorkerThread*)JS_GetContextPrivate(cx);

      // Only allowed to communicate to other threads in the same pool.
      nsRefPtr<nsDOMWorkerPool> sourcePool = worker->Pool();
      NS_ENSURE_TRUE(sourcePool == Pool(), NS_ERROR_NOT_AVAILABLE);

      source = worker;
    }
  }
  else {
    source = this;
  }

  nsresult rv = PostMessageInternal(aMessage, source);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsDOMWorkerBase::Cancel()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG(("Canceling %s [0x%p]", Pool() == this ? "pool" : "worker",
       static_cast<void*>(this)));

#ifdef DEBUG
  PRInt32 cancel =
#endif
  PR_AtomicSet(&mCanceled, 1);
  NS_ASSERTION(!cancel, "Canceled more than once?!");
}

void
nsDOMWorkerBase::Suspend()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG(("Suspending %s [0x%p]", Pool() == this ? "pool" : "worker",
       static_cast<void*>(this)));

#ifdef DEBUG
  PRInt32 suspended =
#endif
  PR_AtomicSet(&mSuspended, 1);
  NS_ASSERTION(!suspended, "Suspended more than once?!");
}

void
nsDOMWorkerBase::Resume()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG(("Resuming %s [0x%p]", Pool() == this ? "pool" : "worker",
       static_cast<void*>(this)));

#ifdef DEBUG
  PRInt32 suspended =
#endif
  PR_AtomicSet(&mSuspended, 0);
  NS_ASSERTION(suspended, "Not suspended!");
}
