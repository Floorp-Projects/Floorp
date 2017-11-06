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

#if defined(XP_WIN)
UlongPairToIdMap sPairToIdMap;
IdToUlongPairMap sIdToPairMap;
PtrToIdMap sPtrToIdMap;
IdToPtrMap sIdToPtrMap;
#endif // defined(XP_WIN)

/* static */ FunctionBrokerParent*
FunctionBrokerParent::Create(Endpoint<PFunctionBrokerParent>&& aParentEnd)
{
  FunctionBrokerThread* thread = FunctionBrokerThread::Create();
  if (!thread) {
    return nullptr;
  }

  // We get the FunctionHooks so that they are created here, not on the
  // message thread.
  FunctionHook::GetHooks();

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

mozilla::ipc::IPCResult
FunctionBrokerParent::RecvBrokerFunction(const FunctionHookId &aFunctionId,
                                         const IpdlTuple &aInTuple,
                                         IpdlTuple *aOutTuple)
{
#if defined(XP_WIN)
  MOZ_ASSERT(mThread->IsOnThread());
  if (RunBrokeredFunction(OtherPid(), aFunctionId, aInTuple, aOutTuple)) {
    return IPC_OK();
  }
  return IPC_FAIL_NO_REASON(this);
#else
  MOZ_ASSERT_UNREACHABLE("BrokerFunction is currently only implemented on Windows.");
  return IPC_FAIL_NO_REASON(this);
#endif
}

// static
bool
FunctionBrokerParent::RunBrokeredFunction(base::ProcessId aClientId,
                                        const FunctionHookId &aFunctionId,
                                        const IPC::IpdlTuple &aInTuple,
                                        IPC::IpdlTuple *aOutTuple)
{
  if ((size_t)aFunctionId >= FunctionHook::GetHooks()->Length()) {
    MOZ_ASSERT_UNREACHABLE("Invalid function ID");
    return false;
  }

  FunctionHook* hook = FunctionHook::GetHooks()->ElementAt(aFunctionId);
  MOZ_ASSERT(hook->FunctionId() == aFunctionId);
  return hook->RunOriginalFunction(aClientId, aInTuple, aOutTuple);
}

} // namespace plugins
} // namespace mozilla
