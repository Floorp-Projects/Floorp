#ifndef mozilla__ipdltest_TestOpens_h
#define mozilla__ipdltest_TestOpens_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestOpensParent.h"
#include "mozilla/_ipdltest/PTestOpensChild.h"

#include "mozilla/_ipdltest2/PTestOpensOpenedParent.h"
#include "mozilla/_ipdltest2/PTestOpensOpenedChild.h"

namespace mozilla {

// parent process

namespace _ipdltest {

class TestOpensParent : public PTestOpensParent
{
public:
    TestOpensParent() {}
    virtual ~TestOpensParent() {}

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

    void Main();

protected:
    virtual PTestOpensOpenedParent*
    AllocPTestOpensOpened(Transport* transport, ProcessId otherProcess) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
};

} // namespace _ipdltest

namespace _ipdltest2 {

class TestOpensOpenedParent : public PTestOpensOpenedParent
{
public:
    TestOpensOpenedParent(Transport* aTransport)
        : mTransport(aTransport)
    {}
    virtual ~TestOpensOpenedParent() {}

protected:
    virtual bool RecvHello() MOZ_OVERRIDE;
    virtual bool RecvHelloSync() MOZ_OVERRIDE;
    virtual bool AnswerHelloRpc() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

    Transport* mTransport;
};

} // namespace _ipdltest2

// child process

namespace _ipdltest {

class TestOpensChild : public PTestOpensChild
{
public:
    TestOpensChild();
    virtual ~TestOpensChild() {}

protected:
    virtual bool RecvStart() MOZ_OVERRIDE;

    virtual PTestOpensOpenedChild*
    AllocPTestOpensOpened(Transport* transport, ProcessId otherProcess) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
};

} // namespace _ipdltest

namespace _ipdltest2 {

class TestOpensOpenedChild : public PTestOpensOpenedChild
{
public:
    TestOpensOpenedChild(Transport* aTransport)
        : mGotHi(false)
        , mTransport(aTransport)
    {}
    virtual ~TestOpensOpenedChild() {}

protected:
    virtual bool RecvHi() MOZ_OVERRIDE;
    virtual bool AnswerHiRpc() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

    bool mGotHi;
    Transport* mTransport;
};

} // namespace _ipdltest2

} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestOpens_h
