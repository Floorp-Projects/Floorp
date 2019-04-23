/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MultiWriterQueue.h"

#include "DDTimeStamp.h"
#include "mozilla/Assertions.h"
#include "nsDeque.h"
#include "nsThreadUtils.h"

#include <gtest/gtest.h>

using mozilla::MultiWriterQueue;
using mozilla::MultiWriterQueueDefaultBufferSize;
using mozilla::MultiWriterQueueReaderLocking_Mutex;
using mozilla::MultiWriterQueueReaderLocking_None;

template <size_t BufferSize>
static void TestMultiWriterQueueST(const int loops) {
  using Q = MultiWriterQueue<int, BufferSize>;
  Q q;

  int pushes = 0;
  // Go through 2 cycles of pushes&pops, to exercize reusable buffers.
  for (int max = loops; max <= loops * 2; max *= 2) {
    // Push all numbers.
    for (int i = 1; i <= max; ++i) {
      bool newBuffer = q.Push(i);
      // A new buffer should be added at the last push of each buffer.
      EXPECT_EQ(++pushes % BufferSize == 0, newBuffer);
    }

    // Pop numbers, should be FIFO.
    int x = 0;
    q.PopAll([&](int& i) { EXPECT_EQ(++x, i); });

    // We should have got all numbers.
    EXPECT_EQ(max, x);

    // Nothing left.
    q.PopAll([&](int&) { EXPECT_TRUE(false); });
  }
}

TEST(MultiWriterQueue, SingleThreaded)
{
  TestMultiWriterQueueST<1>(10);
  TestMultiWriterQueueST<2>(10);
  TestMultiWriterQueueST<4>(10);

  TestMultiWriterQueueST<10>(9);
  TestMultiWriterQueueST<10>(10);
  TestMultiWriterQueueST<10>(11);
  TestMultiWriterQueueST<10>(19);
  TestMultiWriterQueueST<10>(20);
  TestMultiWriterQueueST<10>(21);
  TestMultiWriterQueueST<10>(999);
  TestMultiWriterQueueST<10>(1000);
  TestMultiWriterQueueST<10>(1001);

  TestMultiWriterQueueST<8192>(8192 * 4 + 1);
}

