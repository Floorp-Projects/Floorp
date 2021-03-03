#include "mozilla/dom/SynchronizedSharedMemory.h"
#include "mozilla/dom/SynchronizedSharedMemoryRemoteInfo.h"

#include "gtest/gtest.h"

// Dead-simple test for the shared memory fallback: As long as CreateNew()
// returns `Nothing`, there really isn't anything else that can be done with
// the API

namespace mozilla::dom {

// Ensure that we can't create SynchronizedSharedMemory
TEST(SynchronizedSharedMemoryTest, NoCreate)
{
  auto maybeSharedMemory = SynchronizedSharedMemory<int>::CreateNew();
  ASSERT_TRUE(!maybeSharedMemory);
}

}  // namespace mozilla::dom
