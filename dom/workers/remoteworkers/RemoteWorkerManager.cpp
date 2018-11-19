/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerManager.h"

#include "mozilla/dom/RemoteWorkerParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "RemoteWorkerServiceParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

// Raw pointer because this object is kept alive by RemoteWorkerServiceParent
// actors.
RemoteWorkerManager* sRemoteWorkerManager;

} // anonymous

/* static */ already_AddRefed<RemoteWorkerManager>
RemoteWorkerManager::GetOrCreate()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!sRemoteWorkerManager) {
    sRemoteWorkerManager = new RemoteWorkerManager();
  }

  RefPtr<RemoteWorkerManager> rwm = sRemoteWorkerManager;
  return rwm.forget();
}

RemoteWorkerManager::RemoteWorkerManager()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!sRemoteWorkerManager);
}

RemoteWorkerManager::~RemoteWorkerManager()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(sRemoteWorkerManager == this);
  sRemoteWorkerManager = nullptr;
}

void
RemoteWorkerManager::RegisterActor(RemoteWorkerServiceParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActors.Contains(aActor));

  mActors.AppendElement(aActor);

  if (!mPendings.IsEmpty()) {
    // Flush pending launching.
    for (const Pending& p : mPendings) {
      LaunchInternal(p.mController, aActor, p.mData);
    }

    mPendings.Clear();

    // We don't need to keep this manager alive manually. The Actor is doing it
    // for us.
    Release();
  }
}

void
RemoteWorkerManager::UnregisterActor(RemoteWorkerServiceParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(mActors.Contains(aActor));

  mActors.RemoveElement(aActor);
}

void
RemoteWorkerManager::Launch(RemoteWorkerController* aController,
                            const RemoteWorkerData& aData)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  RemoteWorkerServiceParent* targetActor = SelectTargetActor();

  // If there is not an available actor, let's store the data, and let's spawn a
  // new process.
  if (!targetActor) {
    // If this is the first time we have a pending launching, we must keep alive
    // the manager.
    if (mPendings.IsEmpty()) {
      AddRef();
    }

    Pending* pending = mPendings.AppendElement();
    pending->mController = aController;
    pending->mData = aData;

    LaunchNewContentProcess();
    return;
  }

  LaunchInternal(aController, targetActor, aData);
}

void
RemoteWorkerManager::LaunchInternal(RemoteWorkerController* aController,
                                    RemoteWorkerServiceParent* aTargetActor,
                                    const RemoteWorkerData& aData)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aController);
  MOZ_ASSERT(aTargetActor);
  MOZ_ASSERT(mActors.Contains(aTargetActor));

  RemoteWorkerParent* workerActor =
    static_cast<RemoteWorkerParent*>(
      aTargetActor->Manager()->SendPRemoteWorkerConstructor(aData));
  if (NS_WARN_IF(!workerActor)) {
    AsyncCreationFailed(aController);
    return;
  }

  // This makes the link better the 2 actors.
  aController->SetWorkerActor(workerActor);
  workerActor->SetController(aController);
}

void
RemoteWorkerManager::AsyncCreationFailed(RemoteWorkerController* aController)
{
  RefPtr<RemoteWorkerController> controller = aController;
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction("RemoteWorkerManager::AsyncCreationFailed",
                           [controller]() {
      controller->CreationFailed();
  });

  NS_DispatchToCurrentThread(r.forget());
}

RemoteWorkerServiceParent*
RemoteWorkerManager::SelectTargetActor()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  // TODO

  return nullptr;
}

void
RemoteWorkerManager::LaunchNewContentProcess()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  // TODO: exec a new process
}

} // dom namespace
} // mozilla namespace
