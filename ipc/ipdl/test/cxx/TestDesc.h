#ifndef mozilla_ipdltest_TestDesc_h
#define mozilla_ipdltest_TestDesc_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestDescParent.h"
#include "mozilla/_ipdltest/PTestDescChild.h"

#include "mozilla/_ipdltest/PTestDescSubParent.h"
#include "mozilla/_ipdltest/PTestDescSubChild.h"

#include "mozilla/_ipdltest/PTestDescSubsubParent.h"
#include "mozilla/_ipdltest/PTestDescSubsubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Top-level
//
class TestDescParent :
    public PTestDescParent
{
public:
    TestDescParent() { }
    virtual ~TestDescParent() { }

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

    NS_OVERRIDE
    virtual bool RecvOk(PTestDescSubsubParent* a);

protected:
    NS_OVERRIDE
    virtual PTestDescSubParent* AllocPTestDescSub(PTestDescSubsubParent*);
    NS_OVERRIDE
    virtual bool DeallocPTestDescSub(PTestDescSubParent* actor);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestDescChild :
    public PTestDescChild
{
public:
    TestDescChild() { }
    virtual ~TestDescChild() { }

protected:
    NS_OVERRIDE
    virtual PTestDescSubChild* AllocPTestDescSub(PTestDescSubsubChild*);

    NS_OVERRIDE
    virtual bool DeallocPTestDescSub(PTestDescSubChild* actor);

    NS_OVERRIDE
    virtual bool RecvTest(PTestDescSubsubChild* a);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


//-----------------------------------------------------------------------------
// First descendent
//
class TestDescSubParent :
    public PTestDescSubParent
{
public:
    TestDescSubParent() { }
    virtual ~TestDescSubParent() { }

protected:
    NS_OVERRIDE
    virtual PTestDescSubsubParent* AllocPTestDescSubsub();

    NS_OVERRIDE
    virtual bool DeallocPTestDescSubsub(PTestDescSubsubParent* actor);
};


class TestDescSubChild :
    public PTestDescSubChild
{
public:
    TestDescSubChild() { }
    virtual ~TestDescSubChild() { }

protected:
    NS_OVERRIDE
    virtual PTestDescSubsubChild* AllocPTestDescSubsub();
    NS_OVERRIDE
    virtual bool DeallocPTestDescSubsub(PTestDescSubsubChild* actor);
};


//-----------------------------------------------------------------------------
// Grand-descendent
//
class TestDescSubsubParent :
    public PTestDescSubsubParent
{
public:
    TestDescSubsubParent() { }
    virtual ~TestDescSubsubParent() { }
};

class TestDescSubsubChild :
    public PTestDescSubsubChild
{
public:
    TestDescSubsubChild() { }
    virtual ~TestDescSubsubChild() { }
};


}
}

#endif // ifndef mozilla_ipdltest_TestDesc_h
