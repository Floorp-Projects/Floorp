/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#ifndef mozilla__ipdltest_TestEndpointBridgeMain_h
#define mozilla__ipdltest_TestEndpointBridgeMain_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestEndpointBridgeMainParent.h"
#include "mozilla/_ipdltest/PTestEndpointBridgeMainChild.h"

#include "mozilla/_ipdltest/PTestEndpointBridgeSubParent.h"
#include "mozilla/_ipdltest/PTestEndpointBridgeSubChild.h"

#include "mozilla/_ipdltest/PTestEndpointBridgeMainSubParent.h"
#include "mozilla/_ipdltest/PTestEndpointBridgeMainSubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// "Main" process
//
class TestEndpointBridgeMainParent : public PTestEndpointBridgeMainParent {
  friend class PTestEndpointBridgeMainParent;

 public:
  TestEndpointBridgeMainParent() {}
  virtual ~TestEndpointBridgeMainParent() {}

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return false; }

  void Main();

 protected:
  mozilla::ipc::IPCResult RecvBridged(
      mozilla::ipc::Endpoint<PTestEndpointBridgeMainSubParent>&& endpoint);

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

class TestEndpointBridgeMainSubParent
    : public PTestEndpointBridgeMainSubParent {
  friend class PTestEndpointBridgeMainSubParent;

 public:
  explicit TestEndpointBridgeMainSubParent() {}
  virtual ~TestEndpointBridgeMainSubParent() {}

 protected:
  mozilla::ipc::IPCResult RecvHello();
  mozilla::ipc::IPCResult RecvHelloSync();
  mozilla::ipc::IPCResult AnswerHelloRpc();

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

//-----------------------------------------------------------------------------
// "Sub" process --- child of "main"
//
class TestEndpointBridgeSubParent;

class TestEndpointBridgeMainChild : public PTestEndpointBridgeMainChild {
  friend class PTestEndpointBridgeMainChild;

 public:
  TestEndpointBridgeMainChild();
  virtual ~TestEndpointBridgeMainChild() {}

 protected:
  mozilla::ipc::IPCResult RecvStart();

  virtual void ActorDestroy(ActorDestroyReason why) override;

  IPDLUnitTestSubprocess* mSubprocess;
};

class TestEndpointBridgeSubParent : public PTestEndpointBridgeSubParent {
  friend class PTestEndpointBridgeSubParent;

 public:
  TestEndpointBridgeSubParent() {}
  virtual ~TestEndpointBridgeSubParent() {}

  void Main();

 protected:
  mozilla::ipc::IPCResult RecvBridgeEm();

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

//-----------------------------------------------------------------------------
// "Subsub" process --- child of "sub"
//
class TestEndpointBridgeSubChild : public PTestEndpointBridgeSubChild {
  friend class PTestEndpointBridgeSubChild;

 public:
  TestEndpointBridgeSubChild();
  virtual ~TestEndpointBridgeSubChild() {}

 protected:
  mozilla::ipc::IPCResult RecvPing();

  mozilla::ipc::IPCResult RecvBridged(
      Endpoint<PTestEndpointBridgeMainSubChild>&& endpoint);

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

class TestEndpointBridgeMainSubChild : public PTestEndpointBridgeMainSubChild {
  friend class PTestEndpointBridgeMainSubChild;

 public:
  explicit TestEndpointBridgeMainSubChild() : mGotHi(false) {}
  virtual ~TestEndpointBridgeMainSubChild() {}

 protected:
  mozilla::ipc::IPCResult RecvHi();
  mozilla::ipc::IPCResult AnswerHiRpc();

  virtual void ActorDestroy(ActorDestroyReason why) override;

  bool mGotHi;
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestEndpointBridgeMain_h
