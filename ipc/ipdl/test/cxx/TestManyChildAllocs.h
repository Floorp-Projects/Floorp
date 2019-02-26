#ifndef mozilla__ipdltest_TestManyChildAllocs_h
#define mozilla__ipdltest_TestManyChildAllocs_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestManyChildAllocsParent.h"
#include "mozilla/_ipdltest/PTestManyChildAllocsChild.h"

#include "mozilla/_ipdltest/PTestManyChildAllocsSubParent.h"
#include "mozilla/_ipdltest/PTestManyChildAllocsSubChild.h"

namespace mozilla {
namespace _ipdltest {

// top-level protocol

class TestManyChildAllocsParent : public PTestManyChildAllocsParent {
  friend class PTestManyChildAllocsParent;

 public:
  TestManyChildAllocsParent();
  virtual ~TestManyChildAllocsParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  mozilla::ipc::IPCResult RecvDone();
  bool DeallocPTestManyChildAllocsSubParent(PTestManyChildAllocsSubParent* __a);
  PTestManyChildAllocsSubParent* AllocPTestManyChildAllocsSubParent();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }
};

class TestManyChildAllocsChild : public PTestManyChildAllocsChild {
  friend class PTestManyChildAllocsChild;

 public:
  TestManyChildAllocsChild();
  virtual ~TestManyChildAllocsChild();

 protected:
  mozilla::ipc::IPCResult RecvGo();
  bool DeallocPTestManyChildAllocsSubChild(PTestManyChildAllocsSubChild* __a);
  PTestManyChildAllocsSubChild* AllocPTestManyChildAllocsSubChild();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }
};

// do-nothing sub-protocol actors

class TestManyChildAllocsSubParent : public PTestManyChildAllocsSubParent {
  friend class PTestManyChildAllocsSubParent;

 public:
  TestManyChildAllocsSubParent() {}
  virtual ~TestManyChildAllocsSubParent() {}

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override {}
  mozilla::ipc::IPCResult RecvHello() { return IPC_OK(); }
};

class TestManyChildAllocsSubChild : public PTestManyChildAllocsSubChild {
 public:
  TestManyChildAllocsSubChild() {}
  virtual ~TestManyChildAllocsSubChild() {}
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestManyChildAllocs_h
