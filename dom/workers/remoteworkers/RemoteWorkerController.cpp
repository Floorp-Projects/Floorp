/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerController.h"

#include <utility>

#include "nsDebug.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/MessagePortParent.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/dom/ServiceWorkerCloneData.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "RemoteWorkerControllerParent.h"
#include "RemoteWorkerManager.h"
#include "RemoteWorkerParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

/* static */
already_AddRefed<RemoteWorkerController> RemoteWorkerController::Create(
    const RemoteWorkerData& aData, RemoteWorkerObserver* aObserver,
    base::ProcessId aProcessId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
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
}

RemoteWorkerController::~RemoteWorkerController() {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(mPendingOps.IsEmpty());
}

void RemoteWorkerController::SetWorkerActor(RemoteWorkerParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(aActor);

  mActor = aActor;
}

void RemoteWorkerController::NoteDeadWorkerActor() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mActor);

  // The actor has been destroyed without a proper close() notification. Let's
  // inform the observer.
  if (mState == eReady) {
    mObserver->Terminated();
  }

  mActor = nullptr;

  Shutdown();
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

  NoteDeadWorker();

  mObserver->CreationFailed();
}

void RemoteWorkerController::CreationSucceeded() {
  AssertIsOnBackgroundThread();
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

  auto pendingOps = std::move(mPendingOps);

  for (auto& op : pendingOps) {
    DebugOnly<bool> started = op->MaybeStart(this);
    MOZ_ASSERT(started);
  }
}

void RemoteWorkerController::ErrorPropagation(const ErrorValue& aValue) {
  AssertIsOnBackgroundThread();

  mObserver->ErrorReceived(aValue);
}

void RemoteWorkerController::WorkerTerminated() {
  AssertIsOnBackgroundThread();

  NoteDeadWorker();

  mObserver->Terminated();
}

void RemoteWorkerController::CancelAllPendingOps() {
  AssertIsOnBackgroundThread();

  auto pendingOps = std::move(mPendingOps);

  for (auto& op : pendingOps) {
    op->Cancel();
  }
}

void RemoteWorkerController::Shutdown() {
  AssertIsOnBackgroundThread();
  Unused << NS_WARN_IF(mIsServiceWorker && !mPendingOps.IsEmpty());

  if (mState == eTerminated) {
    MOZ_ASSERT(mPendingOps.IsEmpty());
    return;
  }

  mState = eTerminated;

  CancelAllPendingOps();

  if (!mActor) {
    return;
  }

  mActor->SetController(nullptr);

  /**
   * The "non-remote-side" of the Service Worker will have ensured that the
   * remote worker is terminated before calling `Shutdown().`
   */
  if (mIsServiceWorker) {
    mActor->MaybeSendDelete();
  } else {
    Unused << mActor->SendExecOp(RemoteWorkerTerminateOp());
  }

  mActor = nullptr;
}

void RemoteWorkerController::NoteDeadWorker() {
  AssertIsOnBackgroundThread();

  CancelAllPendingOps();

  /**
   * The "non-remote-side" of the Service Worker will initiate `Shutdown()`
   * once it's notified that all dispatched operations have either completed
   * or canceled. That is, it'll explicitly call `Shutdown()` later.
   */
  if (!mIsServiceWorker) {
    Shutdown();
  }
}

template <typename... Args>
void RemoteWorkerController::MaybeStartSharedWorkerOp(Args&&... aArgs) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mIsServiceWorker);

  UniquePtr<PendingSharedWorkerOp> op =
      MakeUnique<PendingSharedWorkerOp>(std::forward<Args>(aArgs)...);

  if (!op->MaybeStart(this)) {
    mPendingOps.AppendElement(std::move(op));
  }
}

void RemoteWorkerController::AddWindowID(uint64_t aWindowID) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aWindowID);

  MaybeStartSharedWorkerOp(PendingSharedWorkerOp::eAddWindowID, aWindowID);
}

void RemoteWorkerController::RemoveWindowID(uint64_t aWindowID) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aWindowID);

  MaybeStartSharedWorkerOp(PendingSharedWorkerOp::eRemoveWindowID, aWindowID);
}

void RemoteWorkerController::AddPortIdentifier(
    const MessagePortIdentifier& aPortIdentifier) {
  AssertIsOnBackgroundThread();

  MaybeStartSharedWorkerOp(aPortIdentifier);
}

void RemoteWorkerController::Terminate() {
  AssertIsOnBackgroundThread();

  MaybeStartSharedWorkerOp(PendingSharedWorkerOp::eTerminate);
}

void RemoteWorkerController::Suspend() {
  AssertIsOnBackgroundThread();

  MaybeStartSharedWorkerOp(PendingSharedWorkerOp::eSuspend);
}

void RemoteWorkerController::Resume() {
  AssertIsOnBackgroundThread();

  MaybeStartSharedWorkerOp(PendingSharedWorkerOp::eResume);
}

void RemoteWorkerController::Freeze() {
  AssertIsOnBackgroundThread();

  MaybeStartSharedWorkerOp(PendingSharedWorkerOp::eFreeze);
}

void RemoteWorkerController::Thaw() {
  AssertIsOnBackgroundThread();

  MaybeStartSharedWorkerOp(PendingSharedWorkerOp::eThaw);
}

