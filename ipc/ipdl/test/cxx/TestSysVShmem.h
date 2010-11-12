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

    void Main();

protected:
    NS_OVERRIDE
    virtual bool RecvTake(
            Shmem& mem,
            Shmem& unsafe,
            const size_t& expectedSize);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
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
    NS_OVERRIDE
    virtual bool RecvGive(
            Shmem& mem,
            Shmem& unsafe,
            const size_t& expectedSize);

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

#endif // ifndef mozilla__ipdltest_TestSysVShmem_h
