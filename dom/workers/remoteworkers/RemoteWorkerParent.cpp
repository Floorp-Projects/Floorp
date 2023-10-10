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
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Unused.h"
#include "nsProxyRelease.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

class UnregisterActorRunnable final : public Runnable {
 public:
  explicit UnregisterActorRunnable(
      already_AddRefed<ThreadsafeContentParentHandle> aParent)
      : Runnable("UnregisterActorRunnable"), mContentHandle(aParent) {
    AssertIsOnBackgroundThread();
  }

  NS_IMETHOD
  Run() override {
    AssertIsOnMainThread();
    if (RefPtr<ContentParent> contentParent =
            mContentHandle->GetContentParent()) {
      contentParent->UnregisterRemoveWorkerActor();
    }

    return NS_OK;
  }

 private:
  RefPtr<ThreadsafeContentParentHandle> mContentHandle;
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
  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(Manager());

  // Parent is null if the child actor runs on the parent process.
  if (parent) {
    if (!aAlreadyRegistered) {
      parent->RegisterRemoteWorkerActor();
    }

    NS_ReleaseOnMainThread("RemoteWorkerParent::Initialize ContentParent",
                           parent.forget());
  }
}

already_AddRefed<PFetchEventOpProxyParent>
RemoteWorkerParent::AllocPFetchEventOpProxyParent(
    const ParentToChildServiceWorkerFetchEventOpArgs& aArgs) {
  MOZ_CRASH("PFetchEventOpProxyParent actors must be manually constructed!");
  return nullptr;
}

void RemoteWorkerParent::ActorDestroy(IProtocol::ActorDestroyReason) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(Manager());

  // Parent is null if the child actor runs on the parent process.
  if (parent) {
    RefPtr<UnregisterActorRunnable> r =
        new UnregisterActorRunnable(parent.forget());
    SchedulerGroup::Dispatch(r.forget());
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

IPCResult RemoteWorkerParent::RecvNotifyLock(const bool& aCreated) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mController) {
    mController->NotifyLock(aCreated);
  }

  return IPC_OK();
}

IPCResult RemoteWorkerParent::RecvNotifyWebTransport(const bool& aCreated) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mController) {
    mController->NotifyWebTransport(aCreated);
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
        GetCurrentSerialEventTarget(), __func__,
        [resolve = aResolve](bool /* unused */) { resolve(true); },
        [resolve = aResolve](nsresult /* unused */) { resolve(false); });
  } else {
    aResolve(false);
  }

  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla
