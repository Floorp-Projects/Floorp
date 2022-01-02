#ifndef mozilla__ipdltest_TestMultiMgrs_h
#define mozilla__ipdltest_TestMultiMgrs_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestMultiMgrsParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsChild.h"
#include "mozilla/_ipdltest/PTestMultiMgrsBottomParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsBottomChild.h"
#include "mozilla/_ipdltest/PTestMultiMgrsLeftParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsLeftChild.h"
#include "mozilla/_ipdltest/PTestMultiMgrsRightParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsRightChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Parent side
//

class TestMultiMgrsBottomParent : public PTestMultiMgrsBottomParent {
 public:
  TestMultiMgrsBottomParent() {}
  virtual ~TestMultiMgrsBottomParent() {}

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override {}
};

class TestMultiMgrsLeftParent : public PTestMultiMgrsLeftParent {
  friend class PTestMultiMgrsLeftParent;

 public:
  TestMultiMgrsLeftParent() {}
  virtual ~TestMultiMgrsLeftParent() {}

  bool HasChild(TestMultiMgrsBottomParent* c) {
    return ManagedPTestMultiMgrsBottomParent().Contains(c);
  }

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override {}

  PTestMultiMgrsBottomParent* AllocPTestMultiMgrsBottomParent() {
    return new TestMultiMgrsBottomParent();
  }

  bool DeallocPTestMultiMgrsBottomParent(PTestMultiMgrsBottomParent* actor) {
    delete actor;
    return true;
  }
};

class TestMultiMgrsRightParent : public PTestMultiMgrsRightParent {
  friend class PTestMultiMgrsRightParent;

 public:
  TestMultiMgrsRightParent() {}
  virtual ~TestMultiMgrsRightParent() {}

  bool HasChild(TestMultiMgrsBottomParent* c) {
    return ManagedPTestMultiMgrsBottomParent().Contains(c);
  }

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override {}

  PTestMultiMgrsBottomParent* AllocPTestMultiMgrsBottomParent() {
    return new TestMultiMgrsBottomParent();
  }

  bool DeallocPTestMultiMgrsBottomParent(PTestMultiMgrsBottomParent* actor) {
    delete actor;
    return true;
  }
};

class TestMultiMgrsParent : public PTestMultiMgrsParent {
  friend class PTestMultiMgrsParent;

 public:
  TestMultiMgrsParent() {}
  virtual ~TestMultiMgrsParent() {}

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  mozilla::ipc::IPCResult RecvOK();

  PTestMultiMgrsLeftParent* AllocPTestMultiMgrsLeftParent() {
    return new TestMultiMgrsLeftParent();
  }

  bool DeallocPTestMultiMgrsLeftParent(PTestMultiMgrsLeftParent* actor) {
    delete actor;
    return true;
  }

  PTestMultiMgrsRightParent* AllocPTestMultiMgrsRightParent() {
    return new TestMultiMgrsRightParent();
  }

  bool DeallocPTestMultiMgrsRightParent(PTestMultiMgrsRightParent* actor) {
    delete actor;
    return true;
  }

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }
};

//-----------------------------------------------------------------------------
// Child side
//

class TestMultiMgrsBottomChild : public PTestMultiMgrsBottomChild {
 public:
  TestMultiMgrsBottomChild() {}
  virtual ~TestMultiMgrsBottomChild() {}
};

class TestMultiMgrsLeftChild : public PTestMultiMgrsLeftChild {
  friend class PTestMultiMgrsLeftChild;

 public:
  TestMultiMgrsLeftChild() {}
  virtual ~TestMultiMgrsLeftChild() {}

  bool HasChild(PTestMultiMgrsBottomChild* c) {
    return ManagedPTestMultiMgrsBottomChild().Contains(c);
  }

 protected:
  virtual mozilla::ipc::IPCResult RecvPTestMultiMgrsBottomConstructor(
      PTestMultiMgrsBottomChild* actor) override;

  PTestMultiMgrsBottomChild* AllocPTestMultiMgrsBottomChild() {
    return new TestMultiMgrsBottomChild();
  }

  bool DeallocPTestMultiMgrsBottomChild(PTestMultiMgrsBottomChild* actor) {
    delete actor;
    return true;
  }
};

class TestMultiMgrsRightChild : public PTestMultiMgrsRightChild {
  friend class PTestMultiMgrsRightChild;

 public:
  TestMultiMgrsRightChild() {}
  virtual ~TestMultiMgrsRightChild() {}

  bool HasChild(PTestMultiMgrsBottomChild* c) {
    return ManagedPTestMultiMgrsBottomChild().Contains(c);
  }

 protected:
  virtual mozilla::ipc::IPCResult RecvPTestMultiMgrsBottomConstructor(
      PTestMultiMgrsBottomChild* actor) override;

  PTestMultiMgrsBottomChild* AllocPTestMultiMgrsBottomChild() {
    return new TestMultiMgrsBottomChild();
  }

  bool DeallocPTestMultiMgrsBottomChild(PTestMultiMgrsBottomChild* actor) {
    delete actor;
    return true;
  }
};

class TestMultiMgrsChild : public PTestMultiMgrsChild {
  friend class PTestMultiMgrsChild;

 public:
  TestMultiMgrsChild() {}
  virtual ~TestMultiMgrsChild() {}

  void Main();

  PTestMultiMgrsBottomChild* mBottomL;
  PTestMultiMgrsBottomChild* mBottomR;

 protected:
  mozilla::ipc::IPCResult RecvCheck();

  PTestMultiMgrsLeftChild* AllocPTestMultiMgrsLeftChild() {
    return new TestMultiMgrsLeftChild();
  }

  bool DeallocPTestMultiMgrsLeftChild(PTestMultiMgrsLeftChild* actor) {
    delete actor;
    return true;
  }

  PTestMultiMgrsRightChild* AllocPTestMultiMgrsRightChild() {
    return new TestMultiMgrsRightChild();
  }

  bool DeallocPTestMultiMgrsRightChild(PTestMultiMgrsRightChild* actor) {
    delete actor;
    return true;
  }

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitChild();
  }
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestMultiMgrs_h
