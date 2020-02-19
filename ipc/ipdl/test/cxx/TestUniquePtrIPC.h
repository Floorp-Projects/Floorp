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
  TestUniquePtrIPCParent() { MOZ_COUNT_CTOR(TestUniquePtrIPCParent); }
  virtual ~TestUniquePtrIPCParent() { MOZ_COUNT_DTOR(TestUniquePtrIPCParent); }

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
  TestUniquePtrIPCChild() { MOZ_COUNT_CTOR(TestUniquePtrIPCChild); }
  virtual ~TestUniquePtrIPCChild() { MOZ_COUNT_DTOR(TestUniquePtrIPCChild); }

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
