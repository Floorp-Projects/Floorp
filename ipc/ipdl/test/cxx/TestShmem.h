#ifndef mozilla__ipdltest_TestShmem_h
#define mozilla__ipdltest_TestShmem_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestShmemParent.h"
#include "mozilla/_ipdltest/PTestShmemChild.h"

namespace mozilla {
namespace _ipdltest {

class TestShmemParent : public PTestShmemParent {
  friend class PTestShmemParent;

 public:
  TestShmemParent() {}
  virtual ~TestShmemParent() {}

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  mozilla::ipc::IPCResult RecvTake(Shmem&& mem, Shmem&& unsafe,
                                   const size_t& expectedSize);

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }
};

class TestShmemChild : public PTestShmemChild {
  friend class PTestShmemChild;

 public:
  TestShmemChild() {}
  virtual ~TestShmemChild() {}

 protected:
  mozilla::ipc::IPCResult RecvGive(Shmem&& mem, Shmem&& unsafe,
                                   const size_t& expectedSize);

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestShmem_h
