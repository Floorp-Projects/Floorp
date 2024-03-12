#ifndef mozilla__ipdltest_TestBadActor_h
#define mozilla__ipdltest_TestBadActor_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestBadActorParent.h"
#include "mozilla/_ipdltest/PTestBadActorChild.h"

#include "mozilla/_ipdltest/PTestBadActorSubParent.h"
#include "mozilla/_ipdltest/PTestBadActorSubChild.h"

namespace mozilla {
namespace _ipdltest {

class TestBadActorParent : public PTestBadActorParent {
  friend class PTestBadActorParent;

 public:
  TestBadActorParent() {}
  virtual ~TestBadActorParent() {}

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return false; }

  void Main();

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (AbnormalShutdown != why) fail("unexpected destruction");
    passed("ok");
    QuitParent();
  }

  virtual void HandleFatalError(const char* aErrorMsg) override;

  PTestBadActorSubParent* AllocPTestBadActorSubParent();

  bool DeallocPTestBadActorSubParent(PTestBadActorSubParent* actor) {
    delete actor;
    return true;
  }
};

class TestBadActorSubParent : public PTestBadActorSubParent {
  friend class PTestBadActorSubParent;

 public:
  TestBadActorSubParent() {}
  virtual ~TestBadActorSubParent() {}

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override {}
  mozilla::ipc::IPCResult RecvPing();
};

class TestBadActorChild : public PTestBadActorChild {
  friend class PTestBadActorChild;

 public:
  TestBadActorChild() {}
  virtual ~TestBadActorChild() {}

 protected:
  virtual PTestBadActorSubChild* AllocPTestBadActorSubChild();

  virtual bool DeallocPTestBadActorSubChild(PTestBadActorSubChild* actor) {
    delete actor;
    return true;
  }

  virtual mozilla::ipc::IPCResult RecvPTestBadActorSubConstructor(
      PTestBadActorSubChild* actor);
};

class TestBadActorSubChild : public PTestBadActorSubChild {
 public:
  TestBadActorSubChild() {}
  virtual ~TestBadActorSubChild() {}
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // mozilla__ipdltest_TestBadActor_h
