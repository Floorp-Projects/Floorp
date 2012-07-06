#ifndef mozilla__ipdltest_TestRacyReentry_h
#define mozilla__ipdltest_TestRacyReentry_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRacyReentryParent.h"
#include "mozilla/_ipdltest/PTestRacyReentryChild.h"

namespace mozilla {
namespace _ipdltest {


class TestRacyReentryParent :
    public PTestRacyReentryParent
{
public:
    TestRacyReentryParent();
    virtual ~TestRacyReentryParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:
    virtual bool AnswerE() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

    bool mRecvdE;
};


class TestRacyReentryChild :
    public PTestRacyReentryChild
{
public:
    TestRacyReentryChild();
    virtual ~TestRacyReentryChild();

protected:
    virtual bool RecvStart() MOZ_OVERRIDE;

    virtual bool RecvN() MOZ_OVERRIDE;

    virtual bool AnswerH() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRacyReentry_h
