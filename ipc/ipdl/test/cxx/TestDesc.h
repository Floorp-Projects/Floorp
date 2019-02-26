#ifndef mozilla_ipdltest_TestDesc_h
#define mozilla_ipdltest_TestDesc_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestDescParent.h"
#include "mozilla/_ipdltest/PTestDescChild.h"

#include "mozilla/_ipdltest/PTestDescSubParent.h"
#include "mozilla/_ipdltest/PTestDescSubChild.h"

#include "mozilla/_ipdltest/PTestDescSubsubParent.h"
#include "mozilla/_ipdltest/PTestDescSubsubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Top-level
//
class TestDescParent : public PTestDescParent {
  friend class PTestDescParent;

 public:
  TestDescParent() {}
  virtual ~TestDescParent() {}

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

  mozilla::ipc::IPCResult RecvOk(PTestDescSubsubParent* a);

 protected:
  PTestDescSubParent* AllocPTestDescSubParent(PTestDescSubsubParent*);
  bool DeallocPTestDescSubParent(PTestDescSubParent* actor);

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }
};

class TestDescChild : public PTestDescChild {
  friend class PTestDescChild;

 public:
  TestDescChild() {}
  virtual ~TestDescChild() {}

 protected:
  PTestDescSubChild* AllocPTestDescSubChild(PTestDescSubsubChild*);

  bool DeallocPTestDescSubChild(PTestDescSubChild* actor);

  mozilla::ipc::IPCResult RecvTest(PTestDescSubsubChild* a);

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }
};

//-----------------------------------------------------------------------------
// First descendent
//
class TestDescSubParent : public PTestDescSubParent {
  friend class PTestDescSubParent;

 public:
  TestDescSubParent() {}
  virtual ~TestDescSubParent() {}

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override {}
  PTestDescSubsubParent* AllocPTestDescSubsubParent();
  bool DeallocPTestDescSubsubParent(PTestDescSubsubParent* actor);
};

class TestDescSubChild : public PTestDescSubChild {
  friend class PTestDescSubChild;

 public:
  TestDescSubChild() {}
  virtual ~TestDescSubChild() {}

 protected:
  PTestDescSubsubChild* AllocPTestDescSubsubChild();
  bool DeallocPTestDescSubsubChild(PTestDescSubsubChild* actor);
};

//-----------------------------------------------------------------------------
// Grand-descendent
//
class TestDescSubsubParent : public PTestDescSubsubParent {
 public:
  TestDescSubsubParent() {}
  virtual ~TestDescSubsubParent() {}

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override {}
};

class TestDescSubsubChild : public PTestDescSubsubChild {
 public:
  TestDescSubsubChild() {}
  virtual ~TestDescSubsubChild() {}
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla_ipdltest_TestDesc_h
