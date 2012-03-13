/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  rv = event->SetTrusted(PR_TRUE);
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

