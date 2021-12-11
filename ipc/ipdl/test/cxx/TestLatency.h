#ifndef mozilla__ipdltest_TestLatency_h
#define mozilla__ipdltest_TestLatency_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestLatencyParent.h"
#include "mozilla/_ipdltest/PTestLatencyChild.h"

#include "mozilla/TimeStamp.h"

#define NR_TRIALS 10000
#define NR_SPAMS 25000

namespace mozilla {
namespace _ipdltest {

class TestLatencyParent : public PTestLatencyParent {
  friend class PTestLatencyParent;

 private:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

 public:
  TestLatencyParent();
  virtual ~TestLatencyParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  mozilla::ipc::IPCResult RecvPong();
  mozilla::ipc::IPCResult RecvPong5();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");

    passed(
        "\n"
        "  average #ping-pong/sec:        %g\n"
        "  average #ping5-pong5/sec:      %g\n"
        "  average #RPC call-answer/sec:  %g\n"
        "  average #spams/sec:            %g\n"
        "  pct. spams compressed away:    %g\n",
        double(NR_TRIALS) / mPPTimeTotal.ToSecondsSigDigits(),
        double(NR_TRIALS) / mPP5TimeTotal.ToSecondsSigDigits(),
        double(NR_TRIALS) / mRpcTimeTotal.ToSecondsSigDigits(),
        double(NR_SPAMS) / mSpamTimeTotal.ToSecondsSigDigits(),
        100.0 * (double(NR_SPAMS - mNumChildProcessedCompressedSpams) /
                 double(NR_SPAMS)));

    QuitParent();
  }

 private:
  void PingPongTrial();
  void Ping5Pong5Trial();
  void RpcTrials();
  void SpamTrial();
  void CompressedSpamTrial();
  void Exit();

  TimeStamp mStart;
  TimeDuration mPPTimeTotal;
  TimeDuration mPP5TimeTotal;
  TimeDuration mRpcTimeTotal;
  TimeDuration mSpamTimeTotal;

  int mPPTrialsToGo;
  int mPP5TrialsToGo;
  uint32_t mNumChildProcessedCompressedSpams;
  uint32_t mWhichPong5;
};

class TestLatencyChild : public PTestLatencyChild {
  friend class PTestLatencyChild;

 public:
  TestLatencyChild();
  virtual ~TestLatencyChild();

 protected:
  mozilla::ipc::IPCResult RecvPing();
  mozilla::ipc::IPCResult RecvPing5();
  mozilla::ipc::IPCResult AnswerRpc();
  mozilla::ipc::IPCResult RecvSpam();
  mozilla::ipc::IPCResult AnswerSynchro();
  mozilla::ipc::IPCResult RecvCompressedSpam(const uint32_t& seqno);
  mozilla::ipc::IPCResult AnswerSynchro2(uint32_t* lastSeqno,
                                         uint32_t* numMessagesDispatched);

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    QuitChild();
  }

  uint32_t mLastSeqno;
  uint32_t mNumProcessedCompressedSpams;
  uint32_t mWhichPing5;
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestLatency_h
