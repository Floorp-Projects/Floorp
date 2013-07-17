/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_network_Connection_h
#define mozilla_dom_network_Connection_h

#include "nsIDOMConnection.h"
#include "nsINetworkProperties.h"
#include "nsDOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Observer.h"
#include "Types.h"

namespace mozilla {

namespace hal {
class NetworkInformation;
} // namespace hal

namespace dom {
namespace network {

class Connection : public nsDOMEventTargetHelper
                 , public nsIDOMMozConnection
                 , public NetworkObserver
                 , public nsINetworkProperties
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMMOZCONNECTION
  NS_DECL_NSINETWORKPROPERTIES

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)

  Connection();

  void Init(nsPIDOMWindow *aWindow);
  void Shutdown();

  // For IObserver
  void Notify(const hal::NetworkInformation& aNetworkInfo);

private:
  /**
   * Update the connection information stored in the object using a
   * NetworkInformation object.
   */
  void UpdateFromNetworkInfo(const hal::NetworkInformation& aNetworkInfo);

  /**
   * If the connection is of a type that can be metered.
   */
  bool mCanBeMetered;

  /**
   * The connection bandwidth.
   */
  double mBandwidth;

  /**
   * If the connection is WIFI
   */
  bool mIsWifi;

  /**
   * DHCP Gateway information for IPV4, in network byte order. 0 if unassigned.
   */
  uint32_t mDHCPGateway;

  static const char* sMeteredPrefName;
  static const bool  sMeteredDefaultValue;
};

} // namespace network
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_network_Connection_h
