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
    virtual bool AnswerSpam() MOZ_OVERRIDE;

    virtual bool AnswerRaceWinTwice() MOZ_OVERRIDE;

    virtual bool RecvDone() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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
    virtual bool RecvStart() MOZ_OVERRIDE;

    virtual bool RecvAwakenSpam() MOZ_OVERRIDE;
    virtual bool RecvAwakenRaceWinTwice() MOZ_OVERRIDE;

    virtual bool AnswerRace() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRacyUndefer_h
