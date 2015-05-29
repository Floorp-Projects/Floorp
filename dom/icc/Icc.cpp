/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Icc.h"

#include "IccCallback.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/IccInfo.h"
#include "mozilla/dom/MozStkCommandEvent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsIIccInfo.h"
#include "nsIIccProvider.h"
#include "nsIIccService.h"
#include "nsIStkCmdFactory.h"
#include "nsIStkProactiveCmd.h"
#include "nsRadioInterfaceLayer.h"
#include "nsServiceManagerUtils.h"

using mozilla::dom::icc::IccCallback;

namespace mozilla {
namespace dom {

namespace {

bool
IsPukCardLockType(IccLockType aLockType)
{
  switch(aLockType) {
    case IccLockType::Puk:
    case IccLockType::Puk2:
    case IccLockType::NckPuk:
    case IccLockType::Nck1Puk:
    case IccLockType::Nck2Puk:
    case IccLockType::HnckPuk:
    case IccLockType::CckPuk:
    case IccLockType::SpckPuk:
    case IccLockType::RcckPuk:
    case IccLockType::RspckPuk:
    case IccLockType::NsckPuk:
    case IccLockType::PckPuk:
      return true;
    default:
      return false;
  }
}

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_INHERITED(Icc, DOMEventTargetHelper, mIccInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Icc)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Icc, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Icc, DOMEventTargetHelper)

Icc::Icc(nsPIDOMWindow* aWindow, long aClientId, nsIIcc* aHandler, nsIIccInfo* aIccInfo)
  : mLive(true)
  , mClientId(aClientId)
  , mHandler(aHandler)
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

Icc::~Icc()
{
}

void
Icc::Shutdown()
{
  mIccInfo.SetNull();
  mProvider = nullptr;
  mHandler = nullptr;
  mLive = false;
}

nsresult
Icc::NotifyEvent(const nsAString& aName)
{
  return DispatchTrustedEvent(aName);
}

nsresult
Icc::NotifyStkEvent(const nsAString& aName, nsIStkProactiveCmd* aStkProactiveCmd)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.InitWithLegacyErrorReporting(GetOwner()))) {
    return NS_ERROR_UNEXPECTED;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> value(cx);

  nsCOMPtr<nsIStkCmdFactory> cmdFactory =
    do_GetService(ICC_STK_CMD_FACTORY_CONTRACTID);
  NS_ENSURE_TRUE(cmdFactory, NS_ERROR_UNEXPECTED);

  cmdFactory->CreateCommandMessage(aStkProactiveCmd, &value);
  NS_ENSURE_TRUE(value.isObject(), NS_ERROR_UNEXPECTED);

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
Icc::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozIccBinding::Wrap(aCx, this, aGivenProto);
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

  uint32_t cardState = nsIIcc::CARD_STATE_UNDETECTED;
  if (mHandler &&
      NS_SUCCEEDED(mHandler->GetCardState(&cardState)) &&
      cardState != nsIIcc::CARD_STATE_UNDETECTED) {
    MOZ_ASSERT(cardState < static_cast<uint32_t>(IccCardState::EndGuard_));
    result.SetValue(static_cast<IccCardState>(cardState));
  }

  return result;
}

