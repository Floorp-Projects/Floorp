/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test what happens when a sync message fails.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestSyncErrorChild.h"
#include "mozilla/_ipdltest/PTestSyncErrorParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestSyncErrorParent : public PTestSyncErrorParent {
  NS_INLINE_DECL_REFCOUNTING(TestSyncErrorParent, override)
 private:
  IPCResult RecvError() final override { return IPC_TEST_FAIL(this); }

  ~TestSyncErrorParent() = default;
};

class TestSyncErrorChild : public PTestSyncErrorChild {
  NS_INLINE_DECL_REFCOUNTING(TestSyncErrorChild, override)
 private:
  IPCResult RecvStart() final override {
    EXPECT_FALSE(SendError()) << "Error() should have return false";

    Close();

    return IPC_OK();
  }
  ~TestSyncErrorChild() = default;
};

IPDL_TEST(TestSyncError) { EXPECT_TRUE(mActor->SendStart()); }

}  // namespace mozilla::_ipdltest
