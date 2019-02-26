#ifndef mozilla_ipdltest_TestFailedCtor_h
#define mozilla_ipdltest_TestFailedCtor_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestFailedCtorParent.h"
#include "mozilla/_ipdltest/PTestFailedCtorChild.h"

#include "mozilla/_ipdltest/PTestFailedCtorSubParent.h"
#include "mozilla/_ipdltest/PTestFailedCtorSubChild.h"

#include "mozilla/_ipdltest/PTestFailedCtorSubsubParent.h"
#include "mozilla/_ipdltest/PTestFailedCtorSubsubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Top-level
//
class TestFailedCtorParent : public PTestFailedCtorParent {
  friend class PTestFailedCtorParent;

 public:
  TestFailedCtorParent() {}
  virtual ~TestFailedCtorParent() {}

  static bool RunTestInProcesses() { return true; }

  // FIXME/bug 703322 Disabled because child calls exit() to end
  //                  test, not clear how to handle failed ctor in
  //                  threaded mode.
  static bool RunTestInThreads() { return false; }

  void Main();

 protected:
  PTestFailedCtorSubParent* AllocPTestFailedCtorSubParent();
  bool DeallocPTestFailedCtorSubParent(PTestFailedCtorSubParent* actor);

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (AbnormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }
};

class TestFailedCtorChild : public PTestFailedCtorChild {
  friend class PTestFailedCtorChild;

 public:
  TestFailedCtorChild() {}
  virtual ~TestFailedCtorChild() {}

 protected:
  PTestFailedCtorSubChild* AllocPTestFailedCtorSubChild();

  mozilla::ipc::IPCResult AnswerPTestFailedCtorSubConstructor(
      PTestFailedCtorSubChild* actor) override;

  bool DeallocPTestFailedCtorSubChild(PTestFailedCtorSubChild* actor);

  virtual void ProcessingError(Result aCode, const char* aReason) override;

  virtual void ActorDestroy(ActorDestroyReason why) override {
    fail("should have _exit()ed");
  }
};

//-----------------------------------------------------------------------------
// First descendent
//
class TestFailedCtorSubsub;

class TestFailedCtorSubParent : public PTestFailedCtorSubParent {
  friend class PTestFailedCtorSubParent;

 public:
  TestFailedCtorSubParent() : mOne(nullptr), mTwo(nullptr), mThree(nullptr) {}
  virtual ~TestFailedCtorSubParent();

 protected:
  PTestFailedCtorSubsubParent* AllocPTestFailedCtorSubsubParent();

  bool DeallocPTestFailedCtorSubsubParent(PTestFailedCtorSubsubParent* actor);
  mozilla::ipc::IPCResult RecvSync() { return IPC_OK(); }

  virtual void ActorDestroy(ActorDestroyReason why) override;

  TestFailedCtorSubsub* mOne;
  TestFailedCtorSubsub* mTwo;
  TestFailedCtorSubsub* mThree;
};

class TestFailedCtorSubChild : public PTestFailedCtorSubChild {
  friend class PTestFailedCtorSubChild;

 public:
  TestFailedCtorSubChild() {}
  virtual ~TestFailedCtorSubChild() {}

 protected:
  PTestFailedCtorSubsubChild* AllocPTestFailedCtorSubsubChild();
  bool DeallocPTestFailedCtorSubsubChild(PTestFailedCtorSubsubChild* actor);

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

//-----------------------------------------------------------------------------
// Grand-descendent
//
class TestFailedCtorSubsub : public PTestFailedCtorSubsubParent,
                             public PTestFailedCtorSubsubChild {
 public:
  TestFailedCtorSubsub() : mWhy(ActorDestroyReason(-1)), mDealloced(false) {}
  virtual ~TestFailedCtorSubsub() {}

  virtual void ActorDestroy(ActorDestroyReason why) override { mWhy = why; }

  ActorDestroyReason mWhy;
  bool mDealloced;
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla_ipdltest_TestFailedCtor_h
