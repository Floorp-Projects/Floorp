/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorkerManager.h"
#include "SharedWorkerParent.h"
#include "SharedWorkerService.h"
#include "mozilla/dom/PSharedWorker.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/dom/RemoteWorkerController.h"
#include "nsIConsoleReportCollector.h"
#include "nsINetworkInterceptController.h"
#include "nsIPrincipal.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {

SharedWorkerManager::SharedWorkerManager(
    nsIEventTarget* aPBackgroundEventTarget, const RemoteWorkerData& aData,
    nsIPrincipal* aLoadingPrincipal)
    : mPBackgroundEventTarget(aPBackgroundEventTarget),
      mLoadingPrincipal(aLoadingPrincipal),
      mDomain(aData.domain()),
      mResolvedScriptURL(DeserializeURI(aData.resolvedScriptURL())),
      mName(aData.name()),
      mIsSecureContext(aData.isSecureContext()),
      mSuspended(false),
      mFrozen(false) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aLoadingPrincipal);
}

SharedWorkerManager::~SharedWorkerManager() {
  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Other);

  NS_ProxyRelease("SharedWorkerManager::mLoadingPrincipal", target,
                  mLoadingPrincipal.forget());
  NS_ProxyRelease("SharedWorkerManager::mRemoteWorkerController",
                  mPBackgroundEventTarget, mRemoteWorkerController.forget());
}

bool SharedWorkerManager::MaybeCreateRemoteWorker(
    const RemoteWorkerData& aData, uint64_t aWindowID,
    const MessagePortIdentifier& aPortIdentifier, base::ProcessId aProcessId) {
  AssertIsOnBackgroundThread();

  if (!mRemoteWorkerController) {
    mRemoteWorkerController =
        RemoteWorkerController::Create(aData, this, aProcessId);
    if (NS_WARN_IF(!mRemoteWorkerController)) {
      return false;
    }
  }

  if (aWindowID) {
    mRemoteWorkerController->AddWindowID(aWindowID);
  }

  mRemoteWorkerController->AddPortIdentifier(aPortIdentifier);
  return true;
}

bool SharedWorkerManager::MatchOnMainThread(
    const nsACString& aDomain, nsIURI* aScriptURL, const nsAString& aName,
    nsIPrincipal* aLoadingPrincipal) const {
  MOZ_ASSERT(NS_IsMainThread());
  bool urlEquals;
  if (NS_FAILED(aScriptURL->Equals(mResolvedScriptURL, &urlEquals))) {
    return false;
  }

  return aDomain == mDomain && urlEquals && aName == mName &&
         // We want to be sure that the window's principal subsumes the
         // SharedWorker's loading principal and vice versa.
         mLoadingPrincipal->Subsumes(aLoadingPrincipal) &&
         aLoadingPrincipal->Subsumes(mLoadingPrincipal);
}

void SharedWorkerManager::AddActor(SharedWorkerParent* aParent) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(!mActors.Contains(aParent));

  mActors.AppendElement(aParent);

  // NB: We don't update our Suspended/Frozen state here, yet. The aParent is
  // responsible for doing so from SharedWorkerParent::ManagerCreated.
  // XXX But we could avoid iterating all of our actors because if aParent is
  // not frozen and we are, we would just need to thaw ourselves.
}

void SharedWorkerManager::RemoveActor(SharedWorkerParent* aParent) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mActors.Contains(aParent));

  uint64_t windowID = aParent->WindowID();
  if (windowID) {
    mRemoteWorkerController->RemoveWindowID(windowID);
  }

  mActors.RemoveElement(aParent);

  if (!mActors.IsEmpty()) {
    // Our remaining actors could be all suspended or frozen.
    UpdateSuspend();
    UpdateFrozen();
    return;
  }

  // Time to go.

  mRemoteWorkerController->Terminate();
  mRemoteWorkerController = nullptr;

  // SharedWorkerService exists because it is kept alive by SharedWorkerParent.
  SharedWorkerService::Get()->RemoveWorkerManager(this);
}

void SharedWorkerManager::UpdateSuspend() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mRemoteWorkerController);

  uint32_t suspended = 0;

  for (SharedWorkerParent* actor : mActors) {
    if (actor->IsSuspended()) {
      ++suspended;
    }
  }

  // Call Suspend only when all of our actors' windows are suspended and call
  // Resume only when one of them resumes.
  if ((mSuspended && suspended == mActors.Length()) ||
      (!mSuspended && suspended != mActors.Length())) {
    return;
  }

  if (!mSuspended) {
    mSuspended = true;
    mRemoteWorkerController->Suspend();
  } else {
    mSuspended = false;
    mRemoteWorkerController->Resume();
  }
}

void SharedWorkerManager::UpdateFrozen() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mRemoteWorkerController);

  uint32_t frozen = 0;

  for (SharedWorkerParent* actor : mActors) {
    if (actor->IsFrozen()) {
      ++frozen;
    }
  }

  // Similar to UpdateSuspend, above, we only want to be frozen when all of our
  // actors are frozen.
  if ((mFrozen && frozen == mActors.Length()) ||
      (!mFrozen && frozen != mActors.Length())) {
    return;
  }

  if (!mFrozen) {
    mFrozen = true;
    mRemoteWorkerController->Freeze();
  } else {
    mFrozen = false;
    mRemoteWorkerController->Thaw();
  }
}

bool SharedWorkerManager::IsSecureContext() const { return mIsSecureContext; }

void SharedWorkerManager::CreationFailed() {
  AssertIsOnBackgroundThread();

  for (SharedWorkerParent* actor : mActors) {
    Unused << actor->SendError(NS_ERROR_FAILURE);
  }
}

void SharedWorkerManager::CreationSucceeded() {
  AssertIsOnBackgroundThread();
  // Nothing to do here.
}

void SharedWorkerManager::ErrorReceived(const ErrorValue& aValue) {
  AssertIsOnBackgroundThread();

  for (SharedWorkerParent* actor : mActors) {
    Unused << actor->SendError(aValue);
  }
}

void SharedWorkerManager::Terminated() {
  AssertIsOnBackgroundThread();

  for (SharedWorkerParent* actor : mActors) {
    Unused << actor->SendTerminate();
  }
}

}  // namespace dom
}  // namespace mozilla
