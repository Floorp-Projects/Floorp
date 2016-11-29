#include "TestBadActor.h"
#include "IPDLUnitTests.h"
#include "mozilla/Unused.h"

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

  Unused << child->Call__delete__(child);
}

// By default, fatal errors kill the parent process, but this makes it
// hard to test, so instead we use the previous behavior and kill the
// child process.
void
TestBadActorParent::HandleFatalError(const char* aProtocolName, const char* aErrorMsg) const
{
  if (!!strcmp(aProtocolName, "PTestBadActorSubParent")) {
    fail("wrong protocol hit a fatal error");
  }

  if (!!strcmp(aErrorMsg, "incoming message racing with actor deletion")) {
    fail("wrong fatal error");
  }

  ipc::ScopedProcessHandle otherProcessHandle;
  if (!base::OpenProcessHandle(OtherPid(), &otherProcessHandle.rwget())) {
    fail("couldn't open child process");
  } else {
    if (!base::KillProcess(otherProcessHandle, 0, false)) {
      fail("terminating child process");
    }
  }
}

PTestBadActorSubParent*
TestBadActorParent::AllocPTestBadActorSubParent()
{
  return new TestBadActorSubParent();
}

mozilla::ipc::IPCResult
TestBadActorSubParent::RecvPing()
{
  fail("Shouldn't have received ping.");
  return IPC_FAIL_NO_REASON(this);
}

PTestBadActorSubChild*
TestBadActorChild::AllocPTestBadActorSubChild()
{
  return new TestBadActorSubChild();
}

mozilla::ipc::IPCResult
TestBadActorChild::RecvPTestBadActorSubConstructor(PTestBadActorSubChild* actor)
{
  if (!actor->SendPing()) {
    fail("Couldn't send ping to an actor which supposedly isn't dead yet.");
  }
  return IPC_OK();
}

} // namespace _ipdltest
} // namespace mozilla
