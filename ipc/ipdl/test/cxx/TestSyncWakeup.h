#ifndef mozilla__ipdltest_TestSyncWakeup_h
#define mozilla__ipdltest_TestSyncWakeup_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestSyncWakeupParent.h"
#include "mozilla/_ipdltest/PTestSyncWakeupChild.h"

namespace mozilla {
namespace _ipdltest {


class TestSyncWakeupParent :
    public PTestSyncWakeupParent
{
public:
    TestSyncWakeupParent();
    virtual ~TestSyncWakeupParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:
    virtual bool AnswerStackFrame() MOZ_OVERRIDE;

    virtual bool RecvSync1() MOZ_OVERRIDE;

    virtual bool RecvSync2() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestSyncWakeupChild :
    public PTestSyncWakeupChild
{
public:
    TestSyncWakeupChild();
    virtual ~TestSyncWakeupChild();

protected:
    virtual bool RecvStart() MOZ_OVERRIDE;

    virtual bool RecvNote1() MOZ_OVERRIDE;

    virtual bool AnswerStackFrame() MOZ_OVERRIDE;

    virtual bool RecvNote2() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }

private:
    bool mDone;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestSyncWakeup_h
