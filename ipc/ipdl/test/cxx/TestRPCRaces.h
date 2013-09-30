#ifndef mozilla__ipdltest_TestRPCRaces_h
#define mozilla__ipdltest_TestRPCRaces_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRPCRacesParent.h"
#include "mozilla/_ipdltest/PTestRPCRacesChild.h"

namespace mozilla {
namespace _ipdltest {

mozilla::ipc::RacyRPCPolicy
MediateRace(const mozilla::ipc::MessageChannel::Message& parent,
            const mozilla::ipc::MessageChannel::Message& child);

class TestRPCRacesParent :
    public PTestRPCRacesParent
{
public:
    TestRPCRacesParent() : mHasReply(false),
                           mChildHasReply(false),
                           mAnsweredParent(false)
    { }
    virtual ~TestRPCRacesParent() { }

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:
    virtual bool
    RecvStartRace() MOZ_OVERRIDE;

    virtual bool
    AnswerRace(bool* hasRace) MOZ_OVERRIDE;

    virtual bool
    AnswerStackFrame() MOZ_OVERRIDE;

    virtual bool
    AnswerStackFrame3() MOZ_OVERRIDE;

    virtual bool
    AnswerParent() MOZ_OVERRIDE;

    virtual bool
    RecvGetAnsweredParent(bool* answeredParent) MOZ_OVERRIDE;

    virtual mozilla::ipc::RacyRPCPolicy
    MediateRPCRace(const Message& parent, const Message& child) MOZ_OVERRIDE
    {
        return MediateRace(parent, child);
    }

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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


class TestRPCRacesChild :
    public PTestRPCRacesChild
{
public:
    TestRPCRacesChild() : mHasReply(false) { }
    virtual ~TestRPCRacesChild() { }

protected:
    virtual bool
    RecvStart() MOZ_OVERRIDE;

    virtual bool
    AnswerRace(bool* hasRace) MOZ_OVERRIDE;

    virtual bool
    AnswerStackFrame() MOZ_OVERRIDE;

    virtual bool
    AnswerStackFrame3() MOZ_OVERRIDE;

    virtual bool
    RecvWakeup() MOZ_OVERRIDE;

    virtual bool
    RecvWakeup3() MOZ_OVERRIDE;

    virtual bool
    AnswerChild() MOZ_OVERRIDE;

    virtual mozilla::ipc::RacyRPCPolicy
    MediateRPCRace(const Message& parent, const Message& child) MOZ_OVERRIDE
    {
        return MediateRace(parent, child);
    }

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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


#endif // ifndef mozilla__ipdltest_TestRPCRaces_h
