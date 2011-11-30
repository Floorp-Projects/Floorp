#ifndef mozilla__ipdltest_TestManyChildAllocs_h
#define mozilla__ipdltest_TestManyChildAllocs_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestManyChildAllocsParent.h"
#include "mozilla/_ipdltest/PTestManyChildAllocsChild.h"

#include "mozilla/_ipdltest/PTestManyChildAllocsSubParent.h"
#include "mozilla/_ipdltest/PTestManyChildAllocsSubChild.h"

namespace mozilla {
namespace _ipdltest {

// top-level protocol

class TestManyChildAllocsParent :
    public PTestManyChildAllocsParent
{
public:
    TestManyChildAllocsParent();
    virtual ~TestManyChildAllocsParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:
    NS_OVERRIDE
    virtual bool RecvDone();
    NS_OVERRIDE
    virtual bool DeallocPTestManyChildAllocsSub(PTestManyChildAllocsSubParent* __a);
    NS_OVERRIDE
    virtual PTestManyChildAllocsSubParent* AllocPTestManyChildAllocsSub();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        passed("ok");
        QuitParent();
    }
};


class TestManyChildAllocsChild :
    public PTestManyChildAllocsChild
{
public:
    TestManyChildAllocsChild();
    virtual ~TestManyChildAllocsChild();

protected:
    NS_OVERRIDE
    virtual bool RecvGo();
    NS_OVERRIDE
    virtual bool DeallocPTestManyChildAllocsSub(PTestManyChildAllocsSubChild* __a);
    NS_OVERRIDE
    virtual PTestManyChildAllocsSubChild* AllocPTestManyChildAllocsSub();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


// do-nothing sub-protocol actors

class TestManyChildAllocsSubParent :
    public PTestManyChildAllocsSubParent
{
public:
    TestManyChildAllocsSubParent() { }
    virtual ~TestManyChildAllocsSubParent() { }

protected:
    NS_OVERRIDE
    virtual bool RecvHello() { return true; }
};


class TestManyChildAllocsSubChild :
    public PTestManyChildAllocsSubChild
{
public:
    TestManyChildAllocsSubChild() { }
    virtual ~TestManyChildAllocsSubChild() { }
};



} // namepsace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestManyChildAllocs_h
