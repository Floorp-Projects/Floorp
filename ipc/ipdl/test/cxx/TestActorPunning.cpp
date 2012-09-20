#include "TestActorPunning.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

void
TestActorPunningParent::Main()
{
    if (!SendStart())
        fail("sending Start");
}

bool
TestActorPunningParent::RecvPun(PTestActorPunningSubParent* a, const Bad& bad)
{
    if (a->SendBad())
        fail("bad!");
    fail("shouldn't have received this message in the first place");
    return true;
}

PTestActorPunningPunnedParent*
TestActorPunningParent::AllocPTestActorPunningPunned()
{
    return new TestActorPunningPunnedParent();
}

bool
TestActorPunningParent::DeallocPTestActorPunningPunned(PTestActorPunningPunnedParent* a)
{
    delete a;
    return true;
}

PTestActorPunningSubParent*
TestActorPunningParent::AllocPTestActorPunningSub()
{
    return new TestActorPunningSubParent();
}

bool
TestActorPunningParent::DeallocPTestActorPunningSub(PTestActorPunningSubParent* a)
{
    delete a;
    return true;
}

//-----------------------------------------------------------------------------
// child

PTestActorPunningPunnedChild*
TestActorPunningChild::AllocPTestActorPunningPunned()
{
    return new TestActorPunningPunnedChild();
}

bool
TestActorPunningChild::DeallocPTestActorPunningPunned(PTestActorPunningPunnedChild*)
{
    fail("should have died by now");
    return true;
}

PTestActorPunningSubChild*
TestActorPunningChild::AllocPTestActorPunningSub()
{
    return new TestActorPunningSubChild();
}

bool
TestActorPunningChild::DeallocPTestActorPunningSub(PTestActorPunningSubChild*)
{
    fail("should have died by now");
    return true;
}

bool
TestActorPunningChild::RecvStart()
{
    SendPTestActorPunningSubConstructor();
    SendPTestActorPunningPunnedConstructor();
    PTestActorPunningSubChild* a = SendPTestActorPunningSubConstructor();
    // We can't assert whether this succeeds or fails, due to race
    // conditions.
    SendPun(a, Bad());
    return true;
}

bool
TestActorPunningSubChild::RecvBad()
{
    fail("things are going really badly right now");
    return true;
}


} // namespace _ipdltest
} // namespace mozilla

namespace IPC {
using namespace mozilla::_ipdltest;
using namespace mozilla::ipc;

/*static*/ void
ParamTraits<Bad>::Write(Message* aMsg, const paramType& aParam)
{
    char* ptr = aMsg->BeginWriteData(4);
    ptr -= intptr_t(sizeof(int));
    ptr -= intptr_t(sizeof(ActorHandle));
    ActorHandle* ah = reinterpret_cast<ActorHandle*>(ptr);
    if (ah->mId != -3)
        fail("guessed wrong offset (value is %d, should be -3)", ah->mId);
    ah->mId = -2;
}

/*static*/ bool
ParamTraits<Bad>::Read(const Message* aMsg, void** aIter, paramType* aResult)
{
    const char* ptr;
    int len;
    aMsg->ReadData(aIter, &ptr, &len);
    return true;
}

} // namespace IPC

