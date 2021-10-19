/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LockManagerChild.h"
#include "LockRequestChild.h"

namespace mozilla::dom::locks {

NS_IMPL_CYCLE_COLLECTION(LockManagerChild, mOwner)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(LockManagerChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(LockManagerChild, Release)

void LockManagerChild::RequestLock(const LockRequest& aRequest,
                                   const LockOptions& aOptions) {
  auto requestActor = MakeRefPtr<LockRequestChild>(aRequest, aOptions.mSignal);
  SendPLockRequestConstructor(
      requestActor, IPCLockRequest(nsString(aRequest.mName), aOptions.mMode,
                                   aOptions.mIfAvailable, aOptions.mSteal));
}

}  // namespace mozilla::dom::locks
