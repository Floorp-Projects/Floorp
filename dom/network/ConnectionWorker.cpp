/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include "mozilla/Hal.h"
#include "ConnectionWorker.h"
#include "WorkerRunnable.h"

namespace mozilla {
namespace dom {
namespace network {

class ConnectionProxy final : public NetworkObserver
                            , public WorkerHolder
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ConnectionProxy)

  static already_AddRefed<ConnectionProxy>
  Create(WorkerPrivate* aWorkerPrivate, ConnectionWorker* aConnection)
  {
    RefPtr<ConnectionProxy> proxy =
      new ConnectionProxy(aWorkerPrivate, aConnection);
    if (!proxy->HoldWorker(aWorkerPrivate, Closing)) {
      proxy->mConnection = nullptr;
      return nullptr;
    }

    return proxy.forget();
  }

  // For IObserver - main-thread only.
  void Notify(const hal::NetworkInformation& aNetworkInfo) override;

  // Worker notification
  virtual bool Notify(Status aStatus) override
  {
    Shutdown();
    return true;
  }

  void Shutdown();

  void Update(ConnectionType aType, bool aIsWifi, bool aDHCPGateway)
  {
    MOZ_ASSERT(mConnection);
    mWorkerPrivate->AssertIsOnWorkerThread();
    mConnection->Update(aType, aIsWifi, aDHCPGateway, true);
  }

private:
  ConnectionProxy(WorkerPrivate* aWorkerPrivate, ConnectionWorker* aConnection)
    : mConnection(aConnection)
    , mWorkerPrivate(aWorkerPrivate)
  {
    MOZ_ASSERT(mWorkerPrivate);
    MOZ_ASSERT(mConnection);
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  ~ConnectionProxy() = default;

  // Raw pointer because the ConnectionWorker keeps alive the proxy.
  // This is touched only on the worker-thread and it's nullified when the
  // shutdown procedure starts.
  ConnectionWorker* mConnection;

  WorkerPrivate* mWorkerPrivate;
};

namespace {

// This class initializes the hal observer on the main-thread.
class InitializeRunnable : public WorkerMainThreadRunnable
{
private:
  // raw pointer because this is a sync runnable.
  ConnectionProxy* mProxy;
  hal::NetworkInformation& mNetworkInfo;

public:
  InitializeRunnable(WorkerPrivate* aWorkerPrivate,
                     ConnectionProxy* aProxy,
                     hal::NetworkInformation& aNetworkInfo)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               NS_LITERAL_CSTRING("ConnectionWorker :: Initialize"))
    , mProxy(aProxy)
    , mNetworkInfo(aNetworkInfo)
  {
    MOZ_ASSERT(aProxy);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun()
  {
    MOZ_ASSERT(NS_IsMainThread());
    hal::RegisterNetworkObserver(mProxy);
    hal::GetCurrentNetworkInformation(&mNetworkInfo);
    return true;
  }
};

// This class turns down the hal observer on the main-thread.
class ShutdownRunnable : public WorkerMainThreadRunnable
{
private:
  // raw pointer because this is a sync runnable.
  ConnectionProxy* mProxy;

public:
  ShutdownRunnable(WorkerPrivate* aWorkerPrivate, ConnectionProxy* aProxy)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               NS_LITERAL_CSTRING("ConnectionWorker :: Shutdown"))
    , mProxy(aProxy)
  {
    MOZ_ASSERT(aProxy);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun()
  {
    MOZ_ASSERT(NS_IsMainThread());
    hal::UnregisterNetworkObserver(mProxy);
    return true;
  }
};

class NotifyRunnable : public WorkerRunnable
{
private:
  RefPtr<ConnectionProxy> mProxy;

  const ConnectionType mConnectionType;
  const bool mIsWifi;
  const bool mDHCPGateway;

public:
  NotifyRunnable(WorkerPrivate* aWorkerPrivate,
                 ConnectionProxy* aProxy, ConnectionType aType,
                 bool aIsWifi, bool aDHCPGateway)
    : WorkerRunnable(aWorkerPrivate)
    , mProxy(aProxy)
    , mConnectionType(aType)
    , mIsWifi(aIsWifi)
    , mDHCPGateway(aDHCPGateway)
  {
    MOZ_ASSERT(aProxy);
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    mProxy->Update(mConnectionType, mIsWifi, mDHCPGateway);
    return true;
  }
};

} // anonymous namespace

/* static */ already_AddRefed<ConnectionWorker>
ConnectionWorker::Create(WorkerPrivate* aWorkerPrivate, ErrorResult& aRv)
{
  RefPtr<ConnectionWorker> c = new ConnectionWorker(aWorkerPrivate);
  c->mProxy = ConnectionProxy::Create(aWorkerPrivate, c);
  if (!c->mProxy) {
    aRv.ThrowTypeError<MSG_WORKER_THREAD_SHUTTING_DOWN>();
    return nullptr;
  }

  hal::NetworkInformation networkInfo;
  RefPtr<InitializeRunnable> runnable =
    new InitializeRunnable(aWorkerPrivate, c->mProxy, networkInfo);

  runnable->Dispatch(Terminating, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  c->Update(static_cast<ConnectionType>(networkInfo.type()),
            networkInfo.isWifi(), networkInfo.dhcpGateway(), false);
  return c.forget();
}

ConnectionWorker::ConnectionWorker(WorkerPrivate* aWorkerPrivate)
  : Connection(nullptr)
  , mWorkerPrivate(aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
}

ConnectionWorker::~ConnectionWorker()
{
  Shutdown();
}

void
ConnectionWorker::ShutdownInternal()
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mProxy->Shutdown();
}

void
ConnectionProxy::Notify(const hal::NetworkInformation& aNetworkInfo)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<NotifyRunnable> runnable =
    new NotifyRunnable(mWorkerPrivate, this,
                       static_cast<ConnectionType>(aNetworkInfo.type()),
                       aNetworkInfo.isWifi(), aNetworkInfo.dhcpGateway());
  runnable->Dispatch();
}

void
ConnectionProxy::Shutdown()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  // Already shut down.
  if (!mConnection) {
    return;
  }

  mConnection = nullptr;

  RefPtr<ShutdownRunnable> runnable =
    new ShutdownRunnable(mWorkerPrivate, this);

  ErrorResult rv;
  // This runnable _must_ be executed.
  runnable->Dispatch(Killing, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
  }

  ReleaseWorker();
}

} // namespace network
} // namespace dom
} // namespace mozilla
