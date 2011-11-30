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
    TestShutdownSubsubParent(bool expectParentDeleted) :
        mExpectParentDeleted(expectParentDeleted)
    {
    }

    virtual ~TestShutdownSubsubParent()
    {
    }

protected:
    NS_OVERRIDE
    virtual void
    ActorDestroy(ActorDestroyReason why);

private:
    bool mExpectParentDeleted;
};


class TestShutdownSubParent :
    public PTestShutdownSubParent
{
public:
    TestShutdownSubParent(bool expectCrash) :
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
    NS_OVERRIDE
    virtual bool
    AnswerStackFrame()
    {
        return CallStackFrame();
    }

    NS_OVERRIDE
    virtual PTestShutdownSubsubParent*
    AllocPTestShutdownSubsub(const bool& expectParentDelete)
    {
        return new TestShutdownSubsubParent(expectParentDelete);
    }

    NS_OVERRIDE
    virtual bool
    DeallocPTestShutdownSubsub(PTestShutdownSubsubParent* actor)
    {
        delete actor;
        ++mDeletedCount;
        return true;
    }

    NS_OVERRIDE
    virtual void
    ActorDestroy(ActorDestroyReason why);

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
    NS_OVERRIDE virtual bool RecvSync() { return true; }

    NS_OVERRIDE
    virtual PTestShutdownSubParent*
    AllocPTestShutdownSub(const bool& expectCrash)
    {
        return new TestShutdownSubParent(expectCrash);
    }

    NS_OVERRIDE
    virtual bool
    DeallocPTestShutdownSub(PTestShutdownSubParent* actor)
    {
        delete actor;
        return true;
    }

    NS_OVERRIDE
    virtual void
    ActorDestroy(ActorDestroyReason why);
};


//-----------------------------------------------------------------------------
// Child side

class TestShutdownSubsubChild :
    public PTestShutdownSubsubChild
{
public:
    TestShutdownSubsubChild(bool expectParentDeleted) :
        mExpectParentDeleted(expectParentDeleted)
    {
    }
    virtual ~TestShutdownSubsubChild()
    {
    }

protected:
    NS_OVERRIDE
    virtual void
    ActorDestroy(ActorDestroyReason why);

private:
    bool mExpectParentDeleted;
};


class TestShutdownSubChild :
    public PTestShutdownSubChild
{
public:
    TestShutdownSubChild(bool expectCrash) : mExpectCrash(expectCrash)
    {
    }

    virtual ~TestShutdownSubChild()
    {
    }

protected:
    NS_OVERRIDE
    virtual bool AnswerStackFrame();

    NS_OVERRIDE
    virtual PTestShutdownSubsubChild*
    AllocPTestShutdownSubsub(const bool& expectParentDelete)
    {
        return new TestShutdownSubsubChild(expectParentDelete);
    }

    NS_OVERRIDE
    virtual bool
    DeallocPTestShutdownSubsub(PTestShutdownSubsubChild* actor)
    {
        delete actor;
        return true;
    }

    NS_OVERRIDE
    virtual void
    ActorDestroy(ActorDestroyReason why);

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
    NS_OVERRIDE
    virtual bool
    RecvStart();

    NS_OVERRIDE
    virtual PTestShutdownSubChild*
    AllocPTestShutdownSub(
        const bool& expectCrash)
    {
        return new TestShutdownSubChild(expectCrash);
    }

    NS_OVERRIDE
    virtual bool
    DeallocPTestShutdownSub(PTestShutdownSubChild* actor)
    {
        delete actor;
        return true;
    }

    NS_OVERRIDE
    virtual void
    ActorDestroy(ActorDestroyReason why);
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestShutdown_h
