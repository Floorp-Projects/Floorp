#ifndef mozilla__ipdltest_TestInterruptRaces_h
#define mozilla__ipdltest_TestInterruptRaces_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestInterruptRacesParent.h"
#include "mozilla/_ipdltest/PTestInterruptRacesChild.h"

namespace mozilla {
namespace _ipdltest {

mozilla::ipc::RacyInterruptPolicy MediateRace(
    const mozilla::ipc::MessageChannel::MessageInfo& parent,
    const mozilla::ipc::MessageChannel::MessageInfo& child);

class TestInterruptRacesParent : public PTestInterruptRacesParent {
  friend class PTestInterruptRacesParent;

 public:
  TestInterruptRacesParent()
      : mHasReply(false), mChildHasReply(false), mAnsweredParent(false) {}
  virtual ~TestInterruptRacesParent() {}

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  mozilla::ipc::IPCResult RecvStartRace();

  mozilla::ipc::IPCResult AnswerRace(bool* hasRace);

  mozilla::ipc::IPCResult AnswerStackFrame();

  mozilla::ipc::IPCResult AnswerStackFrame3();

  mozilla::ipc::IPCResult AnswerParent();

  mozilla::ipc::IPCResult RecvGetAnsweredParent(bool* answeredParent);

  mozilla::ipc::RacyInterruptPolicy MediateInterruptRace(
      const MessageInfo& parent, const MessageInfo& child) override {
    return MediateRace(parent, child);
  }

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    if (!(mHasReply && mChildHasReply)) fail("both sides should have replies!");
    passed("ok");
    QuitParent();
  }

 private:
  void OnRaceTime();

  void Test2();
  void Test3();

  bool mHasReply;
  bool mChildHasReply;
  bool mAnsweredParent;
};

class TestInterruptRacesChild : public PTestInterruptRacesChild {
  friend class PTestInterruptRacesChild;

 public:
  TestInterruptRacesChild() : mHasReply(false) {}
  virtual ~TestInterruptRacesChild() {}

 protected:
  mozilla::ipc::IPCResult RecvStart();

  mozilla::ipc::IPCResult AnswerRace(bool* hasRace);

  mozilla::ipc::IPCResult AnswerStackFrame();

  mozilla::ipc::IPCResult AnswerStackFrame3();

  mozilla::ipc::IPCResult RecvWakeup();

  mozilla::ipc::IPCResult RecvWakeup3();

  mozilla::ipc::IPCResult AnswerChild();

  virtual mozilla::ipc::RacyInterruptPolicy MediateInterruptRace(
      const MessageInfo& parent, const MessageInfo& child) override {
    return MediateRace(parent, child);
  }

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }

 private:
  bool mHasReply;
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestInterruptRaces_h
