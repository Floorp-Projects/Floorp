#ifndef mozilla__ipdltest_TestRacyReentry_h
#define mozilla__ipdltest_TestRacyReentry_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRacyReentryParent.h"
#include "mozilla/_ipdltest/PTestRacyReentryChild.h"

namespace mozilla {
namespace _ipdltest {

class TestRacyReentryParent : public PTestRacyReentryParent {
  friend class PTestRacyReentryParent;

 public:
  TestRacyReentryParent();
  virtual ~TestRacyReentryParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  mozilla::ipc::IPCResult AnswerE();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }

  bool mRecvdE;
};

class TestRacyReentryChild : public PTestRacyReentryChild {
  friend class PTestRacyReentryChild;

 public:
  TestRacyReentryChild();
  virtual ~TestRacyReentryChild();

 protected:
  mozilla::ipc::IPCResult RecvStart();

  mozilla::ipc::IPCResult RecvN();

  mozilla::ipc::IPCResult AnswerH();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestRacyReentry_h
