/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestUniquePtrIPC.h"

namespace mozilla {
namespace _ipdltest {

// ---------------------------------------------------------------------------
// PARENT PROCESS
// ---------------------------------------------------------------------------

void TestUniquePtrIPCParent::Main() {
  UniquePtr<int> a1 = MakeUnique<int>(1);
  UniquePtr<DummyStruct> a2 = MakeUnique<DummyStruct>(2);
  DummyStruct a3(3);
  UniquePtr<int> a4;

  if (!SendTestMessage(std::move(a1), std::move(a2), a3, std::move(a4))) {
    fail("failed sending UniquePtr items");
  }

  if (a1 || a2) {
    fail("did not move TestMessage items in parent");
  }

  if (a4) {
    fail("somehow turned null ptr into non-null by sending it");
  }

  // Pass UniquePtr by reference
  UniquePtr<DummyStruct> b = MakeUnique<DummyStruct>(1);

  if (!SendTestSendReference(std::move(b))) {
    fail("failed sending UniquePtr by reference");
  }
  if (b) {
    fail("did not move UniquePtr sent by reference");
  }
}

// ---------------------------------------------------------------------------
// CHILD PROCESS
// ---------------------------------------------------------------------------

mozilla::ipc::IPCResult TestUniquePtrIPCChild::RecvTestMessage(
    UniquePtr<int>&& aA1, UniquePtr<DummyStruct>&& aA2, const DummyStruct& aA3,
    UniquePtr<int>&& aA4) {
  if ((!aA1) || (!aA2)) {
    fail("TestMessage received NULL items in child");
  }

  if (aA4) {
    fail("TestMessage received non-NULL when expecting NULL");
  }

  if ((*aA1 != 1) || (aA2->x() != 2) || (aA3.x() != 3)) {
    fail("TestMessage received incorrect items in child");
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult TestUniquePtrIPCChild::RecvTestSendReference(
    UniquePtr<DummyStruct>&& aA) {
  if (!aA) {
    fail("TestSendReference received NULL item in child");
  }

  if (*aA != 1) {
    fail("TestSendReference received incorrect item in child");
  }

  Close();
  return IPC_OK();
}

}  // namespace _ipdltest
}  // namespace mozilla
