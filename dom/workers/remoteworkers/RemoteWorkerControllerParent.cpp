/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerControllerParent.h"

#include <utility>

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsThreadUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

RemoteWorkerControllerParent::RemoteWorkerControllerParent(
    const RemoteWorkerData& aRemoteWorkerData)
    : mRemoteWorkerController(RemoteWorkerController::Create(
          aRemoteWorkerData, this, 0 /* random process ID */)) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mRemoteWorkerController);
}

RefPtr<RemoteWorkerParent> RemoteWorkerControllerParent::GetRemoteWorkerParent()
    const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mRemoteWorkerController);

  return mRemoteWorkerController->mActor;
}

RemoteWorkerControllerParent::~RemoteWorkerControllerParent() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mIPCActive);
  MOZ_ASSERT(!mRemoteWorkerController);
}

IPCResult RemoteWorkerControllerParent::RecvShutdown(
    ShutdownResolver&& aResolve) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mIPCActive);
  MOZ_ASSERT(mRemoteWorkerController);

  mIPCActive = false;

  mRemoteWorkerController->Shutdown();
  mRemoteWorkerController = nullptr;

  aResolve(true);

  return IPC_OK();
}

IPCResult RemoteWorkerControllerParent::Recv__delete__() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mIPCActive);
  MOZ_ASSERT(!mRemoteWorkerController);

  return IPC_OK();
}

void RemoteWorkerControllerParent::ActorDestroy(ActorDestroyReason aReason) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mIPCActive)) {
    mIPCActive = false;
  }

  if (NS_WARN_IF(mRemoteWorkerController)) {
    mRemoteWorkerController->Shutdown();
    mRemoteWorkerController = nullptr;
  }
}

void RemoteWorkerControllerParent::CreationFailed() {
  AssertIsOnBackgroundThread();

  if (!mIPCActive) {
    return;
  }

  Unused << SendCreationFailed();
}

void RemoteWorkerControllerParent::CreationSucceeded() {
  AssertIsOnBackgroundThread();

  if (!mIPCActive) {
    return;
  }

  Unused << SendCreationSucceeded();
}

void RemoteWorkerControllerParent::ErrorReceived(const ErrorValue& aValue) {
  AssertIsOnBackgroundThread();

  if (!mIPCActive) {
    return;
  }

  Unused << SendErrorReceived(aValue);
}

void RemoteWorkerControllerParent::Terminated() {
  AssertIsOnBackgroundThread();

  if (!mIPCActive) {
    return;
  }

  Unused << SendTerminated();
}

}  // namespace dom
}  // namespace mozilla
