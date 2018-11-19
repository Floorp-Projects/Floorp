/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorkerChild.h"

namespace mozilla {

using namespace ipc;

namespace dom {

SharedWorkerChild::SharedWorkerChild()
  : mParent(nullptr)
  , mActive(true)
{
}

SharedWorkerChild::~SharedWorkerChild() = default;

void
SharedWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mActive = false;
}

void
SharedWorkerChild::SendClose()
{
  if (mActive) {
    // This is the last message.
    mActive = false;
    PSharedWorkerChild::SendClose();
  }
}

void
SharedWorkerChild::SendSuspend()
{
  if (mActive) {
    PSharedWorkerChild::SendSuspend();
  }
}

void
SharedWorkerChild::SendResume()
{
  if (mActive) {
    PSharedWorkerChild::SendResume();
  }
}

void
SharedWorkerChild::SendFreeze()
{
  if (mActive) {
    PSharedWorkerChild::SendFreeze();
  }
}

void
SharedWorkerChild::SendThaw()
{
  if (mActive) {
    PSharedWorkerChild::SendThaw();
  }
}

IPCResult
SharedWorkerChild::RecvError(const nsresult& aError)
{
  MOZ_ASSERT(mActive);

  if (mParent) {
    mParent->ErrorPropagation(aError);
  }

  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
