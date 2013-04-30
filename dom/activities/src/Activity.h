/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_activities_Activity_h
#define mozilla_dom_activities_Activity_h

#include "DOMRequest.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsIActivityProxy.h"
#include "mozilla/Preferences.h"

#define NS_DOMACTIVITY_CID                          \
 {0x1c5b0930, 0xc90c, 0x4e9c, {0xaf, 0x4e, 0xb0, 0xb7, 0xa6, 0x59, 0xb4, 0xed}}

#define NS_DOMACTIVITY_CONTRACTID "@mozilla.org/dom/activity;1"

namespace mozilla {
namespace dom {

class Activity : public DOMRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(Activity, DOMRequest)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  static bool PrefEnabled()
  {
    return Preferences::GetBool("dom.sysmsg.enabled", false);
  }

  static already_AddRefed<Activity>
  Constructor(const GlobalObject& aOwner,
              nsIDOMMozActivityOptions* aOptions,
              ErrorResult& aRv)
  {
    nsRefPtr<Activity> activity = new Activity();
    aRv = activity->Initialize(aOwner.Get(), aOptions);
    return activity.forget();
  }

  Activity();

protected:
  nsresult Initialize(nsISupports* aOwner,
                      nsIDOMMozActivityOptions* aOptions);

  nsCOMPtr<nsIActivityProxy> mProxy;

  ~Activity();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_activities_Activity_h
