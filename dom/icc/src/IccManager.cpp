/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Services.h"
#include "nsIDOMClassInfo.h"
#include "nsIObserverService.h"
#include "IccManager.h"
#include "SimToolKit.h"
#include "StkCommandEvent.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

#define STKCOMMAND_EVENTNAME      NS_LITERAL_STRING("stkcommand")
#define STKSESSIONEND_EVENTNAME   NS_LITERAL_STRING("stksessionend")

DOMCI_DATA(MozIccManager, mozilla::dom::icc::IccManager)

namespace mozilla {
namespace dom {
namespace icc {

const char* kStkCommandTopic     = "icc-manager-stk-command";
const char* kStkSessionEndTopic  = "icc-manager-stk-session-end";

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IccManager,
                                                  nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IccManager,
                                                nsDOMEventTargetHelper)
  tmp->mProvider = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IccManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozIccManager)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
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

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    NS_WARNING("Could not acquire nsIObserverService!");
    return;
  }

  obs->AddObserver(this, kStkCommandTopic, false);
  obs->AddObserver(this, kStkSessionEndTopic, false);
}

void
IccManager::Shutdown()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    NS_WARNING("Could not acquire nsIObserverService!");
    return;
  }

  obs->RemoveObserver(this, kStkCommandTopic);
  obs->RemoveObserver(this, kStkSessionEndTopic);
}

// nsIObserver

NS_IMETHODIMP
IccManager::Observe(nsISupports* aSubject,
                    const char* aTopic,
                    const PRUnichar* aData)
{
  if (!strcmp(aTopic, kStkCommandTopic)) {
    nsString stkMsg;
    stkMsg.Assign(aData);
    nsRefPtr<StkCommandEvent> event = StkCommandEvent::Create(stkMsg);

    NS_ASSERTION(event, "This should never fail!");

    nsresult rv = event->Dispatch(ToIDOMEventTarget(), STKCOMMAND_EVENTNAME);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  if (!strcmp(aTopic, kStkSessionEndTopic)) {
    DispatchTrustedEvent(STKSESSIONEND_EVENTNAME);
    return NS_OK;
  }

  MOZ_NOT_REACHED("Unknown observer topic!");

  return NS_OK;
}

// nsIDOMMozIccManager

NS_IMETHODIMP
IccManager::SendStkResponse(const JS::Value& aCommand,
                            const JS::Value& aResponse)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  mProvider->SendStkResponse(GetOwner(), aCommand, aResponse);
  return NS_OK;
}

NS_IMETHODIMP
IccManager::SendStkMenuSelection(uint16_t aItemIdentifier, bool aHelpRequested)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  mProvider->SendStkMenuSelection(GetOwner(), aItemIdentifier, aHelpRequested);
  return NS_OK;
}

NS_IMETHODIMP
IccManager::SendStkTimerExpiration(const JS::Value& aTimer)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  mProvider->SendStkTimerExpiration(GetOwner(), aTimer);
  return NS_OK;
}

NS_IMETHODIMP
IccManager::SendStkEventDownload(const JS::Value& aEvent)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  mProvider->SendStkEventDownload(GetOwner(), aEvent);
  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(IccManager, stkcommand)
NS_IMPL_EVENT_HANDLER(IccManager, stksessionend)

} // namespace icc
} // namespace dom
} // namespace mozilla
