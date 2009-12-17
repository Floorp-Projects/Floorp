// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/directory_watcher.h"

#include <limits>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#if defined(OS_WIN)
#include "base/win_util.h"
#endif  // defined(OS_WIN)
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// For tests where we wait a bit to verify nothing happened
const int kWaitForEventTime = 1000;

class DirectoryWatcherTest : public testing::Test {
 public:
  // Implementation of DirectoryWatcher on Mac requires UI loop.
  DirectoryWatcherTest() : loop_(MessageLoop::TYPE_UI) {
  }

  void OnTestDelegateFirstNotification(const FilePath& path) {
    notified_delegates_++;
    if (notified_delegates_ >= expected_notified_delegates_)
      MessageLoop::current()->Quit();
  }

 protected:
  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    FilePath path;
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &path));
    test_dir_ = path.Append(FILE_PATH_LITERAL("DirectoryWatcherTest"));

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);
  }

  virtual void TearDown() {
    // Make sure there are no tasks in the loop.
    loop_.RunAllPending();

    // Clean up test directory.
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // Write |content| to the |filename|. Returns true on success.
  bool WriteTestFile(const FilePath& filename,
                     const std::string& content) {
    return (file_util::WriteFile(filename, content.c_str(), content.length()) ==
            static_cast<int>(content.length()));
  }

  // Create directory |name| under test_dir_. If |sync| is true, runs
  // SyncIfPOSIX. Returns path to the created directory, including test_dir_.
  FilePath CreateTestDirDirectoryASCII(const std::string& name, bool sync) {
    FilePath path(test_dir_.AppendASCII(name));
    EXPECT_TRUE(file_util::CreateDirectory(path));
    if (sync)
      SyncIfPOSIX();
    return path;
  }

  void SetExpectedNumberOfNotifiedDelegates(int n) {
    notified_delegates_ = 0;
    expected_notified_delegates_ = n;
  }

  void VerifyExpectedNumberOfNotifiedDelegates() {
    // Check that we get at least the expected number of notified delegates.
    if (expected_notified_delegates_ - notified_delegates_ > 0)
      loop_.Run();

    // Check that we get no more than the expected number of notified delegates.
    loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
                          kWaitForEventTime);
    loop_.Run();
    EXPECT_EQ(expected_notified_delegates_, notified_delegates_);
  }

  // We need this function for reliable tests on Mac OS X. FSEvents API
  // has a latency interval and can merge multiple events into one,
  // and we need a clear distinction between events triggered by test setup code
  // and test code.
  void SyncIfPOSIX() {
#if defined(OS_POSIX)
    sync();
#endif  // defined(OS_POSIX)
  }

  MessageLoop loop_;

  // The path to a temporary directory used for testing.
  FilePath test_dir_;

  // The number of test delegates which received their notification.
  int notified_delegates_;

  // The number of notified test delegates after which we quit the message loop.
  int expected_notified_delegates_;
};

class TestDelegate : public DirectoryWatcher::Delegate {
 public:
  TestDelegate(DirectoryWatcherTest* test)
      : test_(test),
        got_notification_(false),
        original_thread_id_(PlatformThread::CurrentId()) {
  }

  bool got_notification() const {
    return got_notification_;
  }

  void reset() {
    got_notification_ = false;
  }

  virtual void OnDirectoryChanged(const FilePath& path) {
    EXPECT_EQ(original_thread_id_, PlatformThread::CurrentId());
    if (!got_notification_)
      test_->OnTestDelegateFirstNotification(path);
    got_notification_ = true;
  }

 private:
  // Hold a pointer to current test fixture to inform it on first notification.
  DirectoryWatcherTest* test_;

  // Set to true after first notification.
  bool got_notification_;

  // Keep track of original thread id to verify that callbacks are called
  // on the same thread.
  PlatformThreadId original_thread_id_;
};

