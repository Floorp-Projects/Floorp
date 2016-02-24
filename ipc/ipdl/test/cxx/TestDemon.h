#ifndef mozilla__ipdltest_TestDemon_h
#define mozilla__ipdltest_TestDemon_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestDemonParent.h"
#include "mozilla/_ipdltest/PTestDemonChild.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace _ipdltest {


class TestDemonParent :
    public PTestDemonParent
{
public:
    TestDemonParent();
    virtual ~TestDemonParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

#ifdef DEBUG
    bool ShouldContinueFromReplyTimeout() override;
    bool ArtificialTimeout() override;

    bool NeedArtificialSleep() override { return true; }
    void ArtificialSleep() override;
#endif

    bool RecvAsyncMessage(const int& n) override;
    bool RecvHiPrioSyncMessage() override;

    bool RecvSyncMessage(const int& n) override;
    bool RecvUrgentAsyncMessage(const int& n) override;
    bool RecvUrgentSyncMessage(const int& n) override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
	mDone = true;
	printf("Parent ActorDestroy\n");
        passed("ok");
        QuitParent();
    }

private:
    bool mDone;
    int mIncoming[3];
    int mOutgoing[3];

    enum {
	ASYNC_ONLY = 1,
    };

    void RunUnlimitedSequence();
    void RunLimitedSequence(int flags = 0);
    bool DoAction(int flags = 0);
};


class TestDemonChild :
    public PTestDemonChild
{
public:
    TestDemonChild();
    virtual ~TestDemonChild();

    bool RecvStart() override;

#ifdef DEBUG
    bool NeedArtificialSleep() override { return true; }
    void ArtificialSleep() override;
#endif

    bool RecvAsyncMessage(const int& n) override;
    bool RecvHiPrioSyncMessage() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
	_exit(0);
    }

    virtual void IntentionalCrash() override
    {
	_exit(0);
    }

private:
    int mIncoming[3];
    int mOutgoing[3];

    void RunUnlimitedSequence();
    void RunLimitedSequence();
    bool DoAction();
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestDemon_h
