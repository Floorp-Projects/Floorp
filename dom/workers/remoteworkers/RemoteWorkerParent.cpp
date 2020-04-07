/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerParent.h"
#include "RemoteWorkerController.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/PFetchEventOpProxyParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Unused.h"
#include "nsProxyRelease.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

class UnregisterActorRunnable final : public Runnable {
 public:
  explicit UnregisterActorRunnable(already_AddRefed<ContentParent> aParent)
      : Runnable("UnregisterActorRunnable"), mContentParent(aParent) {
    AssertIsOnBackgroundThread();
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    mContentParent->UnregisterRemoveWorkerActor();
    mContentParent = nullptr;

    return NS_OK;
  }

 private:
  RefPtr<ContentParent> mContentParent;
};

}  // namespace

RemoteWorkerParent::RemoteWorkerParent() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
}

RemoteWorkerParent::~RemoteWorkerParent() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
}

void RemoteWorkerParent::Initialize(bool aAlreadyRegistered) {
  RefPtr<ContentParent> parent = BackgroundParent::GetContentParent(Manager());

  // Parent is null if the child actor runs on the parent process.
  if (parent) {
    if (!aAlreadyRegistered) {
      parent->RegisterRemoteWorkerActor();
    }

    NS_ReleaseOnMainThread("RemoteWorkerParent::Initialize ContentParent",
                           parent.forget());
  }
}

PFetchEventOpProxyParent* RemoteWorkerParent::AllocPFetchEventOpProxyParent(
    const ServiceWorkerFetchEventOpArgs& aArgs) {
  MOZ_CRASH("PFetchEventOpProxyParent actors must be manually constructed!");
  return nullptr;
}

bool RemoteWorkerParent::DeallocPFetchEventOpProxyParent(
    PFetchEventOpProxyParent* aActor) {
  MOZ_ASSERT(XRE_IsParentProcess());
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

void RemoteWorkerParent::ActorDestroy(IProtocol::ActorDestroyReason) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  RefPtr<ContentParent> parent = BackgroundParent::GetContentParent(Manager());

  // Parent is null if the child actor runs on the parent process.
  if (parent) {
    RefPtr<UnregisterActorRunnable> r =
        new UnregisterActorRunnable(parent.forget());

    SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
  }

  if (mController) {
    mController->NoteDeadWorkerActor();
    mController = nullptr;
  }
}

IPCResult RemoteWorkerParent::RecvCreated(const bool& aStatus) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!mController) {
    return IPC_OK();
  }

  if (aStatus) {
    mController->CreationSucceeded();
  } else {
    mController->CreationFailed();
  }

  return IPC_OK();
}

IPCResult RemoteWorkerParent::RecvError(const ErrorValue& aValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mController) {
    mController->ErrorPropagation(aValue);
  }

  return IPC_OK();
}

void RemoteWorkerParent::MaybeSendDelete() {
  if (mDeleteSent) {
    return;
  }

  // For some reason, if the following two lines are swapped, ASan says there's
  // a UAF...
  mDeleteSent = true;
  Unused << Send__delete__(this);
}

IPCResult RemoteWorkerParent::RecvClose() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mController) {
    mController->WorkerTerminated();
  }

  MaybeSendDelete();

  return IPC_OK();
}

void RemoteWorkerParent::SetController(RemoteWorkerController* aController) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  mController = aController;
}

IPCResult RemoteWorkerParent::RecvSetServiceWorkerSkipWaitingFlag(
    SetServiceWorkerSkipWaitingFlagResolver&& aResolve) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mController) {
    mController->SetServiceWorkerSkipWaitingFlag()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [resolve = aResolve](bool /* unused */) { resolve(true); },
        [resolve = aResolve](nsresult /* unused */) { resolve(false); });
  } else {
    aResolve(false);
  }

  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla
