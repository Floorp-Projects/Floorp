/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorkerManager.h"
#include "SharedWorkerParent.h"
#include "SharedWorkerService.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/dom/PSharedWorker.h"
#include "mozilla/dom/WorkerError.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/workerinternals/ScriptLoader.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsIConsoleReportCollector.h"
#include "nsIPrincipal.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {

using workerinternals::ChannelFromScriptURLMainThread;

namespace {

class MessagePortRunnable final : public WorkerRunnable
{
  MessagePortIdentifier mPortIdentifier;

public:
  MessagePortRunnable(WorkerPrivate* aWorkerPrivate,
                      const MessagePortIdentifier& aPortIdentifier)
    : WorkerRunnable(aWorkerPrivate)
    , mPortIdentifier(aPortIdentifier)
  {}

private:
  ~MessagePortRunnable() = default;

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->ConnectMessagePort(aCx, mPortIdentifier);
  }

  nsresult
  Cancel() override
  {
    MessagePort::ForceClose(mPortIdentifier);
    return WorkerRunnable::Cancel();
  }
};

} // anonymous

SharedWorkerManager::SharedWorkerManager(nsIEventTarget* aPBackgroundEventTarget,
                                         const SharedWorkerLoadInfo& aInfo,
                                         nsIPrincipal* aPrincipal,
                                         nsIPrincipal* aLoadingPrincipal)
  : mPBackgroundEventTarget(aPBackgroundEventTarget)
  , mInfo(aInfo)
  , mPrincipal(aPrincipal)
  , mLoadingPrincipal(aLoadingPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aLoadingPrincipal);
}

SharedWorkerManager::~SharedWorkerManager()
{
  nsCOMPtr<nsIEventTarget> target =
    SystemGroup::EventTargetFor(TaskCategory::Other);

  NS_ProxyRelease("SharedWorkerManager::mPrincipal",
                  target, mPrincipal.forget());
  NS_ProxyRelease("SharedWorkerManager::mLoadingPrincipal",
                  target, mLoadingPrincipal.forget());
  NS_ProxyRelease("SharedWorkerManager::mWorkerPrivate",
                  target, mWorkerPrivate.forget());
}

nsresult
SharedWorkerManager::CreateWorkerOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure that the IndexedDatabaseManager is initialized
  Unused << NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate());

  WorkerLoadInfo info;
  nsresult rv = NS_NewURI(getter_AddRefs(info.mBaseURI),
                          mInfo.baseScriptURL(),
                          nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = NS_NewURI(getter_AddRefs(info.mResolvedScriptURI),
                 mInfo.resolvedScriptURL(), nullptr, info.mBaseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mPrincipalInfo = new PrincipalInfo();
  rv = PrincipalToPrincipalInfo(mPrincipal, info.mPrincipalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mResolvedScriptURI = info.mBaseURI;

  info.mDomain = mInfo.domain();
  info.mPrincipal = mPrincipal;
  info.mLoadingPrincipal = mLoadingPrincipal;

  nsContentUtils::StorageAccess access =
    nsContentUtils::StorageAllowedForPrincipal(info.mPrincipal);
  info.mStorageAllowed =
    access > nsContentUtils::StorageAccess::ePrivateBrowsing;
  info.mOriginAttributes =
    BasePrincipal::Cast(mPrincipal)->OriginAttributesRef();

  // Default CSP permissions for now.  These will be overrided if necessary
  // based on the script CSP headers during load in ScriptLoader.
  info.mEvalAllowed = true;
  info.mReportCSPViolations = false;
  info.mSecureContext = mInfo.isSecureContext()
    ? WorkerLoadInfo::eSecureContext : WorkerLoadInfo::eInsecureContext;

  WorkerPrivate::OverrideLoadInfoLoadGroup(info, info.mLoadingPrincipal);

  rv = info.SetPrincipalOnMainThread(info.mPrincipal, info.mLoadGroup);
  Maybe<ClientInfo> clientInfo;
  if (mInfo.clientInfo().type() == OptionalIPCClientInfo::TIPCClientInfo) {
    clientInfo.emplace(ClientInfo(mInfo.clientInfo().get_IPCClientInfo()));
  }

  // Top level workers' main script use the document charset for the script
  // uri encoding.
  rv = ChannelFromScriptURLMainThread(info.mLoadingPrincipal,
                                      info.mBaseURI,
                                      nullptr /* parent document */,
                                      info.mLoadGroup,
                                      mInfo.originalScriptURL(),
                                      clientInfo,
                                      nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER,
                                      false /* default encoding */,
                                      getter_AddRefs(info.mChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoJSAPI jsapi;
  jsapi.Init();

  ErrorResult error;
  mWorkerPrivate = WorkerPrivate::Constructor(jsapi.cx(),
                                              mInfo.originalScriptURL(),
                                              false,
                                              WorkerTypeShared,
                                              mInfo.name(),
                                              VoidCString(),
                                              &info, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  mWorkerPrivate->SetSharedWorkerManager(this);
  return NS_OK;
}

nsresult
SharedWorkerManager::ConnectPortOnMainThread(const MessagePortIdentifier& aPortIdentifier)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<MessagePortRunnable> runnable =
    new MessagePortRunnable(mWorkerPrivate, aPortIdentifier);
  if (NS_WARN_IF(!runnable->Dispatch())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

bool
SharedWorkerManager::MatchOnMainThread(const nsACString& aDomain,
                                       const nsACString& aScriptURL,
                                       const nsAString& aName,
                                       nsIPrincipal* aLoadingPrincipal) const
{
  MOZ_ASSERT(NS_IsMainThread());
  return aDomain == mInfo.domain() &&
         aScriptURL == mInfo.resolvedScriptURL() &&
         aName == mInfo.name() &&
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

  mActors.AppendElement(aParent);
}

void
SharedWorkerManager::RemoveActor(SharedWorkerParent* aParent)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mActors.Contains(aParent));

  mActors.RemoveElement(aParent);

  if (!mActors.IsEmpty()) {
    return;
  }

  // Time to go.

  RefPtr<SharedWorkerManager> self = this;
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction("SharedWorkerManager::RemoveActor",
                           [self]() {
    self->CloseOnMainThread();
  });

  nsCOMPtr<nsIEventTarget> target =
    SystemGroup::EventTargetFor(TaskCategory::Other);
  target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);

  // SharedWorkerService exists because it is kept alive by SharedWorkerParent.
  SharedWorkerService::Get()->RemoveWorkerManager(this);
}

void
SharedWorkerManager::UpdateSuspend()
{
  AssertIsOnBackgroundThread();

  uint32_t suspended = 0;

  for (SharedWorkerParent* actor : mActors) {
    if (actor->IsSuspended()) {
      ++suspended;
    }
  }

  if (suspended != 0 && suspended != mActors.Length()) {
    return;
  }

  RefPtr<SharedWorkerManager> self = this;
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction("SharedWorkerManager::UpdateSuspend",
                           [self, suspended]() {
    if (suspended) {
      self->SuspendOnMainThread();
    } else {
      self->ResumeOnMainThread();
    }
  });

  nsCOMPtr<nsIEventTarget> target =
    SystemGroup::EventTargetFor(TaskCategory::Other);
  target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void
SharedWorkerManager::UpdateFrozen()
{
  AssertIsOnBackgroundThread();

  uint32_t frozen = 0;

  for (SharedWorkerParent* actor : mActors) {
    if (actor->IsFrozen()) {
      ++frozen;
    }
  }

  if (frozen != 0 && frozen != mActors.Length()) {
    return;
  }

  RefPtr<SharedWorkerManager> self = this;
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction("SharedWorkerManager::UpdateFrozen",
                           [self, frozen]() {
    if (frozen) {
      self->FreezeOnMainThread();
    } else {
      self->ThawOnMainThread();
    }
  });

  nsCOMPtr<nsIEventTarget> target =
    SystemGroup::EventTargetFor(TaskCategory::Other);
  target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

bool
SharedWorkerManager::IsSecureContext() const
{
  return mInfo.isSecureContext();
}

void
SharedWorkerManager::FreezeOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWorkerPrivate || mWorkerPrivate->IsFrozen()) {
    // Already released.
    return;
  }

  mWorkerPrivate->Freeze(nullptr);
}

void
SharedWorkerManager::ThawOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWorkerPrivate || !mWorkerPrivate->IsFrozen()) {
    // Already released.
    return;
  }

  mWorkerPrivate->Thaw(nullptr);
}

void
SharedWorkerManager::SuspendOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWorkerPrivate) {
    // Already released.
    return;
  }

  mWorkerPrivate->ParentWindowPaused();
}

void
SharedWorkerManager::ResumeOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWorkerPrivate) {
    // Already released.
    return;
  }

  mWorkerPrivate->ParentWindowResumed();
}

void
SharedWorkerManager::CloseOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWorkerPrivate) {
    // Already released.
    return;
  }

  mWorkerPrivate->Cancel();
  mWorkerPrivate = nullptr;
}

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

} // dom namespace
} // mozilla namespace
