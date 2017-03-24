/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUpdaterChild.h"

namespace mozilla {
namespace dom {
namespace workers {

ServiceWorkerUpdaterChild::ServiceWorkerUpdaterChild(Runnable* aSuccessRunnable,
                                                     Runnable* aFailureRunnable)
  : mSuccessRunnable(aSuccessRunnable)
  , mFailureRunnable(aFailureRunnable)
{
  MOZ_ASSERT(aSuccessRunnable);
}

mozilla::ipc::IPCResult
ServiceWorkerUpdaterChild::RecvProceed(const bool& aAllowed)
{
  if (aAllowed) {
    mSuccessRunnable->Run();
  } else if (mFailureRunnable) {
    mFailureRunnable->Run();
  }

  Unused << Send__delete__(this);
  return IPC_OK();
}

} // namespace workers
} // namespace dom
} // namespace mozilla
