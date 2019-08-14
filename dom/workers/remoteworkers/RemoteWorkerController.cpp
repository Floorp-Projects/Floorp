/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortParent.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "RemoteWorkerController.h"
#include "RemoteWorkerManager.h"
#include "RemoteWorkerParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

/* static */
already_AddRefed<RemoteWorkerController> RemoteWorkerController::Create(
    const RemoteWorkerData& aData, RemoteWorkerObserver* aObserver,
    base::ProcessId aProcessId) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aObserver);

  RefPtr<RemoteWorkerController> controller =
      new RemoteWorkerController(aData, aObserver);

  RefPtr<RemoteWorkerManager> manager = RemoteWorkerManager::GetOrCreate();
  MOZ_ASSERT(manager);

  manager->Launch(controller, aData, aProcessId);

  return controller.forget();
}

RemoteWorkerController::RemoteWorkerController(const RemoteWorkerData& aData,
                                               RemoteWorkerObserver* aObserver)
    : mObserver(aObserver),
      mState(ePending),
      mIsServiceWorker(aData.serviceWorkerData().type() ==
                       OptionalServiceWorkerData::TServiceWorkerData) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
}

RemoteWorkerController::~RemoteWorkerController() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
}

void RemoteWorkerController::SetWorkerActor(RemoteWorkerParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(aActor);

  mActor = aActor;
}

void RemoteWorkerController::CreationFailed() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mState == ePending || mState == eTerminated);

  if (mState == eTerminated) {
    MOZ_ASSERT(!mActor);
    MOZ_ASSERT(mPendingOps.IsEmpty());
    // Nothing to do.
    return;
  }

  Shutdown();
  mObserver->CreationFailed();
}

void RemoteWorkerController::CreationSucceeded() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mState == ePending || mState == eTerminated);

  if (mState == eTerminated) {
    MOZ_ASSERT(!mActor);
    MOZ_ASSERT(mPendingOps.IsEmpty());
    // Nothing to do.
    return;
  }

  MOZ_ASSERT(mActor);
  mState = eReady;

  mObserver->CreationSucceeded();

  for (UniquePtr<Op>& op : mPendingOps) {
    switch (op->mType) {
      case Op::eTerminate:
        Terminate();
        break;

      case Op::eSuspend:
        Suspend();
        break;

      case Op::eResume:
        Resume();
        break;

      case Op::eFreeze:
        Freeze();
        break;

      case Op::eThaw:
        Thaw();
        break;

      case Op::ePortIdentifier:
        AddPortIdentifier(op->mPortIdentifier);
        break;

      case Op::eAddWindowID:
        AddWindowID(op->mWindowID);
        break;

      case Op::eRemoveWindowID:
        RemoveWindowID(op->mWindowID);
        break;

      default:
        MOZ_CRASH("Unknown op.");
    }

    op->Completed();
  }

  mPendingOps.Clear();
}

void RemoteWorkerController::ErrorPropagation(const ErrorValue& aValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  mObserver->ErrorReceived(aValue);
}

void RemoteWorkerController::WorkerTerminated() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mState == eReady);

  mObserver->Terminated();
  Shutdown();
}

void RemoteWorkerController::Shutdown() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mState == ePending || mState == eReady);

  mState = eTerminated;

  mPendingOps.Clear();

  if (mActor) {
    mActor->SetController(nullptr);
    Unused << mActor->SendExecOp(RemoteWorkerTerminateOp());
    mActor = nullptr;
  }
}

void RemoteWorkerController::AddWindowID(uint64_t aWindowID) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aWindowID);

  if (mState == ePending) {
    mPendingOps.AppendElement(new Op(Op::eAddWindowID, aWindowID));
    return;
  }

  if (mState == eTerminated) {
    return;
  }

  MOZ_ASSERT(mState == eReady);
  Unused << mActor->SendExecOp(RemoteWorkerAddWindowIDOp(aWindowID));
}

void RemoteWorkerController::RemoveWindowID(uint64_t aWindowID) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aWindowID);

  if (mState == ePending) {
    mPendingOps.AppendElement(new Op(Op::eRemoveWindowID, aWindowID));
    return;
  }

  if (mState == eTerminated) {
    return;
  }

  MOZ_ASSERT(mState == eReady);
  Unused << mActor->SendExecOp(RemoteWorkerRemoveWindowIDOp(aWindowID));
}

void RemoteWorkerController::AddPortIdentifier(
    const MessagePortIdentifier& aPortIdentifier) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mState == ePending) {
    mPendingOps.AppendElement(new Op(aPortIdentifier));
    return;
  }

  if (mState == eTerminated) {
    return;
  }

  MOZ_ASSERT(mState == eReady);
  Unused << mActor->SendExecOp(RemoteWorkerPortIdentifierOp(aPortIdentifier));
}

void RemoteWorkerController::ForgetActorAndTerminate() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  // The actor has been destroyed without a proper close() notification. Let's
  // inform the observer.
  if (mState == eReady) {
    mObserver->Terminated();
  }

  mActor = nullptr;
  Terminate();
}

void RemoteWorkerController::Terminate() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mState == eTerminated) {
    return;
  }

  Shutdown();
}

void RemoteWorkerController::Suspend() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mState == ePending) {
    mPendingOps.AppendElement(new Op(Op::eSuspend));
    return;
  }

  if (mState == eTerminated) {
    return;
  }

  MOZ_ASSERT(mState == eReady);
  Unused << mActor->SendExecOp(RemoteWorkerSuspendOp());
}

void RemoteWorkerController::Resume() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mState == ePending) {
    mPendingOps.AppendElement(new Op(Op::eResume));
    return;
  }

  if (mState == eTerminated) {
    return;
  }

  MOZ_ASSERT(mState == eReady);
  Unused << mActor->SendExecOp(RemoteWorkerResumeOp());
}

void RemoteWorkerController::Freeze() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mState == ePending) {
    mPendingOps.AppendElement(new Op(Op::eFreeze));
    return;
  }

  if (mState == eTerminated) {
    return;
  }

  MOZ_ASSERT(mState == eReady);
  Unused << mActor->SendExecOp(RemoteWorkerFreezeOp());
}

void RemoteWorkerController::Thaw() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mState == ePending) {
    mPendingOps.AppendElement(new Op(Op::eThaw));
    return;
  }

  if (mState == eTerminated) {
    return;
  }

  MOZ_ASSERT(mState == eReady);
  Unused << mActor->SendExecOp(RemoteWorkerThawOp());
}

RemoteWorkerController::Op::~Op() {
  MOZ_COUNT_DTOR(Op);

  // We don't want to leak the port if the operation has not been processed.
  if (!mCompleted && mType == ePortIdentifier) {
    MessagePortParent::ForceClose(mPortIdentifier.uuid(),
                                  mPortIdentifier.destinationUuid(),
                                  mPortIdentifier.sequenceId());
  }
}

}  // namespace dom
}  // namespace mozilla
