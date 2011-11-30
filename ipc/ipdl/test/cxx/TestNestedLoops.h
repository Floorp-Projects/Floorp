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
    NS_OVERRIDE
    virtual bool RecvNonce();

    void BreakNestedLoop();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
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
    NS_OVERRIDE
    virtual bool RecvStart();

    NS_OVERRIDE
    virtual bool AnswerR();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestNestedLoops_h
