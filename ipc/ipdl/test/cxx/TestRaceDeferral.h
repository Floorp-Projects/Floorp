#ifndef mozilla__ipdltest_TestRaceDeferral_h
#define mozilla__ipdltest_TestRaceDeferral_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRaceDeferralParent.h"
#include "mozilla/_ipdltest/PTestRaceDeferralChild.h"

namespace mozilla {
namespace _ipdltest {

class TestRaceDeferralParent :
    public PTestRaceDeferralParent
{
public:
    TestRaceDeferralParent();
    virtual ~TestRaceDeferralParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:
    void Test1();

    NS_OVERRIDE
    virtual bool AnswerLose();

    NS_OVERRIDE
    virtual mozilla::ipc::RPCChannel::RacyRPCPolicy
    MediateRPCRace(const Message& parent, const Message& child);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

    bool mProcessedLose;
};


class TestRaceDeferralChild :
    public PTestRaceDeferralChild
{
public:
    TestRaceDeferralChild();
    virtual ~TestRaceDeferralChild();

protected:
    NS_OVERRIDE
    virtual bool RecvStartRace();

    NS_OVERRIDE
    virtual bool AnswerWin();

    NS_OVERRIDE
    virtual bool AnswerRpc();

    NS_OVERRIDE
    virtual mozilla::ipc::RPCChannel::RacyRPCPolicy
    MediateRPCRace(const Message& parent, const Message& child);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRaceDeferral_h
