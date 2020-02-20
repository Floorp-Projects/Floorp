#ifndef mozilla__ipdltest_TestSelfManageRoot_h
#define mozilla__ipdltest_TestSelfManageRoot_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestSelfManageRootParent.h"
#include "mozilla/_ipdltest/PTestSelfManageRootChild.h"
#include "mozilla/_ipdltest/PTestSelfManageParent.h"
#include "mozilla/_ipdltest/PTestSelfManageChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Parent side

class TestSelfManageParent : public PTestSelfManageParent {
  friend class PTestSelfManageParent;

 public:
  MOZ_COUNTED_DEFAULT_CTOR(TestSelfManageParent)
  MOZ_COUNTED_DTOR_OVERRIDE(TestSelfManageParent)

  ActorDestroyReason mWhy;

 protected:
  PTestSelfManageParent* AllocPTestSelfManageParent() {
    return new TestSelfManageParent();
  }

  bool DeallocPTestSelfManageParent(PTestSelfManageParent* a) { return true; }

  virtual void ActorDestroy(ActorDestroyReason why) override { mWhy = why; }
};

class TestSelfManageRootParent : public PTestSelfManageRootParent {
  friend class PTestSelfManageRootParent;

 public:
  MOZ_COUNTED_DEFAULT_CTOR(TestSelfManageRootParent)
  virtual ~TestSelfManageRootParent() {
    MOZ_COUNT_DTOR(TestSelfManageRootParent);
  }

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  PTestSelfManageParent* AllocPTestSelfManageParent() {
    return new TestSelfManageParent();
  }

  bool DeallocPTestSelfManageParent(PTestSelfManageParent* a) { return true; }

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }
};

//-----------------------------------------------------------------------------
// Child side

class TestSelfManageChild : public PTestSelfManageChild {
  friend class PTestSelfManageChild;

 public:
  MOZ_COUNTED_DEFAULT_CTOR(TestSelfManageChild)
  MOZ_COUNTED_DTOR_OVERRIDE(TestSelfManageChild)

 protected:
  PTestSelfManageChild* AllocPTestSelfManageChild() {
    return new TestSelfManageChild();
  }

  bool DeallocPTestSelfManageChild(PTestSelfManageChild* a) {
    delete a;
    return true;
  }

  virtual void ActorDestroy(ActorDestroyReason why) override {}
};

class TestSelfManageRootChild : public PTestSelfManageRootChild {
  friend class PTestSelfManageRootChild;

 public:
  MOZ_COUNTED_DEFAULT_CTOR(TestSelfManageRootChild)
  virtual ~TestSelfManageRootChild() {
    MOZ_COUNT_DTOR(TestSelfManageRootChild);
  }

  void Main();

 protected:
  PTestSelfManageChild* AllocPTestSelfManageChild() {
    return new TestSelfManageChild();
  }

  bool DeallocPTestSelfManageChild(PTestSelfManageChild* a) {
    delete a;
    return true;
  }

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestSelfManageRoot_h
