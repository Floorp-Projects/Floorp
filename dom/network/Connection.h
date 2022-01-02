/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_network_Connection_h
#define mozilla_dom_network_Connection_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/NetworkInformationBinding.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {

namespace hal {
class NetworkInformation;
}  // namespace hal

namespace dom {

class WorkerPrivate;

namespace network {

class Connection : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  static Connection* CreateForWindow(nsPIDOMWindowInner* aWindow);

  static already_AddRefed<Connection> CreateForWorker(
      WorkerPrivate* aWorkerPrivate, ErrorResult& aRv);

  void Shutdown();

  // WebIDL

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  ConnectionType Type() const {
    return nsContentUtils::ShouldResistFingerprinting()
               ? static_cast<ConnectionType>(ConnectionType::Unknown)
               : mType;
  }

  bool GetIsWifi() {
    NS_ASSERT_OWNINGTHREAD(Connection);

    return mIsWifi;
  }
  uint32_t GetDhcpGateway() {
    NS_ASSERT_OWNINGTHREAD(Connection);

    return mDHCPGateway;
  }

  IMPL_EVENT_HANDLER(typechange)

 protected:
  Connection(nsPIDOMWindowInner* aWindow);
  virtual ~Connection();

  void Update(ConnectionType aType, bool aIsWifi, uint32_t aDHCPGateway,
              bool aNotify);

  virtual void ShutdownInternal() = 0;

 private:
  /**
   * The type of current connection.
   */
  ConnectionType mType;

  /**
   * If the connection is WIFI
   */
  bool mIsWifi;

  /**
   * DHCP Gateway information for IPV4, in network byte order. 0 if unassigned.
   */
  uint32_t mDHCPGateway;

  bool mBeenShutDown;
};

}  // namespace network
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_network_Connection_h
