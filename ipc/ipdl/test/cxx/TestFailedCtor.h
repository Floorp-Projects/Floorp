#ifndef mozilla_ipdltest_TestFailedCtor_h
#define mozilla_ipdltest_TestFailedCtor_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestFailedCtorParent.h"
#include "mozilla/_ipdltest/PTestFailedCtorChild.h"

#include "mozilla/_ipdltest/PTestFailedCtorSubParent.h"
#include "mozilla/_ipdltest/PTestFailedCtorSubChild.h"

#include "mozilla/_ipdltest/PTestFailedCtorSubsubParent.h"
#include "mozilla/_ipdltest/PTestFailedCtorSubsubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Top-level
//
class TestFailedCtorParent :
    public PTestFailedCtorParent
{
public:
    TestFailedCtorParent() { }
    virtual ~TestFailedCtorParent() { }

    static bool RunTestInProcesses() { return true; }

    // FIXME/bug 703322 Disabled because child calls exit() to end
    //                  test, not clear how to handle failed ctor in
    //                  threaded mode.
    static bool RunTestInThreads() { return false; }

    void Main();

protected:
    NS_OVERRIDE
    virtual PTestFailedCtorSubParent* AllocPTestFailedCtorSub();
    NS_OVERRIDE
    virtual bool DeallocPTestFailedCtorSub(PTestFailedCtorSubParent* actor);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (AbnormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestFailedCtorChild :
    public PTestFailedCtorChild
{
public:
    TestFailedCtorChild() { }
    virtual ~TestFailedCtorChild() { }

protected:
    NS_OVERRIDE
    virtual PTestFailedCtorSubChild* AllocPTestFailedCtorSub();

    NS_OVERRIDE
    virtual bool AnswerPTestFailedCtorSubConstructor(PTestFailedCtorSubChild* actor);

    NS_OVERRIDE
    virtual bool DeallocPTestFailedCtorSub(PTestFailedCtorSubChild* actor);

    NS_OVERRIDE
    virtual void ProcessingError(Result what);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        fail("should have _exit()ed");
    }
};


//-----------------------------------------------------------------------------
// First descendent
//
class TestFailedCtorSubsub;

class TestFailedCtorSubParent :
    public PTestFailedCtorSubParent
{
public:
    TestFailedCtorSubParent() : mOne(NULL), mTwo(NULL), mThree(NULL) { }
    virtual ~TestFailedCtorSubParent();

protected:
    NS_OVERRIDE
    virtual PTestFailedCtorSubsubParent* AllocPTestFailedCtorSubsub();

    NS_OVERRIDE
    virtual bool DeallocPTestFailedCtorSubsub(PTestFailedCtorSubsubParent* actor);
    NS_OVERRIDE
    virtual bool RecvSync() { return true; }

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why);

    TestFailedCtorSubsub* mOne;
    TestFailedCtorSubsub* mTwo;
    TestFailedCtorSubsub* mThree;
};


class TestFailedCtorSubChild :
    public PTestFailedCtorSubChild
{
public:
    TestFailedCtorSubChild() { }
    virtual ~TestFailedCtorSubChild() { }

protected:
    NS_OVERRIDE
    virtual PTestFailedCtorSubsubChild* AllocPTestFailedCtorSubsub();
    NS_OVERRIDE
    virtual bool DeallocPTestFailedCtorSubsub(PTestFailedCtorSubsubChild* actor);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why);
};


//-----------------------------------------------------------------------------
// Grand-descendent
//
class TestFailedCtorSubsub :
        public PTestFailedCtorSubsubParent,
        public PTestFailedCtorSubsubChild
{
public:
    TestFailedCtorSubsub() : mWhy(ActorDestroyReason(-1)), mDealloced(false) {}
    virtual ~TestFailedCtorSubsub() {}

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why) { mWhy = why; }

    ActorDestroyReason mWhy;
    bool mDealloced;
};


}
}

#endif // ifndef mozilla_ipdltest_TestFailedCtor_h
