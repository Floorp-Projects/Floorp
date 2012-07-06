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
    virtual bool RecvDone() MOZ_OVERRIDE;
    virtual bool DeallocPTestManyChildAllocsSub(PTestManyChildAllocsSubParent* __a) MOZ_OVERRIDE;
    virtual PTestManyChildAllocsSubParent* AllocPTestManyChildAllocsSub() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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
    virtual bool RecvGo() MOZ_OVERRIDE;
    virtual bool DeallocPTestManyChildAllocsSub(PTestManyChildAllocsSubChild* __a) MOZ_OVERRIDE;
    virtual PTestManyChildAllocsSubChild* AllocPTestManyChildAllocsSub() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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
    virtual bool RecvHello() MOZ_OVERRIDE { return true; }
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
