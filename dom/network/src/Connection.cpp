/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include "mozilla/Hal.h"
#include "mozilla/dom/network/Connection.h"
#include "mozilla/dom/MozConnectionBinding.h"
#include "nsIDOMClassInfo.h"
#include "mozilla/Preferences.h"
#include "nsDOMEvent.h"
#include "Constants.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define CHANGE_EVENT_NAME NS_LITERAL_STRING("change")

namespace mozilla {
namespace dom {
namespace network {

const char* Connection::sMeteredPrefName     = "dom.network.metered";
const bool  Connection::sMeteredDefaultValue = false;

NS_IMPL_QUERY_INTERFACE_INHERITED1(Connection, nsDOMEventTargetHelper,
                                   nsINetworkProperties)

// Don't use |Connection| alone, since that confuses nsTraceRefcnt since
// we're not the only class with that name.
NS_IMPL_ADDREF_INHERITED(dom::network::Connection, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(dom::network::Connection, nsDOMEventTargetHelper)

Connection::Connection()
  : mCanBeMetered(kDefaultCanBeMetered)
  , mBandwidth(kDefaultBandwidth)
  , mIsWifi(kDefaultIsWifi)
  , mDHCPGateway(kDefaultDHCPGateway)
{
  SetIsDOMBinding();
}

void
Connection::Init(nsPIDOMWindow* aWindow)
{
  BindToOwner(aWindow);

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

double
Connection::Bandwidth() const
{
  if (mBandwidth == kDefaultBandwidth) {
    return std::numeric_limits<double>::infinity();
  }

  return mBandwidth;
}

bool
Connection::Metered() const
{
  if (!mCanBeMetered) {
    return false;
  }

  return Preferences::GetBool(sMeteredPrefName, sMeteredDefaultValue);
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
  mBandwidth = aNetworkInfo.bandwidth();
  mCanBeMetered = aNetworkInfo.canBeMetered();
  mIsWifi = aNetworkInfo.isWifi();
  mDHCPGateway = aNetworkInfo.dhcpGateway();
}

void
Connection::Notify(const hal::NetworkInformation& aNetworkInfo)
{
  double previousBandwidth = mBandwidth;
  bool previousCanBeMetered = mCanBeMetered;

  UpdateFromNetworkInfo(aNetworkInfo);

  if (previousBandwidth == mBandwidth &&
      previousCanBeMetered == mCanBeMetered) {
    return;
  }

  DispatchTrustedEvent(CHANGE_EVENT_NAME);
}

JSObject*
Connection::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MozConnectionBinding::Wrap(aCx, aScope, this);
}

} // namespace network
} // namespace dom
} // namespace mozilla
