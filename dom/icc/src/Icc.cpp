/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Icc.h"

#include "mozilla/dom/MozIccBinding.h"
#include "mozilla/dom/MozStkCommandEvent.h"
#include "nsIDOMDOMRequest.h"
#include "nsIDOMIccInfo.h"
#include "nsJSON.h"
#include "nsRadioInterfaceLayer.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::dom;

Icc::Icc(nsPIDOMWindow* aWindow, long aClientId, const nsAString& aIccId)
  : mLive(true)
  , mClientId(aClientId)
  , mIccId(aIccId)
{
  SetIsDOMBinding();
  BindToOwner(aWindow);

  mProvider = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);

  // Not being able to acquire the provider isn't fatal since we check
  // for it explicitly below.
  if (!mProvider) {
    NS_WARNING("Could not acquire nsIIccProvider!");
  }
}

void
Icc::Shutdown()
{
  mProvider = nullptr;
  mLive = false;
}

nsresult
Icc::NotifyEvent(const nsAString& aName)
{
  return DispatchTrustedEvent(aName);
}

nsresult
Icc::NotifyStkEvent(const nsAString& aName, const nsAString& aMessage)
{
  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoPushJSContext cx(sc->GetNativeContext());
  JS::Rooted<JS::Value> value(cx);

  if (!aMessage.IsEmpty()) {
    nsCOMPtr<nsIJSON> json(new nsJSON());
    nsresult rv = json->DecodeToJSVal(aMessage, cx, &value);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    value = JS::NullValue();
  }

  MozStkCommandEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mCommand = value;

  nsRefPtr<MozStkCommandEvent> event =
    MozStkCommandEvent::Constructor(this, aName, init);

  return DispatchTrustedEvent(event);
}

// WrapperCache

JSObject*
Icc::WrapObject(JSContext* aCx)
{
  return MozIccBinding::Wrap(aCx, this);
}

// MozIcc WebIDL

already_AddRefed<nsIDOMMozIccInfo>
Icc::GetIccInfo() const
{
  if (!mProvider) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMMozIccInfo> iccInfo;
  nsresult rv = mProvider->GetIccInfo(mClientId, getter_AddRefs(iccInfo));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return iccInfo.forget();
}

void
Icc::GetCardState(nsString& aCardState) const
{
  aCardState.SetIsVoid(true);

  if (!mProvider) {
    return;
  }

  nsresult rv = mProvider->GetCardState(mClientId, aCardState);
  if (NS_FAILED(rv)) {
    aCardState.SetIsVoid(true);
  }
}

void
Icc::SendStkResponse(const JSContext* aCx, JS::Handle<JS::Value> aCommand,
                     JS::Handle<JS::Value> aResponse, ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = mProvider->SendStkResponse(mClientId, GetOwner(), aCommand,
                                           aResponse);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
Icc::SendStkMenuSelection(uint16_t aItemIdentifier, bool aHelpRequested,
                          ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = mProvider->SendStkMenuSelection(mClientId,
                                                GetOwner(),
                                                aItemIdentifier,
                                                aHelpRequested);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
Icc::SendStkTimerExpiration(const JSContext* aCx, JS::Handle<JS::Value> aTimer,
                            ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = mProvider->SendStkTimerExpiration(mClientId, GetOwner(),
                                                  aTimer);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
Icc::SendStkEventDownload(const JSContext* aCx, JS::Handle<JS::Value> aEvent,
                          ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = mProvider->SendStkEventDownload(mClientId, GetOwner(), aEvent);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

already_AddRefed<nsISupports>
Icc::GetCardLock(const nsAString& aLockType, ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->GetCardLockState(mClientId, GetOwner(), aLockType,
                                            getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<nsISupports>
Icc::UnlockCardLock(const JSContext* aCx, JS::Handle<JS::Value> aInfo,
                    ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->UnlockCardLock(mClientId, GetOwner(), aInfo,
                                          getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<nsISupports>
Icc::SetCardLock(const JSContext* aCx, JS::Handle<JS::Value> aInfo,
                 ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SetCardLock(mClientId, GetOwner(), aInfo,
                                       getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<nsISupports>
Icc::GetCardLockRetryCount(const nsAString& aLockType, ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->GetCardLockRetryCount(mClientId,
                                                 GetOwner(),
                                                 aLockType,
                                                 getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<nsISupports>
Icc::ReadContacts(const nsAString& aContactType, ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->ReadContacts(mClientId, GetOwner(), aContactType,
                                        getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<nsISupports>
Icc::UpdateContact(const JSContext* aCx, const nsAString& aContactType,
                   JS::Handle<JS::Value> aContact, const nsAString& aPin2,
                   ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->UpdateContact(mClientId, GetOwner(), aContactType,
                                         aContact, aPin2,
                                         getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<nsISupports>
Icc::IccOpenChannel(const nsAString& aAid, ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->IccOpenChannel(mClientId, GetOwner(), aAid,
                                          getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<nsISupports>
Icc::IccExchangeAPDU(const JSContext* aCx, int32_t aChannel,
                     JS::Handle<JS::Value> aApdu, ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->IccExchangeAPDU(mClientId, GetOwner(), aChannel,
                                           aApdu, getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<nsISupports>
Icc::IccCloseChannel(int32_t aChannel, ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->IccCloseChannel(mClientId, GetOwner(), aChannel,
                                           getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<nsISupports>
Icc::MatchMvno(const nsAString& aMvnoType,
               const nsAString& aMvnoData,
               ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->MatchMvno(mClientId, GetOwner(),
                                     aMvnoType, aMvnoData,
                                     getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}
