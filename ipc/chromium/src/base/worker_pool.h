// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WORKER_POOL_H_
#define BASE_WORKER_POOL_H_

#include "base/tracked.h"

class Task;

// This is a facility that runs tasks that don't require a specific thread or
// a message loop.
class WorkerPool {
 public:
  // This function posts |task| to run on a worker thread.  |task_is_slow|
  // should be used for tasks that will take a long time to execute.  Returns
  // false if |task| could not be posted to a worker thread.  Regardless of
  // return value, ownership of |task| is transferred to the worker pool.
  static bool PostTask(const tracked_objects::Location& from_here,
                       Task* task, bool task_is_slow);
};

#endif  // BASE_WORKER_POOL_H_
