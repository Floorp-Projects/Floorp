/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test UniquePtr IPC arguments.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestUniquePtrIPCChild.h"
#include "mozilla/_ipdltest/PTestUniquePtrIPCParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestUniquePtrIPCParent : public PTestUniquePtrIPCParent {
  NS_INLINE_DECL_REFCOUNTING(TestUniquePtrIPCParent, override)
 private:
  ~TestUniquePtrIPCParent() = default;
};

class TestUniquePtrIPCChild : public PTestUniquePtrIPCChild {
  NS_INLINE_DECL_REFCOUNTING(TestUniquePtrIPCChild, override)
 private:
  IPCResult RecvTestMessage(const UniquePtr<std::string>& aA1,
                            const UniquePtr<DummyStruct>& aA2,
                            const DummyStruct& aA3,
                            const UniquePtr<std::string>& aA4,
                            const DummyUnion& aA5) final override {
    EXPECT_TRUE(aA1) << "TestMessage received NULL aA1";
    EXPECT_TRUE(aA2) << "TestMessage received NULL aA2";
    EXPECT_FALSE(aA4)
        << "TestMessage received non-NULL when expecting NULL aA4";

    EXPECT_EQ(*aA1, std::string("unique"));
    EXPECT_EQ(aA2->x(), std::string("uniqueStruct"));
    EXPECT_EQ(aA3.x(), std::string("struct"));
    EXPECT_EQ(*aA5.get_string(), std::string("union"));

    return IPC_OK();
  }

  IPCResult RecvTestSendReference(
      const UniquePtr<DummyStruct>& aA) final override {
    EXPECT_TRUE(aA) << "TestSendReference received NULL item in child";
    EXPECT_EQ(aA->x(), std::string("reference"));

    Close();
    return IPC_OK();
  }

  ~TestUniquePtrIPCChild() = default;
};

IPDL_TEST(TestUniquePtrIPC) {
  UniquePtr<std::string> a1 = MakeUnique<std::string>("unique");
  UniquePtr<DummyStruct> a2 = MakeUnique<DummyStruct>("uniqueStruct");
  DummyStruct a3("struct");
  UniquePtr<std::string> a4;
  DummyUnion a5(MakeUnique<std::string>("union"));

  EXPECT_TRUE(mActor->SendTestMessage(a1, a2, a3, a4, a5));

  EXPECT_TRUE(a1)
      << "IPC arguments are passed by const reference and shouldn't be moved";
  EXPECT_TRUE(a2)
      << "IPC arguments are passed by const reference and shouldn't be moved";

  EXPECT_FALSE(a4) << "somehow turned null ptr into non-null by sending it";

  // Pass UniquePtr by reference
  UniquePtr<DummyStruct> b = MakeUnique<DummyStruct>("reference");

  EXPECT_TRUE(mActor->SendTestSendReference(b));
  EXPECT_TRUE(b)
      << "IPC arguments are passed by const reference and shouldn't be moved";
}

}  // namespace mozilla::_ipdltest
