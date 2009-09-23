// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/multiprocess_test.h"
#include "base/platform_thread.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/shared_memory.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

static const int kNumThreads = 5;
static const int kNumTasks = 5;

namespace base {

namespace {

// Each thread will open the shared memory.  Each thread will take a different 4
// byte int pointer, and keep changing it, with some small pauses in between.
// Verify that each thread's value in the shared memory is always correct.
class MultipleThreadMain : public PlatformThread::Delegate {
 public:
  explicit MultipleThreadMain(int16 id) : id_(id) {}
  ~MultipleThreadMain() {}

  static void CleanUp() {
    SharedMemory memory;
    memory.Delete(s_test_name_);
  }

  // PlatformThread::Delegate interface.
  void ThreadMain() {
    ScopedNSAutoreleasePool pool;  // noop if not OSX
    const int kDataSize = 1024;
    SharedMemory memory;
    bool rv = memory.Create(s_test_name_, false, true, kDataSize);
    EXPECT_TRUE(rv);
    rv = memory.Map(kDataSize);
    EXPECT_TRUE(rv);
    int *ptr = static_cast<int*>(memory.memory()) + id_;
    EXPECT_EQ(*ptr, 0);

    for (int idx = 0; idx < 100; idx++) {
      *ptr = idx;
      PlatformThread::Sleep(1);  // Short wait.
      EXPECT_EQ(*ptr, idx);
    }

    memory.Close();
  }

 private:
  int16 id_;

  static const wchar_t* const s_test_name_;

  DISALLOW_COPY_AND_ASSIGN(MultipleThreadMain);
};

const wchar_t* const MultipleThreadMain::s_test_name_ =
    L"SharedMemoryOpenThreadTest";

// TODO(port):
// This test requires the ability to pass file descriptors between processes.
// We haven't done that yet in Chrome for POSIX.
#if defined(OS_WIN)
// Each thread will open the shared memory.  Each thread will take the memory,
// and keep changing it while trying to lock it, with some small pauses in
// between. Verify that each thread's value in the shared memory is always
// correct.
class MultipleLockThread : public PlatformThread::Delegate {
 public:
  explicit MultipleLockThread(int id) : id_(id) {}
  ~MultipleLockThread() {}

  // PlatformThread::Delegate interface.
  void ThreadMain() {
    const int kDataSize = sizeof(int);
    SharedMemoryHandle handle = NULL;
    {
      SharedMemory memory1;
      EXPECT_TRUE(memory1.Create(L"SharedMemoryMultipleLockThreadTest",
                                 false, true, kDataSize));
      EXPECT_TRUE(memory1.ShareToProcess(GetCurrentProcess(), &handle));
      // TODO(paulg): Implement this once we have a posix version of
      // SharedMemory::ShareToProcess.
      EXPECT_TRUE(true);
    }

    SharedMemory memory2(handle, false);
    EXPECT_TRUE(memory2.Map(kDataSize));
    volatile int* const ptr = static_cast<int*>(memory2.memory());

    for (int idx = 0; idx < 20; idx++) {
      memory2.Lock();
      int i = (id_ << 16) + idx;
      *ptr = i;
      PlatformThread::Sleep(1);  // Short wait.
      EXPECT_EQ(*ptr, i);
      memory2.Unlock();
    }

    memory2.Close();
  }

 private:
  int id_;

