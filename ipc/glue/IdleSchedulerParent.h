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
  IPCResult RecvRequestGC(RequestGCResolver&& aResolve);
  IPCResult RecvStartedGC();
  IPCResult RecvDoneGC();

 private:
  friend class BackgroundParentImpl;
  IdleSchedulerParent();
  ~IdleSchedulerParent();

  static void CalculateNumIdleTasks();

  static int32_t ActiveCount();
  static void Schedule(IdleSchedulerParent* aRequester);
  static bool HasSpareCycles(int32_t aActiveCount);
  static bool HasSpareGCCycles();
  using PIdleSchedulerParent::SendIdleTime;
  void SendIdleTime();
  void SendMayGC();

  static void EnsureStarvationTimer();
  static void StarvationCallback(nsITimer* aTimer, void* aData);

  uint64_t mCurrentRequestId = 0;
  // For now we don't really use idle budget for scheduling.  Zero if the
  // process isn't requestiong or running an idle task.
  TimeDuration mRequestedIdleBudget;

  // Counting all the prioritized operations the process is doing.
  uint32_t mRunningPrioritizedOperation = 0;

  // Only one of these may be true at a time, giving three states:
  // No active GC request, A pending GC request, or a granted GC request.
  Maybe<RequestGCResolver> mRequestingGC;
  bool mDoingGC = false;

  uint32_t mChildId = 0;

  // Current state, only one of these may be true at a time.
  bool IsWaitingForIdle() const { return isInList() && mRequestedIdleBudget; }
  bool IsDoingIdleTask() const { return !isInList() && mRequestedIdleBudget; }
  bool IsNotDoingIdleTask() const { return !mRequestedIdleBudget; }

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

  // Processes on this list have requested (but the request hasn't yet been
  // granted) idle time or to start a GC or both.
  //
  // Either or both their mRequestedIdleBudget or mRequestingGC fields are
  // non-zero.  Child processes not on this list have either been granted all
  // their requests not made a request ever or since they last finished an idle
  // or GC task.
  //
  // Use the methods above to determine a process' idle time state, or check the
  // mRequestingGC and mDoingGC fields for the GC state.
  static LinkedList<IdleSchedulerParent> sIdleAndGCRequests;

  static int32_t sMaxConcurrentIdleTasksInChildProcesses;
  static uint32_t sMaxConcurrentGCs;
  static uint32_t sActiveGCs;

  // True if we should record some telemetry for GCs in the next Schedule().
  // This is set to true by either requesting a GC job or scheduling a GC job.
  static bool sRecordGCTelemetry;
  // The current number of waiting GCs.
  static uint32_t sNumWaitingGC;

  // Counting all the child processes which have at least one prioritized
  // operation.
  static uint32_t sChildProcessesRunningPrioritizedOperation;

  // When this hits zero, it's time to free the shared memory and pack up.
  static uint32_t sChildProcessesAlive;

  static nsITimer* sStarvationPreventer;

  static uint32_t sNumCPUs;
  static uint32_t sPrefConcurrentGCsMax;
  static uint32_t sPrefConcurrentGCsCPUDivisor;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_IdleSchedulerParent_h__
