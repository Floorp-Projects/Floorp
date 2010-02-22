#ifndef mozilla__ipdltest_TestHangs_h
#define mozilla__ipdltest_TestHangs_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestHangsParent.h"
#include "mozilla/_ipdltest/PTestHangsChild.h"

namespace mozilla {
namespace _ipdltest {


class TestHangsParent :
    public PTestHangsParent
{
public:
    TestHangsParent();
    virtual ~TestHangsParent();

    void Main();

protected:
    NS_OVERRIDE
    virtual bool ShouldContinueFromReplyTimeout();

    NS_OVERRIDE
    virtual bool AnswerStackFrame();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (AbnormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

    // XXX hack around lack of State()
    int mFramesToGo;
};


class TestHangsChild :
    public PTestHangsChild
{
public:
    TestHangsChild();
    virtual ~TestHangsChild();

protected:
    NS_OVERRIDE
    virtual bool AnswerStackFrame()
    {
        if (!CallStackFrame())
            fail("shouldn't be able to observe this failure");
        return true;
    }

    NS_OVERRIDE
    virtual bool AnswerHang();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        fail("should have been mercilessly killed");
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestHangs_h
