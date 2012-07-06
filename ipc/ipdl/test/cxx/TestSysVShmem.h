#ifndef mozilla__ipdltest_TestSysVShmem_h
#define mozilla__ipdltest_TestSysVShmem_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestSysVShmemParent.h"
#include "mozilla/_ipdltest/PTestSysVShmemChild.h"

namespace mozilla {
namespace _ipdltest {


class TestSysVShmemParent :
    public PTestSysVShmemParent
{
public:
    TestSysVShmemParent() { }
    virtual ~TestSysVShmemParent() { }

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:
    virtual bool RecvTake(
            Shmem& mem,
            Shmem& unsafe,
            const size_t& expectedSize) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestSysVShmemChild :
    public PTestSysVShmemChild
{
public:
    TestSysVShmemChild() { }
    virtual ~TestSysVShmemChild() { }

protected:
    virtual bool RecvGive(
            Shmem& mem,
            Shmem& unsafe,
            const size_t& expectedSize) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla

#endif // ifndef mozilla__ipdltest_TestSysVShmem_h
