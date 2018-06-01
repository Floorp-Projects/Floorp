/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FunctionBrokerChild.h"
#include "FunctionBrokerThread.h"

namespace mozilla {
namespace plugins {

FunctionBrokerChild* FunctionBrokerChild::sInstance = nullptr;

bool
FunctionBrokerChild::IsDispatchThread()
{
  return mThread->IsOnThread();
}

void
FunctionBrokerChild::PostToDispatchThread(already_AddRefed<nsIRunnable>&& runnable)
{
  mThread->Dispatch(std::move(runnable));
}

/* static */ bool
FunctionBrokerChild::Initialize(Endpoint<PFunctionBrokerChild>&& aBrokerEndpoint)
{
  MOZ_RELEASE_ASSERT(XRE_IsPluginProcess(),
                     "FunctionBrokerChild can only be used in plugin processes");

  MOZ_ASSERT(!sInstance);
  FunctionBrokerThread* thread = FunctionBrokerThread::Create();
  if (!thread) {
    return false;
  }
  sInstance = new FunctionBrokerChild(thread, std::move(aBrokerEndpoint));
  return true;
}

/* static */ FunctionBrokerChild*
FunctionBrokerChild::GetInstance()
{
  MOZ_RELEASE_ASSERT(XRE_IsPluginProcess(),
                     "FunctionBrokerChild can only be used in plugin processes");

  MOZ_ASSERT(sInstance, "Must initialize FunctionBrokerChild before using it");
  return sInstance;
}

FunctionBrokerChild::FunctionBrokerChild(FunctionBrokerThread* aThread,
                                         Endpoint<PFunctionBrokerChild>&& aEndpoint) :
    mThread(aThread)
  , mShutdownDone(false)
  , mMonitor("FunctionBrokerChild Lock")
{
  MOZ_ASSERT(aThread);
  PostToDispatchThread(NewNonOwningRunnableMethod<Endpoint<PFunctionBrokerChild>&&>(
                       "FunctionBrokerChild::Bind", this, &FunctionBrokerChild::Bind,
                       std::move(aEndpoint)));
}

void
FunctionBrokerChild::Bind(Endpoint<PFunctionBrokerChild>&& aEndpoint)
{
  MOZ_RELEASE_ASSERT(mThread->IsOnThread());
  DebugOnly<bool> ok = aEndpoint.Bind(this);
  MOZ_ASSERT(ok);
}

void
FunctionBrokerChild::ShutdownOnDispatchThread()
{
  MOZ_ASSERT(mThread->IsOnThread());

  // Set mShutdownDone and notify waiting thread (if any) that we are done.
  MonitorAutoLock lock(mMonitor);
  mShutdownDone = true;
  mMonitor.Notify();
}

void
FunctionBrokerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_ASSERT(mThread->IsOnThread());

  // Queue up a task on the PD thread.  When that task is executed then
  // we know that anything queued before ActorDestroy has completed.
  // At that point, we can set mShutdownDone and alert any waiting
  // threads that it is safe to destroy us.
  sInstance->PostToDispatchThread(NewNonOwningRunnableMethod(
                                  "FunctionBrokerChild::ShutdownOnDispatchThread", sInstance,
                                  &FunctionBrokerChild::ShutdownOnDispatchThread));
}

void
FunctionBrokerChild::Destroy()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sInstance) {
    return;
  }

  // mShutdownDone will tell us when ActorDestroy has been run and any tasks
  // on the FunctionBrokerThread have completed.  At that point, we can
  // safely delete the actor.
  {
    MonitorAutoLock lock(sInstance->mMonitor);
    while (!sInstance->mShutdownDone) {
      // Release lock and wait.  Regain lock when we are notified that
      // we have ShutdownOnDispatchThread.
      sInstance->mMonitor.Wait();
    }
  }

  delete sInstance;
  sInstance = nullptr;
}

} // namespace plugins
} // namespace mozilla
