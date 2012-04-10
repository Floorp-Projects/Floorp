/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMClassInfo.h"
#include "IccManager.h"
#include "SimToolKit.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

DOMCI_DATA(MozIccManager, mozilla::dom::icc::IccManager)

namespace mozilla {
namespace dom {
namespace icc {

NS_IMPL_CYCLE_COLLECTION_CLASS(IccManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IccManager,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(stkcommand)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(stksessionend)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IccManager,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(stkcommand)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(stksessionend)
  tmp->mProvider = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IccManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozIccManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozIccManager)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozIccManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IccManager, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IccManager, nsDOMEventTargetHelper)

IccManager::IccManager()
{
  mProvider = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);

  // Not being able to acquire the provider isn't fatal since we check
  // for it explicitly below.
  if (!mProvider) {
    NS_WARNING("Could not acquire nsIMobileConnectionProvider!");
  }
}

void
IccManager::Init(nsPIDOMWindow* aWindow)
{
  BindToOwner(aWindow);
}

void
IccManager::Shutdown()
{
}

// nsIObserver

NS_IMETHODIMP
IccManager::Observe(nsISupports* aSubject,
                    const char* aTopic,
                    const PRUnichar* aData)
{
  return NS_OK;
}

// nsIDOMMozIccManager

NS_IMETHODIMP
IccManager::SendStkResponse(const JS::Value& aResponse)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  mProvider->SendStkResponse(GetOwner(), aResponse);
  return NS_OK;
}

NS_IMETHODIMP
IccManager::SendStkMenuSelection(PRUint16 aItemIdentifier, bool aHelpRequested)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  mProvider->SendStkMenuSelection(GetOwner(), aItemIdentifier, aHelpRequested);
  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(IccManager, stkcommand)
NS_IMPL_EVENT_HANDLER(IccManager, stksessionend)

} // namespace icc
} // namespace dom
} // namespace mozilla
