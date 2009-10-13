#ifndef mozilla__ipdltest_TestLatency_h
#define mozilla__ipdltest_TestLatency_h 1


#include "mozilla/TimeStamp.h"

#include "mozilla/_ipdltest/PTestLatencyParent.h"
#include "mozilla/_ipdltest/PTestLatencyChild.h"

namespace mozilla {
namespace _ipdltest {


class TestLatencyParent :
    public PTestLatencyParent
{
private:
    typedef mozilla::TimeStamp TimeStamp;
    typedef mozilla::TimeDuration TimeDuration;

public:
    TestLatencyParent();
    virtual ~TestLatencyParent();

    void Main();

protected:    
    virtual bool RecvPong();
    virtual bool RecvPong5();

private:
    void PingPongTrial();
    void Ping5Pong5Trial();
    void Exit();

    TimeStamp mStart;
    TimeDuration mPPTimeTotal;
    TimeDuration mPP5TimeTotal;

    int mPPTrialsToGo;
    int mPP5TrialsToGo;

    // FIXME/cjones: HACK ALERT: don't need this once IPDL exposes actor state
    int mPongsToGo;
};


class TestLatencyChild :
    public PTestLatencyChild
{
public:
    TestLatencyChild();
    virtual ~TestLatencyChild();

protected:
    virtual bool RecvPing();
    virtual bool RecvPing5();
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestLatency_h
