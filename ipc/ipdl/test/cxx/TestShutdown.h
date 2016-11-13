#ifndef mozilla__ipdltest_TestShutdown_h
#define mozilla__ipdltest_TestShutdown_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestShutdownParent.h"
#include "mozilla/_ipdltest/PTestShutdownChild.h"

#include "mozilla/_ipdltest/PTestShutdownSubParent.h"
#include "mozilla/_ipdltest/PTestShutdownSubChild.h"

#include "mozilla/_ipdltest/PTestShutdownSubsubParent.h"
#include "mozilla/_ipdltest/PTestShutdownSubsubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Parent side

class TestShutdownSubsubParent :
    public PTestShutdownSubsubParent
{
public:
    explicit TestShutdownSubsubParent(bool expectParentDeleted) :
        mExpectParentDeleted(expectParentDeleted)
    {
    }

    virtual ~TestShutdownSubsubParent()
    {
    }

protected:
    virtual void
    ActorDestroy(ActorDestroyReason why) override;

private:
    bool mExpectParentDeleted;
};


class TestShutdownSubParent :
    public PTestShutdownSubParent
{
public:
    explicit TestShutdownSubParent(bool expectCrash) :
        mExpectCrash(expectCrash),
        mDeletedCount(0)
    {
    }

    virtual ~TestShutdownSubParent()
    {
        if (2 != mDeletedCount)
            fail("managees outliving manager!");
    }

protected:
    virtual bool
    AnswerStackFrame() override
    {
        return CallStackFrame();
    }

    virtual PTestShutdownSubsubParent*
    AllocPTestShutdownSubsubParent(const bool& expectParentDelete) override
    {
        return new TestShutdownSubsubParent(expectParentDelete);
    }

    virtual bool
    DeallocPTestShutdownSubsubParent(PTestShutdownSubsubParent* actor) override
    {
        delete actor;
        ++mDeletedCount;
        return true;
    }

    virtual void
    ActorDestroy(ActorDestroyReason why) override;

private:
    bool mExpectCrash;
    int mDeletedCount;
};


class TestShutdownParent :
    public PTestShutdownParent
{
public:
    TestShutdownParent()
    {
    }
    virtual ~TestShutdownParent()
    {
    }

    static bool RunTestInProcesses() { return true; }
    // FIXME/bug 703323 Could work if modified
    static bool RunTestInThreads() { return false; }

    void Main();

protected:
    virtual bool RecvSync() override { return true; }

    virtual PTestShutdownSubParent*
    AllocPTestShutdownSubParent(const bool& expectCrash) override
    {
        return new TestShutdownSubParent(expectCrash);
    }

    virtual bool
    DeallocPTestShutdownSubParent(PTestShutdownSubParent* actor) override
    {
        delete actor;
        return true;
    }

    virtual void
    ActorDestroy(ActorDestroyReason why) override;
};


//-----------------------------------------------------------------------------
// Child side

class TestShutdownSubsubChild :
    public PTestShutdownSubsubChild
{
public:
    explicit TestShutdownSubsubChild(bool expectParentDeleted) :
        mExpectParentDeleted(expectParentDeleted)
    {
    }
    virtual ~TestShutdownSubsubChild()
    {
    }

protected:
    virtual void
    ActorDestroy(ActorDestroyReason why) override;

private:
    bool mExpectParentDeleted;
};


class TestShutdownSubChild :
    public PTestShutdownSubChild
{
public:
    explicit TestShutdownSubChild(bool expectCrash) : mExpectCrash(expectCrash)
    {
    }

    virtual ~TestShutdownSubChild()
    {
    }

protected:
    virtual bool AnswerStackFrame() override;

    virtual PTestShutdownSubsubChild*
    AllocPTestShutdownSubsubChild(const bool& expectParentDelete) override
    {
        return new TestShutdownSubsubChild(expectParentDelete);
    }

    virtual bool
    DeallocPTestShutdownSubsubChild(PTestShutdownSubsubChild* actor) override
    {
        delete actor;
        return true;
    }

    virtual void
    ActorDestroy(ActorDestroyReason why) override;

private:
    bool mExpectCrash;
};


class TestShutdownChild :
    public PTestShutdownChild
{
public:
    TestShutdownChild()
    {
    }
    virtual ~TestShutdownChild()
    {
    }

protected:
    virtual bool
    RecvStart() override;

    virtual PTestShutdownSubChild*
    AllocPTestShutdownSubChild(
        const bool& expectCrash) override
    {
        return new TestShutdownSubChild(expectCrash);
    }

    virtual bool
    DeallocPTestShutdownSubChild(PTestShutdownSubChild* actor) override
    {
        delete actor;
        return true;
    }

    virtual void
    ActorDestroy(ActorDestroyReason why) override;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestShutdown_h
