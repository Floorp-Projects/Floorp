/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerControllerChild.h"

#include <utility>

#include "MainThreadUtils.h"
#include "nsError.h"
#include "nsThreadUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/PFetchEventOpChild.h"

namespace mozilla {

using ipc::IPCResult;

namespace dom {

RemoteWorkerControllerChild::RemoteWorkerControllerChild(
    RefPtr<RemoteWorkerObserver> aObserver)
    : mObserver(std::move(aObserver)) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mObserver);
}

PFetchEventOpChild* RemoteWorkerControllerChild::AllocPFetchEventOpChild(
    const ServiceWorkerFetchEventOpArgs& aArgs) {
  MOZ_CRASH("PFetchEventOpChild actors must be manually constructed!");
  return nullptr;
}

bool RemoteWorkerControllerChild::DeallocPFetchEventOpChild(
    PFetchEventOpChild* aActor) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

void RemoteWorkerControllerChild::ActorDestroy(ActorDestroyReason aReason) {
  AssertIsOnMainThread();

  mIPCActive = false;

  if (NS_WARN_IF(mObserver)) {
    mObserver->ErrorReceived(NS_ERROR_DOM_ABORT_ERR);
  }
}

IPCResult RemoteWorkerControllerChild::RecvCreationFailed() {
  AssertIsOnMainThread();

  if (mObserver) {
    mObserver->CreationFailed();
  }

  return IPC_OK();
}

IPCResult RemoteWorkerControllerChild::RecvCreationSucceeded() {
  AssertIsOnMainThread();

  if (mObserver) {
    mObserver->CreationSucceeded();
  }

  return IPC_OK();
}

IPCResult RemoteWorkerControllerChild::RecvErrorReceived(
    const ErrorValue& aError) {
  AssertIsOnMainThread();

  if (mObserver) {
    mObserver->ErrorReceived(aError);
  }

  return IPC_OK();
}

IPCResult RemoteWorkerControllerChild::RecvTerminated() {
  AssertIsOnMainThread();

  if (mObserver) {
    mObserver->Terminated();
  }

  return IPC_OK();
}

void RemoteWorkerControllerChild::RevokeObserver(
    RemoteWorkerObserver* aObserver) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aObserver);
  MOZ_ASSERT(aObserver == mObserver);

  mObserver = nullptr;
}

void RemoteWorkerControllerChild::MaybeSendDelete() {
  AssertIsOnMainThread();

  if (!mIPCActive) {
    return;
  }

  RefPtr<RemoteWorkerControllerChild> self = this;

  SendShutdown()->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [self = std::move(self)](const ShutdownPromise::ResolveOrRejectValue&) {
        if (self->mIPCActive) {
          Unused << self->Send__delete__(self);
        }
      });
}

}  // namespace dom
}  // namespace mozilla
