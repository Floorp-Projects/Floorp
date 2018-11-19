/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerParent.h"
#include "RemoteWorkerController.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Unused.h"

namespace mozilla {

using namespace ipc;

namespace dom {

RemoteWorkerParent::RemoteWorkerParent()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
}

RemoteWorkerParent::~RemoteWorkerParent()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
}

void
RemoteWorkerParent::ActorDestroy(IProtocol::ActorDestroyReason)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  mController = nullptr;
}

IPCResult
RemoteWorkerParent::RecvCreated(const bool& aStatus)
{
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

IPCResult
RemoteWorkerParent::RecvError(const ErrorValue& aValue)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mController) {
    mController->ErrorPropagation(aValue);
  }

  return IPC_OK();
}

IPCResult
RemoteWorkerParent::RecvClose()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mController) {
    mController->WorkerTerminated();
  }

  Unused << Send__delete__(this);
  return IPC_OK();
}

void
RemoteWorkerParent::SetController(RemoteWorkerController* aController)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  mController = aController;
}

} // dom namespace
} // mozilla namespace