// Basic test: add a file and verify we notice it.
TEST_F(DirectoryWatcherTest, NewFile) {
  DirectoryWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, false));

  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(WriteTestFile(test_dir_.AppendASCII("test_file"), "content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}

// Verify that modifying a file is caught.
TEST_F(DirectoryWatcherTest, ModifiedFile) {
  // Write a file to the test dir.
  ASSERT_TRUE(WriteTestFile(test_dir_.AppendASCII("test_file"), "content"));
  SyncIfPOSIX();

  DirectoryWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, false));

  // Now make sure we get notified if the file is modified.
  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(WriteTestFile(test_dir_.AppendASCII("test_file"), "new content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}

TEST_F(DirectoryWatcherTest, DeletedFile) {
  // Write a file to the test dir.
  ASSERT_TRUE(WriteTestFile(test_dir_.AppendASCII("test_file"), "content"));
  SyncIfPOSIX();

  DirectoryWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, false));

  // Now make sure we get notified if the file is deleted.
  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(file_util::Delete(test_dir_.AppendASCII("test_file"), false));
  VerifyExpectedNumberOfNotifiedDelegates();
}

// Verify that letting the watcher go out of scope stops notifications.
TEST_F(DirectoryWatcherTest, Unregister) {
  TestDelegate delegate(this);

  {
    DirectoryWatcher watcher;
    ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, false));

    // And then let it fall out of scope, clearing its watch.
  }

  // Write a file to the test dir.
  SetExpectedNumberOfNotifiedDelegates(0);
  ASSERT_TRUE(WriteTestFile(test_dir_.AppendASCII("test_file"), "content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}

TEST_F(DirectoryWatcherTest, SubDirRecursive) {
  FilePath subdir(CreateTestDirDirectoryASCII("SubDir", true));

#if defined(OS_LINUX)
  // TODO(port): Recursive watches are not implemented on Linux.
  return;
#endif  // !defined(OS_WIN)

  // Verify that modifications to a subdirectory are noticed by recursive watch.
  TestDelegate delegate(this);
  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, true));
  // Write a file to the subdir.
  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(WriteTestFile(subdir.AppendASCII("test_file"), "some content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}

TEST_F(DirectoryWatcherTest, SubDirNonRecursive) {
#if defined(OS_WIN)
  // Disable this test for earlier version of Windows. It turned out to be
  // very difficult to create a reliable test for them.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)

  FilePath subdir(CreateTestDirDirectoryASCII("SubDir", false));

  // Create a test file before the test. On Windows we get a notification
  // when creating a file in a subdir even with a non-recursive watch.
  ASSERT_TRUE(WriteTestFile(subdir.AppendASCII("test_file"), "some content"));

  SyncIfPOSIX();

  // Verify that modifications to a subdirectory are not noticed
  // by a not-recursive watch.
  DirectoryWatcher watcher;
  TestDelegate delegate(this);
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, false));

  // Modify the test file. There should be no notifications.
  SetExpectedNumberOfNotifiedDelegates(0);
  ASSERT_TRUE(WriteTestFile(subdir.AppendASCII("test_file"), "other content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}

namespace {
// Used by the DeleteDuringNotify test below.
// Deletes the DirectoryWatcher when it's notified.
class Deleter : public DirectoryWatcher::Delegate {
 public:
  Deleter(DirectoryWatcher* watcher, MessageLoop* loop)
      : watcher_(watcher),
        loop_(loop) {
  }

  virtual void OnDirectoryChanged(const FilePath& path) {
    watcher_.reset(NULL);
    loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  scoped_ptr<DirectoryWatcher> watcher_;
  MessageLoop* loop_;
};
}  // anonymous namespace

// Verify that deleting a watcher during the callback
TEST_F(DirectoryWatcherTest, DeleteDuringNotify) {
  DirectoryWatcher* watcher = new DirectoryWatcher;
  Deleter deleter(watcher, &loop_);  // Takes ownership of watcher.
  ASSERT_TRUE(watcher->Watch(test_dir_, &deleter, false));

  ASSERT_TRUE(WriteTestFile(test_dir_.AppendASCII("test_file"), "content"));
  loop_.Run();

  // We win if we haven't crashed yet.
  // Might as well double-check it got deleted, too.
  ASSERT_TRUE(deleter.watcher_.get() == NULL);
}

TEST_F(DirectoryWatcherTest, MultipleWatchersSingleFile) {
  DirectoryWatcher watcher1, watcher2;
  TestDelegate delegate1(this), delegate2(this);
  ASSERT_TRUE(watcher1.Watch(test_dir_, &delegate1, false));
  ASSERT_TRUE(watcher2.Watch(test_dir_, &delegate2, false));

  SetExpectedNumberOfNotifiedDelegates(2);
  ASSERT_TRUE(WriteTestFile(test_dir_.AppendASCII("test_file"), "content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}

TEST_F(DirectoryWatcherTest, MultipleWatchersDifferentFiles) {
  const int kNumberOfWatchers = 5;
  DirectoryWatcher watchers[kNumberOfWatchers];
  TestDelegate delegates[kNumberOfWatchers] = {this, this, this, this, this};
  FilePath subdirs[kNumberOfWatchers];
  for (int i = 0; i < kNumberOfWatchers; i++) {
    subdirs[i] = CreateTestDirDirectoryASCII("Dir" + IntToString(i), false);
    ASSERT_TRUE(watchers[i].Watch(subdirs[i], &delegates[i], false));
  }
  for (int i = 0; i < kNumberOfWatchers; i++) {
    // Verify that we only get modifications from one watcher (each watcher has
    // different directory).

    for (int j = 0; j < kNumberOfWatchers; j++)
      delegates[j].reset();

    // Write a file to the subdir.
    SetExpectedNumberOfNotifiedDelegates(1);
    ASSERT_TRUE(WriteTestFile(subdirs[i].AppendASCII("test_file"), "content"));
    VerifyExpectedNumberOfNotifiedDelegates();

    loop_.RunAllPending();
  }
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// TODO(phajdan.jr): Enable when support for Linux recursive watches is added.

TEST_F(DirectoryWatcherTest, WatchCreatedDirectory) {
  TestDelegate delegate(this);
  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, true));

  SetExpectedNumberOfNotifiedDelegates(1);
  FilePath subdir(CreateTestDirDirectoryASCII("SubDir", true));
  // Create a file inside the subdir to force Windows to fire notifications.
  ASSERT_TRUE(WriteTestFile(subdir.AppendASCII("test_file"), "some content"));
  VerifyExpectedNumberOfNotifiedDelegates();

  delegate.reset();

  // Verify that changes inside the subdir are noticed.
  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(WriteTestFile(subdir.AppendASCII("test_file"), "other content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}

TEST_F(DirectoryWatcherTest, RecursiveWatchDeletedSubdirectory) {
  FilePath subdir(CreateTestDirDirectoryASCII("SubDir", true));

  TestDelegate delegate(this);
  DirectoryWatcher watcher;
  ASSERT_TRUE(watcher.Watch(test_dir_, &delegate, true));

  // Write a file to the subdir.
  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(WriteTestFile(subdir.AppendASCII("test_file"), "some content"));
  VerifyExpectedNumberOfNotifiedDelegates();

  delegate.reset();

  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(file_util::Delete(subdir, true));
  VerifyExpectedNumberOfNotifiedDelegates();
}

TEST_F(DirectoryWatcherTest, MoveFileAcrossWatches) {
  FilePath subdir1(CreateTestDirDirectoryASCII("SubDir1", true));
  FilePath subdir2(CreateTestDirDirectoryASCII("SubDir2", true));

  TestDelegate delegate1(this), delegate2(this);
  DirectoryWatcher watcher1, watcher2;
  ASSERT_TRUE(watcher1.Watch(subdir1, &delegate1, true));
  ASSERT_TRUE(watcher2.Watch(subdir2, &delegate2, true));

  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(WriteTestFile(subdir1.AppendASCII("file"), "some content"));
  SyncIfPOSIX();
  VerifyExpectedNumberOfNotifiedDelegates();

  delegate1.reset();
  delegate2.reset();

  SetExpectedNumberOfNotifiedDelegates(2);
  ASSERT_TRUE(file_util::Move(subdir1.AppendASCII("file"),
                              subdir2.AppendASCII("file")));
  VerifyExpectedNumberOfNotifiedDelegates();

  delegate1.reset();
  delegate2.reset();

  SetExpectedNumberOfNotifiedDelegates(1);
  ASSERT_TRUE(WriteTestFile(subdir2.AppendASCII("file"), "other content"));
  VerifyExpectedNumberOfNotifiedDelegates();
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

// Verify that watching a directory that doesn't exist fails, but doesn't
// asssert.
// Basic test: add a file and verify we notice it.
TEST_F(DirectoryWatcherTest, NonExistentDirectory) {
  DirectoryWatcher watcher;
  ASSERT_FALSE(watcher.Watch(test_dir_.AppendASCII("does-not-exist"), NULL,
                             false));
}

}  // namespace
