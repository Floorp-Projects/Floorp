/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IdleSchedulerParent_h__
#define mozilla_ipc_IdleSchedulerParent_h__

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/ipc/PIdleSchedulerParent.h"
#include "base/shared_memory.h"
#include <bitset>

#define NS_IDLE_SCHEDULER_COUNTER_ARRAY_LENGHT 1024
#define NS_IDLE_SCHEDULER_INDEX_OF_ACTIVITY_COUNTER 0
#define NS_IDLE_SCHEDULER_INDEX_OF_CPU_COUNTER 1

class nsITimer;

namespace mozilla {

namespace ipc {

class BackgroundParentImpl;

class IdleSchedulerParent final
    : public PIdleSchedulerParent,
      public LinkedListElement<IdleSchedulerParent> {
 public:
  NS_INLINE_DECL_REFCOUNTING(IdleSchedulerParent)

  IPCResult RecvInitForIdleUse(InitForIdleUseResolver&& aResolve);
  IPCResult RecvRequestIdleTime(uint64_t aId, TimeDuration aBudget);
  IPCResult RecvIdleTimeUsed(uint64_t aId);
  IPCResult RecvSchedule();
  IPCResult RecvRunningPrioritizedOperation();
  IPCResult RecvPrioritizedOperationDone();

 private:
  friend class BackgroundParentImpl;
  IdleSchedulerParent();
  ~IdleSchedulerParent();

  static int32_t ActiveCount();
  static void Schedule(IdleSchedulerParent* aRequester);

  static void EnsureStarvationTimer();
  static void StarvationCallback(nsITimer* aTimer, void* aData);

  uint64_t mCurrentRequestId = 0;
  // For now we don't really use idle budget for scheduling.
  TimeDuration mRequestedIdleBudget;

  // Counting all the prioritized operations the process is doing.
  uint32_t mRunningPrioritizedOperation = 0;

  uint32_t mChildId = 0;

  // Shared memory for counting how many child processes are running
  // tasks. This memory is shared across all the child processes.
  // The [0] is used for counting all the processes and
  // [childId] is for counting per process activity.
  // This way the global activity can be checked in a fast way by just looking
  // at [0] value.
  // [1] is used for cpu count for child processes.
  static base::SharedMemory* sActiveChildCounter;
  // A bit is set if there is a child with child Id as the offset.
  // The bit is used to check per child specific activity counters in
  // sActiveChildCounter.
  static std::bitset<NS_IDLE_SCHEDULER_COUNTER_ARRAY_LENGHT>
      sInUseChildCounters;

  // Child is either running non-idle tasks or doesn't have any tasks to run.
  static LinkedList<IdleSchedulerParent> sDefault;

  // Child has requested idle time, but hasn't got it yet.
  static LinkedList<IdleSchedulerParent> sWaitingForIdle;

  // Child has gotten idle time and is running idle or normal tasks.
  static LinkedList<IdleSchedulerParent> sIdle;

  static AutoTArray<IdleSchedulerParent*, 8>* sPrioritized;
  static Atomic<int32_t> sCPUsForChildProcesses;

  // Counting all the child processes which have at least one prioritized
  // operation.
  static uint32_t sChildProcessesRunningPrioritizedOperation;

  static nsITimer* sStarvationPreventer;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_IdleSchedulerParent_h__
