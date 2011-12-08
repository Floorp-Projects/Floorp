#ifndef mozilla__ipdltest_TestCrashCleanup_h
#define mozilla__ipdltest_TestCrashCleanup_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestCrashCleanupParent.h"
#include "mozilla/_ipdltest/PTestCrashCleanupChild.h"

namespace mozilla {
namespace _ipdltest {


class TestCrashCleanupParent :
    public PTestCrashCleanupParent
{
public:
    TestCrashCleanupParent();
    virtual ~TestCrashCleanupParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

    void Main();

protected:    
    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (AbnormalShutdown != why)
            fail("unexpected destruction!");
        mCleanedUp = true;
    }

    bool mCleanedUp;
};


class TestCrashCleanupChild :
    public PTestCrashCleanupChild
{
public:
    TestCrashCleanupChild();
    virtual ~TestCrashCleanupChild();

protected:
    NS_OVERRIDE
    virtual bool AnswerDIEDIEDIE();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        fail("should have 'crashed'!");
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestCrashCleanup_h
