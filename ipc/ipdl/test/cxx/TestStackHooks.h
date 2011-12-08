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
    NS_OVERRIDE
    virtual bool RecvAsync() {
        if (!mOnStack)
            fail("not on C++ stack?!");
        return true;
    }

    NS_OVERRIDE
    virtual bool RecvSync() {
        if (!mOnStack)
            fail("not on C++ stack?!");
        return true;
    }

    NS_OVERRIDE
    virtual bool AnswerRpc() {
        if (!mOnStack)
            fail("not on C++ stack?!");
        return true;
    }

    NS_OVERRIDE
    virtual bool AnswerStackFrame();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

    NS_OVERRIDE
    virtual void EnteredCxxStack() {
        mOnStack = true;
    }
    NS_OVERRIDE
    virtual void ExitedCxxStack() {
        mOnStack = false;
    }

    NS_OVERRIDE
    virtual void EnteredCall() {
        ++mIncallDepth;
    }
    NS_OVERRIDE
    virtual void ExitedCall() {
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
    NS_OVERRIDE
    virtual bool RecvStart();

    NS_OVERRIDE
    virtual bool AnswerStackFrame();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");

        if (mEntered != mExited)
            fail("unbalanced enter/exit notifications");

        if (mOnStack)
            fail("computing mOnStack went awry; should have failed above assertion");

        QuitChild();
    }

    NS_OVERRIDE
    virtual void EnteredCxxStack() {
        ++mEntered;
        mOnStack = true;
    }
    NS_OVERRIDE
    virtual void ExitedCxxStack() {
        ++mExited;
        mOnStack = false;
    }

    NS_OVERRIDE
    virtual void EnteredCall() {
        ++mIncallDepth;
    }
    NS_OVERRIDE
    virtual void ExitedCall() {
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
