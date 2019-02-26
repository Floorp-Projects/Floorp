#ifndef mozilla__ipdltest_TestRacyUndefer_h
#define mozilla__ipdltest_TestRacyUndefer_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRacyUndeferParent.h"
#include "mozilla/_ipdltest/PTestRacyUndeferChild.h"

namespace mozilla {
namespace _ipdltest {

class TestRacyUndeferParent : public PTestRacyUndeferParent {
  friend class PTestRacyUndeferParent;

 public:
  TestRacyUndeferParent();
  virtual ~TestRacyUndeferParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  mozilla::ipc::IPCResult AnswerSpam();

  mozilla::ipc::IPCResult AnswerRaceWinTwice();

  mozilla::ipc::IPCResult RecvDone();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }
};

class TestRacyUndeferChild : public PTestRacyUndeferChild {
  friend class PTestRacyUndeferChild;

 public:
  TestRacyUndeferChild();
  virtual ~TestRacyUndeferChild();

 protected:
  mozilla::ipc::IPCResult RecvStart();

  mozilla::ipc::IPCResult RecvAwakenSpam();
  mozilla::ipc::IPCResult RecvAwakenRaceWinTwice();

  mozilla::ipc::IPCResult AnswerRace();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestRacyUndefer_h
