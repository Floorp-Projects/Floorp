#ifndef mozilla__ipdltest_TestAsyncReturns_h
#define mozilla__ipdltest_TestAsyncReturns_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestAsyncReturnsParent.h"
#include "mozilla/_ipdltest/PTestAsyncReturnsChild.h"

namespace mozilla {
namespace _ipdltest {


class TestAsyncReturnsParent :
    public PTestAsyncReturnsParent
{
public:
  TestAsyncReturnsParent();
  virtual ~TestAsyncReturnsParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

protected:
  mozilla::ipc::IPCResult RecvPong(PongResolver&& aResolve) override;

  virtual void ActorDestroy(ActorDestroyReason why) override
  {
    if (NormalShutdown != why)
      fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }
};


class TestAsyncReturnsChild :
    public PTestAsyncReturnsChild
{
public:
  TestAsyncReturnsChild();
  virtual ~TestAsyncReturnsChild();

protected:
  mozilla::ipc::IPCResult RecvPing(PingResolver&& aResolve) override;
  mozilla::ipc::IPCResult RecvNoReturn(NoReturnResolver&& aResolve) override;

  virtual void ActorDestroy(ActorDestroyReason why) override
  {
    if (NormalShutdown != why)
      fail("unexpected destruction!");
    QuitChild();
  }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestAsyncReturns_h
