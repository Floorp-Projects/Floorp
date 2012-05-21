/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include "mozilla/Hal.h"
#include "Connection.h"
#include "nsIDOMClassInfo.h"
#include "mozilla/Preferences.h"
#include "nsDOMEvent.h"
#include "Constants.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define CHANGE_EVENT_NAME NS_LITERAL_STRING("change")

DOMCI_DATA(MozConnection, mozilla::dom::network::Connection)

namespace mozilla {
namespace dom {
namespace network {

const char* Connection::sMeteredPrefName     = "dom.network.metered";
const bool  Connection::sMeteredDefaultValue = false;

NS_IMPL_CYCLE_COLLECTION_CLASS(Connection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Connection,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(change)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Connection,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(change)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Connection)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozConnection)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozConnection)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozConnection)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Connection, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Connection, nsDOMEventTargetHelper)

Connection::Connection()
  : mCanBeMetered(kDefaultCanBeMetered)
  , mBandwidth(kDefaultBandwidth)
{
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

NS_IMETHODIMP
Connection::GetBandwidth(double* aBandwidth)
{
  if (mBandwidth == kDefaultBandwidth) {
    *aBandwidth = std::numeric_limits<double>::infinity();
    return NS_OK;
  }

  *aBandwidth = mBandwidth;
  return NS_OK;
}

NS_IMETHODIMP
Connection::GetMetered(bool* aMetered)
{
  if (!mCanBeMetered) {
    *aMetered = false;
    return NS_OK;
  }

  *aMetered = Preferences::GetBool(sMeteredPrefName,
                                   sMeteredDefaultValue);
  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(Connection, change)

nsresult
Connection::DispatchTrustedEventToSelf(const nsAString& aEventName)
{
  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nsnull, nsnull);
  nsresult rv = event->InitEvent(aEventName, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
Connection::UpdateFromNetworkInfo(const hal::NetworkInformation& aNetworkInfo)
{
  mBandwidth = aNetworkInfo.bandwidth();
  mCanBeMetered = aNetworkInfo.canBeMetered();
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

  DispatchTrustedEventToSelf(CHANGE_EVENT_NAME);
}

} // namespace network
} // namespace dom
} // namespace mozilla

