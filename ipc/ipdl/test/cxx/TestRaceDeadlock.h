#ifndef mozilla__ipdltest_TestRaceDeadlock_h
#define mozilla__ipdltest_TestRaceDeadlock_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRaceDeadlockParent.h"
#include "mozilla/_ipdltest/PTestRaceDeadlockChild.h"

namespace mozilla {
namespace _ipdltest {

class TestRaceDeadlockParent : public PTestRaceDeadlockParent {
  friend class PTestRaceDeadlockParent;

 public:
  TestRaceDeadlockParent();
  virtual ~TestRaceDeadlockParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  virtual bool ShouldContinueFromReplyTimeout() override;

  void Test1();

  mozilla::ipc::IPCResult RecvStartRace();
  mozilla::ipc::IPCResult AnswerLose();

  virtual mozilla::ipc::RacyInterruptPolicy MediateInterruptRace(
      const MessageInfo& parent, const MessageInfo& child) override;

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }
};

class TestRaceDeadlockChild : public PTestRaceDeadlockChild {
  friend class PTestRaceDeadlockChild;

 public:
  TestRaceDeadlockChild();
  virtual ~TestRaceDeadlockChild();

 protected:
  mozilla::ipc::IPCResult RecvStartRace();

  mozilla::ipc::IPCResult AnswerWin();

  mozilla::ipc::IPCResult AnswerRpc();

  virtual mozilla::ipc::RacyInterruptPolicy MediateInterruptRace(
      const MessageInfo& parent, const MessageInfo& child) override;

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestRaceDeadlock_h
