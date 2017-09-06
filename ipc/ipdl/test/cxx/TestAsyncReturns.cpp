#include "TestAsyncReturns.h"

#include "IPDLUnitTests.h"      // fail etc.

#include "mozilla/AbstractThread.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace _ipdltest {

static uint32_t sMagic1 = 0x105b59fb;
static uint32_t sMagic2 = 0x09b6f5e3;

//-----------------------------------------------------------------------------
// parent

TestAsyncReturnsParent::TestAsyncReturnsParent()
{
  MOZ_COUNT_CTOR(TestAsyncReturnsParent);
}

TestAsyncReturnsParent::~TestAsyncReturnsParent()
{
  MOZ_COUNT_DTOR(TestAsyncReturnsParent);
}

void
TestAsyncReturnsParent::Main()
{
  SendNoReturn()->Then(MessageLoop::current()->SerialEventTarget(), __func__,
                       [](bool unused) {
                         fail("resolve handler should not be called");
                       },
                       [](PromiseRejectReason aReason) {
                         // MozPromise asserts in debug build if the
                         // handler is not called
                         if (aReason != PromiseRejectReason::ChannelClosed) {
                           fail("reject with wrong reason");
                         }
                         passed("reject handler called on channel close");
                       });
  SendPing()->Then(MessageLoop::current()->SerialEventTarget(), __func__,
                   [this](bool one) {
                     if (one) {
                       passed("take one argument");
                     } else {
                       fail("get one argument but has wrong value");
                     }
                     Close();
                   },
                   [](PromiseRejectReason aReason) {
                     fail("sending Ping");
                   });
}


mozilla::ipc::IPCResult
TestAsyncReturnsParent::RecvPong(PongResolver&& aResolve)
{
  aResolve(Tuple<const uint32_t&, const uint32_t&>(sMagic1, sMagic2));
  return IPC_OK();
}


//-----------------------------------------------------------------------------
// child

TestAsyncReturnsChild::TestAsyncReturnsChild()
{
  MOZ_COUNT_CTOR(TestAsyncReturnsChild);
}

TestAsyncReturnsChild::~TestAsyncReturnsChild()
{
  MOZ_COUNT_DTOR(TestAsyncReturnsChild);
}

mozilla::ipc::IPCResult
TestAsyncReturnsChild::RecvNoReturn(NoReturnResolver&& aResolve)
{
  // Not resolving the promise intentionally
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestAsyncReturnsChild::RecvPing(PingResolver&& aResolve)
{
  SendPong()->Then(MessageLoop::current()->SerialEventTarget(), __func__,
                   [aResolve](const Tuple<uint32_t, uint32_t>& aParam) {
                     if (Get<0>(aParam) == sMagic1 && Get<1>(aParam) == sMagic2) {
                       passed("take two arguments");
                     } else {
                       fail("get two argument but has wrong value");
                     }
                     aResolve(true);
                   },
                   [](PromiseRejectReason aReason) {
                     fail("sending Pong");
                   });
  return IPC_OK();
}


} // namespace _ipdltest
} // namespace mozilla
