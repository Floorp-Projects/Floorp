/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUpdaterChild.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

ServiceWorkerUpdaterChild::ServiceWorkerUpdaterChild(GenericPromise* aPromise,
                                                     CancelableRunnable* aSuccessRunnable,
                                                     CancelableRunnable* aFailureRunnable)
  : mSuccessRunnable(aSuccessRunnable)
  , mFailureRunnable(aFailureRunnable)
{
  // TODO: remove the main thread restriction after fixing bug 1364821.
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_DIAGNOSTIC_ASSERT(!ServiceWorkerParentInterceptEnabled());

  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(aSuccessRunnable);
  MOZ_ASSERT(aFailureRunnable);

  aPromise->Then(GetMainThreadSerialEventTarget(), __func__,
    [this]() {
      mPromiseHolder.Complete();
      Unused << Send__delete__(this);
  }).Track(mPromiseHolder);
}

mozilla::ipc::IPCResult
ServiceWorkerUpdaterChild::RecvProceed(const bool& aAllowed)
{
  // If we have a callback, it will resolve the promise.

  if (aAllowed) {
    mSuccessRunnable->Run();
    mFailureRunnable->Cancel();
  } else {
    mFailureRunnable->Run();
    mSuccessRunnable->Cancel();
  }

  mSuccessRunnable = nullptr;
  mFailureRunnable = nullptr;

  return IPC_OK();
}

void
ServiceWorkerUpdaterChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mSuccessRunnable) {
    mSuccessRunnable->Cancel();
  }

  if (mFailureRunnable) {
    mFailureRunnable->Cancel();
  }

  mPromiseHolder.DisconnectIfExists();
}

} // namespace dom
} // namespace mozilla
