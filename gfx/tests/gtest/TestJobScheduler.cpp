/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/gfx/JobScheduler.h"

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
  mozilla::gfx::CriticalSection mSection;

  explicit SanityChecker(uint64_t aNumCmdBuffers)
  {
    for (uint32_t i = 0; i < aNumCmdBuffers; ++i) {
      mAdvancements.push_back(0);
    }
  }

  virtual void Check(uint64_t aJobId, uint64_t aCmdId)
  {
    MaybeYieldThread();
    CriticalSectionAutoEnter lock(&mSection);
    MOZ_RELEASE_ASSERT(mAdvancements[aJobId] == aCmdId-1);
    mAdvancements[aJobId] = aCmdId;
  }
};

/// Run checks that are specific to TestSchulerJoin.
struct JoinTestSanityCheck : public SanityChecker {
  bool mSpecialJobHasRun;

  explicit JoinTestSanityCheck(uint64_t aNumCmdBuffers)
  : SanityChecker(aNumCmdBuffers)
  , mSpecialJobHasRun(false)
  {}

  virtual void Check(uint64_t aJobId, uint64_t aCmdId) override
  {
    // Job 0 is the special task executed when everything is joined after task 1
    if (aCmdId == 0) {
      MOZ_RELEASE_ASSERT(!mSpecialJobHasRun, "GFX: A special task has been executed.");
      mSpecialJobHasRun = true;
      for (auto advancement : mAdvancements) {
        // Because of the synchronization point (beforeFilter), all
        // task buffers should have run task 1 when task 0 is run.
        MOZ_RELEASE_ASSERT(advancement == 1, "GFX: task buffer has not run task 1.");
      }
    } else {
      // This check does not apply to task 0.
      SanityChecker::Check(aJobId, aCmdId);
    }

    if (aCmdId == 2) {
      MOZ_RELEASE_ASSERT(mSpecialJobHasRun, "GFX: Special job has not run.");
    }
  }
};

class TestJob : public Job
{
public:
  TestJob(uint64_t aCmdId, uint64_t aJobId, SanityChecker* aChecker,
           SyncObject* aStart, SyncObject* aCompletion)
  : Job(aStart, aCompletion, nullptr)
  , mCmdId(aCmdId)
  , mCmdBufferId(aJobId)
  , mSanityChecker(aChecker)
  {}

  JobStatus Run()
  {
    MaybeYieldThread();
    mSanityChecker->Check(mCmdBufferId, mCmdId);
    MaybeYieldThread();
    return JobStatus::Complete;
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

  RefPtr<SyncObject> beforeFilter = new SyncObject(aNumCmdBuffers);
  RefPtr<SyncObject> afterFilter = new SyncObject();
  RefPtr<SyncObject> completion = new SyncObject(aNumCmdBuffers);


  for (uint32_t i = 0; i < aNumCmdBuffers; ++i) {
    Job* t1 = new TestJob(1, i, &check, nullptr, beforeFilter);
    JobScheduler::SubmitJob(t1);
    MaybeYieldThread();
  }
  beforeFilter->FreezePrerequisites();

  // This task buffer is executed when all other tasks have joined after task 1
  JobScheduler::SubmitJob(
    new TestJob(0, 0, &check, beforeFilter, afterFilter)
  );
  afterFilter->FreezePrerequisites();

  for (uint32_t i = 0; i < aNumCmdBuffers; ++i) {
    Job* t2 = new TestJob(2, i, &check, afterFilter, completion);
    JobScheduler::SubmitJob(t2);
    MaybeYieldThread();
  }
  completion->FreezePrerequisites();

  JobScheduler::Join(completion);

  MaybeYieldThread();

  for (auto advancement : check.mAdvancements) {
    EXPECT_TRUE(advancement == 2);
  }
}

/// This test creates several chains of 10 task, tasks of a given chain are executed
/// sequentially, and chains are exectuted in parallel.
/// This simulates the typical scenario where we want to process sequences of drawing
/// commands for several tiles in parallel.
void TestSchedulerChain(uint32_t aNumThreads, uint32_t aNumCmdBuffers)
{
  SanityChecker check(aNumCmdBuffers);

  RefPtr<SyncObject> completion = new SyncObject(aNumCmdBuffers);

  uint32_t numJobs = 10;

  for (uint32_t i = 0; i < aNumCmdBuffers; ++i) {

    std::vector<RefPtr<SyncObject>> syncs;
    std::vector<Job*> tasks;
    syncs.reserve(numJobs);
    tasks.reserve(numJobs);

    for (uint32_t t = 0; t < numJobs-1; ++t) {
      syncs.push_back(new SyncObject());
      tasks.push_back(new TestJob(t+1, i, &check, t == 0 ? nullptr
                                                          : syncs[t-1].get(),
                                   syncs[t]));
      syncs.back()->FreezePrerequisites();
    }

    tasks.push_back(new TestJob(numJobs, i, &check, syncs.back(), completion));

    if (i % 2 == 0) {
      // submit half of the tasks in order
      for (Job* task : tasks) {
        JobScheduler::SubmitJob(task);
        MaybeYieldThread();
      }
    } else {
      // ... and submit the other half in reverse order
      for (int32_t reverse = numJobs-1; reverse >= 0; --reverse) {
        JobScheduler::SubmitJob(tasks[reverse]);
        MaybeYieldThread();
      }
    }
  }
  completion->FreezePrerequisites();

  JobScheduler::Join(completion);

  for (auto advancement : check.mAdvancements) {
    EXPECT_TRUE(advancement == numJobs);
  }
}

} // namespace test_scheduler

TEST(Moz2D, JobScheduler_Shutdown) {
  srand(time(nullptr));
  for (uint32_t threads = 1; threads < 16; ++threads) {
    for (uint32_t i = 1; i < 1000; ++i) {
      mozilla::gfx::JobScheduler::Init(threads, threads);
      mozilla::gfx::JobScheduler::ShutDown();
    }
  }
}

TEST(Moz2D, JobScheduler_Join) {
  srand(time(nullptr));
  for (uint32_t threads = 1; threads < 8; ++threads) {
    for (uint32_t queues = 1; queues < threads; ++queues) {
      for (uint32_t buffers = 1; buffers < 100; buffers += 3) {
        mozilla::gfx::JobScheduler::Init(threads, queues);
        test_scheduler::TestSchedulerJoin(threads, buffers);
        mozilla::gfx::JobScheduler::ShutDown();
      }
    }
  }
}

TEST(Moz2D, JobScheduler_Chain) {
  srand(time(nullptr));
  for (uint32_t threads = 1; threads < 8; ++threads) {
    for (uint32_t queues = 1; queues < threads; ++queues) {
      for (uint32_t buffers = 1; buffers < 100; buffers += 3) {
        mozilla::gfx::JobScheduler::Init(threads, queues);
        test_scheduler::TestSchedulerChain(threads, buffers);
        mozilla::gfx::JobScheduler::ShutDown();
      }
    }
  }
}
