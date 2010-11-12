#ifndef mozilla__ipdltest_TestShmem_h
#define mozilla__ipdltest_TestShmem_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestShmemParent.h"
#include "mozilla/_ipdltest/PTestShmemChild.h"

namespace mozilla {
namespace _ipdltest {


class TestShmemParent :
    public PTestShmemParent
{
public:
    TestShmemParent() { }
    virtual ~TestShmemParent() { }

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


class TestShmemChild :
    public PTestShmemChild
{
public:
    TestShmemChild() { }
    virtual ~TestShmemChild() { }

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

#endif // ifndef mozilla__ipdltest_TestShmem_h
