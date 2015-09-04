/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/gfx/TaskScheduler.h"

#ifndef WIN32
#include <pthread.h>
#include <sched.h>
#endif

#include <stdlib.h>
#include <time.h>

namespace test_scheduler {

using namespace mozilla::gfx;
using namespace mozilla;

// Artificially cause threads to yield randomly in an attempt to make racy
// things more apparent (if any).
void MaybeYieldThread()
{
#ifndef WIN32
  if (rand() % 5 == 0) {
    sched_yield();
  }
#endif
}

/// Used by the TestCommand to check that tasks are processed in the right order.
struct SanityChecker {
  std::vector<uint64_t> mAdvancements;
  mozilla::gfx::Mutex mMutex;

  explicit SanityChecker(uint64_t aNumCmdBuffers)
  {
    for (uint32_t i = 0; i < aNumCmdBuffers; ++i) {
      mAdvancements.push_back(0);
    }
  }

  virtual void Check(uint64_t aTaskId, uint64_t aCmdId)
  {
    MaybeYieldThread();
    MutexAutoLock lock(&mMutex);
    ASSERT_EQ(mAdvancements[aTaskId], aCmdId-1);
    mAdvancements[aTaskId] = aCmdId;
  }
};

/// Run checks that are specific to TestSchulerJoin.
struct JoinTestSanityCheck : public SanityChecker {
  bool mSpecialTaskHasRun;

  explicit JoinTestSanityCheck(uint64_t aNumCmdBuffers)
  : SanityChecker(aNumCmdBuffers)
  , mSpecialTaskHasRun(false)
  {}

  virtual void Check(uint64_t aTaskId, uint64_t aCmdId) override
  {
    // Task 0 is the special task executed when everything is joined after task 1
    if (aCmdId == 0) {
      ASSERT_FALSE(mSpecialTaskHasRun);
      mSpecialTaskHasRun = true;
      for (auto advancement : mAdvancements) {
        // Because of the synchronization point (beforeFilter), all
        // task buffers should have run task 1 when task 0 is run.
        ASSERT_EQ(advancement, (uint32_t)1);
      }
    } else {
      // This check does not apply to task 0.
      SanityChecker::Check(aTaskId, aCmdId);
    }

    if (aCmdId == 2) {
      ASSERT_TRUE(mSpecialTaskHasRun);
    }
  }
};

class TestTask : public Task
{
public:
  TestTask(uint64_t aCmdId, uint64_t aTaskId, SanityChecker* aChecker,
           MultiThreadedTaskQueue* aQueue,
           SyncObject* aStart, SyncObject* aCompletion)
  : Task(aQueue, aStart, aCompletion)
  , mCmdId(aCmdId)
  , mCmdBufferId(aTaskId)
  , mSanityChecker(aChecker)
  {}

  TaskStatus Run()
  {
    MaybeYieldThread();
    mSanityChecker->Check(mCmdBufferId, mCmdId);
    MaybeYieldThread();
    return TaskStatus::Complete;
  }

