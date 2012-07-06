#ifndef mozilla__ipdltest_TestRPCErrorCleanup_h
#define mozilla__ipdltest_TestRPCErrorCleanup_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRPCErrorCleanupParent.h"
#include "mozilla/_ipdltest/PTestRPCErrorCleanupChild.h"

namespace mozilla {
namespace _ipdltest {


class TestRPCErrorCleanupParent :
    public PTestRPCErrorCleanupParent
{
public:
    TestRPCErrorCleanupParent();
    virtual ~TestRPCErrorCleanupParent();

    static bool RunTestInProcesses() { return true; }
    // FIXME/bug 703323 Could work if modified
    static bool RunTestInThreads() { return false; }

    void Main();

protected:    
    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (AbnormalShutdown != why)
            fail("unexpected destruction!");  
    }

    virtual void ProcessingError(Result what) MOZ_OVERRIDE;

    bool mGotProcessingError;
};


class TestRPCErrorCleanupChild :
    public PTestRPCErrorCleanupChild
{
public:
    TestRPCErrorCleanupChild();
    virtual ~TestRPCErrorCleanupChild();

protected:
    virtual bool AnswerError() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        fail("should have 'crashed'!");
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRPCErrorCleanup_h
