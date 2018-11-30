/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorkerParent.h"
#include "SharedWorkerManager.h"
#include "SharedWorkerService.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/Unused.h"

namespace mozilla {

using namespace ipc;

namespace dom {

SharedWorkerParent::SharedWorkerParent()
    : mBackgroundEventTarget(GetCurrentThreadEventTarget()),
      mStatus(eInit),
      mSuspended(false),
      mFrozen(false) {
  AssertIsOnBackgroundThread();
}

SharedWorkerParent::~SharedWorkerParent() = default;

void SharedWorkerParent::ActorDestroy(IProtocol::ActorDestroyReason aReason) {
  AssertIsOnBackgroundThread();

  if (mWorkerManager) {
    mWorkerManager->RemoveActor(this);
    mWorkerManager = nullptr;
  }
}

void SharedWorkerParent::Initialize(
    const RemoteWorkerData& aData, uint64_t aWindowID,
    const MessagePortIdentifier& aPortIdentifier) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mStatus == eInit);

  // Let's keep the service alive.
  mService = SharedWorkerService::GetOrCreate();
  MOZ_ASSERT(mService);

  mWindowID = aWindowID;

  mStatus = ePending;
  mService->GetOrCreateWorkerManager(this, aData, aWindowID, aPortIdentifier);
}

IPCResult SharedWorkerParent::RecvClose() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mStatus == ePending || mStatus == eActive);

  mStatus = eClosed;

  if (mWorkerManager) {
    mWorkerManager->RemoveActor(this);
    mWorkerManager = nullptr;
  }

  Unused << Send__delete__(this);
  return IPC_OK();
}

IPCResult SharedWorkerParent::RecvSuspend() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mSuspended);
  MOZ_ASSERT(mStatus == ePending || mStatus == eActive);

  mSuspended = true;

  if (mStatus == eActive) {
    MOZ_ASSERT(mWorkerManager);
    mWorkerManager->UpdateSuspend();
  }

  return IPC_OK();
}

IPCResult SharedWorkerParent::RecvResume() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mSuspended);
  MOZ_ASSERT(mStatus == ePending || mStatus == eActive);

  mSuspended = false;

  if (mStatus == eActive) {
    MOZ_ASSERT(mWorkerManager);
    mWorkerManager->UpdateSuspend();
  }

  return IPC_OK();
}

IPCResult SharedWorkerParent::RecvFreeze() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mFrozen);
  MOZ_ASSERT(mStatus == ePending || mStatus == eActive);

  mFrozen = true;

  if (mStatus == eActive) {
    MOZ_ASSERT(mWorkerManager);
    mWorkerManager->UpdateFrozen();
  }

  return IPC_OK();
}

IPCResult SharedWorkerParent::RecvThaw() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mFrozen);
  MOZ_ASSERT(mStatus == ePending || mStatus == eActive);

  mFrozen = false;

  if (mStatus == eActive) {
    MOZ_ASSERT(mWorkerManager);
    mWorkerManager->UpdateFrozen();
  }

  return IPC_OK();
}

void SharedWorkerParent::ManagerCreated(SharedWorkerManager* aWorkerManager) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aWorkerManager);
  MOZ_ASSERT(!mWorkerManager);
  MOZ_ASSERT(mStatus == ePending || mStatus == eClosed);

  // Already gone.
  if (mStatus == eClosed) {
    aWorkerManager->RemoveActor(this);
    return;
  }

  mStatus = eActive;
  mWorkerManager = aWorkerManager;

  mWorkerManager->UpdateFrozen();
  mWorkerManager->UpdateSuspend();
}

void SharedWorkerParent::ErrorPropagation(nsresult aError) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(NS_FAILED(aError));
  MOZ_ASSERT(mStatus == ePending || mStatus == eClosed);

  // Already gone.
  if (mStatus == eClosed) {
    return;
  }

  Unused << SendError(aError);
}

}  // namespace dom
}  // namespace mozilla
