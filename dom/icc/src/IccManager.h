/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_IccManager_h
#define mozilla_dom_icc_IccManager_h

#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIDOMIccManager.h"
#include "nsIMobileConnectionProvider.h"
#include "nsIObserver.h"

namespace mozilla {
namespace dom {
namespace icc {

class IccManager : public nsDOMEventTargetHelper
                 , public nsIDOMMozIccManager
                 , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMMOZICCMANAGER

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  IccManager();

  void Init(nsPIDOMWindow *aWindow);
  void Shutdown();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IccManager,
                                           nsDOMEventTargetHelper)

private:
  nsCOMPtr<nsIMobileConnectionProvider> mProvider;

  NS_DECL_EVENT_HANDLER(stkcommand)
  NS_DECL_EVENT_HANDLER(stksessionend)
};

} // namespace icc
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_icc_IccManager_h
