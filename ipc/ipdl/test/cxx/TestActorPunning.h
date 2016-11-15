#ifndef mozilla__ipdltest_TestActorPunning_h
#define mozilla__ipdltest_TestActorPunning_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestActorPunningParent.h"
#include "mozilla/_ipdltest/PTestActorPunningPunnedParent.h"
#include "mozilla/_ipdltest/PTestActorPunningSubParent.h"
#include "mozilla/_ipdltest/PTestActorPunningChild.h"
#include "mozilla/_ipdltest/PTestActorPunningPunnedChild.h"
#include "mozilla/_ipdltest/PTestActorPunningSubChild.h"

namespace mozilla {
namespace _ipdltest {


class TestActorPunningParent :
    public PTestActorPunningParent
{
public:
    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

    void Main();

protected:
    PTestActorPunningPunnedParent* AllocPTestActorPunningPunnedParent() override;
    bool DeallocPTestActorPunningPunnedParent(PTestActorPunningPunnedParent* a) override;

    PTestActorPunningSubParent* AllocPTestActorPunningSubParent() override;
    bool DeallocPTestActorPunningSubParent(PTestActorPunningSubParent* a) override;

    virtual mozilla::ipc::IPCResult RecvPun(PTestActorPunningSubParent* a, const Bad& bad) override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown == why)
            fail("should have died from error!");  
        passed("ok");
        QuitParent();
    }
};

class TestActorPunningPunnedParent :
    public PTestActorPunningPunnedParent
{
public:
    TestActorPunningPunnedParent() {}
    virtual ~TestActorPunningPunnedParent() {}
protected:
    virtual void ActorDestroy(ActorDestroyReason why) override {}
};

class TestActorPunningSubParent :
    public PTestActorPunningSubParent
{
public:
    TestActorPunningSubParent() {}
    virtual ~TestActorPunningSubParent() {}
protected:
    virtual void ActorDestroy(ActorDestroyReason why) override {}
};


class TestActorPunningChild :
    public PTestActorPunningChild
{
public:
    TestActorPunningChild() {}
    virtual ~TestActorPunningChild() {}

protected:
    PTestActorPunningPunnedChild* AllocPTestActorPunningPunnedChild() override;
    bool DeallocPTestActorPunningPunnedChild(PTestActorPunningPunnedChild* a) override;

    PTestActorPunningSubChild* AllocPTestActorPunningSubChild() override;
    bool DeallocPTestActorPunningSubChild(PTestActorPunningSubChild* a) override;

    virtual mozilla::ipc::IPCResult RecvStart() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        fail("should have been killed off!");
    }
};

class TestActorPunningPunnedChild :
    public PTestActorPunningPunnedChild
{
public:
    TestActorPunningPunnedChild() {}
    virtual ~TestActorPunningPunnedChild() {}
};

class TestActorPunningSubChild :
    public PTestActorPunningSubChild
{
public:
    TestActorPunningSubChild() {}
    virtual ~TestActorPunningSubChild() {}

    virtual mozilla::ipc::IPCResult RecvBad() override;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestActorPunning_h