void
Icc::SendStkResponse(const JSContext* aCx, JS::Handle<JS::Value> aCommand,
                     JS::Handle<JS::Value> aResponse, ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIStkCmdFactory> cmdFactory =
    do_GetService(ICC_STK_CMD_FACTORY_CONTRACTID);
  if (!cmdFactory) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIStkProactiveCmd> command;
  cmdFactory->CreateCommand(aCommand, getter_AddRefs(command));
  if (!command) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIStkTerminalResponse> response;
  cmdFactory->CreateResponse(aResponse, getter_AddRefs(response));
  if (!response) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = mHandler->SendStkResponse(command, response);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
Icc::SendStkMenuSelection(uint16_t aItemIdentifier, bool aHelpRequested,
                          ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = mHandler->SendStkMenuSelection(aItemIdentifier, aHelpRequested);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
Icc::SendStkTimerExpiration(const JSContext* aCx, JS::Handle<JS::Value> aTimer,
                            ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIStkCmdFactory> cmdFactory =
    do_GetService(ICC_STK_CMD_FACTORY_CONTRACTID);
  if (!cmdFactory) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIStkTimer> timer;
  cmdFactory->CreateTimer(aTimer, getter_AddRefs(timer));
  if (!timer) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  uint16_t timerId;
  nsresult rv = timer->GetTimerId(&timerId);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  uint32_t timerValue;
  rv = timer->GetTimerValue(&timerValue);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  rv = mHandler->SendStkTimerExpiration(timerId, timerValue);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
Icc::SendStkEventDownload(const JSContext* aCx, JS::Handle<JS::Value> aEvent,
                          ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIStkCmdFactory> cmdFactory =
    do_GetService(ICC_STK_CMD_FACTORY_CONTRACTID);
  if (!cmdFactory) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIStkDownloadEvent> event;
  cmdFactory->CreateEvent(aEvent, getter_AddRefs(event));
  if (!event) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = mHandler->SendStkEventDownload(event);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

already_AddRefed<DOMRequest>
Icc::GetCardLock(IccLockType aLockType, ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  // TODO: Bug 1125018 - Simplify The Result of GetCardLock and
  // getCardLockRetryCount in MozIcc.webidl without a wrapper object.
  nsRefPtr<IccCallback> requestCallback =
    new IccCallback(GetOwner(), request, true);
  nsresult rv = mHandler->GetCardLockEnabled(static_cast<uint32_t>(aLockType),
                                             requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
Icc::UnlockCardLock(const IccUnlockCardLockOptions& aOptions, ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsRefPtr<IccCallback> requestCallback =
    new IccCallback(GetOwner(), request);
  const nsString& password = IsPukCardLockType(aOptions.mLockType)
                           ? aOptions.mPuk : aOptions.mPin;
  nsresult rv =
    mHandler->UnlockCardLock(static_cast<uint32_t>(aOptions.mLockType),
                             password, aOptions.mNewPin, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
Icc::SetCardLock(const IccSetCardLockOptions& aOptions, ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsresult rv;
  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsRefPtr<IccCallback> requestCallback =
    new IccCallback(GetOwner(), request);

  if (aOptions.mEnabled.WasPassed()) {
    // Enable card lock.
    const nsString& password = (aOptions.mLockType == IccLockType::Fdn) ?
                               aOptions.mPin2 : aOptions.mPin;

    rv =
      mHandler->SetCardLockEnabled(static_cast<uint32_t>(aOptions.mLockType),
                                   password, aOptions.mEnabled.Value(),
                                   requestCallback);
  } else {
    // Change card lock password.
    rv =
      mHandler->ChangeCardLockPassword(static_cast<uint32_t>(aOptions.mLockType),
                                       aOptions.mPin, aOptions.mNewPin,
                                       requestCallback);
  }

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
Icc::GetCardLockRetryCount(IccLockType aLockType, ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsRefPtr<IccCallback> requestCallback =
    new IccCallback(GetOwner(), request);
  nsresult rv = mHandler->GetCardLockRetryCount(static_cast<uint32_t>(aLockType),
                                                requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
Icc::ReadContacts(IccContactType aContactType, ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->ReadContacts(mClientId, GetOwner(),
                                        static_cast<uint32_t>(aContactType),
                                        getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
Icc::UpdateContact(const JSContext* aCx, IccContactType aContactType,
                   JS::Handle<JS::Value> aContact, const nsAString& aPin2,
                   ErrorResult& aRv)
{
  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->UpdateContact(mClientId, GetOwner(),
                                         static_cast<uint32_t>(aContactType),
                                         aContact, aPin2,
                                         getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
Icc::MatchMvno(IccMvnoType aMvnoType, const nsAString& aMvnoData,
               ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsRefPtr<IccCallback> requestCallback =
    new IccCallback(GetOwner(), request);
  nsresult rv = mHandler->MatchMvno(static_cast<uint32_t>(aMvnoType),
                                    aMvnoData, requestCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<Promise>
Icc::GetServiceState(IccService aService, ErrorResult& aRv)
{
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<IccCallback> requestCallback =
    new IccCallback(GetOwner(), promise);

  nsresult rv =
    mHandler->GetServiceStateEnabled(static_cast<uint32_t>(aService),
                                     requestCallback);

  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    // fall-through to return promise.
  }

  return promise.forget();
}

} // namespace dom
} // namespace mozilla
