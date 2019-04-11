/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerParent.h"
#include "RemoteWorkerController.h"
#include "mozilla/dom/ContentParent.h"
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

void RemoteWorkerParent::Initialize() {
  RefPtr<ContentParent> parent = BackgroundParent::GetContentParent(Manager());

  // Parent is null if the child actor runs on the parent process.
  if (parent) {
    parent->RegisterRemoteWorkerActor();

    nsCOMPtr<nsIEventTarget> target =
        SystemGroup::EventTargetFor(TaskCategory::Other);

    NS_ProxyRelease("RemoteWorkerParent::Initialize ContentParent", target,
                    parent.forget());
  }
}

void RemoteWorkerParent::ActorDestroy(IProtocol::ActorDestroyReason) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  RefPtr<ContentParent> parent = BackgroundParent::GetContentParent(Manager());

  // Parent is null if the child actor runs on the parent process.
  if (parent) {
    RefPtr<UnregisterActorRunnable> r =
        new UnregisterActorRunnable(parent.forget());

    nsCOMPtr<nsIEventTarget> target =
        SystemGroup::EventTargetFor(TaskCategory::Other);
    target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  }

  if (mController) {
    mController->ForgetActorAndTerminate();
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

IPCResult RemoteWorkerParent::RecvClose() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mController) {
    mController->WorkerTerminated();
  }

  Unused << Send__delete__(this);
  return IPC_OK();
}

void RemoteWorkerParent::SetController(RemoteWorkerController* aController) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  mController = aController;
}

}  // namespace dom
}  // namespace mozilla
