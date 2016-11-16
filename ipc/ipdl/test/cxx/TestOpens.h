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
    AllocPTestOpensOpenedParent(Transport* transport, ProcessId otherProcess) override;

    virtual void ActorDestroy(ActorDestroyReason why) override;
};

} // namespace _ipdltest

namespace _ipdltest2 {

class TestOpensOpenedParent : public PTestOpensOpenedParent
{
public:
    explicit TestOpensOpenedParent(Transport* aTransport)
        : mTransport(aTransport)
    {}
    virtual ~TestOpensOpenedParent() {}

protected:
    virtual mozilla::ipc::IPCResult RecvHello() override;
    virtual mozilla::ipc::IPCResult RecvHelloSync() override;
    virtual mozilla::ipc::IPCResult AnswerHelloRpc() override;

    virtual void ActorDestroy(ActorDestroyReason why) override;

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
    virtual mozilla::ipc::IPCResult RecvStart() override;

    virtual PTestOpensOpenedChild*
    AllocPTestOpensOpenedChild(Transport* transport, ProcessId otherProcess) override;

    virtual void ActorDestroy(ActorDestroyReason why) override;
};

} // namespace _ipdltest

namespace _ipdltest2 {

class TestOpensOpenedChild : public PTestOpensOpenedChild
{
public:
    explicit TestOpensOpenedChild(Transport* aTransport)
        : mGotHi(false)
        , mTransport(aTransport)
    {}
    virtual ~TestOpensOpenedChild() {}

protected:
    virtual mozilla::ipc::IPCResult RecvHi() override;
    virtual mozilla::ipc::IPCResult AnswerHiRpc() override;

    virtual void ActorDestroy(ActorDestroyReason why) override;

    bool mGotHi;
    Transport* mTransport;
};

} // namespace _ipdltest2

} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestOpens_h
