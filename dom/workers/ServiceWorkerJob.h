/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerjob_h
#define mozilla_dom_workers_serviceworkerjob_h

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIPrincipal;

namespace mozilla {

class ErrorResult;

namespace dom {
namespace workers {

class ServiceWorkerJob
{
public:
  // Implement this interface to receive notification when a job completes.
  class Callback
  {
  public:
    // Called once when the job completes.  If the job is started, then this
    // will be called.  If a job is never executed due to browser shutdown,
    // then this method will never be called.  This method is always called
    // on the main thread asynchronously after Start() completes.
    virtual void JobFinished(ServiceWorkerJob* aJob, ErrorResult& aStatus) = 0;

    NS_IMETHOD_(MozExternalRefCountType)
    AddRef(void) = 0;

    NS_IMETHOD_(MozExternalRefCountType)
    Release(void) = 0;
  };

  enum class Type
  {
    Register,
    Update,
    Unregister
  };

  enum class State
  {
    Initial,
    Started,
    Finished
  };

  Type
  GetType() const;

  State
  GetState() const;

  // Determine if the job has been canceled.  This does not change the
  // current State, but indicates that the job should progress to Finished
  // as soon as possible.
  bool
  Canceled() const;

  // Determine if the result callbacks have already been called.  This is
  // equivalent to the spec checked to see if the job promise has settled.
  bool
  ResultCallbacksInvoked() const;

  bool
  IsEquivalentTo(ServiceWorkerJob* aJob) const;

  // Add a callback that will be invoked when the job's result is available.
  // Some job types will invoke this before the job is actually finished.
  // If an early callback does not occur, then it will be called automatically
  // when Finish() is called.  These callbacks will be invoked while the job
  // state is Started.
  void
  AppendResultCallback(Callback* aCallback);

  // This takes ownership of any result callbacks associated with the given job
  // and then appends them to this job's callback list.
  void
  StealResultCallbacksFrom(ServiceWorkerJob* aJob);

  // Start the job.  All work will be performed asynchronously on
  // the main thread.  The Finish() method must be called exactly
  // once after this point.  A final callback must be provided.  It
  // will be invoked after all other callbacks have been processed.
  void
  Start(Callback* aFinalCallback);

  // Set an internal flag indicating that a started job should finish as
  // soon as possible.
  void
  Cancel();

protected:
  ServiceWorkerJob(Type aType,
                   nsIPrincipal* aPrincipal,
                   const nsACString& aScope,
                   const nsACString& aScriptSpec);

  virtual ~ServiceWorkerJob();

  // Invoke the result callbacks immediately.  The job must be in the
  // Started state.  The callbacks are cleared after being invoked,
  // so subsequent method calls have no effect.
  void
  InvokeResultCallbacks(ErrorResult& aRv);

  // Convenience method that converts to ErrorResult and calls real method.
  void
  InvokeResultCallbacks(nsresult aRv);

  // Indicate that the job has completed.  The must be called exactly
  // once after Start() has initiated job execution.  It may not be
  // called until Start() has returned.
  void
  Finish(ErrorResult& aRv);

  // Convenience method that converts to ErrorResult and calls real method.
  void
  Finish(nsresult aRv);

  // Specific job types should define AsyncExecute to begin their work.
  // All errors and successes must result in Finish() being called.
  virtual void
  AsyncExecute() = 0;

  const Type mType;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  const nsCString mScope;
  const nsCString mScriptSpec;

private:
  RefPtr<Callback> mFinalCallback;
  nsTArray<RefPtr<Callback>> mResultCallbackList;
  State mState;
  bool mCanceled;
  bool mResultCallbacksInvoked;

public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerJob)
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerjob_h
