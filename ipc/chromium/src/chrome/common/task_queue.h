// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TASK_QUEUE_H__
#define CHROME_COMMON_TASK_QUEUE_H__

#include <deque>

#include "base/task.h"

// A TaskQueue is a queue of tasks waiting to be run.  To run the tasks, call
// the Run method.  A task queue is itself a Task so that it can be placed in a
// message loop or another task queue.
class TaskQueue : public Task {
 public:
  TaskQueue();
  ~TaskQueue();

  // Run all the tasks in the queue.  New tasks pushed onto the queue during
  // a run will be run next time |Run| is called.
  virtual void Run();

  // Push the specified task onto the queue.  When the queue is run, the tasks
  // will be run in the order they are pushed.
  //
  // This method takes ownership of |task| and will delete task after it is run
  // (or when the TaskQueue is destroyed, if we never got a chance to run it).
  void Push(Task* task);

  // Remove all tasks from the queue.  The tasks are deleted.
  void Clear();

  // Returns true if this queue contains no tasks.
  bool Empty() const;

 private:
   // The list of tasks we are waiting to run.
   std::deque<Task*> queue_;
};

#endif  // CHROME_COMMON_TASK_QUEUE_H__
