/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Services.h"
#include "nsIDOMClassInfo.h"
#include "IccManager.h"
#include "SimToolKit.h"
#include "StkCommandEvent.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

using namespace mozilla::dom::icc;

class IccManager::Listener : public nsIIccListener
{
  IccManager* mIccManager;

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIICCLISTENER(mIccManager)

  Listener(IccManager* aIccManager)
    : mIccManager(aIccManager)
  {
    MOZ_ASSERT(mIccManager);
  }

  void
  Disconnect()
  {
    MOZ_ASSERT(mIccManager);
    mIccManager = nullptr;
  }
};

NS_IMPL_ISUPPORTS1(IccManager::Listener, nsIIccListener)

DOMCI_DATA(MozIccManager, mozilla::dom::icc::IccManager)

NS_INTERFACE_MAP_BEGIN(IccManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozIccManager)
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
    NS_WARNING("Could not acquire nsIIccProvider!");
    return;
  }

  mListener = new Listener(this);
  DebugOnly<nsresult> rv = mProvider->RegisterIccMsg(mListener);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "Failed registering icc messages with provider");
}

void
IccManager::Init(nsPIDOMWindow* aWindow)
{
  BindToOwner(aWindow);
}

void
IccManager::Shutdown()
{
  if (mProvider && mListener) {
    mListener->Disconnect();
    mProvider->UnregisterIccMsg(mListener);
    mProvider = nullptr;
    mListener = nullptr;
  }
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

NS_IMETHODIMP
IccManager::IccOpenChannel(const nsAString& aAid, nsIDOMDOMRequest** aRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->IccOpenChannel(GetOwner(), aAid, aRequest);
}

NS_IMETHODIMP
IccManager::IccExchangeAPDU(int32_t aChannel, const jsval& aApdu, nsIDOMDOMRequest** aRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->IccExchangeAPDU(GetOwner(), aChannel, aApdu, aRequest);
}

NS_IMETHODIMP
IccManager::IccCloseChannel(int32_t aChannel, nsIDOMDOMRequest** aRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->IccCloseChannel(GetOwner(), aChannel, aRequest);
}

NS_IMETHODIMP
IccManager::UpdateContact(const nsAString& aContactType,
                          nsIDOMContact* aContact,
                          const nsAString& aPin2,
                          nsIDOMDOMRequest** aRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->UpdateContact(GetOwner(), aContactType, aContact, aPin2, aRequest);
}

NS_IMPL_EVENT_HANDLER(IccManager, stkcommand)
NS_IMPL_EVENT_HANDLER(IccManager, stksessionend)

// nsIIccListener

NS_IMETHODIMP
IccManager::NotifyStkCommand(const nsAString& aMessage)
{
  nsRefPtr<StkCommandEvent> event = StkCommandEvent::Create(this, aMessage);
  NS_ASSERTION(event, "This should never fail!");

  return event->Dispatch(ToIDOMEventTarget(), NS_LITERAL_STRING("stkcommand"));
}

NS_IMETHODIMP
IccManager::NotifyStkSessionEnd()
{
  return DispatchTrustedEvent(NS_LITERAL_STRING("stksessionend"));
}
