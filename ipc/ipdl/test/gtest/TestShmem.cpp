/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test sending and receiving Shmem values.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestShmemChild.h"
#include "mozilla/_ipdltest/PTestShmemParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestShmemParent : public PTestShmemParent {
  NS_INLINE_DECL_REFCOUNTING(TestShmemParent, override)
 private:
  IPCResult RecvTake(Shmem&& mem, Shmem&& unsafe,
                     const uint32_t& expectedSize) final override {
    EXPECT_EQ(mem.Size<char>(), expectedSize)
        << "expected shmem size " << expectedSize << ", but it has size "
        << mem.Size<char>();
    EXPECT_EQ(unsafe.Size<char>(), expectedSize)
        << "expected shmem size " << expectedSize << ", but it has size "
        << unsafe.Size<char>();

    EXPECT_FALSE(strcmp(mem.get<char>(), "And yourself!"))
        << "expected message was not written";
    EXPECT_FALSE(strcmp(unsafe.get<char>(), "And yourself!"))
        << "expected message was not written";

    EXPECT_TRUE(DeallocShmem(mem));
    EXPECT_TRUE(DeallocShmem(unsafe));

    Close();

    return IPC_OK();
  }

  ~TestShmemParent() = default;
};

class TestShmemChild : public PTestShmemChild {
  NS_INLINE_DECL_REFCOUNTING(TestShmemChild, override)
 private:
  IPCResult RecvGive(Shmem&& mem, Shmem&& unsafe,
                     const uint32_t& expectedSize) final override {
    EXPECT_EQ(mem.Size<char>(), expectedSize)
        << "expected shmem size " << expectedSize << ", but it has size "
        << mem.Size<char>();
    EXPECT_EQ(unsafe.Size<char>(), expectedSize)
        << "expected shmem size " << expectedSize << ", but it has size "
        << unsafe.Size<char>();

    EXPECT_FALSE(strcmp(mem.get<char>(), "Hello!"))
        << "expected message was not written";
    EXPECT_FALSE(strcmp(unsafe.get<char>(), "Hello!"))
        << "expected message was not written";

    char* unsafeptr = unsafe.get<char>();

    memcpy(mem.get<char>(), "And yourself!", sizeof("And yourself!"));
    memcpy(unsafeptr, "And yourself!", sizeof("And yourself!"));

    Shmem unsafecopy = unsafe;
    EXPECT_TRUE(SendTake(std::move(mem), std::move(unsafe), expectedSize));

    // these checks also shouldn't fail in the child
    char uc1 = *unsafeptr;
    (void)uc1;
    char uc2 = *unsafecopy.get<char>();
    (void)uc2;

    return IPC_OK();
  }

  ~TestShmemChild() = default;
};

IPDL_TEST(TestShmem) {
  Shmem mem;
  Shmem unsafe;

  uint32_t size = 12345;
  EXPECT_TRUE(mActor->AllocShmem(size, &mem)) << "can't alloc shmem";
  EXPECT_TRUE(mActor->AllocUnsafeShmem(size, &unsafe)) << "can't alloc shmem";

  EXPECT_EQ(mem.Size<char>(), size) << "shmem is wrong size: expected " << size
                                    << ", got " << mem.Size<char>();
  EXPECT_EQ(unsafe.Size<char>(), size)
      << "shmem is wrong size: expected " << size << ", got "
      << unsafe.Size<char>();

  char* ptr = mem.get<char>();
  memcpy(ptr, "Hello!", sizeof("Hello!"));

  char* unsafeptr = unsafe.get<char>();
  memcpy(unsafeptr, "Hello!", sizeof("Hello!"));

  Shmem unsafecopy = unsafe;
  EXPECT_TRUE(mActor->SendGive(std::move(mem), std::move(unsafe), size));

  // uncomment the following line for a (nondeterministic) surprise!
  // char c1 = *ptr;  (void)c1;

  // uncomment the following line for a deterministic surprise!
  // char c2 = *mem.get<char>(); (void)c2;

  // unsafe shmem gets rid of those checks
  char uc1 = *unsafeptr;
  (void)uc1;
  char uc2 = *unsafecopy.get<char>();
  (void)uc2;
}

}  // namespace mozilla::_ipdltest
