/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_ParallelMarking_h
#define gc_ParallelMarking_h

#include "mozilla/Atomics.h"
#include "mozilla/DoublyLinkedList.h"

#include "gc/GCMarker.h"
#include "js/HeapAPI.h"
#include "js/SliceBudget.h"
#include "threading/ConditionVariable.h"
#include "threading/ProtectedData.h"

namespace js {

class AutoLockGC;
class AutoLockHelperThreadState;
class AutoUnlockGC;

namespace gc {

class ParallelMarkTask;

// Per-runtime parallel marking state.
//
// This is created on the main thread and coordinates parallel marking using
// several helper threads.
class MOZ_STACK_CLASS ParallelMarker {
 public:
  explicit ParallelMarker(GCRuntime* gc);

  bool mark(SliceBudget& sliceBudget);

  bool hasWaitingTasks() { return waitingTaskCount != 0; }
  void stealWorkFrom(GCMarker* victim);

 private:
  bool markOneColor(MarkColor color, SliceBudget& sliceBudget);

  bool hasWork(MarkColor color) const;

  void addTask(ParallelMarkTask* task, const AutoLockGC& lock);

  void addTaskToWaitingList(ParallelMarkTask* task, const AutoLockGC& lock);
#ifdef DEBUG
  bool isTaskInWaitingList(const ParallelMarkTask* task,
                           const AutoLockGC& lock) const;
#endif

  bool hasActiveTasks(const AutoLockGC& lock) const { return activeTasks; }
  void incActiveTasks(ParallelMarkTask* task, const AutoLockGC& lock);
  void decActiveTasks(ParallelMarkTask* task, const AutoLockGC& lock);

  size_t workerCount() const;

  friend class ParallelMarkTask;

  GCRuntime* const gc;

  using ParallelMarkTaskList = mozilla::DoublyLinkedList<ParallelMarkTask>;
  GCLockData<ParallelMarkTaskList> waitingTasks;
  mozilla::Atomic<uint32_t, mozilla::Relaxed> waitingTaskCount;

  GCLockData<size_t> activeTasks;
  GCLockData<ConditionVariable> activeTasksAvailable;
};

// A helper thread task that performs parallel marking.
class MOZ_STACK_CLASS ParallelMarkTask
    : public GCParallelTask,
      public mozilla::DoublyLinkedListElement<ParallelMarkTask> {
 public:
  friend class ParallelMarker;

  ParallelMarkTask(ParallelMarker* pm, GCMarker* marker, MarkColor color,
                   const SliceBudget& budget);
  ~ParallelMarkTask();

  void run(AutoLockHelperThreadState& lock) override;
  void markOrSteal(AutoLockGC& lock);
  bool tryMarking(AutoLockGC& lock);
  bool tryStealing(AutoLockGC& lock);

  void waitUntilResumed(AutoLockGC& lock);
  void resume(const AutoLockGC& lock);

 private:
  bool hasWork() const;

  // The following fields are only accessed by the marker thread:
  ParallelMarker* const pm;
  GCMarker* const marker;
  AutoSetMarkColor color;
  SliceBudget budget;
  ConditionVariable resumed;

  GCLockData<bool> isWaiting;
};

}  // namespace gc
}  // namespace js

#endif /* gc_ParallelMarking_h */
