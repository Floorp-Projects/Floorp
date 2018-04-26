/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerEventTarget.h"

namespace mozilla {
namespace dom {

namespace {

class WrappedControlRunnable final : public WorkerControlRunnable
{
  nsCOMPtr<nsIRunnable> mInner;

  ~WrappedControlRunnable()
  {
  }

public:
  WrappedControlRunnable(WorkerPrivate* aWorkerPrivate,
                         already_AddRefed<nsIRunnable>&& aInner)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , mInner(aInner)
  {
  }

  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    // Silence bad assertions, this can be dispatched from any thread.
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    // Silence bad assertions, this can be dispatched from any thread.
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mInner->Run();
    return true;
  }

  nsresult
  Cancel() override
  {
    nsCOMPtr<nsICancelableRunnable> cr = do_QueryInterface(mInner);

    // If the inner runnable is not cancellable, then just do the normal
    // WorkerControlRunnable thing.  This will end up calling Run().
    if (!cr) {
      WorkerControlRunnable::Cancel();
      return NS_OK;
    }

    // Otherwise call the inner runnable's Cancel() and treat this like
    // a WorkerRunnable cancel.  We can't call WorkerControlRunnable::Cancel()
    // in this case since that would result in both Run() and the inner
    // Cancel() being called.
    Unused << cr->Cancel();
    return WorkerRunnable::Cancel();
  }
};

} // anonymous namespace

NS_IMPL_ISUPPORTS(WorkerEventTarget, nsIEventTarget, nsISerialEventTarget)

WorkerEventTarget::WorkerEventTarget(WorkerPrivate* aWorkerPrivate,
                                    Behavior aBehavior)
  : mMutex("WorkerEventTarget")
  , mWorkerPrivate(aWorkerPrivate)
  , mBehavior(aBehavior)
{
  MOZ_DIAGNOSTIC_ASSERT(mWorkerPrivate);
}

void
WorkerEventTarget::ForgetWorkerPrivate(WorkerPrivate* aWorkerPrivate)
{
  MutexAutoLock lock(mMutex);
  MOZ_DIAGNOSTIC_ASSERT(!mWorkerPrivate || mWorkerPrivate == aWorkerPrivate);
  mWorkerPrivate = nullptr;
}

NS_IMETHODIMP
WorkerEventTarget::DispatchFromScript(nsIRunnable* aRunnable,
                                      uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  return Dispatch(runnable.forget(), aFlags);
}

NS_IMETHODIMP
WorkerEventTarget::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                            uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    return NS_ERROR_FAILURE;
  }

  if (mBehavior == Behavior::Hybrid) {
    RefPtr<WorkerRunnable> r =
      mWorkerPrivate->MaybeWrapAsWorkerRunnable(runnable.forget());
    if (r->Dispatch()) {
      return NS_OK;
    }

    runnable = r.forget();
  }

  RefPtr<WorkerControlRunnable> r =
    new WrappedControlRunnable(mWorkerPrivate, runnable.forget());
  if (!r->Dispatch()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
WorkerEventTarget::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(bool)
WorkerEventTarget::IsOnCurrentThreadInfallible()
{
  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    return false;
  }

  return mWorkerPrivate->IsOnCurrentThread();
}

NS_IMETHODIMP
WorkerEventTarget::IsOnCurrentThread(bool* aIsOnCurrentThread)
{
  MOZ_ASSERT(aIsOnCurrentThread);
  *aIsOnCurrentThread = IsOnCurrentThreadInfallible();
  return NS_OK;
}

} // dom namespace
} // mozilla namespace
