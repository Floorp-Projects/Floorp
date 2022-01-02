#ifndef mozilla__ipdltest_TestSyncWakeup_h
#define mozilla__ipdltest_TestSyncWakeup_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestSyncWakeupParent.h"
#include "mozilla/_ipdltest/PTestSyncWakeupChild.h"

namespace mozilla {
namespace _ipdltest {

class TestSyncWakeupParent : public PTestSyncWakeupParent {
  friend class PTestSyncWakeupParent;

 public:
  TestSyncWakeupParent();
  virtual ~TestSyncWakeupParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  mozilla::ipc::IPCResult AnswerStackFrame();

  mozilla::ipc::IPCResult RecvSync1();

  mozilla::ipc::IPCResult RecvSync2();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }
};

class TestSyncWakeupChild : public PTestSyncWakeupChild {
  friend class PTestSyncWakeupChild;

 public:
  TestSyncWakeupChild();
  virtual ~TestSyncWakeupChild();

 protected:
  mozilla::ipc::IPCResult RecvStart();

  mozilla::ipc::IPCResult RecvNote1();

  mozilla::ipc::IPCResult AnswerStackFrame();

  mozilla::ipc::IPCResult RecvNote2();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }

 private:
  bool mDone;
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestSyncWakeup_h