  DISALLOW_COPY_AND_ASSIGN(MultipleLockThread);
};
#endif

}  // namespace

TEST(SharedMemoryTest, OpenClose) {
  const int kDataSize = 1024;
  std::wstring test_name = L"SharedMemoryOpenCloseTest";

  // Open two handles to a memory segment, confirm that they are mapped
  // separately yet point to the same space.
  SharedMemory memory1;
  bool rv = memory1.Delete(test_name);
  EXPECT_TRUE(rv);
  rv = memory1.Delete(test_name);
  EXPECT_TRUE(rv);
  rv = memory1.Open(test_name, false);
  EXPECT_FALSE(rv);
  rv = memory1.Create(test_name, false, false, kDataSize);
  EXPECT_TRUE(rv);
  rv = memory1.Map(kDataSize);
  EXPECT_TRUE(rv);
  SharedMemory memory2;
  rv = memory2.Open(test_name, false);
  EXPECT_TRUE(rv);
  rv = memory2.Map(kDataSize);
  EXPECT_TRUE(rv);
  EXPECT_NE(memory1.memory(), memory2.memory());  // Compare the pointers.

  // Make sure we don't segfault. (it actually happened!)
  ASSERT_NE(memory1.memory(), static_cast<void*>(NULL));
  ASSERT_NE(memory2.memory(), static_cast<void*>(NULL));

  // Write data to the first memory segment, verify contents of second.
  memset(memory1.memory(), '1', kDataSize);
  EXPECT_EQ(memcmp(memory1.memory(), memory2.memory(), kDataSize), 0);

  // Close the first memory segment, and verify the second has the right data.
  memory1.Close();
  char *start_ptr = static_cast<char *>(memory2.memory());
  char *end_ptr = start_ptr + kDataSize;
  for (char* ptr = start_ptr; ptr < end_ptr; ptr++)
    EXPECT_EQ(*ptr, '1');

  // Close the second memory segment.
  memory2.Close();

  rv = memory1.Delete(test_name);
  EXPECT_TRUE(rv);
  rv = memory2.Delete(test_name);
  EXPECT_TRUE(rv);
}

// Create a set of N threads to each open a shared memory segment and write to
// it. Verify that they are always reading/writing consistent data.
TEST(SharedMemoryTest, MultipleThreads) {
  MultipleThreadMain::CleanUp();
  // On POSIX we have a problem when 2 threads try to create the shmem
  // (a file) at exactly the same time, since create both creates the
  // file and zerofills it.  We solve the problem for this unit test
  // (make it not flaky) by starting with 1 thread, then
  // intentionally don't clean up its shmem before running with
  // kNumThreads.

  int threadcounts[] = { 1, kNumThreads };
  for (size_t i = 0; i < sizeof(threadcounts) / sizeof(threadcounts); i++) {
    int numthreads = threadcounts[i];
    scoped_array<PlatformThreadHandle> thread_handles;
    scoped_array<MultipleThreadMain*> thread_delegates;

    thread_handles.reset(new PlatformThreadHandle[numthreads]);
    thread_delegates.reset(new MultipleThreadMain*[numthreads]);

    // Spawn the threads.
    for (int16 index = 0; index < numthreads; index++) {
      PlatformThreadHandle pth;
      thread_delegates[index] = new MultipleThreadMain(index);
      EXPECT_TRUE(PlatformThread::Create(0, thread_delegates[index], &pth));
      thread_handles[index] = pth;
    }

    // Wait for the threads to finish.
    for (int index = 0; index < numthreads; index++) {
      PlatformThread::Join(thread_handles[index]);
      delete thread_delegates[index];
    }
  }
  MultipleThreadMain::CleanUp();
}

// TODO(port): this test requires the MultipleLockThread class
// (defined above), which requires the ability to pass file
// descriptors between processes.  We haven't done that yet in Chrome
// for POSIX.
#if defined(OS_WIN)
// Create a set of threads to each open a shared memory segment and write to it
// with the lock held. Verify that they are always reading/writing consistent
// data.
TEST(SharedMemoryTest, Lock) {
  PlatformThreadHandle thread_handles[kNumThreads];
  MultipleLockThread* thread_delegates[kNumThreads];

  // Spawn the threads.
  for (int index = 0; index < kNumThreads; ++index) {
    PlatformThreadHandle pth;
    thread_delegates[index] = new MultipleLockThread(index);
    EXPECT_TRUE(PlatformThread::Create(0, thread_delegates[index], &pth));
    thread_handles[index] = pth;
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kNumThreads; ++index) {
    PlatformThread::Join(thread_handles[index]);
    delete thread_delegates[index];
  }
}
#endif

// Allocate private (unique) shared memory with an empty string for a
// name.  Make sure several of them don't point to the same thing as
// we might expect if the names are equal.
TEST(SharedMemoryTest, AnonymousPrivate) {
  int i, j;
  int count = 4;
  bool rv;
  const int kDataSize = 8192;

  scoped_array<SharedMemory> memories(new SharedMemory[count]);
  scoped_array<int*> pointers(new int*[count]);
  ASSERT_TRUE(memories.get());
  ASSERT_TRUE(pointers.get());

  for (i = 0; i < count; i++) {
    rv = memories[i].Create(L"", false, true, kDataSize);
    EXPECT_TRUE(rv);
    rv = memories[i].Map(kDataSize);
    EXPECT_TRUE(rv);
    int *ptr = static_cast<int*>(memories[i].memory());
    EXPECT_TRUE(ptr);
    pointers[i] = ptr;
  }

  for (i = 0; i < count; i++) {
    // zero out the first int in each except for i; for that one, make it 100.
    for (j = 0; j < count; j++) {
      if (i == j)
        pointers[j][0] = 100;
      else
        pointers[j][0] = 0;
    }
    // make sure there is no bleeding of the 100 into the other pointers
    for (j = 0; j < count; j++) {
      if (i == j)
        EXPECT_EQ(100, pointers[j][0]);
      else
        EXPECT_EQ(0, pointers[j][0]);
    }
  }

  for (int i = 0; i < count; i++) {
    memories[i].Close();
  }

}


// On POSIX it is especially important we test shmem across processes,
// not just across threads.  But the test is enabled on all platforms.
class SharedMemoryProcessTest : public MultiProcessTest {
 public:

  static void CleanUp() {
    SharedMemory memory;
    memory.Delete(s_test_name_);
  }

  static int TaskTestMain() {
    int errors = 0;
    ScopedNSAutoreleasePool pool;  // noop if not OSX
    const int kDataSize = 1024;
    SharedMemory memory;
    bool rv = memory.Create(s_test_name_, false, true, kDataSize);
    EXPECT_TRUE(rv);
    if (rv != true)
      errors++;
    rv = memory.Map(kDataSize);
    EXPECT_TRUE(rv);
    if (rv != true)
      errors++;
    int *ptr = static_cast<int*>(memory.memory());

    for (int idx = 0; idx < 20; idx++) {
      memory.Lock();
      int i = (1 << 16) + idx;
      *ptr = i;
      PlatformThread::Sleep(10);  // Short wait.
      if (*ptr != i)
        errors++;
      memory.Unlock();
    }

    memory.Close();
    return errors;
  }

 private:
  static const wchar_t* const s_test_name_;
};

const wchar_t* const SharedMemoryProcessTest::s_test_name_ = L"MPMem";


TEST_F(SharedMemoryProcessTest, Tasks) {
  SharedMemoryProcessTest::CleanUp();

  base::ProcessHandle handles[kNumTasks];
  for (int index = 0; index < kNumTasks; ++index) {
    handles[index] = SpawnChild(L"SharedMemoryTestMain");
  }

  int exit_code = 0;
  for (int index = 0; index < kNumTasks; ++index) {
    EXPECT_TRUE(base::WaitForExitCode(handles[index], &exit_code));
    EXPECT_TRUE(exit_code == 0);
  }

  SharedMemoryProcessTest::CleanUp();
}

MULTIPROCESS_TEST_MAIN(SharedMemoryTestMain) {
  return SharedMemoryProcessTest::TaskTestMain();
}


}  // namespace base
