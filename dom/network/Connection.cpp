/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Connection.h"
#include "ConnectionMainThread.h"
#include "ConnectionWorker.h"
#include "Constants.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/WorkerPrivate.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define CHANGE_EVENT_NAME u"typechange"_ns

namespace mozilla::dom::network {

// Don't use |Connection| alone, since that confuses nsTraceRefcnt since
// we're not the only class with that name.
NS_IMPL_ISUPPORTS_INHERITED0(dom::network::Connection, DOMEventTargetHelper)

Connection::Connection(nsPIDOMWindowInner* aWindow,
                       bool aShouldResistFingerprinting)
    : DOMEventTargetHelper(aWindow),
      mShouldResistFingerprinting(aShouldResistFingerprinting),
      mType(static_cast<ConnectionType>(kDefaultType)),
      mIsWifi(kDefaultIsWifi),
      mDHCPGateway(kDefaultDHCPGateway),
      mBeenShutDown(false) {
  Telemetry::Accumulate(Telemetry::NETWORK_CONNECTION_COUNT, 1);
}

Connection::~Connection() {
  NS_ASSERT_OWNINGTHREAD(Connection);
  MOZ_ASSERT(mBeenShutDown);
}

void Connection::Shutdown() {
  NS_ASSERT_OWNINGTHREAD(Connection);

  if (mBeenShutDown) {
    return;
  }

  mBeenShutDown = true;
  ShutdownInternal();
}

JSObject* Connection::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return NetworkInformation_Binding::Wrap(aCx, this, aGivenProto);
}

void Connection::Update(ConnectionType aType, bool aIsWifi,
                        uint32_t aDHCPGateway, bool aNotify) {
  NS_ASSERT_OWNINGTHREAD(Connection);

  ConnectionType previousType = mType;

  mType = aType;
  mIsWifi = aIsWifi;
  mDHCPGateway = aDHCPGateway;

  if (aNotify && previousType != aType && !mShouldResistFingerprinting) {
    DispatchTrustedEvent(CHANGE_EVENT_NAME);
  }
}

/* static */
Connection* Connection::CreateForWindow(nsPIDOMWindowInner* aWindow,
                                        bool aShouldResistFingerprinting) {
  MOZ_ASSERT(aWindow);
  return new ConnectionMainThread(aWindow, aShouldResistFingerprinting);
}

/* static */
already_AddRefed<Connection> Connection::CreateForWorker(
    WorkerPrivate* aWorkerPrivate, ErrorResult& aRv) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  return ConnectionWorker::Create(aWorkerPrivate, aRv);
}

}  // namespace mozilla::dom::network
