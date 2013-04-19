/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_network_MobileConnection_h
#define mozilla_dom_network_MobileConnection_h

#include "nsIDOMMobileConnection.h"
#include "nsIMobileConnectionProvider.h"
#include "nsDOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

namespace icc {
  class IccManager;
} // namespace icc

namespace network {

class MobileConnection : public nsDOMEventTargetHelper
                       , public nsIDOMMozMobileConnection
{
  /**
   * Class MobileConnection doesn't actually inherit
   * nsIMobileConnectionListener. Instead, it owns an
   * nsIMobileConnectionListener derived instance mListener and passes it to
   * nsIMobileConnectionProvider. The onreceived events are first delivered to
   * mListener and then forwarded to its owner, MobileConnection. See also bug
   * 775997 comment #51.
   */
  class Listener;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMMOZMOBILECONNECTION
  NS_DECL_NSIMOBILECONNECTIONLISTENER

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)

  MobileConnection();

  void Init(nsPIDOMWindow *aWindow);
  void Shutdown();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MobileConnection,
                                           nsDOMEventTargetHelper)

private:
  nsCOMPtr<nsIMobileConnectionProvider> mProvider;
  nsRefPtr<Listener> mListener;
  nsRefPtr<icc::IccManager> mIccManager;
};

} // namespace network
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_network_MobileConnection_h
