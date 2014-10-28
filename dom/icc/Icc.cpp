/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Icc.h"

#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/IccInfo.h"
#include "mozilla/dom/MozStkCommandEvent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsIIccInfo.h"
#include "nsIIccProvider.h"
#include "nsJSON.h"
#include "nsRadioInterfaceLayer.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED(Icc, DOMEventTargetHelper, mIccInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Icc)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Icc, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Icc, DOMEventTargetHelper)

Icc::Icc(nsPIDOMWindow* aWindow, long aClientId, nsIIccInfo* aIccInfo)
  : mLive(true)
  , mClientId(aClientId)
{
  BindToOwner(aWindow);

  mProvider = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);

  if (aIccInfo) {
    aIccInfo->GetIccid(mIccId);
    UpdateIccInfo(aIccInfo);
  }

  // Not being able to acquire the provider isn't fatal since we check
  // for it explicitly below.
  if (!mProvider) {
    NS_WARNING("Could not acquire nsIIccProvider!");
  }
}

void
Icc::Shutdown()
{
  mIccInfo.SetNull();
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
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.InitWithLegacyErrorReporting(GetOwner()))) {
    return NS_ERROR_UNEXPECTED;
  }
  JSContext* cx = jsapi.cx();
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

void
Icc::UpdateIccInfo(nsIIccInfo* aIccInfo)
{
  if (!aIccInfo) {
    mIccInfo.SetNull();
    return;
  }

  nsCOMPtr<nsIGsmIccInfo> gsmIccInfo(do_QueryInterface(aIccInfo));
  if (gsmIccInfo) {
    if (mIccInfo.IsNull() || !mIccInfo.Value().IsMozGsmIccInfo()) {
      mIccInfo.SetValue().SetAsMozGsmIccInfo() = new GsmIccInfo(GetOwner());
    }
    mIccInfo.Value().GetAsMozGsmIccInfo().get()->Update(gsmIccInfo);
    return;
  }

  nsCOMPtr<nsICdmaIccInfo> cdmaIccInfo(do_QueryInterface(aIccInfo));
  if (cdmaIccInfo) {
    if (mIccInfo.IsNull() || !mIccInfo.Value().IsMozCdmaIccInfo()) {
      mIccInfo.SetValue().SetAsMozCdmaIccInfo() = new CdmaIccInfo(GetOwner());
    }
    mIccInfo.Value().GetAsMozCdmaIccInfo().get()->Update(cdmaIccInfo);
    return;
  }

  if (mIccInfo.IsNull() || !mIccInfo.Value().IsMozIccInfo()) {
    mIccInfo.SetValue().SetAsMozIccInfo() = new IccInfo(GetOwner());
  }
  mIccInfo.Value().GetAsMozIccInfo().get()->Update(aIccInfo);
}

// WrapperCache

JSObject*
Icc::WrapObject(JSContext* aCx)
{
  return MozIccBinding::Wrap(aCx, this);
}

// MozIcc WebIDL

void
Icc::GetIccInfo(Nullable<OwningMozIccInfoOrMozGsmIccInfoOrMozCdmaIccInfo>& aIccInfo) const
{
  aIccInfo = mIccInfo;
}

Nullable<IccCardState>
Icc::GetCardState() const
{
  Nullable<IccCardState> result;

  uint32_t cardState = nsIIccProvider::CARD_STATE_UNDETECTED;
  if (mProvider &&
      NS_SUCCEEDED(mProvider->GetCardState(mClientId, &cardState)) &&
      cardState != nsIIccProvider::CARD_STATE_UNDETECTED) {
    MOZ_ASSERT(cardState < static_cast<uint32_t>(IccCardState::EndGuard_));
    result.SetValue(static_cast<IccCardState>(cardState));
  }

  return result;
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

already_AddRefed<DOMRequest>
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

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
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

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
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

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
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

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
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

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
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

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
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

  return request.forget().downcast<DOMRequest>();
}
