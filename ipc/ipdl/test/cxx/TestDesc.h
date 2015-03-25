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

    virtual bool RecvOk(PTestDescSubsubParent* a) override;

protected:
    virtual PTestDescSubParent* AllocPTestDescSubParent(PTestDescSubsubParent*) override;
    virtual bool DeallocPTestDescSubParent(PTestDescSubParent* actor) override;

    virtual void ActorDestroy(ActorDestroyReason why) override
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
    virtual PTestDescSubChild* AllocPTestDescSubChild(PTestDescSubsubChild*) override;

    virtual bool DeallocPTestDescSubChild(PTestDescSubChild* actor) override;

    virtual bool RecvTest(PTestDescSubsubChild* a) override;

    virtual void ActorDestroy(ActorDestroyReason why) override
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
    virtual void ActorDestroy(ActorDestroyReason why) override {}
    virtual PTestDescSubsubParent* AllocPTestDescSubsubParent() override;
    virtual bool DeallocPTestDescSubsubParent(PTestDescSubsubParent* actor) override;
};


class TestDescSubChild :
    public PTestDescSubChild
{
public:
    TestDescSubChild() { }
    virtual ~TestDescSubChild() { }

protected:
    virtual PTestDescSubsubChild* AllocPTestDescSubsubChild() override;
    virtual bool DeallocPTestDescSubsubChild(PTestDescSubsubChild* actor) override;
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

protected:
    virtual void ActorDestroy(ActorDestroyReason why) override {}
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
