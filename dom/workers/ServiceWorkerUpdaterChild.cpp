/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUpdaterChild.h"

namespace mozilla {
namespace dom {
namespace workers {

ServiceWorkerUpdaterChild::ServiceWorkerUpdaterChild(GenericPromise* aPromise,
                                                     Runnable* aSuccessRunnable,
                                                     Runnable* aFailureRunnable)
  : mSuccessRunnable(aSuccessRunnable)
  , mFailureRunnable(aFailureRunnable)
{
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(aSuccessRunnable);
  MOZ_ASSERT(aFailureRunnable);

  aPromise->Then(AbstractThread::GetCurrent(), __func__,
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
  } else {
    mFailureRunnable->Run();
  }

  return IPC_OK();
}

void
ServiceWorkerUpdaterChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mPromiseHolder.DisconnectIfExists();
}

} // namespace workers
} // namespace dom
} // namespace mozilla
