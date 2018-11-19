/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/BackgroundParent.h"
#include "RemoteWorkerController.h"
#include "RemoteWorkerManager.h"

namespace mozilla {

using namespace ipc;

namespace dom {

/* static */ already_AddRefed<RemoteWorkerController>
RemoteWorkerController::Create()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  RefPtr<RemoteWorkerController> controller = new RemoteWorkerController();

  // This will be populated, eventually.
  RemoteWorkerData data;

  RefPtr<RemoteWorkerManager> manager = RemoteWorkerManager::GetOrCreate();
  MOZ_ASSERT(manager);

  manager->Launch(controller, data);

  return controller.forget();
}

RemoteWorkerController::RemoteWorkerController()
  : mState(ePending)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
}

RemoteWorkerController::~RemoteWorkerController()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
}

void
RemoteWorkerController::SetWorkerActor(RemoteWorkerParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!mActor);
  MOZ_ASSERT(aActor);

  mActor = aActor;
}

void
RemoteWorkerController::CreationFailed()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mActor);

  mState = eTerminated;
  mActor = nullptr;
}

void
RemoteWorkerController::CreationSucceeded()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mActor);

  mState = eReady;

  // TODO: flush the pending op queue.
}

} // dom namespace
} // mozilla namespace
