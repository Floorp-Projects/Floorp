#ifndef mozilla__ipdltest_TestHangs_h
#define mozilla__ipdltest_TestHangs_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestHangsParent.h"
#include "mozilla/_ipdltest/PTestHangsChild.h"

namespace mozilla {
namespace _ipdltest {

class TestHangsParent : public PTestHangsParent {
  friend class PTestHangsParent;

 public:
  TestHangsParent();
  virtual ~TestHangsParent();

  static bool RunTestInProcesses() { return true; }

  // FIXME/bug 703320 Disabled because parent kills child proc, not
  //                  clear how that should work in threads.
  static bool RunTestInThreads() { return false; }

  void Main();

 protected:
  virtual bool ShouldContinueFromReplyTimeout() override;

  mozilla::ipc::IPCResult RecvNonce() { return IPC_OK(); }

  mozilla::ipc::IPCResult AnswerStackFrame();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (AbnormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }

  void CleanUp();

  bool mDetectedHang;
  int32_t mNumAnswerStackFrame;
};

class TestHangsChild : public PTestHangsChild {
  friend class PTestHangsChild;

 public:
  TestHangsChild();
  virtual ~TestHangsChild();

 protected:
  mozilla::ipc::IPCResult RecvStart() {
    if (!SendNonce()) fail("sending Nonce");
    return IPC_OK();
  }

  mozilla::ipc::IPCResult AnswerStackFrame() {
    if (CallStackFrame()) fail("should have failed");
    return IPC_OK();
  }

  mozilla::ipc::IPCResult AnswerHang();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (AbnormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestHangs_h
