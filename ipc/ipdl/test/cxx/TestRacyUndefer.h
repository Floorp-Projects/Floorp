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
    virtual mozilla::ipc::IPCResult AnswerSpam() override;

    virtual mozilla::ipc::IPCResult AnswerRaceWinTwice() override;

    virtual mozilla::ipc::IPCResult RecvDone() override;

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
    virtual mozilla::ipc::IPCResult RecvStart() override;

    virtual mozilla::ipc::IPCResult RecvAwakenSpam() override;
    virtual mozilla::ipc::IPCResult RecvAwakenRaceWinTwice() override;

    virtual mozilla::ipc::IPCResult AnswerRace() override;

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
