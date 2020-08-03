/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerJob.h"

#include "mozilla/dom/WorkerCommon.h"
#include "nsIPrincipal.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "ServiceWorkerManager.h"

namespace mozilla {
namespace dom {

ServiceWorkerJob::Type ServiceWorkerJob::GetType() const { return mType; }

ServiceWorkerJob::State ServiceWorkerJob::GetState() const { return mState; }

bool ServiceWorkerJob::Canceled() const { return mCanceled; }

bool ServiceWorkerJob::ResultCallbacksInvoked() const {
  return mResultCallbacksInvoked;
}

bool ServiceWorkerJob::IsEquivalentTo(ServiceWorkerJob* aJob) const {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aJob);
  return mType == aJob->mType && mScope.Equals(aJob->mScope) &&
         mScriptSpec.Equals(aJob->mScriptSpec) &&
         mPrincipal->Equals(aJob->mPrincipal);
}

void ServiceWorkerJob::AppendResultCallback(Callback* aCallback) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mState != State::Finished);
  MOZ_DIAGNOSTIC_ASSERT(aCallback);
  MOZ_DIAGNOSTIC_ASSERT(mFinalCallback != aCallback);
  MOZ_ASSERT(!mResultCallbackList.Contains(aCallback));
  MOZ_DIAGNOSTIC_ASSERT(!mResultCallbacksInvoked);
  mResultCallbackList.AppendElement(aCallback);
}

void ServiceWorkerJob::StealResultCallbacksFrom(ServiceWorkerJob* aJob) {
  MOZ_ASSERT(NS_IsMainThread());
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

void ServiceWorkerJob::Start(Callback* aFinalCallback) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!mCanceled);

  MOZ_DIAGNOSTIC_ASSERT(aFinalCallback);
  MOZ_DIAGNOSTIC_ASSERT(!mFinalCallback);
  MOZ_ASSERT(!mResultCallbackList.Contains(aFinalCallback));
  mFinalCallback = aFinalCallback;

  MOZ_DIAGNOSTIC_ASSERT(mState == State::Initial);
  mState = State::Started;

  nsCOMPtr<nsIRunnable> runnable = NewRunnableMethod(
      "ServiceWorkerJob::AsyncExecute", this, &ServiceWorkerJob::AsyncExecute);

  // We may have to wait for the PBackground actor to be initialized
  // before proceeding.  We should always be able to get a ServiceWorkerManager,
  // however, since Start() should not be called during shutdown.
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown
    return;
  }

  // Otherwise start asynchronously.  We should never run a job synchronously.
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable.forget())));
}

void ServiceWorkerJob::Cancel() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mCanceled);
  mCanceled = true;

  if (GetState() != State::Started) {
    MOZ_ASSERT(GetState() == State::Initial);

    ErrorResult error(NS_ERROR_DOM_ABORT_ERR);
    InvokeResultCallbacks(error);

    // The callbacks might not consume the error, which is fine.
    error.SuppressException();
  }
}

ServiceWorkerJob::ServiceWorkerJob(Type aType, nsIPrincipal* aPrincipal,
                                   const nsACString& aScope,
                                   nsCString aScriptSpec)
    : mType(aType),
      mPrincipal(aPrincipal),
      mScope(aScope),
      mScriptSpec(std::move(aScriptSpec)),
      mState(State::Initial),
      mCanceled(false),
      mResultCallbacksInvoked(false) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPrincipal);
  MOZ_ASSERT(!mScope.IsEmpty());

  // Empty script URL if and only if this is an unregister job.
  MOZ_ASSERT((mType == Type::Unregister) == mScriptSpec.IsEmpty());
}

ServiceWorkerJob::~ServiceWorkerJob() {
  MOZ_ASSERT(NS_IsMainThread());
  // Jobs must finish or never be started.  Destroying an actively running
  // job is an error.
  MOZ_ASSERT(mState != State::Started);
  MOZ_ASSERT_IF(mState == State::Finished, mResultCallbacksInvoked);
}

void ServiceWorkerJob::InvokeResultCallbacks(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mState != State::Finished);
  MOZ_DIAGNOSTIC_ASSERT_IF(mState == State::Initial, Canceled());

  MOZ_DIAGNOSTIC_ASSERT(!mResultCallbacksInvoked);
  mResultCallbacksInvoked = true;

  nsTArray<RefPtr<Callback>> callbackList;
  callbackList.SwapElements(mResultCallbackList);

  for (RefPtr<Callback>& callback : callbackList) {
    // The callback might consume an exception on the ErrorResult, so we need
    // to clone in order to maintain the error for the next callback.
    ErrorResult rv;
    aRv.CloneTo(rv);

    if (GetState() == State::Started) {
      callback->JobFinished(this, rv);
    } else {
      callback->JobDiscarded(rv);
    }

    // The callback might not consume the error.
    rv.SuppressException();
  }
}

void ServiceWorkerJob::InvokeResultCallbacks(nsresult aRv) {
  ErrorResult converted(aRv);
  InvokeResultCallbacks(converted);
}

void ServiceWorkerJob::Finish(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  // Avoid double-completion because it can result on operating on cleaned
  // up data.  This should not happen, though, so also assert to try to
  // narrow down the causes.
  MOZ_DIAGNOSTIC_ASSERT(mState == State::Started);
  if (mState != State::Started) {
    return;
  }

  // Ensure that we only surface SecurityErr, TypeErr or InvalidStateErr to
  // script.
  if (aRv.Failed() && !aRv.ErrorCodeIs(NS_ERROR_DOM_SECURITY_ERR) &&
      !aRv.ErrorCodeIs(NS_ERROR_INTERNAL_ERRORRESULT_TYPEERROR) &&
      !aRv.ErrorCodeIs(NS_ERROR_DOM_INVALID_STATE_ERR)) {
    // Remove the old error code so we can replace it with a TypeError.
    aRv.SuppressException();

    // Throw the type error with a generic error message.  We use a stack
    // reference to bypass the normal static analysis for "return right after
    // throwing", since it's not the right check here: this ErrorResult came in
    // pre-thrown.
    ErrorResult& rv = aRv;
    rv.ThrowTypeError<MSG_SW_INSTALL_ERROR>(mScriptSpec, mScope);
  }

  // The final callback may drop the last ref to this object.
  RefPtr<ServiceWorkerJob> kungFuDeathGrip = this;

  if (!mResultCallbacksInvoked) {
    InvokeResultCallbacks(aRv);
  }

  mState = State::Finished;

  MOZ_DIAGNOSTIC_ASSERT(mFinalCallback);
  if (mFinalCallback) {
    mFinalCallback->JobFinished(this, aRv);
    mFinalCallback = nullptr;
  }

  // The callback might not consume the error.
  aRv.SuppressException();

  // Async release this object to ensure that our caller methods complete
  // as well.
  NS_ReleaseOnMainThread("ServiceWorkerJobProxyRunnable",
                         kungFuDeathGrip.forget(), true /* always proxy */);
}

void ServiceWorkerJob::Finish(nsresult aRv) {
  ErrorResult converted(aRv);
  Finish(converted);
}

}  // namespace dom
}  // namespace mozilla
