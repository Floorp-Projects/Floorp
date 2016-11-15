#ifndef mozilla__ipdltest_TestInterruptRaces_h
#define mozilla__ipdltest_TestInterruptRaces_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestInterruptRacesParent.h"
#include "mozilla/_ipdltest/PTestInterruptRacesChild.h"

namespace mozilla {
namespace _ipdltest {

mozilla::ipc::RacyInterruptPolicy
MediateRace(const mozilla::ipc::MessageChannel::MessageInfo& parent,
            const mozilla::ipc::MessageChannel::MessageInfo& child);

class TestInterruptRacesParent :
    public PTestInterruptRacesParent
{
public:
    TestInterruptRacesParent() : mHasReply(false),
                           mChildHasReply(false),
                           mAnsweredParent(false)
    { }
    virtual ~TestInterruptRacesParent() { }

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:
    virtual mozilla::ipc::IPCResult
    RecvStartRace() override;

    virtual mozilla::ipc::IPCResult
    AnswerRace(bool* hasRace) override;

    virtual mozilla::ipc::IPCResult
    AnswerStackFrame() override;

    virtual mozilla::ipc::IPCResult
    AnswerStackFrame3() override;

    virtual mozilla::ipc::IPCResult
    AnswerParent() override;

    virtual mozilla::ipc::IPCResult
    RecvGetAnsweredParent(bool* answeredParent) override;

    virtual mozilla::ipc::RacyInterruptPolicy
    MediateInterruptRace(const MessageInfo& parent,
                         const MessageInfo& child) override
    {
        return MediateRace(parent, child);
    }

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        if (!(mHasReply && mChildHasReply))
            fail("both sides should have replies!");
        passed("ok");
        QuitParent();
    }

private:
    void OnRaceTime();

    void Test2();
    void Test3();

    bool mHasReply;
    bool mChildHasReply;
    bool mAnsweredParent;
};


class TestInterruptRacesChild :
    public PTestInterruptRacesChild
{
public:
    TestInterruptRacesChild() : mHasReply(false) { }
    virtual ~TestInterruptRacesChild() { }

protected:
    virtual mozilla::ipc::IPCResult
    RecvStart() override;

    virtual mozilla::ipc::IPCResult
    AnswerRace(bool* hasRace) override;

    virtual mozilla::ipc::IPCResult
    AnswerStackFrame() override;

    virtual mozilla::ipc::IPCResult
    AnswerStackFrame3() override;

    virtual mozilla::ipc::IPCResult
    RecvWakeup() override;

    virtual mozilla::ipc::IPCResult
    RecvWakeup3() override;

    virtual mozilla::ipc::IPCResult
    AnswerChild() override;

    virtual mozilla::ipc::RacyInterruptPolicy
    MediateInterruptRace(const MessageInfo& parent,
                         const MessageInfo& child) override
    {
        return MediateRace(parent, child);
    }

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }

private:
    bool mHasReply;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestInterruptRaces_h
