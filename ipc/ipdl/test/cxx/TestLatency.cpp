#include "TestLatency.h"

#include "IPDLUnitTests.h"      // fail etc.

// A ping/pong trial takes O(100us) or more, so if we don't have 10us
// resolution or better, the results will not be terribly useful
static const double kTimingResolutionCutoff = 0.00001; // 10us

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestLatencyParent::TestLatencyParent() :
    mStart(),
    mPPTimeTotal(),
    mPP5TimeTotal(),
    mPPTrialsToGo(NR_TRIALS),
    mPP5TrialsToGo(NR_TRIALS),
    mPongsToGo(0)
{
    MOZ_COUNT_CTOR(TestLatencyParent);
}

TestLatencyParent::~TestLatencyParent()
{
    MOZ_COUNT_DTOR(TestLatencyParent);
}

void
TestLatencyParent::Main()
{
    if (TimeDuration::Resolution().ToSeconds() > kTimingResolutionCutoff) {
        puts("  (skipping TestLatency, timing resolution is too poor)");
        Close();
        return;
    }

    if (mozilla::ipc::LoggingEnabled())
        NS_RUNTIMEABORT("you really don't want to log all IPC messages during this test, trust me");

    PingPongTrial();
}

void
TestLatencyParent::PingPongTrial()
{
    mStart = TimeStamp::Now();
    SendPing();
}

void
TestLatencyParent::Ping5Pong5Trial()
{
    mStart = TimeStamp::Now();
    // HACK
    mPongsToGo = 5;

    SendPing5();
    SendPing5();
    SendPing5();
    SendPing5();
    SendPing5();
}

void
TestLatencyParent::Exit()
{
    Close();
}

bool
TestLatencyParent::RecvPong()
{
    TimeDuration thisTrial = (TimeStamp::Now() - mStart);
    mPPTimeTotal += thisTrial;

    if (0 == ((mPPTrialsToGo % 1000)))
        printf("  PP trial %d: %g\n",
               mPPTrialsToGo, thisTrial.ToSecondsSigDigits());

    if (--mPPTrialsToGo > 0)
        PingPongTrial();
    else
        Ping5Pong5Trial();
    return true;
}

bool
TestLatencyParent::RecvPong5()
{
    // HACK
    if (0 < --mPongsToGo)
        return true;

    TimeDuration thisTrial = (TimeStamp::Now() - mStart);
    mPP5TimeTotal += thisTrial;

    if (0 == ((mPP5TrialsToGo % 1000)))
        printf("  PP5 trial %d: %g\n",
               mPP5TrialsToGo, thisTrial.ToSecondsSigDigits());

    if (0 < --mPP5TrialsToGo)
        Ping5Pong5Trial();
    else
        Exit();

    return true;
}


//-----------------------------------------------------------------------------
// child

TestLatencyChild::TestLatencyChild()
{
    MOZ_COUNT_CTOR(TestLatencyChild);
}

TestLatencyChild::~TestLatencyChild()
{
    MOZ_COUNT_DTOR(TestLatencyChild);
}

bool
TestLatencyChild::RecvPing()
{
    SendPong();
    return true;
}

bool
TestLatencyChild::RecvPing5()
{
    SendPong5();
    return true;
}


} // namespace _ipdltest
} // namespace mozilla
