/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_IccManager_h
#define mozilla_dom_icc_IccManager_h

#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIDOMIccManager.h"
#include "nsIIccProvider.h"

namespace mozilla {
namespace dom {
namespace icc {

class IccManager : public nsDOMEventTargetHelper
                 , public nsIDOMMozIccManager
{
  /**
   * Class IccManager doesn't actually inherit nsIIccListener. Instead, it owns
   * an nsIIccListener derived instance mListener and passes it to
   * nsIIccProvider. The onreceived events are first delivered to mListener and
   * then forwarded to its owner, IccManager. See also bug 775997 comment #51.
   */
  class Listener;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMMOZICCMANAGER
  NS_DECL_NSIICCLISTENER

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)

  IccManager();

  void Init(nsPIDOMWindow *aWindow);
  void Shutdown();

private:
  nsCOMPtr<nsIIccProvider> mProvider;
  nsRefPtr<Listener> mListener;
};

} // namespace icc
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_icc_IccManager_h
