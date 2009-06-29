// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The thread pool used in the Linux implementation of WorkerPool dynamically
// adds threads as necessary to handle all tasks.  It keeps old threads around
// for a period of time to allow them to be reused.  After this waiting period,
// the threads exit.  This thread pool uses non-joinable threads, therefore
// worker threads are not joined during process shutdown.  This means that
// potentially long running tasks (such as DNS lookup) do not block process
// shutdown, but also means that process shutdown may "leak" objects.  Note that
// although LinuxDynamicThreadPool spawns the worker threads and manages the
// task queue, it does not own the worker threads.  The worker threads ask the
// LinuxDynamicThreadPool for work and eventually clean themselves up.  The
// worker threads all maintain scoped_refptrs to the LinuxDynamicThreadPool
// instance, which prevents LinuxDynamicThreadPool from disappearing before all
// worker threads exit.  The owner of LinuxDynamicThreadPool should likewise
// maintain a scoped_refptr to the LinuxDynamicThreadPool instance.
//
// NOTE: The classes defined in this file are only meant for use by the Linux
// implementation of WorkerPool.  No one else should be using these classes.
// These symbols are exported in a header purely for testing purposes.

#ifndef BASE_WORKER_POOL_LINUX_H_
#define BASE_WORKER_POOL_LINUX_H_

#include <queue>
#include <string>

#include "base/basictypes.h"
#include "base/condition_variable.h"
#include "base/lock.h"
#include "base/platform_thread.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

class Task;

namespace base {

class LinuxDynamicThreadPool
    : public RefCountedThreadSafe<LinuxDynamicThreadPool> {
 public:
  class LinuxDynamicThreadPoolPeer;

  // All worker threads will share the same |name_prefix|.  They will exit after
  // |idle_seconds_before_exit|.
  LinuxDynamicThreadPool(const std::string& name_prefix,
                         int idle_seconds_before_exit);
  ~LinuxDynamicThreadPool();

  // Indicates that the thread pool is going away.  Stops handing out tasks to
  // worker threads.  Wakes up all the idle threads to let them exit.
  void Terminate();

  // Adds |task| to the thread pool.  LinuxDynamicThreadPool assumes ownership
  // of |task|.
  void PostTask(Task* task);

  // Worker thread method to wait for up to |idle_seconds_before_exit| for more
  // work from the thread pool.  Returns NULL if no work is available.
  Task* WaitForTask();

 private:
  friend class LinuxDynamicThreadPoolPeer;

  const std::string name_prefix_;
  const int idle_seconds_before_exit_;

  Lock lock_;  // Protects all the variables below.

  // Signal()s worker threads to let them know more tasks are available.
  // Also used for Broadcast()'ing to worker threads to let them know the pool
  // is being deleted and they can exit.
  ConditionVariable tasks_available_cv_;
  int num_idle_threads_;
  std::queue<Task*> tasks_;
  bool terminated_;
  // Only used for tests to ensure correct thread ordering.  It will always be
  // NULL in non-test code.
  scoped_ptr<ConditionVariable> num_idle_threads_cv_;

  DISALLOW_COPY_AND_ASSIGN(LinuxDynamicThreadPool);
};

}  // namespace base

#endif  // BASE_WORKER_POOL_LINUX_H_
