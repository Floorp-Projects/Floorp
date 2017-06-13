/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Connection.h"
#include "ConnectionMainThread.h"
#include "ConnectionWorker.h"
#include "nsIDOMClassInfo.h"
#include "Constants.h"
#include "mozilla/Telemetry.h"
#include "WorkerPrivate.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define CHANGE_EVENT_NAME NS_LITERAL_STRING("typechange")

namespace mozilla {
namespace dom {

using namespace workers;

namespace network {

NS_IMPL_QUERY_INTERFACE_INHERITED(Connection, DOMEventTargetHelper,
                                  nsINetworkProperties)

// Don't use |Connection| alone, since that confuses nsTraceRefcnt since
// we're not the only class with that name.
NS_IMPL_ADDREF_INHERITED(dom::network::Connection, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(dom::network::Connection, DOMEventTargetHelper)

Connection::Connection(nsPIDOMWindowInner* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mType(static_cast<ConnectionType>(kDefaultType))
  , mIsWifi(kDefaultIsWifi)
  , mDHCPGateway(kDefaultDHCPGateway)
  , mBeenShutDown(false)
{
  Telemetry::Accumulate(Telemetry::NETWORK_CONNECTION_COUNT, 1);
}

Connection::~Connection()
{
  NS_ASSERT_OWNINGTHREAD(Connection);
  MOZ_ASSERT(mBeenShutDown);
}

void
Connection::Shutdown()
{
  NS_ASSERT_OWNINGTHREAD(Connection);

  if (mBeenShutDown) {
    return;
  }

  mBeenShutDown = true;
  ShutdownInternal();
}

NS_IMETHODIMP
Connection::GetIsWifi(bool* aIsWifi)
{
  NS_ENSURE_ARG_POINTER(aIsWifi);
  NS_ASSERT_OWNINGTHREAD(Connection);

  *aIsWifi = mIsWifi;
  return NS_OK;
}

NS_IMETHODIMP
Connection::GetDhcpGateway(uint32_t* aGW)
{
  NS_ENSURE_ARG_POINTER(aGW);
  NS_ASSERT_OWNINGTHREAD(Connection);

  *aGW = mDHCPGateway;
  return NS_OK;
}

JSObject*
Connection::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return NetworkInformationBinding::Wrap(aCx, this, aGivenProto);
}

void
Connection::Update(ConnectionType aType, bool aIsWifi, bool aDHCPGateway,
                   bool aNotify)
{
  NS_ASSERT_OWNINGTHREAD(Connection);

  ConnectionType previousType = mType;

  mType = aType;
  mIsWifi = aIsWifi;
  mDHCPGateway = aDHCPGateway;

  if (aNotify && previousType != aType &&
      !nsContentUtils::ShouldResistFingerprinting()) {
    DispatchTrustedEvent(CHANGE_EVENT_NAME);
  }
}

/* static */ bool
Connection::IsEnabled(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.netinfo.enabled");
  }

  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  return workerPrivate->NetworkInformationEnabled();
}

/* static */ Connection*
Connection::CreateForWindow(nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(aWindow);
  return new ConnectionMainThread(aWindow);
}

/* static */ already_AddRefed<Connection>
Connection::CreateForWorker(workers::WorkerPrivate* aWorkerPrivate,
                            ErrorResult& aRv)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  return ConnectionWorker::Create(aWorkerPrivate, aRv);
}

} // namespace network
} // namespace dom
} // namespace mozilla
