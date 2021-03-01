#include "mozilla/dom/SynchronizedSharedMemory.h"
#include "mozilla/dom/SynchronizedSharedMemoryRemoteInfo.h"
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <windows.h>

#include "gtest/gtest.h"

namespace mozilla::dom {

// Helpers that have private access to SynchronizedSharedMemory
// (As it turns out, creating an entire IPDL actor tree just for a test is
//  hard)
class SynchronizedSharedMemoryTestFriend {
 public:
  template <typename T>
  static bool GenerateRemoteInfo(SynchronizedSharedMemory<T>& aSharedMemory,
                                 SynchronizedSharedMemoryRemoteInfo* aOut) {
    return aSharedMemory.GenerateTestRemoteInfo(aOut);
  }
};

// A basic test that ensures that reading/writing even works
TEST(SynchronizedSharedMemoryTest, Basic)
{
  auto maybeLocalObject = SynchronizedSharedMemory<int>::CreateNew(0);
  ASSERT_TRUE(maybeLocalObject);

  SynchronizedSharedMemoryRemoteInfo remoteInfo;
  ASSERT_TRUE(SynchronizedSharedMemoryTestFriend::GenerateRemoteInfo(
      *maybeLocalObject, &remoteInfo));

  auto maybeRemoteObject =
      SynchronizedSharedMemory<int>::CreateFromRemoteInfo(remoteInfo);
  ASSERT_TRUE(maybeRemoteObject);

  (*maybeLocalObject).RunWithLock([](int* ptr, int value) { *ptr = value; }, 5);

  int value = 0;

  (*maybeRemoteObject).RunWithLock([&value](int* ptr) { value = *ptr; });

  ASSERT_EQ(value, 5);
}

// Multi-threaded test (to try and simulate multi-process use case)
//
// Create a shared object, create a bunch of threads each with their own
// remote handle to the object, and have each thread perform an action
// that would be unsafe if locking were not working properly.
//
// In this case, the threads are incrementing the elements of an array by a
// a constant value, and the array is intentionally large enough that it can't
// fit in a cache line.
//
// Each thread will assert that it never sees a version of the array where all
// the elements aren't equal, and the main thread won't stop the test until it's
// seen a minimum amount of changes made to the entire array without any
// inconsistency.
TEST(SynchronizedSharedMemoryTest, Multithreaded)
{
  struct TestStruct {
    // Big enough that it can't fit into a single cache line (512 bits on x86)
    std::array<uint64_t, 16> data;

    void WriteValueToAll(uint64_t value) {
      for (auto& x : data) {
        x = value;
      }
    }

    void AddValueToAll(uint64_t value) {
      for (auto& x : data) {
        x += value;
      }
    }

    void AssertAllEqual(uint64_t value) const {
      for (size_t i = 0; i < (data.size() - 1); ++i) {
        ASSERT_EQ(data[i], data[i + 1]);
      }
    }

    void AssertEq(const TestStruct& other) const {
      for (size_t i = 0; i < data.size(); ++i) {
        ASSERT_EQ(data[i], other.data[i]);
      }
    }
  };

  using SharedType = SynchronizedSharedMemory<TestStruct>;

  // Some useful settings for the test

  // Number of worker threads to spin up
  constexpr size_t kNumThreads = 4;
  // Value to start each element at
  constexpr uint64_t kTestStartValue = 0x1234;
  // Value to increment each element by
  constexpr uint64_t kTestIncrementAmount = 0x5678;
  // Number of changes the main thread must see before passing
  constexpr uint32_t kMinimumChangesToWitness = 10000;

  // Initial value to test that the implementation correctly handles copy
  // construction of the contained object
  constexpr TestStruct kInitialStruct = {0, 1, 2,  3,  4,  5,  6,  7,
                                         8, 9, 10, 11, 12, 13, 14, 15};

  // Create the shared object, ensure it constructed properly, set all elements
  // to kTestStartValue, and assert that it worked

  auto maybeLocalObject = SharedType::CreateNew(kInitialStruct);
  ASSERT_TRUE(maybeLocalObject);

  maybeLocalObject->RunWithLock([&](TestStruct* shared) {
    shared->AssertEq(kInitialStruct);
    shared->WriteValueToAll(kTestStartValue);
    shared->AssertAllEqual(kTestStartValue);
  });

  // This variable will be set on the main thread when it has seen enough
  // changes happen, causing the threads to end
  std::atomic_bool done{false};

  // Fire up the worker threads, each with their own "remote info", and have
  // them add a constant to each element in the array while holding the lock
  // If any thread ever witnesses a situation where all elements aren't equal,
  // the test is failed
  std::array<std::thread, kNumThreads> workerThreads;
  for (auto& t : workerThreads) {
    SynchronizedSharedMemoryRemoteInfo remoteInfo;
    ASSERT_TRUE(SynchronizedSharedMemoryTestFriend::GenerateRemoteInfo(
        *maybeLocalObject, &remoteInfo));

    t = std::thread([remoteInfo, &done] {
      auto maybeRemoteObject = SharedType::CreateFromRemoteInfo(remoteInfo);
      ASSERT_TRUE(maybeRemoteObject);

      while (!done.load(std::memory_order_relaxed)) {
        maybeRemoteObject->RunWithLock([&](TestStruct* shared) {
          shared->AssertAllEqual(shared->data[0]);
          shared->AddValueToAll(kTestIncrementAmount);
          shared->AssertAllEqual(shared->data[0]);
        });
      }
    });
  }

  // The main thread will just keep locking the mutex and sampling the state
  // of the array, ensuring all elements are equal and keeping track of the
  // number of times it's seen the value change.
  // Once it's seen `kMinimumChangesToWitness` changes occur, it will set
  // `done` to signal all the threads to stop and join with them.

  uint64_t numChangesWitnessed = 0;
  uint64_t lastValueWitnessed = kTestStartValue;

  while (numChangesWitnessed < kMinimumChangesToWitness) {
    // Grab the current value of the array and ensure they're all equal
    uint64_t valueWitnessed = 0;
    maybeLocalObject->RunWithLock([&](TestStruct* shared) {
      valueWitnessed = shared->data[0];
      shared->AssertAllEqual(valueWitnessed);
    });

    // If the value has changed, take note
    if (lastValueWitnessed != valueWitnessed) {
      ++numChangesWitnessed;
      lastValueWitnessed = valueWitnessed;
    }
  }

  // We passed! Shut it all down
  done.store(true, std::memory_order_relaxed);

  for (auto& t : workerThreads) {
    t.join();
  }
}

}  // namespace mozilla::dom
