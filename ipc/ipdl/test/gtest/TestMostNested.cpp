/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test nested sync message priorities.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestMostNestedChild.h"
#include "mozilla/_ipdltest/PTestMostNestedParent.h"

#if defined(XP_UNIX)
#  include <unistd.h>
#else
#  include <windows.h>
#endif

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestMostNestedChild : public PTestMostNestedChild {
  NS_INLINE_DECL_REFCOUNTING(TestMostNestedChild, override)
 private:
  IPCResult RecvStart() final override {
    EXPECT_TRUE(SendMsg1());
    EXPECT_TRUE(SendMsg2());

    Close();
    return IPC_OK();
  }

  IPCResult RecvStartInner() final override {
    EXPECT_TRUE(SendMsg3());
    EXPECT_TRUE(SendMsg4());

    return IPC_OK();
  }

  ~TestMostNestedChild() = default;
};

class TestMostNestedParent : public PTestMostNestedParent {
  NS_INLINE_DECL_REFCOUNTING(TestMostNestedParent, override)
 private:
  IPCResult RecvMsg1() final override {
    EXPECT_EQ(msg_num, 0);
    msg_num = 1;
    return IPC_OK();
  }

  IPCResult RecvMsg2() final override {
    EXPECT_EQ(msg_num, 1);
    msg_num = 2;

    EXPECT_TRUE(SendStartInner());

    return IPC_OK();
  }

  IPCResult RecvMsg3() final override {
    EXPECT_EQ(msg_num, 2);
    msg_num = 3;
    return IPC_OK();
  }

  IPCResult RecvMsg4() final override {
    EXPECT_EQ(msg_num, 3);
    msg_num = 4;
    return IPC_OK();
  }

  ~TestMostNestedParent() = default;

  int msg_num = 0;
};

// Can only run cross-process because nested sync messages have to come from the
// main thread.
IPDL_TEST_ON(CROSSPROCESS, TestMostNested) { EXPECT_TRUE(mActor->SendStart()); }

}  // namespace mozilla::_ipdltest
