// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <process.h>

#include "base/message_loop.h"
#include "base/object_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class QuitDelegate : public base::ObjectWatcher::Delegate {
 public:
  virtual void OnObjectSignaled(HANDLE object) {
    MessageLoop::current()->Quit();
  }
};

class DecrementCountDelegate : public base::ObjectWatcher::Delegate {
 public:
  DecrementCountDelegate(int* counter) : counter_(counter) {
  }
  virtual void OnObjectSignaled(HANDLE object) {
    --(*counter_);
  }
 private:
  int* counter_;
};

}  // namespace

void RunTest_BasicSignal(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  base::ObjectWatcher watcher;
  EXPECT_EQ(NULL, watcher.GetWatchedObject());

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  QuitDelegate delegate;
  bool ok = watcher.StartWatching(event, &delegate);
  EXPECT_TRUE(ok);
  EXPECT_EQ(event, watcher.GetWatchedObject());

  SetEvent(event);

  MessageLoop::current()->Run();

  EXPECT_EQ(NULL, watcher.GetWatchedObject());
  CloseHandle(event);
}

void RunTest_BasicCancel(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  base::ObjectWatcher watcher;

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  QuitDelegate delegate;
  bool ok = watcher.StartWatching(event, &delegate);
  EXPECT_TRUE(ok);

  watcher.StopWatching();

  CloseHandle(event);
}


void RunTest_CancelAfterSet(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  base::ObjectWatcher watcher;

  int counter = 1;
  DecrementCountDelegate delegate(&counter);

    // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  bool ok = watcher.StartWatching(event, &delegate);
  EXPECT_TRUE(ok);

  SetEvent(event);

  // Let the background thread do its business
  Sleep(30);

  watcher.StopWatching();

  MessageLoop::current()->RunAllPending();

  // Our delegate should not have fired.
  EXPECT_EQ(1, counter);

  CloseHandle(event);
}

void RunTest_OutlivesMessageLoop(MessageLoop::Type message_loop_type) {
  // Simulate a MessageLoop that dies before an ObjectWatcher.  This ordinarily
  // doesn't happen when people use the Thread class, but it can happen when
  // people use the Singleton pattern or atexit.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);  // not signaled
  {
    base::ObjectWatcher watcher;
    {
      MessageLoop message_loop(message_loop_type);

      QuitDelegate delegate;
      watcher.StartWatching(event, &delegate);
    }
  }
  CloseHandle(event);
}

//-----------------------------------------------------------------------------

TEST(ObjectWatcherTest, BasicSignal) {
  RunTest_BasicSignal(MessageLoop::TYPE_DEFAULT);
  RunTest_BasicSignal(MessageLoop::TYPE_IO);
  RunTest_BasicSignal(MessageLoop::TYPE_UI);
}

TEST(ObjectWatcherTest, BasicCancel) {
  RunTest_BasicCancel(MessageLoop::TYPE_DEFAULT);
  RunTest_BasicCancel(MessageLoop::TYPE_IO);
  RunTest_BasicCancel(MessageLoop::TYPE_UI);
}

TEST(ObjectWatcherTest, CancelAfterSet) {
  RunTest_CancelAfterSet(MessageLoop::TYPE_DEFAULT);
  RunTest_CancelAfterSet(MessageLoop::TYPE_IO);
  RunTest_CancelAfterSet(MessageLoop::TYPE_UI);
}

TEST(ObjectWatcherTest, OutlivesMessageLoop) {
  RunTest_OutlivesMessageLoop(MessageLoop::TYPE_DEFAULT);
  RunTest_OutlivesMessageLoop(MessageLoop::TYPE_IO);
  RunTest_OutlivesMessageLoop(MessageLoop::TYPE_UI);
}