  uint64_t mCmdId;
  uint64_t mCmdBufferId;
  SanityChecker* mSanityChecker;
};

/// This test creates aNumCmdBuffers task buffers with sync objects set up
/// so that all tasks will join after command 5 before a task buffer runs
/// a special task (task 0) after which all task buffers fork again.
/// This simulates the kind of scenario where all tiles must join at
/// a certain point to execute, say, a filter, and fork again after the filter
/// has been processed.
/// The main thread is only blocked when waiting for the completion of the entire
/// task stream (it doesn't have to wait at the filter's sync points to orchestrate it).
void TestSchedulerJoin(uint32_t aNumThreads, uint32_t aNumCmdBuffers)
{
  JoinTestSanityCheck check(aNumCmdBuffers);

  RefPtr<SyncObject> beforeFilter = new SyncObject();
  RefPtr<SyncObject> afterFilter = new SyncObject();
  RefPtr<SyncObject> completion = new SyncObject();


  for (uint32_t i = 0; i < aNumCmdBuffers; ++i) {
    Task* t1 = new TestTask(1, i, &check, TaskScheduler::GetDrawingQueue(),
                            nullptr, beforeFilter);
    TaskScheduler::SubmitTask(t1);
    MaybeYieldThread();
  }

  // This task buffer is executed when all other tasks have joined after task 1
  TaskScheduler::SubmitTask(
    new TestTask(0, 0, &check, TaskScheduler::GetDrawingQueue(), beforeFilter, afterFilter)
  );

  for (uint32_t i = 0; i < aNumCmdBuffers; ++i) {
    Task* t2 = new TestTask(2, i, &check, TaskScheduler::GetDrawingQueue(),
                            afterFilter, completion);
    TaskScheduler::SubmitTask(t2);
    MaybeYieldThread();
  }

  auto evtTask = new SetEventTask(TaskScheduler::GetDrawingQueue(), completion);
  RefPtr<EventObject> waitForCompletion = evtTask->GetEvent();
  TaskScheduler::SubmitTask(evtTask);

  MaybeYieldThread();

  waitForCompletion->Wait();

  MaybeYieldThread();

  for (auto advancement : check.mAdvancements) {
    ASSERT_TRUE(advancement == 2);
  }
}

/// This test creates several chains of 10 task, tasks of a given chain are executed
/// sequentially, and chains are exectuted in parallel.
/// This simulates the typical scenario where we want to process sequences of drawing
/// commands for several tiles in parallel.
void TestSchedulerChain(uint32_t aNumThreads, uint32_t aNumCmdBuffers)
{
  SanityChecker check(aNumCmdBuffers);

  RefPtr<SyncObject> completion = new SyncObject();

  uint32_t numTasks = 10;

  for (uint32_t i = 0; i < aNumCmdBuffers; ++i) {

    std::vector<RefPtr<SyncObject>> syncs;
    std::vector<Task*> tasks;
    syncs.reserve(numTasks);
    tasks.reserve(numTasks);

    for (uint32_t t = 0; t < numTasks-1; ++t) {
      syncs.push_back(new SyncObject());
      tasks.push_back(new TestTask(t+1, i, &check, TaskScheduler::GetDrawingQueue(),
                                   t == 0 ? nullptr : syncs[t-1].get(),
                                   syncs[t]));
    }

    tasks.push_back(new TestTask(numTasks, i, &check,
                    TaskScheduler::GetDrawingQueue(),
                    syncs.back(), completion));

    if (i % 2 == 0) {
      // submit half of the tasks in order
      for (Task* task : tasks) {
        TaskScheduler::SubmitTask(task);
        MaybeYieldThread();
      }
    } else {
      // ... and submit the other half in reverse order
      for (int32_t reverse = numTasks-1; reverse >= 0; --reverse) {
        TaskScheduler::SubmitTask(tasks[reverse]);
        MaybeYieldThread();
      }
    }
  }

  auto evtTask = new SetEventTask(TaskScheduler::GetDrawingQueue(), completion);
  RefPtr<EventObject> waitForCompletion = evtTask->GetEvent();
  TaskScheduler::SubmitTask(evtTask);

  MaybeYieldThread();

  waitForCompletion->Wait();

  for (auto advancement : check.mAdvancements) {
    ASSERT_TRUE(advancement == numTasks);
  }
}

} // namespace test_scheduler

TEST(Moz2D, TaskScheduler_Join) {
  srand(time(nullptr));
  for (uint32_t threads = 1; threads < 16; ++threads) {
    for (uint32_t queues = 1; queues < threads; ++queues) {
      for (uint32_t buffers = 1; buffers < 100; buffers += 3) {
        mozilla::gfx::TaskScheduler::Init(threads, queues);
        test_scheduler::TestSchedulerJoin(threads, buffers);
        mozilla::gfx::TaskScheduler::ShutDown();
      }
    }
  }
}

TEST(Moz2D, TaskScheduler_Chain) {
  srand(time(nullptr));
  for (uint32_t threads = 1; threads < 16; ++threads) {
    for (uint32_t queues = 1; queues < threads; ++queues) {
      for (uint32_t buffers = 1; buffers < 50; buffers += 3) {
        mozilla::gfx::TaskScheduler::Init(threads, queues);
        test_scheduler::TestSchedulerChain(threads, buffers);
        mozilla::gfx::TaskScheduler::ShutDown();
      }
    }
  }
}
