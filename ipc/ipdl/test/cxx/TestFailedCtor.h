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
    virtual PTestFailedCtorSubParent* AllocPTestFailedCtorSubParent() MOZ_OVERRIDE;
    virtual bool DeallocPTestFailedCtorSubParent(PTestFailedCtorSubParent* actor) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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
    virtual PTestFailedCtorSubChild* AllocPTestFailedCtorSubChild() MOZ_OVERRIDE;

    virtual bool AnswerPTestFailedCtorSubConstructor(PTestFailedCtorSubChild* actor) MOZ_OVERRIDE;

    virtual bool DeallocPTestFailedCtorSubChild(PTestFailedCtorSubChild* actor) MOZ_OVERRIDE;

    virtual void ProcessingError(Result aCode, const char* aReason) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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
    TestFailedCtorSubParent() : mOne(nullptr), mTwo(nullptr), mThree(nullptr) { }
    virtual ~TestFailedCtorSubParent();

protected:
    virtual PTestFailedCtorSubsubParent* AllocPTestFailedCtorSubsubParent() MOZ_OVERRIDE;

    virtual bool DeallocPTestFailedCtorSubsubParent(PTestFailedCtorSubsubParent* actor) MOZ_OVERRIDE;
    virtual bool RecvSync() MOZ_OVERRIDE { return true; }

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

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
    virtual PTestFailedCtorSubsubChild* AllocPTestFailedCtorSubsubChild() MOZ_OVERRIDE;
    virtual bool DeallocPTestFailedCtorSubsubChild(PTestFailedCtorSubsubChild* actor) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
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

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE { mWhy = why; }

    ActorDestroyReason mWhy;
    bool mDealloced;
};


}
}

#endif // ifndef mozilla_ipdltest_TestFailedCtor_h
