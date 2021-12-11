#ifndef mozilla__ipdltest_TestShutdown_h
#define mozilla__ipdltest_TestShutdown_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestShutdownParent.h"
#include "mozilla/_ipdltest/PTestShutdownChild.h"

#include "mozilla/_ipdltest/PTestShutdownSubParent.h"
#include "mozilla/_ipdltest/PTestShutdownSubChild.h"

#include "mozilla/_ipdltest/PTestShutdownSubsubParent.h"
#include "mozilla/_ipdltest/PTestShutdownSubsubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Parent side

class TestShutdownSubsubParent : public PTestShutdownSubsubParent {
 public:
  explicit TestShutdownSubsubParent(bool expectParentDeleted)
      : mExpectParentDeleted(expectParentDeleted) {}

  virtual ~TestShutdownSubsubParent() {}

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override;

 private:
  bool mExpectParentDeleted;
};

class TestShutdownSubParent : public PTestShutdownSubParent {
  friend class PTestShutdownSubParent;

 public:
  explicit TestShutdownSubParent(bool expectCrash)
      : mExpectCrash(expectCrash), mDeletedCount(0) {}

  virtual ~TestShutdownSubParent() {
    if (2 != mDeletedCount) fail("managees outliving manager!");
  }

 protected:
  mozilla::ipc::IPCResult AnswerStackFrame() {
    if (!CallStackFrame()) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }

  PTestShutdownSubsubParent* AllocPTestShutdownSubsubParent(
      const bool& expectParentDelete) {
    return new TestShutdownSubsubParent(expectParentDelete);
  }

  bool DeallocPTestShutdownSubsubParent(PTestShutdownSubsubParent* actor) {
    delete actor;
    ++mDeletedCount;
    return true;
  }

  virtual void ActorDestroy(ActorDestroyReason why) override;

 private:
  bool mExpectCrash;
  int mDeletedCount;
};

class TestShutdownParent : public PTestShutdownParent {
  friend class PTestShutdownParent;

 public:
  TestShutdownParent() {}
  virtual ~TestShutdownParent() {}

  static bool RunTestInProcesses() { return true; }
  // FIXME/bug 703323 Could work if modified
  static bool RunTestInThreads() { return false; }

  void Main();

 protected:
  mozilla::ipc::IPCResult RecvSync() { return IPC_OK(); }

  PTestShutdownSubParent* AllocPTestShutdownSubParent(const bool& expectCrash) {
    return new TestShutdownSubParent(expectCrash);
  }

  bool DeallocPTestShutdownSubParent(PTestShutdownSubParent* actor) {
    delete actor;
    return true;
  }

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

//-----------------------------------------------------------------------------
// Child side

class TestShutdownSubsubChild : public PTestShutdownSubsubChild {
 public:
  explicit TestShutdownSubsubChild(bool expectParentDeleted)
      : mExpectParentDeleted(expectParentDeleted) {}
  virtual ~TestShutdownSubsubChild() {}

 protected:
  virtual void ActorDestroy(ActorDestroyReason why) override;

 private:
  bool mExpectParentDeleted;
};

class TestShutdownSubChild : public PTestShutdownSubChild {
  friend class PTestShutdownSubChild;

 public:
  explicit TestShutdownSubChild(bool expectCrash) : mExpectCrash(expectCrash) {}

  virtual ~TestShutdownSubChild() {}

 protected:
  mozilla::ipc::IPCResult AnswerStackFrame();

  PTestShutdownSubsubChild* AllocPTestShutdownSubsubChild(
      const bool& expectParentDelete) {
    return new TestShutdownSubsubChild(expectParentDelete);
  }

  bool DeallocPTestShutdownSubsubChild(PTestShutdownSubsubChild* actor) {
    delete actor;
    return true;
  }

  virtual void ActorDestroy(ActorDestroyReason why) override;

 private:
  bool mExpectCrash;
};

class TestShutdownChild : public PTestShutdownChild {
  friend class PTestShutdownChild;

 public:
  TestShutdownChild() {}
  virtual ~TestShutdownChild() {}

 protected:
  mozilla::ipc::IPCResult RecvStart();

  PTestShutdownSubChild* AllocPTestShutdownSubChild(const bool& expectCrash) {
    return new TestShutdownSubChild(expectCrash);
  }

  bool DeallocPTestShutdownSubChild(PTestShutdownSubChild* actor) {
    delete actor;
    return true;
  }

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestShutdown_h
