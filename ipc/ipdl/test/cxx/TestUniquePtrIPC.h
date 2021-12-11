/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TestUniquePtrIPC_h
#define mozilla_TestUniquePtrIPC_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestUniquePtrIPCParent.h"
#include "mozilla/_ipdltest/PTestUniquePtrIPCChild.h"

namespace mozilla {
namespace _ipdltest {

class TestUniquePtrIPCParent : public PTestUniquePtrIPCParent {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(TestUniquePtrIPCParent)
  MOZ_COUNTED_DTOR_OVERRIDE(TestUniquePtrIPCParent)

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return false; }

  void Main();

  bool ShouldContinueFromReplyTimeout() override { return false; }

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) {
      fail("Abnormal shutdown of parent");
    }
    passed("ok");
    QuitParent();
  }
};

class TestUniquePtrIPCChild : public PTestUniquePtrIPCChild {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(TestUniquePtrIPCChild)
  MOZ_COUNTED_DTOR_OVERRIDE(TestUniquePtrIPCChild)

  mozilla::ipc::IPCResult RecvTestMessage(UniquePtr<int>&& aA1,
                                          UniquePtr<DummyStruct>&& aA2,
                                          const DummyStruct& aA3,
                                          UniquePtr<int>&& aA4);

  mozilla::ipc::IPCResult RecvTestSendReference(UniquePtr<DummyStruct>&& aA);

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) {
      fail("Abnormal shutdown of child");
    }
    QuitChild();
  }
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // mozilla_TestUniquePtrIPC_h
