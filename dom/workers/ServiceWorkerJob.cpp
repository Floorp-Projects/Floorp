/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerJob.h"

#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "Workers.h"

namespace mozilla {
namespace dom {
namespace workers {

ServiceWorkerJob::Type
ServiceWorkerJob::GetType() const
{
  return mType;
}

ServiceWorkerJob::State
ServiceWorkerJob::GetState() const
{
  return mState;
}

bool
ServiceWorkerJob::Canceled() const
{
  return mCanceled;
}

bool
ServiceWorkerJob::ResultCallbacksInvoked() const
{
  return mResultCallbacksInvoked;
}

bool
ServiceWorkerJob::IsEquivalentTo(ServiceWorkerJob* aJob) const
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aJob);
  return mType == aJob->mType &&
         mScope.Equals(aJob->mScope) &&
         mScriptSpec.Equals(aJob->mScriptSpec) &&
         mPrincipal->Equals(aJob->mPrincipal);
}

void
ServiceWorkerJob::AppendResultCallback(Callback* aCallback)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mState != State::Finished);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(mFinalCallback != aCallback);
  MOZ_ASSERT(!mResultCallbackList.Contains(aCallback));
  MOZ_ASSERT(!mResultCallbacksInvoked);
  mResultCallbackList.AppendElement(aCallback);
}

void
ServiceWorkerJob::StealResultCallbacksFrom(ServiceWorkerJob* aJob)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aJob);
  MOZ_ASSERT(aJob->mState == State::Initial);

  // Take the callbacks from the other job immediately to avoid the
  // any possibility of them existing on both jobs at once.
  nsTArray<RefPtr<Callback>> callbackList;
  callbackList.SwapElements(aJob->mResultCallbackList);

  for (RefPtr<Callback>& callback : callbackList) {
    // Use AppendResultCallback() so that assertion checking is performed on
    // each callback.
    AppendResultCallback(callback);
  }
}

void
ServiceWorkerJob::Start(Callback* aFinalCallback)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!mCanceled);

  MOZ_ASSERT(aFinalCallback);
  MOZ_ASSERT(!mFinalCallback);
  MOZ_ASSERT(!mResultCallbackList.Contains(aFinalCallback));
  mFinalCallback = aFinalCallback;

  MOZ_ASSERT(mState == State::Initial);
  mState = State::Started;

  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod(this, &ServiceWorkerJob::AsyncExecute);

  // We may have to wait for the PBackground actor to be initialized
  // before proceeding.  We should always be able to get a ServiceWorkerManager,
  // however, since Start() should not be called during shutdown.
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm->HasBackgroundActor()) {
    swm->AppendPendingOperation(runnable);
    return;
  }

  // Otherwise start asynchronously.  We should never run a job synchronously.
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    NS_DispatchToMainThread(runnable.forget())));
}

void
ServiceWorkerJob::Cancel()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!mCanceled);
  mCanceled = true;
}

ServiceWorkerJob::ServiceWorkerJob(Type aType,
                                   nsIPrincipal* aPrincipal,
                                   const nsACString& aScope,
                                   const nsACString& aScriptSpec)
  : mType(aType)
  , mPrincipal(aPrincipal)
  , mScope(aScope)
  , mScriptSpec(aScriptSpec)
  , mState(State::Initial)
  , mCanceled(false)
  , mResultCallbacksInvoked(false)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mPrincipal);
  MOZ_ASSERT(!mScope.IsEmpty());
  // Some job types may have an empty script spec
}

ServiceWorkerJob::~ServiceWorkerJob()
{
  AssertIsOnMainThread();
  // Jobs must finish or never be started.  Destroying an actively running
  // job is an error.
  MOZ_ASSERT(mState != State::Started);
  MOZ_ASSERT_IF(mState == State::Finished, mResultCallbacksInvoked);
}

void
ServiceWorkerJob::InvokeResultCallbacks(ErrorResult& aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mState == State::Started);

  MOZ_ASSERT(!mResultCallbacksInvoked);
  mResultCallbacksInvoked = true;

  nsTArray<RefPtr<Callback>> callbackList;
  callbackList.SwapElements(mResultCallbackList);

  for (RefPtr<Callback>& callback : callbackList) {
    // The callback might consume an exception on the ErrorResult, so we need
    // to clone in order to maintain the error for the next callback.
    ErrorResult rv;
    aRv.CloneTo(rv);

    callback->JobFinished(this, rv);

    // The callback might not consume the error.
    rv.SuppressException();
  }
}

void
ServiceWorkerJob::InvokeResultCallbacks(nsresult aRv)
{
  ErrorResult converted(aRv);
  InvokeResultCallbacks(converted);
}

void
ServiceWorkerJob::Finish(ErrorResult& aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mState == State::Started);

  // Ensure that we only surface SecurityErr, TypeErr or InvalidStateErr to script.
  if (aRv.Failed() && !aRv.ErrorCodeIs(NS_ERROR_DOM_SECURITY_ERR) &&
                      !aRv.ErrorCodeIs(NS_ERROR_DOM_TYPE_ERR) &&
                      !aRv.ErrorCodeIs(NS_ERROR_DOM_INVALID_STATE_ERR)) {

    // Remove the old error code so we can replace it with a TypeError.
    aRv.SuppressException();

    NS_ConvertUTF8toUTF16 scriptSpec(mScriptSpec);
    NS_ConvertUTF8toUTF16 scope(mScope);

    // Throw the type error with a generic error message.
    aRv.ThrowTypeError<MSG_SW_INSTALL_ERROR>(scriptSpec, scope);
  }

  // The final callback may drop the last ref to this object.
  RefPtr<ServiceWorkerJob> kungFuDeathGrip = this;

  if (!mResultCallbacksInvoked) {
    InvokeResultCallbacks(aRv);
  }

  mState = State::Finished;

  mFinalCallback->JobFinished(this, aRv);
  mFinalCallback = nullptr;

  // The callback might not consume the error.
  aRv.SuppressException();

  // Async release this object to ensure that our caller methods complete
  // as well.
  NS_ReleaseOnMainThread(kungFuDeathGrip.forget(), true /* always proxy */);
}

void
ServiceWorkerJob::Finish(nsresult aRv)
{
  ErrorResult converted(aRv);
  Finish(converted);
}

} // namespace workers
} // namespace dom
} // namespace mozilla
