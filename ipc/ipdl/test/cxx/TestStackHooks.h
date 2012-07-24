#ifndef mozilla__ipdltest_TestStackHooks_h
#define mozilla__ipdltest_TestStackHooks_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestStackHooksParent.h"
#include "mozilla/_ipdltest/PTestStackHooksChild.h"

namespace mozilla {
namespace _ipdltest {


class TestStackHooksParent :
    public PTestStackHooksParent
{
public:
    TestStackHooksParent();
    virtual ~TestStackHooksParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:    
    virtual bool RecvAsync() MOZ_OVERRIDE {
        if (!mOnStack)
            fail("not on C++ stack?!");
        return true;
    }

    virtual bool RecvSync() MOZ_OVERRIDE {
        if (!mOnStack)
            fail("not on C++ stack?!");
        return true;
    }

    virtual bool AnswerRpc() MOZ_OVERRIDE {
        if (!mOnStack)
            fail("not on C++ stack?!");
        return true;
    }

    virtual bool AnswerStackFrame() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

    virtual void EnteredCxxStack() MOZ_OVERRIDE {
        mOnStack = true;
    }
    virtual void ExitedCxxStack() MOZ_OVERRIDE {
        mOnStack = false;
    }

    virtual void EnteredCall() MOZ_OVERRIDE {
        ++mIncallDepth;
    }
    virtual void ExitedCall() MOZ_OVERRIDE {
        --mIncallDepth;
    }

private:
    bool mOnStack;
    int mIncallDepth;
};


class TestStackHooksChild :
    public PTestStackHooksChild
{
public:
    TestStackHooksChild();
    virtual ~TestStackHooksChild();

    void RunTests();

protected:
    virtual bool RecvStart() MOZ_OVERRIDE;

    virtual bool AnswerStackFrame() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");

        if (mEntered != mExited)
            fail("unbalanced enter/exit notifications");

        if (mOnStack)
            fail("computing mOnStack went awry; should have failed above assertion");

        QuitChild();
    }

    virtual void EnteredCxxStack() MOZ_OVERRIDE {
        ++mEntered;
        mOnStack = true;
    }
    virtual void ExitedCxxStack() MOZ_OVERRIDE {
        ++mExited;
        mOnStack = false;
    }

    virtual void EnteredCall() MOZ_OVERRIDE {
        ++mIncallDepth;
    }
    virtual void ExitedCall() MOZ_OVERRIDE {
        --mIncallDepth;
    }

private:
    bool mOnStack;
    int mEntered;
    int mExited;
    int mIncallDepth;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestStackHooks_h
