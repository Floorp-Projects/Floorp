// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task.h"
#include "base/waitable_event.h"
#include "base/worker_pool.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::WaitableEvent;

typedef PlatformTest WorkerPoolTest;

namespace {

class PostTaskTestTask : public Task {
 public:
  PostTaskTestTask(WaitableEvent* event) : event_(event) {
  }

  void Run() {
    event_->Signal();
  }

 private:
  WaitableEvent* event_;
};

TEST_F(WorkerPoolTest, PostTask) {
  WaitableEvent test_event(false, false);
  WaitableEvent long_test_event(false, false);
  bool signaled;

  WorkerPool::PostTask(FROM_HERE, new PostTaskTestTask(&test_event), false);
  WorkerPool::PostTask(FROM_HERE, new PostTaskTestTask(&long_test_event), true);

  signaled = test_event.Wait();
  EXPECT_TRUE(signaled);
  signaled = long_test_event.Wait();
  EXPECT_TRUE(signaled);
}

} // namespace
