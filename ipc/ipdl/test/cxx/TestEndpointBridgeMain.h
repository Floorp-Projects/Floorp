/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#ifndef mozilla__ipdltest_TestEndpointBridgeMain_h
#define mozilla__ipdltest_TestEndpointBridgeMain_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestEndpointBridgeMainParent.h"
#include "mozilla/_ipdltest/PTestEndpointBridgeMainChild.h"

#include "mozilla/_ipdltest/PTestEndpointBridgeSubParent.h"
#include "mozilla/_ipdltest/PTestEndpointBridgeSubChild.h"

#include "mozilla/_ipdltest/PTestEndpointBridgeMainSubParent.h"
#include "mozilla/_ipdltest/PTestEndpointBridgeMainSubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// "Main" process
//
class TestEndpointBridgeMainParent :
    public PTestEndpointBridgeMainParent
{
public:
  TestEndpointBridgeMainParent() {}
  virtual ~TestEndpointBridgeMainParent() {}

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return false; }

  void Main();

protected:
  bool RecvBridged(mozilla::ipc::Endpoint<PTestEndpointBridgeMainSubParent>&& endpoint) override;

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

class TestEndpointBridgeMainSubParent :
    public PTestEndpointBridgeMainSubParent
{
public:
  explicit TestEndpointBridgeMainSubParent()
  {}
  virtual ~TestEndpointBridgeMainSubParent() {}

protected:
  virtual bool RecvHello() override;
  virtual bool RecvHelloSync() override;
  virtual bool AnswerHelloRpc() override;

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

//-----------------------------------------------------------------------------
// "Sub" process --- child of "main"
//
class TestEndpointBridgeSubParent;

class TestEndpointBridgeMainChild :
    public PTestEndpointBridgeMainChild
{
public:
  TestEndpointBridgeMainChild();
  virtual ~TestEndpointBridgeMainChild() {}

protected:
  virtual bool RecvStart() override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

  IPDLUnitTestSubprocess* mSubprocess;
};

class TestEndpointBridgeSubParent :
    public PTestEndpointBridgeSubParent
{
public:
  TestEndpointBridgeSubParent() {}
  virtual ~TestEndpointBridgeSubParent() {}

  void Main();

protected:
  virtual bool RecvBridgeEm() override;

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

//-----------------------------------------------------------------------------
// "Subsub" process --- child of "sub"
//
class TestEndpointBridgeSubChild :
    public PTestEndpointBridgeSubChild
{
public:
  TestEndpointBridgeSubChild();
  virtual ~TestEndpointBridgeSubChild() {}

protected:
  virtual bool RecvPing() override;

  bool RecvBridged(Endpoint<PTestEndpointBridgeMainSubChild>&& endpoint) override;

  virtual void ActorDestroy(ActorDestroyReason why) override;
};

class TestEndpointBridgeMainSubChild :
    public PTestEndpointBridgeMainSubChild
{
public:
  explicit TestEndpointBridgeMainSubChild()
   : mGotHi(false)
  {}
  virtual ~TestEndpointBridgeMainSubChild() {}

protected:
  virtual bool RecvHi() override;
  virtual bool AnswerHiRpc() override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

  bool mGotHi;
};

} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestEndpointBridgeMain_h
