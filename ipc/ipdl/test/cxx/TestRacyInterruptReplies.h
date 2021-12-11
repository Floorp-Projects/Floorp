#ifndef mozilla__ipdltest_TestRacyInterruptReplies_h
#define mozilla__ipdltest_TestRacyInterruptReplies_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRacyInterruptRepliesParent.h"
#include "mozilla/_ipdltest/PTestRacyInterruptRepliesChild.h"

namespace mozilla {
namespace _ipdltest {

class TestRacyInterruptRepliesParent : public PTestRacyInterruptRepliesParent {
  friend class PTestRacyInterruptRepliesParent;

 public:
  TestRacyInterruptRepliesParent();
  virtual ~TestRacyInterruptRepliesParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  mozilla::ipc::IPCResult RecvA_();

  mozilla::ipc::IPCResult Answer_R(int* replyNum);

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }

 private:
  int mReplyNum;
};

class TestRacyInterruptRepliesChild : public PTestRacyInterruptRepliesChild {
  friend class PTestRacyInterruptRepliesChild;

 public:
  TestRacyInterruptRepliesChild();
  virtual ~TestRacyInterruptRepliesChild();

 protected:
  mozilla::ipc::IPCResult AnswerR_(int* replyNum);

  mozilla::ipc::IPCResult RecvChildTest();

  mozilla::ipc::IPCResult Recv_A();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }

 private:
  int mReplyNum;
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestRacyInterruptReplies_h