template <typename Q>
static void TestMultiWriterQueueMT(int aWriterThreads, int aReaderThreads,
                                   int aTotalLoops, const char* aPrintPrefix) {
  Q q;

  const int threads = aWriterThreads + aReaderThreads;
  const int loops = aTotalLoops / aWriterThreads;

  nsIThread** array = new nsIThread*[threads];

  mozilla::Atomic<int> pushThreadsCompleted{0};
  int pops = 0;

  nsCOMPtr<nsIRunnable> popper = NS_NewRunnableFunction("MWQPopper", [&]() {
    // int popsBefore = pops;
    // int allocsBefore = q.AllocatedBuffersStats().mCount;
    q.PopAll([&pops](const int& i) { ++pops; });
    // if (pops != popsBefore ||
    //     q.AllocatedBuffersStats().mCount != allocsBefore) {
    //   printf("%s threads=1+%d loops/thread=%d pops=%d "
    //          "buffers: live=%d (w %d) reusable=%d (w %d) "
    //          "alloc=%d (w %d)\n",
    //          aPrintPrefix,
    //          aWriterThreads,
    //          loops,
    //          pops,
    //          q.LiveBuffersStats().mCount,
    //          q.LiveBuffersStats().mWatermark,
    //          q.ReusableBuffersStats().mCount,
    //          q.ReusableBuffersStats().mWatermark,
    //          q.AllocatedBuffersStats().mCount,
    //          q.AllocatedBuffersStats().mWatermark);
    // }
  });
  // Cycle through reader threads.
  mozilla::Atomic<size_t> readerThread{0};

  double start = mozilla::ToSeconds(mozilla::DDNow());

  for (int k = 0; k < threads; k++) {
    // First `aReaderThreads` threads to pop, all others to push.
    if (k < aReaderThreads) {
      nsCOMPtr<nsIThread> t;
      nsresult rv = NS_NewNamedThread("MWQThread", getter_AddRefs(t));
      EXPECT_TRUE(NS_SUCCEEDED(rv));
      NS_ADDREF(array[k] = t);
    } else {
      nsCOMPtr<nsIThread> t;
      nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("MWQPusher", [&, k]() {
        // Give a bit of breathing space to construct other threads.
        PR_Sleep(PR_MillisecondsToInterval(100));

        for (int i = 0; i < loops; ++i) {
          if (q.Push(k * threads + i) && aReaderThreads != 0) {
            // Run a popper task every time we push the last element of a
            // buffer.
            array[++readerThread % aReaderThreads]->Dispatch(
                popper, nsIThread::DISPATCH_NORMAL);
          }
        }
        ++pushThreadsCompleted;
      });
      nsresult rv = NS_NewNamedThread("MWQThread", getter_AddRefs(t), r);
      EXPECT_TRUE(NS_SUCCEEDED(rv));
      NS_ADDREF(array[k] = t);
    }
  }

  for (int k = threads - 1; k >= 0; k--) {
    array[k]->Shutdown();
    NS_RELEASE(array[k]);
  }
  delete[] array;

  // There may be a few more elements that haven't been read yet.
  q.PopAll([&pops](const int& i) { ++pops; });
  const int pushes = aWriterThreads * loops;
  EXPECT_EQ(pushes, pops);
  q.PopAll([](const int& i) { EXPECT_TRUE(false); });

  double duration = mozilla::ToSeconds(mozilla::DDNow()) - start - 0.1;
  printf(
      "%s threads=%dw+%dr loops/thread=%d pushes=pops=%d duration=%fs "
      "pushes/s=%f buffers: live=%d (w %d) reusable=%d (w %d) "
      "alloc=%d (w %d)\n",
      aPrintPrefix, aWriterThreads, aReaderThreads, loops, pushes, duration,
      pushes / duration, q.LiveBuffersStats().mCount,
      q.LiveBuffersStats().mWatermark, q.ReusableBuffersStats().mCount,
      q.ReusableBuffersStats().mWatermark, q.AllocatedBuffersStats().mCount,
      q.AllocatedBuffersStats().mWatermark);
}

// skip test on windows10-aarch64 due to unexpected test timeout at
// MultiWriterSingleReader, bug 1526001
#if !defined(_M_ARM64)
TEST(MultiWriterQueue, MultiWriterSingleReader)
{
  // Small BufferSize, to exercize the buffer management code.
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      1, 0, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      1, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      2, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      3, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      4, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      5, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      6, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      7, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      8, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      9, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      10, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      16, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      32, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_None>>(
      64, 1, 2 * 1024 * 1024, "MultiWriterQueue<int, 10, Locking_None>");

  // A more real-life buffer size.
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, MultiWriterQueueDefaultBufferSize,
                       MultiWriterQueueReaderLocking_None>>(
      64, 1, 2 * 1024 * 1024,
      "MultiWriterQueue<int, DefaultBufferSize, Locking_None>");

  // DEBUG-mode thread-safety checks should make the following (multi-reader
  // with no locking) crash; uncomment to verify.
  // TestMultiWriterQueueMT<
  //   MultiWriterQueue<int, MultiWriterQueueDefaultBufferSize,
  //   MultiWriterQueueReaderLocking_None>>(64, 2, 2*1024*1024);
}
#endif

// skip test on windows10-aarch64 due to unexpected test timeout at
// MultiWriterMultiReade, bug 1526001
#if !defined(_M_ARM64)
TEST(MultiWriterQueue, MultiWriterMultiReader)
{
  static_assert(
      mozilla::IsSame<MultiWriterQueue<int, 10>,
                      MultiWriterQueue<
                          int, 10, MultiWriterQueueReaderLocking_Mutex>>::value,
      "MultiWriterQueue reader locking should use Mutex by default");

  // Small BufferSize, to exercize the buffer management code.
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      1, 2, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      2, 2, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      3, 2, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      4, 2, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      5, 2, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      6, 2, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      7, 2, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      8, 2, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      9, 2, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      10, 4, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      16, 8, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      32, 16, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, 10, MultiWriterQueueReaderLocking_Mutex>>(
      64, 32, 1024 * 1024, "MultiWriterQueue<int, 10, Locking_Mutex>");

  // A more real-life buffer size.
  TestMultiWriterQueueMT<
      MultiWriterQueue<int, MultiWriterQueueDefaultBufferSize,
                       MultiWriterQueueReaderLocking_Mutex>>(
      64, 32, 1024 * 1024,
      "MultiWriterQueue<int, DefaultBufferSize, Locking_Mutex>");
}
#endif

