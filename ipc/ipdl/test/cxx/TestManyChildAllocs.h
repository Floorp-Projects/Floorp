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
    virtual mozilla::ipc::IPCResult RecvDone() override;
    virtual bool DeallocPTestManyChildAllocsSubParent(PTestManyChildAllocsSubParent* __a) override;
    virtual PTestManyChildAllocsSubParent* AllocPTestManyChildAllocsSubParent() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
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
    virtual mozilla::ipc::IPCResult RecvGo() override;
    virtual bool DeallocPTestManyChildAllocsSubChild(PTestManyChildAllocsSubChild* __a) override;
    virtual PTestManyChildAllocsSubChild* AllocPTestManyChildAllocsSubChild() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
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
    virtual void ActorDestroy(ActorDestroyReason why) override {}
    virtual mozilla::ipc::IPCResult RecvHello() override { return IPC_OK(); }
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
