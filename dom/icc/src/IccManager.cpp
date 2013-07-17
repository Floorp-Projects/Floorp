/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/IccManager.h"

#include "GeneratedEvents.h"
#include "mozilla/dom/MozStkCommandEvent.h"
#include "mozilla/Services.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMIccInfo.h"
#include "nsJSON.h"
#include "SimToolKit.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

using namespace mozilla::dom;

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

DOMCI_DATA(MozIccManager, IccManager)

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

  // TODO: Bug 814637 - WebIccManager API: support multiple sim cards
  // In Multi-sim, there is more than one client in iccProvider. Each client
  // represents a icc service. To maintain the backward compatibility with
  // single sim, we always use client 0 for now. Adding support for multiple sim
  // will be addressed in bug 814637.
  mClientId = 0;

  mListener = new Listener(this);
  DebugOnly<nsresult> rv = mProvider->RegisterIccMsg(mClientId, mListener);
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
    mProvider->UnregisterIccMsg(mClientId, mListener);
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

  mProvider->SendStkResponse(mClientId, GetOwner(), aCommand, aResponse);
  return NS_OK;
}

NS_IMETHODIMP
IccManager::SendStkMenuSelection(uint16_t aItemIdentifier, bool aHelpRequested)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  mProvider->SendStkMenuSelection(mClientId, GetOwner(), aItemIdentifier, aHelpRequested);
  return NS_OK;
}

NS_IMETHODIMP
IccManager::SendStkTimerExpiration(const JS::Value& aTimer)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  mProvider->SendStkTimerExpiration(mClientId, GetOwner(), aTimer);
  return NS_OK;
}

NS_IMETHODIMP
IccManager::SendStkEventDownload(const JS::Value& aEvent)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  mProvider->SendStkEventDownload(mClientId, GetOwner(), aEvent);
  return NS_OK;
}

NS_IMETHODIMP
IccManager::GetIccInfo(nsIDOMMozIccInfo** aIccInfo)
{
  *aIccInfo = nullptr;

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetIccInfo(mClientId, aIccInfo);
}

NS_IMETHODIMP
IccManager::GetCardState(nsAString& cardState)
{
  cardState.SetIsVoid(true);

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetCardState(mClientId, cardState);
}

NS_IMETHODIMP
IccManager::GetCardLock(const nsAString& aLockType, nsIDOMDOMRequest** aDomRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetCardLockState(mClientId, GetOwner(), aLockType, aDomRequest);
}

NS_IMETHODIMP
IccManager::SetCardLock(const JS::Value& aInfo, nsIDOMDOMRequest** aDomRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetCardLock(mClientId, GetOwner(), aInfo, aDomRequest);
}

NS_IMETHODIMP
IccManager::UnlockCardLock(const JS::Value& aInfo, nsIDOMDOMRequest** aDomRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->UnlockCardLock(mClientId, GetOwner(), aInfo, aDomRequest);
}

NS_IMETHODIMP
IccManager::GetCardLockRetryCount(const nsAString& aLockType, nsIDOMDOMRequest** aDomRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetCardLockRetryCount(mClientId, GetOwner(), aLockType, aDomRequest);
}

NS_IMETHODIMP
IccManager::IccOpenChannel(const nsAString& aAid, nsIDOMDOMRequest** aRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->IccOpenChannel(mClientId, GetOwner(), aAid, aRequest);
}

NS_IMETHODIMP
IccManager::IccExchangeAPDU(int32_t aChannel, const jsval& aApdu, nsIDOMDOMRequest** aRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->IccExchangeAPDU(mClientId, GetOwner(), aChannel, aApdu, aRequest);
}

NS_IMETHODIMP
IccManager::IccCloseChannel(int32_t aChannel, nsIDOMDOMRequest** aRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->IccCloseChannel(mClientId, GetOwner(), aChannel, aRequest);
}

NS_IMETHODIMP
IccManager::ReadContacts(const nsAString& aContactType, nsIDOMDOMRequest** aRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->ReadContacts(mClientId, GetOwner(), aContactType, aRequest);
}

NS_IMETHODIMP
IccManager::UpdateContact(const nsAString& aContactType,
                          const JS::Value& aContact,
                          const nsAString& aPin2,
                          nsIDOMDOMRequest** aRequest)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->UpdateContact(mClientId, GetOwner(), aContactType, aContact, aPin2, aRequest);
}

NS_IMPL_EVENT_HANDLER(IccManager, stkcommand)
NS_IMPL_EVENT_HANDLER(IccManager, stksessionend)
NS_IMPL_EVENT_HANDLER(IccManager, cardstatechange)
NS_IMPL_EVENT_HANDLER(IccManager, iccinfochange)

// nsIIccListener

NS_IMETHODIMP
IccManager::NotifyStkCommand(const nsAString& aMessage)
{
  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoPushJSContext cx(sc->GetNativeContext());
  JS::Rooted<JS::Value> value(cx);

  if (!aMessage.IsEmpty()) {
    nsCOMPtr<nsIJSON> json(new nsJSON());
    nsresult rv = json->DecodeToJSVal(aMessage, cx, value.address());
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    value = JSVAL_VOID;
  }

  MozStkCommandEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mCommand = value;

  nsRefPtr<MozStkCommandEvent> event =
    MozStkCommandEvent::Constructor(this, NS_LITERAL_STRING("stkcommand"), init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
IccManager::NotifyStkSessionEnd()
{
  return DispatchTrustedEvent(NS_LITERAL_STRING("stksessionend"));
}

NS_IMETHODIMP
IccManager::NotifyCardStateChanged()
{
  return DispatchTrustedEvent(NS_LITERAL_STRING("cardstatechange"));
}

NS_IMETHODIMP
IccManager::NotifyIccInfoChanged()
{
  return DispatchTrustedEvent(NS_LITERAL_STRING("iccinfochange"));
}
