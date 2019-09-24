/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_page_load.h"
#include "mozilla/Unused.h"
#include "mozilla/ipc/IdleSchedulerParent.h"
#include "nsIPropertyBag2.h"
#include "nsSystemInfo.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"

namespace mozilla {
namespace ipc {

base::SharedMemory* IdleSchedulerParent::sActiveChildCounter = nullptr;
std::bitset<NS_IDLE_SCHEDULER_COUNTER_ARRAY_LENGHT>
    IdleSchedulerParent::sInUseChildCounters;
LinkedList<IdleSchedulerParent> IdleSchedulerParent::sDefault;
LinkedList<IdleSchedulerParent> IdleSchedulerParent::sWaitingForIdle;
LinkedList<IdleSchedulerParent> IdleSchedulerParent::sIdle;
AutoTArray<IdleSchedulerParent*, 8>* IdleSchedulerParent::sPrioritized =
    nullptr;
Atomic<int32_t> IdleSchedulerParent::sCPUsForChildProcesses(-1);
uint32_t IdleSchedulerParent::sChildProcessesRunningPrioritizedOperation = 0;
nsITimer* IdleSchedulerParent::sStarvationPreventer = nullptr;

IdleSchedulerParent::IdleSchedulerParent() {
  sDefault.insertBack(this);

  if (sCPUsForChildProcesses == -1) {
    // nsISystemInfo can be initialized only on the main thread.
    // While waiting for the real logical core count behave as if there was just
    // one core.
    sCPUsForChildProcesses = 1;
    nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
    nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction("cpucount getter", [thread]() {
          // Always pretend that there is at least one core for child processes.
          // If there are multiple logical cores, reserve one for the parent
          // process and for the non-main threads.
          nsCOMPtr<nsIPropertyBag2> infoService =
              do_GetService(NS_SYSTEMINFO_CONTRACTID);
          if (infoService) {
            int32_t cpus;
            nsresult rv = infoService->GetPropertyAsInt32(
                NS_LITERAL_STRING("cpucount"), &cpus);
            if (NS_SUCCEEDED(rv) && cpus > 1) {
              sCPUsForChildProcesses = cpus - 1;
            }

            // We have a new cpu count, reschedule idle scheduler.
            nsCOMPtr<nsIRunnable> runnable =
                NS_NewRunnableFunction("IdleSchedulerParent::Schedule", []() {
                  if (sActiveChildCounter && sActiveChildCounter->memory()) {
                    static_cast<Atomic<int32_t>*>(sActiveChildCounter->memory())
                        [NS_IDLE_SCHEDULER_INDEX_OF_CPU_COUNTER] =
                            static_cast<int32_t>(sCPUsForChildProcesses);
                  }
                  IdleSchedulerParent::Schedule(nullptr);
                });
            thread->Dispatch(runnable, NS_DISPATCH_NORMAL);
          }
        });
    NS_DispatchToMainThread(runnable);
  }
}

IdleSchedulerParent::~IdleSchedulerParent() {
  // We can't know if an active process just crashed, so we just always expect
  // that is the case.
  if (mChildId) {
    sInUseChildCounters[mChildId] = false;
    if (sActiveChildCounter && sActiveChildCounter->memory() &&
        static_cast<Atomic<int32_t>*>(
            sActiveChildCounter->memory())[mChildId]) {
      --static_cast<Atomic<int32_t>*>(
          sActiveChildCounter
              ->memory())[NS_IDLE_SCHEDULER_INDEX_OF_ACTIVITY_COUNTER];
      static_cast<Atomic<int32_t>*>(sActiveChildCounter->memory())[mChildId] =
          0;
    }
  }

  if (mRunningPrioritizedOperation) {
    --sChildProcessesRunningPrioritizedOperation;
  }

  if (isInList()) {
    remove();
    if (sDefault.isEmpty() && sWaitingForIdle.isEmpty() && sIdle.isEmpty()) {
      delete sActiveChildCounter;
      sActiveChildCounter = nullptr;

      if (sStarvationPreventer) {
        sStarvationPreventer->Cancel();
        NS_RELEASE(sStarvationPreventer);
      }
    }
  }

  Schedule(nullptr);
}

IPCResult IdleSchedulerParent::RecvInitForIdleUse(
    InitForIdleUseResolver&& aResolve) {
  // Create a shared memory object which is shared across all the relevant
  // processes. Only first 4 bytes of the allocated are used currently to
  // count activity state of child processes
  if (!sActiveChildCounter) {
    sActiveChildCounter = new base::SharedMemory();
    size_t shmemSize = NS_IDLE_SCHEDULER_COUNTER_ARRAY_LENGHT * sizeof(int32_t);
    if (sActiveChildCounter->Create(shmemSize) &&
        sActiveChildCounter->Map(shmemSize)) {
      memset(sActiveChildCounter->memory(), 0, shmemSize);
      sInUseChildCounters[NS_IDLE_SCHEDULER_INDEX_OF_ACTIVITY_COUNTER] = true;
      sInUseChildCounters[NS_IDLE_SCHEDULER_INDEX_OF_CPU_COUNTER] = true;
      static_cast<Atomic<int32_t>*>(
          sActiveChildCounter
              ->memory())[NS_IDLE_SCHEDULER_INDEX_OF_CPU_COUNTER] =
          static_cast<int32_t>(sCPUsForChildProcesses);
    } else {
      delete sActiveChildCounter;
      sActiveChildCounter = nullptr;
    }
  }
  Maybe<SharedMemoryHandle> activeCounter;
  SharedMemoryHandle handle;
  if (sActiveChildCounter &&
      sActiveChildCounter->ShareToProcess(OtherPid(), &handle)) {
    activeCounter.emplace(handle);
  }

  uint32_t unusedId = 0;
  for (uint32_t i = 0; i < NS_IDLE_SCHEDULER_COUNTER_ARRAY_LENGHT; ++i) {
    if (!sInUseChildCounters[i]) {
      sInUseChildCounters[i] = true;
      unusedId = i;
      break;
    }
  }

  // If there wasn't an empty item, we'll fallback to 0.
  mChildId = unusedId;

  aResolve(Tuple<const mozilla::Maybe<SharedMemoryHandle>&, const uint32_t&>(
      activeCounter, mChildId));
  return IPC_OK();
}

IPCResult IdleSchedulerParent::RecvRequestIdleTime(uint64_t aId,
                                                   TimeDuration aBudget) {
  mCurrentRequestId = aId;
  mRequestedIdleBudget = aBudget;

  remove();
  sWaitingForIdle.insertBack(this);

  Schedule(this);
  return IPC_OK();
}

IPCResult IdleSchedulerParent::RecvIdleTimeUsed(uint64_t aId) {
  if (mCurrentRequestId == aId) {
    // Ensure the object is back in the default queue.
    remove();
    sDefault.insertBack(this);
  }
  Schedule(nullptr);
  return IPC_OK();
}

IPCResult IdleSchedulerParent::RecvSchedule() {
  Schedule(nullptr);
  return IPC_OK();
}

IPCResult IdleSchedulerParent::RecvRunningPrioritizedOperation() {
  ++mRunningPrioritizedOperation;
  if (mRunningPrioritizedOperation == 1) {
    ++sChildProcessesRunningPrioritizedOperation;
  }
  return IPC_OK();
}

IPCResult IdleSchedulerParent::RecvPrioritizedOperationDone() {
  MOZ_ASSERT(mRunningPrioritizedOperation);

  --mRunningPrioritizedOperation;
  if (mRunningPrioritizedOperation == 0) {
    --sChildProcessesRunningPrioritizedOperation;
    Schedule(nullptr);
  }
  return IPC_OK();
}

int32_t IdleSchedulerParent::ActiveCount() {
  if (sActiveChildCounter) {
    return (static_cast<Atomic<int32_t>*>(
        sActiveChildCounter
            ->memory())[NS_IDLE_SCHEDULER_INDEX_OF_ACTIVITY_COUNTER]);
  }
  return 0;
}

void IdleSchedulerParent::Schedule(IdleSchedulerParent* aRequester) {
  if (sWaitingForIdle.isEmpty()) {
    return;
  }

  if (!aRequester || !aRequester->mRunningPrioritizedOperation) {
    int32_t activeCount = ActiveCount();
    // Don't bail out so easily if we're running with very few cores.
    if (sCPUsForChildProcesses > 1 && sCPUsForChildProcesses <= activeCount) {
      // Too many processes are running, bail out.
      EnsureStarvationTimer();
      return;
    }

    if (sChildProcessesRunningPrioritizedOperation > 0 &&
        sCPUsForChildProcesses / 2 <= activeCount) {
      // We're running a prioritized operation and don't have too many spare
      // cores for idle tasks, bail out.
      EnsureStarvationTimer();
      return;
    }
  }

  // We can run run an idle task. If the requester is prioritized, just let it
  // run itself.
  RefPtr<IdleSchedulerParent> idleRequester;
  if (aRequester && aRequester->mRunningPrioritizedOperation) {
    aRequester->remove();
    idleRequester = aRequester;
  } else {
    idleRequester = sWaitingForIdle.popFirst();
  }

  sIdle.insertBack(idleRequester);
  Unused << idleRequester->SendIdleTime(idleRequester->mCurrentRequestId,
                                        idleRequester->mRequestedIdleBudget);
}

void IdleSchedulerParent::EnsureStarvationTimer() {
  // Even though idle runnables aren't really guaranteed to get run ever (which
  // is why most of them have the timer fallback), try to not let any child
  // process' idle handling to starve forever in case other processes are busy
  if (!sStarvationPreventer) {
    // Reuse StaticPrefs::page_load_deprioritization_period(), since that
    // is used on child side when deciding the minimum idle period.
    NS_NewTimerWithFuncCallback(
        &sStarvationPreventer, StarvationCallback, nullptr,
        StaticPrefs::page_load_deprioritization_period(),
        nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, "StarvationCallback");
  }
}

void IdleSchedulerParent::StarvationCallback(nsITimer* aTimer, void* aData) {
  if (!sWaitingForIdle.isEmpty()) {
    RefPtr<IdleSchedulerParent> first = sWaitingForIdle.getFirst();
    // Treat the first process waiting for idle time as running prioritized
    // operation so that it gets run.
    ++first->mRunningPrioritizedOperation;
    ++sChildProcessesRunningPrioritizedOperation;
    Schedule(first);
    --first->mRunningPrioritizedOperation;
    --sChildProcessesRunningPrioritizedOperation;
  }
  NS_RELEASE(sStarvationPreventer);
}

}  // namespace ipc
}  // namespace mozilla
