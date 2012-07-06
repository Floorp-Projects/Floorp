#ifndef mozilla__ipdltest_TestLatency_h
#define mozilla__ipdltest_TestLatency_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestLatencyParent.h"
#include "mozilla/_ipdltest/PTestLatencyChild.h"

#include "mozilla/TimeStamp.h"

#define NR_TRIALS 10000
#define NR_SPAMS  50000

namespace mozilla {
namespace _ipdltest {

class TestLatencyParent :
    public PTestLatencyParent
{
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
    virtual bool RecvPong() MOZ_OVERRIDE;
    virtual bool RecvPong5() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  

        passed("\n"
               "  average #ping-pong/sec:        %g\n"
               "  average #ping5-pong5/sec:      %g\n"
               "  average #RPC call-answer/sec:  %g\n"
               "  average #spams/sec:            %g\n",
               double(NR_TRIALS) / mPPTimeTotal.ToSecondsSigDigits(),
               double(NR_TRIALS) / mPP5TimeTotal.ToSecondsSigDigits(),
               double(NR_TRIALS) / mRpcTimeTotal.ToSecondsSigDigits(),
               double(NR_SPAMS) / mSpamTimeTotal.ToSecondsSigDigits());

        QuitParent();
    }

private:
    void PingPongTrial();
    void Ping5Pong5Trial();
    void RpcTrials();
    void SpamTrial();
    void Exit();

    TimeStamp mStart;
    TimeDuration mPPTimeTotal;
    TimeDuration mPP5TimeTotal;
    TimeDuration mRpcTimeTotal;
    TimeDuration mSpamTimeTotal;

    int mPPTrialsToGo;
    int mPP5TrialsToGo;
    int mSpamsToGo;
};


class TestLatencyChild :
    public PTestLatencyChild
{
public:
    TestLatencyChild();
    virtual ~TestLatencyChild();

protected:
    virtual bool RecvPing() MOZ_OVERRIDE;
    virtual bool RecvPing5() MOZ_OVERRIDE;
    virtual bool AnswerRpc() MOZ_OVERRIDE;
    virtual bool RecvSpam() MOZ_OVERRIDE;
    virtual bool AnswerSynchro() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestLatency_h
