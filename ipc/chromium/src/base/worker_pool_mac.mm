// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/worker_pool.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#import "base/singleton_objc.h"
#include "base/task.h"

// TaskOperation adapts Task->Run() for use in an NSOperationQueue.
@interface TaskOperation : NSOperation {
 @private
  Task* task_;  // (strong)
}

// Returns an autoreleased instance of TaskOperation.  See -initWithTask: for
// details.
+ (id)taskOperationWithTask:(Task*)task;

// Designated initializer.  |task| is adopted as the Task* whose Run method
// this operation will call when executed.
- (id)initWithTask:(Task*)task;

@end

@implementation TaskOperation

+ (id)taskOperationWithTask:(Task*)task {
  return [[[TaskOperation alloc] initWithTask:task] autorelease];
}

- (id)init {
  return [self initWithTask:NULL];
}

- (id)initWithTask:(Task*)task {
  if ((self = [super init])) {
    task_ = task;
  }
  return self;
}

- (void)main {
  DCHECK(task_) << "-[TaskOperation main] called with no task";
  task_->Run();
  delete task_;
  task_ = NULL;
}

- (void)dealloc {
  DCHECK(!task_) << "-[TaskOperation dealloc] called on unused TaskOperation";
  delete task_;
  [super dealloc];
}

@end

bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          Task* task, bool task_is_slow) {
  // Ignore |task_is_slow|, it doesn't map directly to any tunable aspect of
  // an NSOperation.

  task->SetBirthPlace(from_here);

  NSOperationQueue* operation_queue = SingletonObjC<NSOperationQueue>::get();
  [operation_queue addOperation:[TaskOperation taskOperationWithTask:task]];

  return true;
}
