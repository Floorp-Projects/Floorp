#ifndef mozilla__ipdltest_TestRaceDeferral_h
#define mozilla__ipdltest_TestRaceDeferral_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRaceDeferralParent.h"
#include "mozilla/_ipdltest/PTestRaceDeferralChild.h"

namespace mozilla {
namespace _ipdltest {

class TestRaceDeferralParent : public PTestRaceDeferralParent {
  friend class PTestRaceDeferralParent;

 public:
  TestRaceDeferralParent();
  virtual ~TestRaceDeferralParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  void Test1();

  mozilla::ipc::IPCResult AnswerLose();

  virtual mozilla::ipc::RacyInterruptPolicy MediateInterruptRace(
      const MessageInfo& parent, const MessageInfo& child) override;

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }

  bool mProcessedLose;
};

class TestRaceDeferralChild : public PTestRaceDeferralChild {
  friend class PTestRaceDeferralChild;

 public:
  TestRaceDeferralChild();
  virtual ~TestRaceDeferralChild();

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

#endif  // ifndef mozilla__ipdltest_TestRaceDeferral_h