RemoteWorkerController::PendingSharedWorkerOp::PendingSharedWorkerOp(
    Type aType, uint64_t aWindowID)
    : mType(aType), mWindowID(aWindowID) {
  AssertIsOnBackgroundThread();
}

RemoteWorkerController::PendingSharedWorkerOp::PendingSharedWorkerOp(
    const MessagePortIdentifier& aPortIdentifier)
    : mType(ePortIdentifier), mPortIdentifier(aPortIdentifier) {
  AssertIsOnBackgroundThread();
}

RemoteWorkerController::PendingSharedWorkerOp::~PendingSharedWorkerOp() {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(mCompleted);
}

bool RemoteWorkerController::PendingSharedWorkerOp::MaybeStart(
    RemoteWorkerController* const aOwner) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mCompleted);
  MOZ_ASSERT(aOwner);

  if (aOwner->mState == RemoteWorkerController::eTerminated) {
    Cancel();
    return true;
  }

  if (aOwner->mState == RemoteWorkerController::ePending &&
      mType != eTerminate) {
    return false;
  }

  switch (mType) {
    case eTerminate:
      aOwner->Shutdown();
      break;
    case eSuspend:
      Unused << aOwner->mActor->SendExecOp(RemoteWorkerSuspendOp());
      break;
    case eResume:
      Unused << aOwner->mActor->SendExecOp(RemoteWorkerResumeOp());
      break;
    case eFreeze:
      Unused << aOwner->mActor->SendExecOp(RemoteWorkerFreezeOp());
      break;
    case eThaw:
      Unused << aOwner->mActor->SendExecOp(RemoteWorkerThawOp());
      break;
    case ePortIdentifier:
      Unused << aOwner->mActor->SendExecOp(
          RemoteWorkerPortIdentifierOp(mPortIdentifier));
      break;
    case eAddWindowID:
      Unused << aOwner->mActor->SendExecOp(
          RemoteWorkerAddWindowIDOp(mWindowID));
      break;
    case eRemoveWindowID:
      Unused << aOwner->mActor->SendExecOp(
          RemoteWorkerRemoveWindowIDOp(mWindowID));
      break;
    default:
      MOZ_CRASH("Unknown op.");
  }

  mCompleted = true;

  return true;
}

void RemoteWorkerController::PendingSharedWorkerOp::Cancel() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mCompleted);

  // We don't want to leak the port if the operation has not been processed.
  if (mType == ePortIdentifier) {
    MessagePortParent::ForceClose(mPortIdentifier.uuid(),
                                  mPortIdentifier.destinationUuid(),
                                  mPortIdentifier.sequenceId());
  }

  mCompleted = true;
}

RemoteWorkerController::PendingServiceWorkerOp::PendingServiceWorkerOp(
    ServiceWorkerOpArgs&& aArgs,
    RefPtr<ServiceWorkerOpPromise::Private> aPromise)
    : mArgs(std::move(aArgs)), mPromise(std::move(aPromise)) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mPromise);
}

RemoteWorkerController::PendingServiceWorkerOp::~PendingServiceWorkerOp() {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(!mPromise);
}

bool RemoteWorkerController::PendingServiceWorkerOp::MaybeStart(
    RemoteWorkerController* const aOwner) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mPromise);
  MOZ_ASSERT(aOwner);

  if (NS_WARN_IF(aOwner->mState == RemoteWorkerController::eTerminated)) {
    mPromise->Reject(NS_ERROR_DOM_ABORT_ERR, __func__);
    mPromise = nullptr;
    return true;
  }

  // The target content process must still be starting up.
  if (!aOwner->mActor) {
    return false;
  }

  /**
   * Allow termination operations to pass through while pending because the
   * remote Service Worker can be terminated while still starting up.
   */
  if (aOwner->mState == RemoteWorkerController::ePending &&
      mArgs.type() !=
          ServiceWorkerOpArgs::TServiceWorkerTerminateWorkerOpArgs) {
    return false;
  }

  if (mArgs.type() == ServiceWorkerOpArgs::TServiceWorkerMessageEventOpArgs) {
    auto& args = mArgs.get_ServiceWorkerMessageEventOpArgs();

    ServiceWorkerMessageEventOpArgs copyArgs;
    copyArgs.clientInfoAndState() = std::move(args.clientInfoAndState());

    RefPtr<ServiceWorkerCloneData> copyData = new ServiceWorkerCloneData();
    copyData->StealFromClonedMessageDataForBackgroundParent(args.clonedData());

    if (!copyData->BuildClonedMessageDataForBackgroundParent(
            aOwner->mActor->Manager(), copyArgs.clonedData())) {
      mPromise->Reject(NS_ERROR_DOM_DATA_CLONE_ERR, __func__);
      mPromise = nullptr;
      return true;
    }

    mArgs = std::move(copyArgs);
  }

  return true;
}

void RemoteWorkerController::PendingServiceWorkerOp::Cancel() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mPromise);

  mPromise->Reject(NS_ERROR_DOM_ABORT_ERR, __func__);
  mPromise = nullptr;
}

}  // namespace dom
}  // namespace mozilla
