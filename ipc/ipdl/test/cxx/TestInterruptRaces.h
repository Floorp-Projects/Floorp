#ifndef mozilla__ipdltest_TestInterruptRaces_h
#define mozilla__ipdltest_TestInterruptRaces_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestInterruptRacesParent.h"
#include "mozilla/_ipdltest/PTestInterruptRacesChild.h"

namespace mozilla {
namespace _ipdltest {

mozilla::ipc::RacyInterruptPolicy
MediateRace(const mozilla::ipc::MessageChannel::Message& parent,
            const mozilla::ipc::MessageChannel::Message& child);

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

    virtual mozilla::ipc::RacyInterruptPolicy
    MediateInterruptRace(const Message& parent, const Message& child) MOZ_OVERRIDE
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


class TestInterruptRacesChild :
    public PTestInterruptRacesChild
{
public:
    TestInterruptRacesChild() : mHasReply(false) { }
    virtual ~TestInterruptRacesChild() { }

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

    virtual mozilla::ipc::RacyInterruptPolicy
    MediateInterruptRace(const Message& parent, const Message& child) MOZ_OVERRIDE
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


#endif // ifndef mozilla__ipdltest_TestInterruptRaces_h
