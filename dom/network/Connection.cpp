/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include "mozilla/Hal.h"
#include "mozilla/dom/network/Connection.h"
#include "nsIDOMClassInfo.h"
#include "mozilla/Preferences.h"
#include "Constants.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define CHANGE_EVENT_NAME NS_LITERAL_STRING("typechange")

namespace mozilla {
namespace dom {
namespace network {

NS_IMPL_QUERY_INTERFACE_INHERITED(Connection, DOMEventTargetHelper,
                                  nsINetworkProperties)

// Don't use |Connection| alone, since that confuses nsTraceRefcnt since
// we're not the only class with that name.
NS_IMPL_ADDREF_INHERITED(dom::network::Connection, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(dom::network::Connection, DOMEventTargetHelper)

Connection::Connection(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mType(static_cast<ConnectionType>(kDefaultType))
  , mIsWifi(kDefaultIsWifi)
  , mDHCPGateway(kDefaultDHCPGateway)
{
  hal::RegisterNetworkObserver(this);

  hal::NetworkInformation networkInfo;
  hal::GetCurrentNetworkInformation(&networkInfo);

  UpdateFromNetworkInfo(networkInfo);
}

void
Connection::Shutdown()
{
  hal::UnregisterNetworkObserver(this);
}

NS_IMETHODIMP
Connection::GetIsWifi(bool *aIsWifi)
{
  *aIsWifi = mIsWifi;
  return NS_OK;
}

NS_IMETHODIMP
Connection::GetDhcpGateway(uint32_t *aGW)
{
  *aGW = mDHCPGateway;
  return NS_OK;
}

void
Connection::UpdateFromNetworkInfo(const hal::NetworkInformation& aNetworkInfo)
{
  mType = static_cast<ConnectionType>(aNetworkInfo.type());
  mIsWifi = aNetworkInfo.isWifi();
  mDHCPGateway = aNetworkInfo.dhcpGateway();
}

void
Connection::Notify(const hal::NetworkInformation& aNetworkInfo)
{
  ConnectionType previousType = mType;

  UpdateFromNetworkInfo(aNetworkInfo);

  if (previousType == mType) {
    return;
  }

  DispatchTrustedEvent(CHANGE_EVENT_NAME);
}

JSObject*
Connection::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return NetworkInformationBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace network
} // namespace dom
} // namespace mozilla
