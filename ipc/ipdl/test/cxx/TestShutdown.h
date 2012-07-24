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
    virtual void
    ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

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
    virtual bool
    AnswerStackFrame() MOZ_OVERRIDE
    {
        return CallStackFrame();
    }

    virtual PTestShutdownSubsubParent*
    AllocPTestShutdownSubsub(const bool& expectParentDelete) MOZ_OVERRIDE
    {
        return new TestShutdownSubsubParent(expectParentDelete);
    }

    virtual bool
    DeallocPTestShutdownSubsub(PTestShutdownSubsubParent* actor) MOZ_OVERRIDE
    {
        delete actor;
        ++mDeletedCount;
        return true;
    }

    virtual void
    ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

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
    virtual bool RecvSync() MOZ_OVERRIDE { return true; }

    virtual PTestShutdownSubParent*
    AllocPTestShutdownSub(const bool& expectCrash) MOZ_OVERRIDE
    {
        return new TestShutdownSubParent(expectCrash);
    }

    virtual bool
    DeallocPTestShutdownSub(PTestShutdownSubParent* actor) MOZ_OVERRIDE
    {
        delete actor;
        return true;
    }

    virtual void
    ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
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
    virtual void
    ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

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
    virtual bool AnswerStackFrame() MOZ_OVERRIDE;

    virtual PTestShutdownSubsubChild*
    AllocPTestShutdownSubsub(const bool& expectParentDelete) MOZ_OVERRIDE
    {
        return new TestShutdownSubsubChild(expectParentDelete);
    }

    virtual bool
    DeallocPTestShutdownSubsub(PTestShutdownSubsubChild* actor) MOZ_OVERRIDE
    {
        delete actor;
        return true;
    }

    virtual void
    ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

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
    RecvStart();

    virtual PTestShutdownSubChild*
    AllocPTestShutdownSub(
        const bool& expectCrash) MOZ_OVERRIDE
    {
        return new TestShutdownSubChild(expectCrash);
    }

    virtual bool
    DeallocPTestShutdownSub(PTestShutdownSubChild* actor) MOZ_OVERRIDE
    {
        delete actor;
        return true;
    }

    virtual void
    ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestShutdown_h
