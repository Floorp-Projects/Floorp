/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/TestBasicChild.h"
#include "mozilla/_ipdltest/TestBasicParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

IPCResult TestBasicChild::RecvHello() {
  EXPECT_TRUE(CanSend());
  Close();
  EXPECT_FALSE(CanSend());
  return IPC_OK();
}

IPDL_TEST(TestBasic) {
  bool ok = mActor->SendHello();
  ASSERT_TRUE(ok);
}

}  // namespace mozilla::_ipdltest
