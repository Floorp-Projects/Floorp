#ifndef mozilla__ipdltest_TestNestedLoops_h
#define mozilla__ipdltest_TestNestedLoops_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestNestedLoopsParent.h"
#include "mozilla/_ipdltest/PTestNestedLoopsChild.h"

namespace mozilla {
namespace _ipdltest {


class TestNestedLoopsParent :
    public PTestNestedLoopsParent
{
public:
    TestNestedLoopsParent();
    virtual ~TestNestedLoopsParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:    
    virtual bool RecvNonce() MOZ_OVERRIDE;

    void BreakNestedLoop();

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

    bool mBreakNestedLoop;
};


class TestNestedLoopsChild :
    public PTestNestedLoopsChild
{
public:
    TestNestedLoopsChild();
    virtual ~TestNestedLoopsChild();

protected:
    virtual bool RecvStart() MOZ_OVERRIDE;

    virtual bool AnswerR() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestNestedLoops_h
