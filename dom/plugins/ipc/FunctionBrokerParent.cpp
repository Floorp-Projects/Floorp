/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FunctionBrokerParent.h"
#include "FunctionBroker.h"
#include "FunctionBrokerThread.h"

namespace mozilla {
namespace plugins {

/* static */ FunctionBrokerParent*
FunctionBrokerParent::Create(Endpoint<PFunctionBrokerParent>&& aParentEnd)
{
  FunctionBrokerThread* thread = FunctionBrokerThread::Create();
  if (!thread) {
    return nullptr;
  }
  return new FunctionBrokerParent(thread, Move(aParentEnd));
}

FunctionBrokerParent::FunctionBrokerParent(FunctionBrokerThread* aThread,
                                           Endpoint<PFunctionBrokerParent>&& aParentEnd) :
    mThread(aThread)
  , mMonitor("FunctionBrokerParent Lock")
  , mShutdownDone(false)
{
  MOZ_ASSERT(mThread);
  mThread->Dispatch(NewNonOwningRunnableMethod<Endpoint<PFunctionBrokerParent>&&>(
          "FunctionBrokerParent::Bind", this, &FunctionBrokerParent::Bind, Move(aParentEnd)));
}

void
FunctionBrokerParent::Bind(Endpoint<PFunctionBrokerParent>&& aEnd)
{
  MOZ_RELEASE_ASSERT(mThread->IsOnThread());
  DebugOnly<bool> ok = aEnd.Bind(this);
  MOZ_ASSERT(ok);
}

void
FunctionBrokerParent::ShutdownOnBrokerThread()
{
  MOZ_ASSERT(mThread->IsOnThread());
  Close();

  // Notify waiting thread that we are done.
  MonitorAutoLock lock(mMonitor);
  mShutdownDone = true;
  mMonitor.Notify();
}

void
FunctionBrokerParent::Destroy(FunctionBrokerParent* aInst)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aInst);

  {
    // Hold the lock while we destroy the actor on the broker thread.
    MonitorAutoLock lock(aInst->mMonitor);
    aInst->mThread->Dispatch(NewNonOwningRunnableMethod(
      "FunctionBrokerParent::ShutdownOnBrokerThread", aInst,
      &FunctionBrokerParent::ShutdownOnBrokerThread));

    // Wait for broker thread to complete destruction.
    while (!aInst->mShutdownDone) {
      aInst->mMonitor.Wait();
    }
  }

  delete aInst;
}

void
FunctionBrokerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_RELEASE_ASSERT(mThread->IsOnThread());
}

} // namespace plugins
} // namespace mozilla
