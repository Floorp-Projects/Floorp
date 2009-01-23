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

#ifndef __NSDOMWORKERTIMEOUT_H__
#define __NSDOMWORKERTIMEOUT_H__

// Interfaces
#include "nsITimer.h"

// Other includes
#include "jsapi.h"
#include "nsAutoJSValHolder.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

// DOMWorker includes
#include "nsDOMWorker.h"

/**
 * The nsDOMWorkerTimeout has a slightly complicated life cycle. It's created
 * by an nsDOMWorker (or one of its JS context functions) and immediately takes
 * a strong reference to the worker that created it. It does this so that the
 * worker can't be collected while a timeout is outstanding. However, the worker
 * needs a weak reference to the timeout so that it can be canceled if the
 * worker is canceled (in the event that the page falls out of the fastback
 * cache or the application is exiting, for instance). The only thing that holds
 * the timeout alive is its mTimer via the nsITimerCallback interface. If the
 * timer is single-shot and has run already or if the timer is canceled then
 * this object should die.
 */
class nsDOMWorkerTimeout : public nsDOMWorkerFeature,
                           public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSITIMERCALLBACK

  nsDOMWorkerTimeout(nsDOMWorker* aWorker,
                     PRUint32 aId);

  nsresult Init(JSContext* aCx,
                PRUint32 aArgc,
                jsval* aArgv,
                PRBool aIsInterval);

  nsresult Start();

  nsresult Run();

  virtual void Cancel();
  virtual void Suspend();
  virtual void Resume();

  PRIntervalTime GetInterval() {
    return mInterval;
  }

  nsDOMWorker* GetWorker() {
    return mWorker;
  }

  PRBool IsSuspended() {
    AutoSpinlock lock(this);
    return IsSuspendedNoLock();
  }

private:
  ~nsDOMWorkerTimeout() { }

  void AcquireSpinlock();
  void ReleaseSpinlock();

  PRBool IsSuspendedNoLock() {
    return mIsSuspended;
  }

  class AutoSpinlock
  {
  public:
    AutoSpinlock(nsDOMWorkerTimeout* aTimeout)
    : mTimeout(aTimeout) {
      aTimeout->AcquireSpinlock();
    }

    ~AutoSpinlock() {
      mTimeout->ReleaseSpinlock();
    }
  private:
    nsDOMWorkerTimeout* mTimeout;
  };

  // We support two types of callbacks (functions and expressions) just like the
  // normal window timeouts. Each type has its own member and rooting needs so
  // we split them into two classes with a common base.
  class CallbackBase
  {
  public:
    virtual ~CallbackBase() { }
    virtual nsresult Run(nsDOMWorkerTimeout* aTimeout,
                         JSContext* aCx) = 0;
  };

  class FunctionCallback : public CallbackBase
  {
  public:
    FunctionCallback(PRUint32 aArgc,
                     jsval* aArgv,
                     nsresult* aRv);
    virtual ~FunctionCallback();
    virtual nsresult Run(nsDOMWorkerTimeout* aTimeout,
                         JSContext* aCx);
  protected:
    nsAutoJSValHolder mCallback;
    nsTArray<nsAutoJSValHolder> mCallbackArgs;
    PRUint32 mCallbackArgsLength;
  };

  class ExpressionCallback : public CallbackBase
  {
  public:
    ExpressionCallback(PRUint32 aArgc,
                       jsval* aArgv,
                       JSContext* aCx,
                       nsresult* aRv);
    virtual ~ExpressionCallback();
    virtual nsresult Run(nsDOMWorkerTimeout* aTimeout,
                         JSContext* aCx);
  protected:
    nsAutoJSValHolder mExpression;
    nsCString mFileName;
    PRUint32 mLineNumber;
  };

  // Hold this object alive!
  nsCOMPtr<nsITimer> mTimer;

  PRUint32 mInterval;

  PRTime mTargetTime;

  nsAutoPtr<CallbackBase> mCallback;

  PRInt32 mSuspendSpinlock;
  PRUint32 mSuspendInterval;
  nsRefPtr<nsDOMWorkerTimeout> mSuspendedRef;

  PRPackedBool mIsInterval;
  PRPackedBool mIsSuspended;
  PRPackedBool mSuspendedBeforeStart;
  PRPackedBool mStarted;
};

#endif /* __NSDOMWORKERTIMEOUT_H__ */
