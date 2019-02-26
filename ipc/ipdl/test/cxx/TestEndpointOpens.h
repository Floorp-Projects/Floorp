/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#ifndef mozilla__ipdltest_TestEndpointOpens_h
#define mozilla__ipdltest_TestEndpointOpens_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestEndpointOpensParent.h"
#include "mozilla/_ipdltest/PTestEndpointOpensChild.h"

#include "mozilla/_ipdltest2/PTestEndpointOpensOpenedParent.h"
#include "mozilla/_ipdltest2/PTestEndpointOpensOpenedChild.h"

namespace mozilla {

// parent process

namespace _ipdltest {

class TestEndpointOpensParent : public PTestEndpointOpensParent {
  friend class PTestEndpointOpensParent;

 public:
  TestEndpointOpensParent() {}
  virtual ~TestEndpointOpensParent() {}

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return false; }

  void Main();

 protected:
  mozilla::ipc::IPCResult RecvStartSubprotocol(
      mozilla::ipc::Endpoint<PTestEndpointOpensOpenedParent>&& endpoint);

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

}  // namespace _ipdltest

namespace _ipdltest2 {

class TestEndpointOpensOpenedParent : public PTestEndpointOpensOpenedParent {
  friend class PTestEndpointOpensOpenedParent;

 public:
  explicit TestEndpointOpensOpenedParent() {}
  virtual ~TestEndpointOpensOpenedParent() {}

 protected:
  mozilla::ipc::IPCResult RecvHello();
  mozilla::ipc::IPCResult RecvHelloSync();
  mozilla::ipc::IPCResult AnswerHelloRpc();

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

}  // namespace _ipdltest2

// child process

namespace _ipdltest {

class TestEndpointOpensChild : public PTestEndpointOpensChild {
  friend class PTestEndpointOpensChild;

 public:
  TestEndpointOpensChild();
  virtual ~TestEndpointOpensChild() {}

 protected:
  mozilla::ipc::IPCResult RecvStart();

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

}  // namespace _ipdltest

namespace _ipdltest2 {

class TestEndpointOpensOpenedChild : public PTestEndpointOpensOpenedChild {
  friend class PTestEndpointOpensOpenedChild;

 public:
  explicit TestEndpointOpensOpenedChild() : mGotHi(false) {}
  virtual ~TestEndpointOpensOpenedChild() {}

 protected:
  mozilla::ipc::IPCResult RecvHi();
  mozilla::ipc::IPCResult AnswerHiRpc();

  virtual void ActorDestroy(ActorDestroyReason why) override;

  bool mGotHi;
};

}  // namespace _ipdltest2

}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestEndpointOpens_h
