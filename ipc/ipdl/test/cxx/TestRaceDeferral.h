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

    virtual bool AnswerLose() MOZ_OVERRIDE;

    virtual mozilla::ipc::RacyInterruptPolicy
    MediateInterruptRace(const Message& parent, const Message& child) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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
    virtual bool RecvStartRace() MOZ_OVERRIDE;

    virtual bool AnswerWin() MOZ_OVERRIDE;

    virtual bool AnswerRpc() MOZ_OVERRIDE;

    virtual mozilla::ipc::RacyInterruptPolicy
    MediateInterruptRace(const Message& parent, const Message& child) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRaceDeferral_h
