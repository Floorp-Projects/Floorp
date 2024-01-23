/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include "mozilla/Hal.h"
#include "ConnectionWorker.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"

namespace mozilla::dom::network {

class ConnectionProxy final : public hal::NetworkObserver {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ConnectionProxy)

  static already_AddRefed<ConnectionProxy> Create(
      WorkerPrivate* aWorkerPrivate, ConnectionWorker* aConnection) {
    RefPtr<ConnectionProxy> proxy = new ConnectionProxy(aConnection);

    RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
        aWorkerPrivate, "ConnectionProxy", [proxy]() { proxy->Shutdown(); });
    if (NS_WARN_IF(!workerRef)) {
      return nullptr;
    }

    proxy->mWorkerRef = new ThreadSafeWorkerRef(workerRef);
    return proxy.forget();
  }

  ThreadSafeWorkerRef* WorkerRef() const { return mWorkerRef; }

  // For IObserver - main-thread only.
  void Notify(const hal::NetworkInformation& aNetworkInfo) override;

  void Shutdown();

  void Update(ConnectionType aType, bool aIsWifi, uint32_t aDHCPGateway) {
    MOZ_ASSERT(mConnection);
    MOZ_ASSERT(IsCurrentThreadRunningWorker());
    mConnection->Update(aType, aIsWifi, aDHCPGateway, true);
  }

 private:
  explicit ConnectionProxy(ConnectionWorker* aConnection)
      : mConnection(aConnection) {}

  ~ConnectionProxy() = default;

  // Raw pointer because the ConnectionWorker keeps alive the proxy.
  // This is touched only on the worker-thread and it's nullified when the
  // shutdown procedure starts.
  ConnectionWorker* mConnection;

  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

namespace {

// This class initializes the hal observer on the main-thread.
class InitializeRunnable : public WorkerMainThreadRunnable {
 private:
  // raw pointer because this is a sync runnable.
  ConnectionProxy* mProxy;
  hal::NetworkInformation& mNetworkInfo;

 public:
  InitializeRunnable(WorkerPrivate* aWorkerPrivate, ConnectionProxy* aProxy,
                     hal::NetworkInformation& aNetworkInfo)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "ConnectionWorker :: Initialize"_ns),
        mProxy(aProxy),
        mNetworkInfo(aNetworkInfo) {
    MOZ_ASSERT(aProxy);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override {
    MOZ_ASSERT(NS_IsMainThread());
    hal::RegisterNetworkObserver(mProxy);
    hal::GetCurrentNetworkInformation(&mNetworkInfo);
    return true;
  }
};

// This class turns down the hal observer on the main-thread.
class ShutdownRunnable : public WorkerMainThreadRunnable {
 private:
  // raw pointer because this is a sync runnable.
  ConnectionProxy* mProxy;

 public:
  ShutdownRunnable(WorkerPrivate* aWorkerPrivate, ConnectionProxy* aProxy)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "ConnectionWorker :: Shutdown"_ns),
        mProxy(aProxy) {
    MOZ_ASSERT(aProxy);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override {
    MOZ_ASSERT(NS_IsMainThread());
    hal::UnregisterNetworkObserver(mProxy);
    return true;
  }
};

class NotifyRunnable final : public WorkerRunnable {
 private:
  RefPtr<ConnectionProxy> mProxy;

  const ConnectionType mConnectionType;
  const bool mIsWifi;
  const uint32_t mDHCPGateway;

 public:
  NotifyRunnable(WorkerPrivate* aWorkerPrivate, ConnectionProxy* aProxy,
                 ConnectionType aType, bool aIsWifi, uint32_t aDHCPGateway)
      : WorkerRunnable(aWorkerPrivate, "NotifyRunnable"),
        mProxy(aProxy),
        mConnectionType(aType),
        mIsWifi(aIsWifi),
        mDHCPGateway(aDHCPGateway) {
    MOZ_ASSERT(aProxy);
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnWorkerThread();
    mProxy->Update(mConnectionType, mIsWifi, mDHCPGateway);
    return true;
  }
};

}  // anonymous namespace

/* static */
already_AddRefed<ConnectionWorker> ConnectionWorker::Create(
    WorkerPrivate* aWorkerPrivate, ErrorResult& aRv) {
  bool shouldResistFingerprinting = aWorkerPrivate->ShouldResistFingerprinting(
      RFPTarget::NavigatorConnection);
  RefPtr<ConnectionWorker> c = new ConnectionWorker(shouldResistFingerprinting);
  c->mProxy = ConnectionProxy::Create(aWorkerPrivate, c);
  if (!c->mProxy) {
    aRv.ThrowTypeError("The Worker thread is shutting down.");
    return nullptr;
  }

  hal::NetworkInformation networkInfo;
  RefPtr<InitializeRunnable> runnable =
      new InitializeRunnable(aWorkerPrivate, c->mProxy, networkInfo);

  runnable->Dispatch(Canceling, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  c->Update(static_cast<ConnectionType>(networkInfo.type()),
            networkInfo.isWifi(), networkInfo.dhcpGateway(), false);
  return c.forget();
}

ConnectionWorker::ConnectionWorker(bool aShouldResistFingerprinting)
    : Connection(nullptr, aShouldResistFingerprinting) {
  MOZ_ASSERT(IsCurrentThreadRunningWorker());
}

ConnectionWorker::~ConnectionWorker() { Shutdown(); }

void ConnectionWorker::ShutdownInternal() {
  MOZ_ASSERT(IsCurrentThreadRunningWorker());
  mProxy->Shutdown();
}

void ConnectionProxy::Notify(const hal::NetworkInformation& aNetworkInfo) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<NotifyRunnable> runnable =
      new NotifyRunnable(mWorkerRef->Private(), this,
                         static_cast<ConnectionType>(aNetworkInfo.type()),
                         aNetworkInfo.isWifi(), aNetworkInfo.dhcpGateway());
  runnable->Dispatch();
}

void ConnectionProxy::Shutdown() {
  MOZ_ASSERT(IsCurrentThreadRunningWorker());

  // Already shut down.
  if (!mConnection) {
    return;
  }

  mConnection = nullptr;

  RefPtr<ShutdownRunnable> runnable =
      new ShutdownRunnable(mWorkerRef->Private(), this);

  ErrorResult rv;
  // This runnable _must_ be executed.
  runnable->Dispatch(Killing, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
  }

  mWorkerRef = nullptr;
}

}  // namespace mozilla::dom::network