// Single-threaded use only.
struct DequeWrapperST {
  nsDeque mDQ;

  bool Push(int i) {
    mDQ.PushFront(reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
    return true;
  }
  template <typename F>
  void PopAll(F&& aF) {
    while (mDQ.GetSize() != 0) {
      int i = static_cast<int>(reinterpret_cast<uintptr_t>(mDQ.Pop()));
      aF(i);
    }
  }

  struct CountAndWatermark {
    int mCount = 0;
    int mWatermark = 0;
  } mLiveBuffersStats, mReusableBuffersStats, mAllocatedBuffersStats;

  CountAndWatermark LiveBuffersStats() const { return mLiveBuffersStats; }
  CountAndWatermark ReusableBuffersStats() const {
    return mReusableBuffersStats;
  }
  CountAndWatermark AllocatedBuffersStats() const {
    return mAllocatedBuffersStats;
  }
};

// Multi-thread (atomic) writes allowed, make sure you don't pop unless writes
// can't happen.
struct DequeWrapperAW : DequeWrapperST {
  mozilla::Atomic<bool> mWriting{false};

  bool Push(int i) {
    while (!mWriting.compareExchange(false, true)) {
    }
    mDQ.PushFront(reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
    mWriting = false;
    return true;
  }
};

// Multi-thread writes allowed, make sure you don't pop unless writes can't
// happen.
struct DequeWrapperMW : DequeWrapperST {
  mozilla::Mutex mMutex;

  DequeWrapperMW() : mMutex("DequeWrapperMW/MT") {}

  bool Push(int i) {
    mozilla::MutexAutoLock lock(mMutex);
    mDQ.PushFront(reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
    return true;
  }
};

// Multi-thread read&writes allowed.
struct DequeWrapperMT : DequeWrapperMW {
  template <typename F>
  void PopAll(F&& aF) {
    while (mDQ.GetSize() != 0) {
      int i;
      {
        mozilla::MutexAutoLock lock(mMutex);
        i = static_cast<int>(reinterpret_cast<uintptr_t>(mDQ.Pop()));
      }
      aF(i);
    }
  }
};

TEST(MultiWriterQueue, nsDequeBenchmark)
{
  TestMultiWriterQueueMT<DequeWrapperST>(1, 0, 2 * 1024 * 1024,
                                         "DequeWrapperST ");

  TestMultiWriterQueueMT<DequeWrapperAW>(1, 0, 2 * 1024 * 1024,
                                         "DequeWrapperAW ");
  TestMultiWriterQueueMT<DequeWrapperMW>(1, 0, 2 * 1024 * 1024,
                                         "DequeWrapperMW ");
  TestMultiWriterQueueMT<DequeWrapperMT>(1, 0, 2 * 1024 * 1024,
                                         "DequeWrapperMT ");
  TestMultiWriterQueueMT<DequeWrapperMT>(1, 1, 2 * 1024 * 1024,
                                         "DequeWrapperMT ");

  TestMultiWriterQueueMT<DequeWrapperAW>(8, 0, 2 * 1024 * 1024,
                                         "DequeWrapperAW ");
  TestMultiWriterQueueMT<DequeWrapperMW>(8, 0, 2 * 1024 * 1024,
                                         "DequeWrapperMW ");
  TestMultiWriterQueueMT<DequeWrapperMT>(8, 0, 2 * 1024 * 1024,
                                         "DequeWrapperMT ");
  TestMultiWriterQueueMT<DequeWrapperMT>(8, 1, 2 * 1024 * 1024,
                                         "DequeWrapperMT ");
}
