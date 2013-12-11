/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IccManager_h
#define mozilla_dom_IccManager_h

#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIDOMIccManager.h"
#include "nsIIccProvider.h"
#include "nsTArrayHelpers.h"

namespace mozilla {
namespace dom {

class IccListener;

class IccManager MOZ_FINAL : public nsDOMEventTargetHelper
                           , public nsIDOMMozIccManager
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMMOZICCMANAGER

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(IccManager,
                                                         nsDOMEventTargetHelper)

  IccManager(nsPIDOMWindow* aWindow);
  ~IccManager();

  void
  Shutdown();

  nsresult
  NotifyIccAdd(const nsAString& aIccId);

  nsresult
  NotifyIccRemove(const nsAString& aIccId);

private:
  nsTArray<nsRefPtr<IccListener>> mIccListeners;

  // Cached iccIds js array object. Cleared whenever the NotifyIccAdd() or
  // NotifyIccRemove() is called, and then rebuilt once a page looks for the
  // iccIds attribute.
  JS::Heap<JSObject*> mJsIccIds;
  bool mRooted;

  void Root();
  void Unroot();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_IccManager_h
