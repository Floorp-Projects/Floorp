#ifndef mozilla__ipdltest_TestSyncError_h
#define mozilla__ipdltest_TestSyncError_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestSyncErrorParent.h"
#include "mozilla/_ipdltest/PTestSyncErrorChild.h"

namespace mozilla {
namespace _ipdltest {


class TestSyncErrorParent :
    public PTestSyncErrorParent
{
public:
    TestSyncErrorParent();
    virtual ~TestSyncErrorParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:    
    virtual bool RecvError() override;

    virtual void ProcessingError(Result aCode, const char* aReason) override
    {
        // Ignore errors
    }

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestSyncErrorChild :
    public PTestSyncErrorChild
{
public:
    TestSyncErrorChild();
    virtual ~TestSyncErrorChild();

protected:
    virtual bool RecvStart() override;

    virtual void ProcessingError(Result aCode, const char* aReason) override
    {
        // Ignore errors
    }

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestSyncError_h
