#ifndef mozilla__ipdltest_TestCancel_h
#define mozilla__ipdltest_TestCancel_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestCancelParent.h"
#include "mozilla/_ipdltest/PTestCancelChild.h"

namespace mozilla {
namespace _ipdltest {

class TestCancelParent : public PTestCancelParent {
 public:
  TestCancelParent();
  virtual ~TestCancelParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return false; }

  void Main();

  mozilla::ipc::IPCResult RecvDone1();
  mozilla::ipc::IPCResult RecvTest2_1();
  mozilla::ipc::IPCResult RecvStart3();
  mozilla::ipc::IPCResult RecvTest3_2();
  mozilla::ipc::IPCResult RecvDone();

  mozilla::ipc::IPCResult RecvCheckParent(uint32_t* reply);

  virtual void ActorDestroy(ActorDestroyReason why) override {
    passed("ok");
    QuitParent();
  }
};

class TestCancelChild : public PTestCancelChild {
 public:
  TestCancelChild();
  virtual ~TestCancelChild();

  mozilla::ipc::IPCResult RecvTest1_1();
  mozilla::ipc::IPCResult RecvStart2();
  mozilla::ipc::IPCResult RecvTest2_2();
  mozilla::ipc::IPCResult RecvTest3_1();

  mozilla::ipc::IPCResult RecvCheckChild(uint32_t* reply);

  virtual void ActorDestroy(ActorDestroyReason why) override { QuitChild(); }
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestCancel_h
