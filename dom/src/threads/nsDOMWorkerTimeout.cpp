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

#include "nsDOMWorkerTimeout.h"

// Interfaces
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsITimer.h"
#include "nsIXPConnect.h"

// Other includes
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"
#include "pratom.h"
#include "prtime.h"

// DOMWorker includes
#include "nsDOMThreadService.h"

#define LOG(_args) PR_LOG(gDOMThreadsLog, PR_LOG_DEBUG, _args)

#define CONSTRUCTOR_ENSURE_TRUE(_cond, _rv) \
  PR_BEGIN_MACRO \
    if (NS_UNLIKELY(!(_cond))) { \
      NS_WARNING("CONSTRUCTOR_ENSURE_TRUE(" #_cond ") failed"); \
      (_rv) = NS_ERROR_FAILURE; \
      return; \
    } \
  PR_END_MACRO

#define SUSPEND_SPINLOCK_COUNT 5000

static const char* kSetIntervalStr = "setInterval";
static const char* kSetTimeoutStr = "setTimeout";

nsDOMWorkerTimeout::FunctionCallback::FunctionCallback(PRUint32 aArgc,
                                                       jsval* aArgv,
                                                       nsresult* aRv)
: mCallback(nsnull),
  mCallbackArgs(nsnull),
  mCallbackArgsLength(0),
  mRuntime(NULL)
{
  MOZ_COUNT_CTOR(nsDOMWorkerTimeout::FunctionCallback);

  *aRv = nsDOMThreadService::JSRuntimeService()->GetRuntime(&mRuntime);
  NS_ENSURE_SUCCESS(*aRv,);

  PRBool success = JS_AddNamedRootRT(mRuntime, &mCallback,
                                     "nsDOMWorkerTimeout Callback Object");
  CONSTRUCTOR_ENSURE_TRUE(success, *aRv);

  mCallback = aArgv[0];

  // We want enough space for an extra lateness arg.
  mCallbackArgsLength = aArgc > 2 ? aArgc - 1 : 1;

  mCallbackArgs = new jsval[mCallbackArgsLength];
  if (NS_UNLIKELY(!mCallbackArgs)) {
    // Reset this!
    mCallbackArgsLength = 0;

    NS_ERROR("Out of memory!");
    *aRv = NS_ERROR_OUT_OF_MEMORY;
    return;
  }

  for (PRUint32 i = 0; i < mCallbackArgsLength - 1; i++) {
    mCallbackArgs[i] = aArgv[i + 2];
    success = JS_AddNamedRootRT(mRuntime, &mCallbackArgs[i],
                                "nsDOMWorkerTimeout Callback Arg");
    if (NS_UNLIKELY(!success)) {
      // Set this to i so that the destructor only unroots the right number of
      // values.
      mCallbackArgsLength = i;

      NS_WARNING("Failed to add root!");
      *aRv = NS_ERROR_FAILURE;
      return;
    }
  }

  // Take care of the last arg.
  mCallbackArgs[mCallbackArgsLength - 1] = 0;
  success = JS_AddNamedRootRT(mRuntime, &mCallbackArgs[mCallbackArgsLength - 1],
                              "nsDOMWorkerTimeout Callback Final Arg");
  if (NS_UNLIKELY(!success)) {
    // Decrement this so that the destructor only unroots the right number of
    // values.
    mCallbackArgsLength -= 1;

    NS_WARNING("Failed to add root!");
    *aRv = NS_ERROR_FAILURE;
    return;
  }

  *aRv = NS_OK;
}

nsDOMWorkerTimeout::FunctionCallback::~FunctionCallback()
{
  MOZ_COUNT_DTOR(nsDOMWorkerTimeout::FunctionCallback);

  if (mCallback) {
    for (PRUint32 i = 0; i < mCallbackArgsLength; i++) {
      JS_RemoveRootRT(mRuntime, &mCallbackArgs[i]);
    }
    JS_RemoveRootRT(mRuntime, &mCallback);
  }

  delete [] mCallbackArgs;
}

nsresult
nsDOMWorkerTimeout::FunctionCallback::Run(nsDOMWorkerTimeout* aTimeout,
                                          JSContext* aCx)
{
  PRInt32 lateness = PR_MAX(0, PRInt32(PR_Now() - aTimeout->mTargetTime)) /
                     (PRTime)PR_USEC_PER_MSEC;
  mCallbackArgs[mCallbackArgsLength - 1] = INT_TO_JSVAL(lateness);

  JSObject* global = JS_GetGlobalObject(aCx);
  NS_ENSURE_TRUE(global, NS_ERROR_FAILURE);

  jsval rval;
  PRBool success =
    JS_CallFunctionValue(aCx, global, mCallback, mCallbackArgsLength,
                         mCallbackArgs, &rval);
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

  return NS_OK;
}

nsDOMWorkerTimeout::ExpressionCallback::ExpressionCallback(PRUint32 aArgc,
                                                           jsval* aArgv,
                                                           JSContext* aCx,
                                                           nsresult* aRv)
: mExpression(nsnull),
  mLineNumber(0),
  mRuntime(NULL)
{
  MOZ_COUNT_CTOR(nsDOMWorkerTimeout::ExpressionCallback);

  JSString* expr = JS_ValueToString(aCx, aArgv[0]);
  *aRv = expr ? NS_OK : NS_ERROR_FAILURE;
  NS_ENSURE_SUCCESS(*aRv,);

  *aRv = nsDOMThreadService::JSRuntimeService()->GetRuntime(&mRuntime);
  NS_ENSURE_SUCCESS(*aRv,);

  PRBool success = JS_AddNamedRootRT(mRuntime, &mExpression,
                                     "nsDOMWorkerTimeout Expression");
  CONSTRUCTOR_ENSURE_TRUE(success, *aRv);

  mExpression = expr;

  // Get the calling location.
  const char* fileName;
  PRUint32 lineNumber;
  if (nsJSUtils::GetCallingLocation(aCx, &fileName, &lineNumber, nsnull)) {
    CopyUTF8toUTF16(nsDependentCString(fileName), mFileName);
    mLineNumber = lineNumber;
  }

  *aRv = NS_OK;
}

nsDOMWorkerTimeout::ExpressionCallback::~ExpressionCallback()
{
  MOZ_COUNT_DTOR(nsDOMWorkerTimeout::ExpressionCallback);

  if (mExpression) {
    JS_RemoveRootRT(mRuntime, &mExpression);
  }
}

nsresult
nsDOMWorkerTimeout::ExpressionCallback::Run(nsDOMWorkerTimeout* aTimeout,
                                            JSContext* aCx)
{
  NS_ERROR("Not yet implemented!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsDOMWorkerTimeout::nsDOMWorkerTimeout(nsDOMWorker* aWorker,
                                       PRUint32 aId)
: nsDOMWorkerFeature(aWorker, aId),
  mInterval(0),
  mSuspendSpinlock(0),
  mSuspendInterval(0),
  mIsInterval(PR_FALSE),
  mIsSuspended(PR_FALSE),
  mSuspendedBeforeStart(PR_FALSE),
  mStarted(PR_FALSE)
{
  NS_ASSERTION(mWorker, "Need a worker here!");
}

NS_IMPL_ISUPPORTS_INHERITED1(nsDOMWorkerTimeout, nsDOMWorkerFeature,
                                                 nsITimerCallback)

nsresult
nsDOMWorkerTimeout::Init(JSContext* aCx, PRUint32 aArgc, jsval* aArgv,
                         PRBool aIsInterval)
{
  NS_ASSERTION(aCx, "Null pointer!");
  NS_ASSERTION(aArgv, "Null pointer!");

  JSAutoRequest ar(aCx);

  if (!aArgc) {
    JS_ReportError(aCx, "Function %s requires at least 1 parameter",
                   aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_INVALID_ARG;
  }

  PRUint32 interval;
  if (aArgc > 1) {
    if (!JS_ValueToECMAUint32(aCx, aArgv[1], (uint32*)&interval)) {
      JS_ReportError(aCx, "Second argument to %s must be a millisecond value",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
      return NS_ERROR_INVALID_ARG;
    }
  }
  else {
    // If no interval was specified, treat this like a timeout, to avoid
    // setting an interval of 0 milliseconds.
    aIsInterval = PR_FALSE;
  }

  mInterval = interval;

  mIsInterval = aIsInterval;

  mTargetTime = PR_Now() + interval * (PRTime)PR_USEC_PER_MSEC;

  nsresult rv;
  switch (JS_TypeOfValue(aCx, aArgv[0])) {
    case JSTYPE_FUNCTION:
      mCallback = new FunctionCallback(aArgc, aArgv, &rv);
      NS_ENSURE_TRUE(mCallback, NS_ERROR_OUT_OF_MEMORY);
      NS_ENSURE_SUCCESS(rv, rv);

      break;

    case JSTYPE_STRING:
    case JSTYPE_OBJECT:
      mCallback = new ExpressionCallback(aArgc, aArgv, aCx, &rv);
      NS_ENSURE_TRUE(mCallback, NS_ERROR_OUT_OF_MEMORY);
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    default:
      JS_ReportError(aCx, "useless %s call (missing quotes around argument?)",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);

      // Return an error that nsGlobalWindow can recognize and turn into NS_OK.
      return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIEventTarget* target =
    static_cast<nsIEventTarget*>(nsDOMThreadService::get());

  rv = timer->SetTarget(target);
  NS_ENSURE_SUCCESS(rv, rv);

  mTimer.swap(timer);
  return NS_OK;
}

nsresult
nsDOMWorkerTimeout::Start()
{
  if (IsSuspended()) {
    NS_ASSERTION(mSuspendedBeforeStart, "Bad state!");
    return NS_OK;
  }

  PRInt32 type;
  if (mIsInterval) {
    type = nsITimer::TYPE_REPEATING_SLACK;
  }
  else {
    type = nsITimer::TYPE_ONE_SHOT;
  }

  nsresult rv = mTimer->InitWithCallback(this, mInterval, type);
  NS_ENSURE_SUCCESS(rv, rv);

  mStarted = PR_TRUE;
  return NS_OK;
}

nsresult
nsDOMWorkerTimeout::Run()
{
  NS_ENSURE_TRUE(mCallback, NS_ERROR_NOT_INITIALIZED);
  LOG(("Worker [0x%p] running timeout [0x%p] with id %u",
       static_cast<void*>(mWorker.get()), static_cast<void*>(this), mId));

  JSContext* cx;
  nsresult rv =
    nsDOMThreadService::ThreadJSContextStack()->GetSafeJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  rv = mCallback->Run(this, cx);

  // Make sure any pending exceptions are converted to errors for the pool.
  JS_ReportPendingException(cx);

  if (mIsInterval) {
    mTargetTime = PR_Now() + mInterval * (PRTime)PR_USEC_PER_MSEC;
  }

  return rv;
}

void
nsDOMWorkerTimeout::Cancel()
{
  NS_ASSERTION(mTimer, "Impossible to get here without a timer!");

  LOG(("Worker [0x%p] canceling timeout [0x%p] with id %u",
       static_cast<void*>(mWorker.get()), static_cast<void*>(this), mId));

  {
    AutoSpinlock lock(this);

    if (IsSuspendedNoLock()) {
      mIsSuspended = PR_FALSE;
      // This should kill us when all is said and done.
      mSuspendedRef = nsnull;
    }
  }

  // This call to Cancel should kill us.
  mTimer->Cancel();
}

void
nsDOMWorkerTimeout::Suspend()
{
#ifdef DEBUG
  if (mStarted) {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  }
#endif

  AutoSpinlock lock(this);

  NS_ASSERTION(!IsSuspendedNoLock(), "Bad state!");

  mIsSuspended = PR_TRUE;
  mSuspendedRef = this;

  if (!mStarted) {
    mSuspendedBeforeStart = PR_TRUE;
    return;
  }

  mTimer->Cancel();

  mSuspendInterval = PR_MAX(0, PRInt32(mTargetTime - PR_Now())) /
                     (PRTime)PR_USEC_PER_MSEC;

  LOG(("Worker [0x%p] suspending timeout [0x%p] with id %u (interval = %u)",
       static_cast<void*>(mWorker.get()), static_cast<void*>(this), mId,
       mSuspendInterval));
}

void
nsDOMWorkerTimeout::Resume()
{
#ifdef DEBUG
  if (!mSuspendedBeforeStart) {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  }
#endif

  NS_ASSERTION(mTimer, "Impossible to get here without a timer!");

  LOG(("Worker [0x%p] resuming timeout [0x%p] with id %u",
       static_cast<void*>(mWorker.get()), static_cast<void*>(this), mId));

  AutoSpinlock lock(this);

  NS_ASSERTION(IsSuspendedNoLock(), "Should be suspended!");

  if (mSuspendedBeforeStart) {
    NS_ASSERTION(!mSuspendInterval, "Bad state!");
    mSuspendedBeforeStart = PR_FALSE;
    mSuspendInterval = mInterval;
    mStarted = PR_TRUE;
  }

  mTargetTime = PR_Now() + mSuspendInterval * (PRTime)PR_USEC_PER_MSEC;

#ifdef DEBUG
  nsresult rv =
#endif
  mTimer->InitWithCallback(this, mSuspendInterval, nsITimer::TYPE_ONE_SHOT);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to init timer!");
}

void
nsDOMWorkerTimeout::AcquireSpinlock()
{
  PRUint32 loopCount = 0;
  while (PR_AtomicSet(&mSuspendSpinlock, 1) == 1) {
    if (++loopCount > SUSPEND_SPINLOCK_COUNT) {
      LOG(("AcquireSpinlock taking too long (looped %u times), yielding.",
           loopCount));
      loopCount = 0;
      PR_Sleep(PR_INTERVAL_NO_WAIT);
    }
  }
#ifdef PR_LOGGING
  if (loopCount) {
    LOG(("AcquireSpinlock needed %u loops", loopCount));
  }
#endif
}

void
nsDOMWorkerTimeout::ReleaseSpinlock()
{
#ifdef DEBUG
  PRInt32 suspended =
#endif
  PR_AtomicSet(&mSuspendSpinlock, 0);
  NS_ASSERTION(suspended == 1, "Huh?!");
}

NS_IMETHODIMP
nsDOMWorkerTimeout::Notify(nsITimer* aTimer)
{
  // Should be on the timer thread.
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTimer == mTimer, "Wrong timer?!");

  PRUint32 type;
  nsresult rv = aTimer->GetType(&type);
  NS_ENSURE_SUCCESS(rv, rv);

  // We only care about one-shot timers here because that may be the one that
  // we set from Resume().
  if (type == nsITimer::TYPE_ONE_SHOT) {
    AutoSpinlock lock(this);
    if (mIsSuspended) {
      mIsSuspended = PR_FALSE;
      mSuspendedRef = nsnull;
      if (mIsInterval) {
        // This is the first fire since we resumed. Set our interval back to the
        // real interval.
        mTargetTime = PR_Now() + mInterval * (PRTime)PR_USEC_PER_MSEC;

        rv = aTimer->InitWithCallback(this, mInterval,
                                      nsITimer::TYPE_REPEATING_SLACK);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  nsDOMThreadService::get()->TimeoutReady(this);
  return NS_OK;
}
