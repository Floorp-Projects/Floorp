#include "TestBadActor.h"
#include "IPDLUnitTests.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace _ipdltest {

void
TestBadActorParent::Main()
{
  // This test is designed to test a race condition where the child sends us
  // a message on an actor that we've already destroyed. The child process
  // should die, and the parent process should not abort.

  PTestBadActorSubParent* child = SendPTestBadActorSubConstructor();
  if (!child)
    fail("Sending constructor");

  unused << child->Call__delete__(child);
}

PTestBadActorSubParent*
TestBadActorParent::AllocPTestBadActorSubParent()
{
  return new TestBadActorSubParent();
}

bool
TestBadActorSubParent::RecvPing()
{
  fail("Shouldn't have received ping.");
  return false;
}

PTestBadActorSubChild*
TestBadActorChild::AllocPTestBadActorSubChild()
{
  return new TestBadActorSubChild();
}

bool
TestBadActorChild::RecvPTestBadActorSubConstructor(PTestBadActorSubChild* actor)
{
  if (!actor->SendPing()) {
    fail("Couldn't send ping to an actor which supposedly isn't dead yet.");
  }
  return true;
}

} // namespace _ipdltest
} // namespace mozilla
