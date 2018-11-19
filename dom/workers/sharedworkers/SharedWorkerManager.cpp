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
#include "mozilla/dom/RemoteWorkerController.h"
#include "nsIConsoleReportCollector.h"
#include "nsINetworkInterceptController.h"
#include "nsIPrincipal.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {

SharedWorkerManager::SharedWorkerManager(nsIEventTarget* aPBackgroundEventTarget,
                                         const RemoteWorkerData& aData,
                                         nsIPrincipal* aLoadingPrincipal)
  : mPBackgroundEventTarget(aPBackgroundEventTarget)
  , mLoadingPrincipal(aLoadingPrincipal)
  , mDomain(aData.domain())
  , mResolvedScriptURL(aData.resolvedScriptURL())
  , mName(aData.name())
  , mIsSecureContext(aData.isSecureContext())
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aLoadingPrincipal);
}

SharedWorkerManager::~SharedWorkerManager()
{
  nsCOMPtr<nsIEventTarget> target =
    SystemGroup::EventTargetFor(TaskCategory::Other);

  NS_ProxyRelease("SharedWorkerManager::mLoadingPrincipal",
                  target, mLoadingPrincipal.forget());
  NS_ProxyRelease("SharedWorkerManager::mRemoteWorkerController",
                  mPBackgroundEventTarget, mRemoteWorkerController.forget());
}

bool
SharedWorkerManager::MaybeCreateRemoteWorker(const RemoteWorkerData& aData,
                                             uint64_t aWindowID,
                                             const MessagePortIdentifier& aPortIdentifier)
{
  AssertIsOnBackgroundThread();

  if (!mRemoteWorkerController) {
    mRemoteWorkerController = RemoteWorkerController::Create(aData);
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

bool
SharedWorkerManager::MatchOnMainThread(const nsACString& aDomain,
                                       const nsACString& aScriptURL,
                                       const nsAString& aName,
                                       nsIPrincipal* aLoadingPrincipal) const
{
  MOZ_ASSERT(NS_IsMainThread());
  return aDomain == mDomain &&
         aScriptURL == mResolvedScriptURL &&
         aName == mName &&
         // We want to be sure that the window's principal subsumes the
         // SharedWorker's loading principal and vice versa.
         mLoadingPrincipal->Subsumes(aLoadingPrincipal) &&
         aLoadingPrincipal->Subsumes(mLoadingPrincipal);
}

void
SharedWorkerManager::AddActor(SharedWorkerParent* aParent)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(!mActors.Contains(aParent));

  uint32_t frozen = 0;

  for (SharedWorkerParent* actor : mActors) {
    if (actor->IsFrozen()) {
      ++frozen;
    }
  }

  bool hadActors = !mActors.IsEmpty();

  mActors.AppendElement(aParent);

  if (hadActors && frozen == mActors.Length() - 1) {
    mRemoteWorkerController->Thaw();
  }
}

void
SharedWorkerManager::RemoveActor(SharedWorkerParent* aParent)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mActors.Contains(aParent));

  uint64_t windowID = aParent->WindowID();
  if (windowID) {
    mRemoteWorkerController->RemoveWindowID(windowID);
  }

  mActors.RemoveElement(aParent);

  if (!mActors.IsEmpty()) {
    return;
  }

  // Time to go.

  mRemoteWorkerController->Terminate();
  mRemoteWorkerController = nullptr;

  // SharedWorkerService exists because it is kept alive by SharedWorkerParent.
  SharedWorkerService::Get()->RemoveWorkerManager(this);
}

void
SharedWorkerManager::UpdateSuspend()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mRemoteWorkerController);

  uint32_t suspended = 0;

  for (SharedWorkerParent* actor : mActors) {
    if (actor->IsSuspended()) {
      ++suspended;
    }
  }

  if (suspended != 0 && suspended != mActors.Length()) {
    return;
  }

  if (suspended) {
    mRemoteWorkerController->Suspend();
  } else {
    mRemoteWorkerController->Resume();
  }
}

void
SharedWorkerManager::UpdateFrozen()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mRemoteWorkerController);

  uint32_t frozen = 0;

  for (SharedWorkerParent* actor : mActors) {
    if (actor->IsFrozen()) {
      ++frozen;
    }
  }

  if (frozen != 0 && frozen != mActors.Length()) {
    return;
  }

  if (frozen) {
    mRemoteWorkerController->Freeze();
  } else {
    mRemoteWorkerController->Thaw();
  }
}

bool
SharedWorkerManager::IsSecureContext() const
{
  return mIsSecureContext;
}

/* TODO
void
SharedWorkerManager::BroadcastErrorToActorsOnMainThread(const WorkerErrorReport* aReport,
                                                        bool aIsErrorEvent)
{
  MOZ_ASSERT(NS_IsMainThread());

  ErrorValue value;
  if (aIsErrorEvent) {
    nsTArray<ErrorDataNote> notes;
    for (size_t i = 0, len = aReport->mNotes.Length(); i < len; i++) {
      const WorkerErrorNote& note = aReport->mNotes.ElementAt(i);
      notes.AppendElement(ErrorDataNote(note.mLineNumber, note.mColumnNumber,
                                        note.mMessage, note.mFilename));
    }

    ErrorData data(aReport->mLineNumber,
                   aReport->mColumnNumber,
                   aReport->mFlags,
                   aReport->mMessage,
                   aReport->mFilename,
                   aReport->mLine,
                   notes);
    value = data;
  } else {
    value = void_t();
  }

  RefPtr<SharedWorkerManager> self = this;
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction("SharedWorkerManager::BroadcastErrorToActorsOnMainThread",
                           [self, value]() {
    self->BroadcastErrorToActors(value);
  });

  mPBackgroundEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void
SharedWorkerManager::BroadcastErrorToActors(const ErrorValue& aValue)
{
  AssertIsOnBackgroundThread();

  for (SharedWorkerParent* actor : mActors) {
    Unused << actor->SendError(aValue);
  }
}

void
SharedWorkerManager::CloseActorsOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  CloseOnMainThread();

  RefPtr<SharedWorkerManager> self = this;
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction("SharedWorkerManager::CloseActorsOnMainThread",
                           [self]() {
    self->CloseActors();
  });

  mPBackgroundEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void
SharedWorkerManager::CloseActors()
{
  AssertIsOnBackgroundThread();

  for (SharedWorkerParent* actor : mActors) {
    Unused << actor->SendTerminate();
  }
}

void
SharedWorkerManager::FlushReportsToActorsOnMainThread(nsIConsoleReportCollector* aReporter)
{
  MOZ_ASSERT(NS_IsMainThread());

  AutoTArray<uint64_t, 10> windowIDs;
  for (SharedWorkerParent* actor : mActors) {
    uint64_t windowID = actor->WindowID();
    if (windowID && !windowIDs.Contains(windowID)) {
      windowIDs.AppendElement(windowID);
    }
  }

  bool reportErrorToBrowserConsole = true;

  // Flush the reports.
  for (uint32_t index = 0; index < windowIDs.Length(); index++) {
    aReporter->FlushReportsToConsole(windowIDs[index],
      nsIConsoleReportCollector::ReportAction::Save);
    reportErrorToBrowserConsole = false;
  }

  // Finally report to browser console if there is no any window or shared
  // worker.
  if (reportErrorToBrowserConsole) {
    aReporter->FlushReportsToConsole(0);
    return;
  }

  aReporter->ClearConsoleReports();
}
*/

} // dom namespace
} // mozilla namespace
