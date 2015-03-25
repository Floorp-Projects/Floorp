#ifndef mozilla__ipdltest_TestRacyUndefer_h
#define mozilla__ipdltest_TestRacyUndefer_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRacyUndeferParent.h"
#include "mozilla/_ipdltest/PTestRacyUndeferChild.h"

namespace mozilla {
namespace _ipdltest {


class TestRacyUndeferParent :
    public PTestRacyUndeferParent
{
public:
    TestRacyUndeferParent();
    virtual ~TestRacyUndeferParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:    
    virtual bool AnswerSpam() override;

    virtual bool AnswerRaceWinTwice() override;

    virtual bool RecvDone() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestRacyUndeferChild :
    public PTestRacyUndeferChild
{
public:
    TestRacyUndeferChild();
    virtual ~TestRacyUndeferChild();

protected:
    virtual bool RecvStart() override;

    virtual bool RecvAwakenSpam() override;
    virtual bool RecvAwakenRaceWinTwice() override;

    virtual bool AnswerRace() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRacyUndefer_h
