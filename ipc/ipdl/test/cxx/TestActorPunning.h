#ifndef mozilla__ipdltest_TestActorPunning_h
#define mozilla__ipdltest_TestActorPunning_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestActorPunningParent.h"
#include "mozilla/_ipdltest/PTestActorPunningPunnedParent.h"
#include "mozilla/_ipdltest/PTestActorPunningSubParent.h"
#include "mozilla/_ipdltest/PTestActorPunningChild.h"
#include "mozilla/_ipdltest/PTestActorPunningPunnedChild.h"
#include "mozilla/_ipdltest/PTestActorPunningSubChild.h"

namespace mozilla {
namespace _ipdltest {

class TestActorPunningParent : public PTestActorPunningParent {
  friend class PTestActorPunningParent;

 public:
  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return false; }

  void Main();

 protected:
  PTestActorPunningPunnedParent* AllocPTestActorPunningPunnedParent();
  bool DeallocPTestActorPunningPunnedParent(PTestActorPunningPunnedParent* a);

  PTestActorPunningSubParent* AllocPTestActorPunningSubParent();
  bool DeallocPTestActorPunningSubParent(PTestActorPunningSubParent* a);

  mozilla::ipc::IPCResult RecvPun(PTestActorPunningSubParent* a,
                                  const Bad& bad);

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown == why) fail("should have died from error!");
    passed("ok");
    QuitParent();
  }

  virtual void HandleFatalError(const char* aErrorMsg) override;
};

class TestActorPunningPunnedParent : public PTestActorPunningPunnedParent {
 public:
  TestActorPunningPunnedParent() {}
  virtual ~TestActorPunningPunnedParent() {}

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override {}
};

class TestActorPunningSubParent : public PTestActorPunningSubParent {
 public:
  TestActorPunningSubParent() {}
  virtual ~TestActorPunningSubParent() {}

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override {}
};

class TestActorPunningChild : public PTestActorPunningChild {
  friend class PTestActorPunningChild;

 public:
  TestActorPunningChild() {}
  virtual ~TestActorPunningChild() {}

 protected:
  PTestActorPunningPunnedChild* AllocPTestActorPunningPunnedChild();
  bool DeallocPTestActorPunningPunnedChild(PTestActorPunningPunnedChild* a);

  PTestActorPunningSubChild* AllocPTestActorPunningSubChild();
  bool DeallocPTestActorPunningSubChild(PTestActorPunningSubChild* a);

  mozilla::ipc::IPCResult RecvStart();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    fail("should have been killed off!");
  }
};

class TestActorPunningPunnedChild : public PTestActorPunningPunnedChild {
 public:
  TestActorPunningPunnedChild() {}
  virtual ~TestActorPunningPunnedChild() {}
};

class TestActorPunningSubChild : public PTestActorPunningSubChild {
 public:
  TestActorPunningSubChild() {}
  virtual ~TestActorPunningSubChild() {}

  mozilla::ipc::IPCResult RecvBad();
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestActorPunning_h
