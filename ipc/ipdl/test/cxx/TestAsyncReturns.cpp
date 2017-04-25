#include "TestAsyncReturns.h"

#include "IPDLUnitTests.h"      // fail etc.

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
  if (!AbstractThread::GetCurrent()) {
    fail("AbstractThread not initalized");
  }
  SendNoReturn()->Then(AbstractThread::GetCurrent(), __func__,
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
  SendPing()->Then(AbstractThread::GetCurrent(), __func__,
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
TestAsyncReturnsParent::RecvPong(RefPtr<PongPromise>&& aPromise)
{
  aPromise->Resolve(MakeTuple(sMagic1, sMagic2), __func__);
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
TestAsyncReturnsChild::RecvNoReturn(RefPtr<NoReturnPromise>&& aPromise)
{
  // Leak the promise intentionally
  Unused << do_AddRef(aPromise);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestAsyncReturnsChild::RecvPing(RefPtr<PingPromise>&& aPromise)
{
  if (!AbstractThread::GetCurrent()) {
    fail("AbstractThread not initalized");
  }
  SendPong()->Then(AbstractThread::GetCurrent(), __func__,
                   [aPromise](const Tuple<uint32_t, uint32_t>& aParam) {
                     if (Get<0>(aParam) == sMagic1 && Get<1>(aParam) == sMagic2) {
                       passed("take two arguments");
                     } else {
                       fail("get two argument but has wrong value");
                     }
                     aPromise->Resolve(true, __func__);
                   },
                   [](PromiseRejectReason aReason) {
                     fail("sending Pong");
                   });
  return IPC_OK();
}


} // namespace _ipdltest
} // namespace mozilla
