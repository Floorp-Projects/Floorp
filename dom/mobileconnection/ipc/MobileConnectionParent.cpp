/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnectionParent.h"

#include "mozilla/AppProcessChecker.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/MobileConnectionIPCSerializer.h"
#include "mozilla/dom/MozMobileConnectionBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsIVariant.h"
#include "nsJSUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

MobileConnectionParent::MobileConnectionParent(uint32_t aClientId)
  : mClientId(aClientId)
  , mLive(true)
{
  MOZ_COUNT_CTOR(MobileConnectionParent);

  mService = do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
  NS_ASSERTION(mService, "This shouldn't fail!");

  if (mService) {
    mService->RegisterListener(mClientId, this);
  }
}

void
MobileConnectionParent::ActorDestroy(ActorDestroyReason why)
{
  mLive = false;
  if (mService) {
    mService->UnregisterListener(mClientId, this);
    mService = nullptr;
  }
}

bool
MobileConnectionParent::RecvPMobileConnectionRequestConstructor(PMobileConnectionRequestParent* aActor,
                                                                const MobileConnectionRequest& aRequest)
{
  MobileConnectionRequestParent* actor = static_cast<MobileConnectionRequestParent*>(aActor);

  switch (aRequest.type()) {
    case MobileConnectionRequest::TGetNetworksRequest:
      return actor->DoRequest(aRequest.get_GetNetworksRequest());
    case MobileConnectionRequest::TSelectNetworkRequest:
      return actor->DoRequest(aRequest.get_SelectNetworkRequest());
    case MobileConnectionRequest::TSelectNetworkAutoRequest:
      return actor->DoRequest(aRequest.get_SelectNetworkAutoRequest());
    case MobileConnectionRequest::TSetPreferredNetworkTypeRequest:
      return actor->DoRequest(aRequest.get_SetPreferredNetworkTypeRequest());
    case MobileConnectionRequest::TGetPreferredNetworkTypeRequest:
      return actor->DoRequest(aRequest.get_GetPreferredNetworkTypeRequest());
    case MobileConnectionRequest::TSetRoamingPreferenceRequest:
      return actor->DoRequest(aRequest.get_SetRoamingPreferenceRequest());
    case MobileConnectionRequest::TGetRoamingPreferenceRequest:
      return actor->DoRequest(aRequest.get_GetRoamingPreferenceRequest());
    case MobileConnectionRequest::TSetVoicePrivacyModeRequest:
      return actor->DoRequest(aRequest.get_SetVoicePrivacyModeRequest());
    case MobileConnectionRequest::TGetVoicePrivacyModeRequest:
      return actor->DoRequest(aRequest.get_GetVoicePrivacyModeRequest());
    case MobileConnectionRequest::TSendMmiRequest:
      return actor->DoRequest(aRequest.get_SendMmiRequest());
    case MobileConnectionRequest::TCancelMmiRequest:
      return actor->DoRequest(aRequest.get_CancelMmiRequest());
    case MobileConnectionRequest::TSetCallForwardingRequest:
      return actor->DoRequest(aRequest.get_SetCallForwardingRequest());
    case MobileConnectionRequest::TGetCallForwardingRequest:
      return actor->DoRequest(aRequest.get_GetCallForwardingRequest());
    case MobileConnectionRequest::TSetCallBarringRequest:
      return actor->DoRequest(aRequest.get_SetCallBarringRequest());
    case MobileConnectionRequest::TGetCallBarringRequest:
      return actor->DoRequest(aRequest.get_GetCallBarringRequest());
    case MobileConnectionRequest::TChangeCallBarringPasswordRequest:
      return actor->DoRequest(aRequest.get_ChangeCallBarringPasswordRequest());
    case MobileConnectionRequest::TSetCallWaitingRequest:
      return actor->DoRequest(aRequest.get_SetCallWaitingRequest());
    case MobileConnectionRequest::TGetCallWaitingRequest:
      return actor->DoRequest(aRequest.get_GetCallWaitingRequest());
    case MobileConnectionRequest::TSetCallingLineIdRestrictionRequest:
      return actor->DoRequest(aRequest.get_SetCallingLineIdRestrictionRequest());
    case MobileConnectionRequest::TGetCallingLineIdRestrictionRequest:
      return actor->DoRequest(aRequest.get_GetCallingLineIdRestrictionRequest());
    case MobileConnectionRequest::TExitEmergencyCbModeRequest:
      return actor->DoRequest(aRequest.get_ExitEmergencyCbModeRequest());
    case MobileConnectionRequest::TSetRadioEnabledRequest:
      return actor->DoRequest(aRequest.get_SetRadioEnabledRequest());
    default:
      MOZ_CRASH("Received invalid request type!");
  }

  return false;
}

PMobileConnectionRequestParent*
MobileConnectionParent::AllocPMobileConnectionRequestParent(const MobileConnectionRequest& request)
{
  if (!AssertAppProcessPermission(Manager(), "mobileconnection")) {
    return nullptr;
  }

  MobileConnectionRequestParent* actor = new MobileConnectionRequestParent(mClientId);
  // Add an extra ref for IPDL. Will be released in
  // MobileConnectionParent::DeallocPMobileConnectionRequestParent().
  actor->AddRef();
  return actor;
}

bool
MobileConnectionParent::DeallocPMobileConnectionRequestParent(PMobileConnectionRequestParent* aActor)
{
  // MobileConnectionRequestParent is refcounted, must not be freed manually.
  static_cast<MobileConnectionRequestParent*>(aActor)->Release();
  return true;
}

bool
MobileConnectionParent::RecvInit(nsMobileConnectionInfo* aVoice,
                                 nsMobileConnectionInfo* aData,
                                 nsString* aLastKnownNetwork,
                                 nsString* aLastKnownHomeNetwork,
                                 nsString* aIccId,
                                 nsString* aNetworkSelectionMode,
                                 nsString* aRadioState,
                                 nsTArray<nsString>* aSupportedNetworkTypes)
{
  NS_ENSURE_TRUE(mService, false);

  NS_ENSURE_SUCCESS(mService->GetVoiceConnectionInfo(mClientId, aVoice), false);
  NS_ENSURE_SUCCESS(mService->GetDataConnectionInfo(mClientId, aData), false);
  NS_ENSURE_SUCCESS(mService->GetLastKnownNetwork(mClientId, *aLastKnownNetwork), false);
  NS_ENSURE_SUCCESS(mService->GetLastKnownHomeNetwork(mClientId, *aLastKnownHomeNetwork), false);
  NS_ENSURE_SUCCESS(mService->GetIccId(mClientId, *aIccId), false);
  NS_ENSURE_SUCCESS(mService->GetNetworkSelectionMode(mClientId, *aNetworkSelectionMode), false);
  NS_ENSURE_SUCCESS(mService->GetRadioState(mClientId, *aRadioState), false);

  nsCOMPtr<nsIVariant> variant;
  mService->GetSupportedNetworkTypes(mClientId, getter_AddRefs(variant));

  uint16_t type;
  nsIID iid;
  uint32_t count;
  void* data;
  if (NS_FAILED(variant->GetAsArray(&type, &iid, &count, &data))) {
    return false;
  }

  // We expect the element type is wstring.
  if (type == nsIDataType::VTYPE_WCHAR_STR) {
    char16_t** rawArray = reinterpret_cast<char16_t**>(data);
    for (uint32_t i = 0; i < count; ++i) {
      nsDependentString networkType(rawArray[i]);
      aSupportedNetworkTypes->AppendElement(networkType);
    }
  }
  NS_Free(data);

  return true;
}

// nsIMobileConnectionListener

NS_IMPL_ISUPPORTS(MobileConnectionParent, nsIMobileConnectionListener)

NS_IMETHODIMP
MobileConnectionParent::NotifyVoiceChanged()
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsCOMPtr<nsIMobileConnectionInfo> info;
  rv = mService->GetVoiceConnectionInfo(mClientId, getter_AddRefs(info));
  NS_ENSURE_SUCCESS(rv, rv);

  // We release the ref after serializing process is finished in
  // MobileConnectionIPCSerializer.
  return SendNotifyVoiceInfoChanged(info.forget().take()) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyDataChanged()
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsCOMPtr<nsIMobileConnectionInfo> info;
  rv = mService->GetDataConnectionInfo(mClientId, getter_AddRefs(info));
  NS_ENSURE_SUCCESS(rv, rv);

  // We release the ref after serializing process is finished in
  // MobileConnectionIPCSerializer.
  return SendNotifyDataInfoChanged(info.forget().take()) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyUssdReceived(const nsAString& aMessage,
                                           bool aSessionEnded)
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyUssdReceived(nsAutoString(aMessage), aSessionEnded)
         ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyDataError(const nsAString& aMessage)
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyDataError(nsAutoString(aMessage)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyCFStateChanged(bool aSuccess,
                                             uint16_t aAction,
                                             uint16_t aReason,
                                             const nsAString &aNumber,
                                             uint16_t aTimeSeconds,
                                             uint16_t aServiceClass)
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyCFStateChanged(aSuccess, aAction, aReason,
                                  nsAutoString(aNumber), aTimeSeconds,
                                  aServiceClass) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyEmergencyCbModeChanged(bool aActive,
                                                     uint32_t aTimeoutMs)
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyEmergencyCbModeChanged(aActive, aTimeoutMs)
         ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyOtaStatusChanged(const nsAString& aStatus)
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyOtaStatusChanged(nsAutoString(aStatus))
         ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyIccChanged()
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsAutoString iccId;
  mService->GetIccId(mClientId, iccId);

  return SendNotifyIccChanged(iccId) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyRadioStateChanged()
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsAutoString radioState;
  rv = mService->GetRadioState(mClientId, radioState);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyRadioStateChanged(radioState) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyClirModeChanged(uint32_t aMode)
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyClirModeChanged(aMode) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyLastKnownNetworkChanged()
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsAutoString network;
  rv = mService->GetLastKnownNetwork(mClientId, network);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyLastNetworkChanged(network) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyLastKnownHomeNetworkChanged()
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsAutoString network;
  rv = mService->GetLastKnownHomeNetwork(mClientId, network);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyLastHomeNetworkChanged(network) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyNetworkSelectionModeChanged()
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsAutoString mode;
  rv = mService->GetNetworkSelectionMode(mClientId, mode);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyNetworkSelectionModeChanged(mode) ? NS_OK : NS_ERROR_FAILURE;
}

/******************************************************************************
 * PMobileConnectionRequestParent
 ******************************************************************************/

void
MobileConnectionRequestParent::ActorDestroy(ActorDestroyReason why)
{
  mLive = false;
  mService = nullptr;
}

bool
MobileConnectionRequestParent::DoRequest(const GetNetworksRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->GetNetworks(mClientId, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SelectNetworkRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileNetworkInfo> network = dont_AddRef(aRequest.network());
  return NS_SUCCEEDED(mService->SelectNetwork(mClientId, network, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SelectNetworkAutoRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->SelectNetworkAutomatically(mClientId, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetPreferredNetworkTypeRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->SetPreferredNetworkType(mClientId, aRequest.type(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetPreferredNetworkTypeRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->GetPreferredNetworkType(mClientId, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetRoamingPreferenceRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->SetRoamingPreference(mClientId, aRequest.mode(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetRoamingPreferenceRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->GetRoamingPreference(mClientId, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetVoicePrivacyModeRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->SetVoicePrivacyMode(mClientId, aRequest.enabled(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetVoicePrivacyModeRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->GetVoicePrivacyMode(mClientId, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SendMmiRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->SendMMI(mClientId, aRequest.mmi(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const CancelMmiRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->CancelMMI(mClientId, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetCallForwardingRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  AutoSafeJSContext cx;
  JS::Rooted<JS::Value> options(cx);
  if (!ToJSValue(cx, aRequest.options(), &options)) {
    JS_ClearPendingException(cx);
    return false;
  }

  return NS_SUCCEEDED(mService->SetCallForwarding(mClientId, options, this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetCallForwardingRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->GetCallForwarding(mClientId, aRequest.reason(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetCallBarringRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  AutoSafeJSContext cx;
  JS::Rooted<JS::Value> options(cx);
  if (!ToJSValue(cx, aRequest.options(), &options)) {
    JS_ClearPendingException(cx);
    return false;
  }

  return NS_SUCCEEDED(mService->SetCallBarring(mClientId, options, this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetCallBarringRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  AutoSafeJSContext cx;
  JS::Rooted<JS::Value> options(cx);
  if (!ToJSValue(cx, aRequest.options(), &options)) {
    JS_ClearPendingException(cx);
    return false;
  }

  return NS_SUCCEEDED(mService->GetCallBarring(mClientId, options, this));
}

bool
MobileConnectionRequestParent::DoRequest(const ChangeCallBarringPasswordRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  AutoSafeJSContext cx;
  JS::Rooted<JS::Value> options(cx);
  if (!ToJSValue(cx, aRequest.options(), &options)) {
    JS_ClearPendingException(cx);
    return false;
  }

  return NS_SUCCEEDED(mService->ChangeCallBarringPassword(mClientId, options, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetCallWaitingRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->SetCallWaiting(mClientId, aRequest.enabled(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetCallWaitingRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->GetCallWaiting(mClientId, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetCallingLineIdRestrictionRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->SetCallingLineIdRestriction(mClientId, aRequest.mode(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetCallingLineIdRestrictionRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->GetCallingLineIdRestriction(mClientId, this));
}

bool
MobileConnectionRequestParent::DoRequest(const ExitEmergencyCbModeRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->ExitEmergencyCbMode(mClientId, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetRadioEnabledRequest& aRequest)
{
  NS_ENSURE_TRUE(mService, false);

  return NS_SUCCEEDED(mService->SetRadioEnabled(mClientId, aRequest.enabled(), this));
}

nsresult
MobileConnectionRequestParent::SendReply(const MobileConnectionReply& aReply)
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return Send__delete__(this, aReply) ? NS_OK : NS_ERROR_FAILURE;
}

// nsIMobileConnectionListener

NS_IMPL_ISUPPORTS(MobileConnectionRequestParent, nsIMobileConnectionCallback);

NS_IMETHODIMP
MobileConnectionRequestParent::NotifySuccess()
{
  return SendReply(MobileConnectionReplySuccess());
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifySuccessWithString(const nsAString& aResult)
{
  return SendReply(MobileConnectionReplySuccessString(nsAutoString(aResult)));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifySuccessWithBoolean(bool aResult)
{
  return SendReply(MobileConnectionReplySuccessBoolean(aResult));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetNetworksSuccess(uint32_t aCount,
                                                        nsIMobileNetworkInfo** aNetworks)
{
  nsTArray<nsIMobileNetworkInfo*> networks;
  for (uint32_t i = 0; i < aCount; i++) {
    nsCOMPtr<nsIMobileNetworkInfo> network = aNetworks[i];
    // We release the ref after serializing process is finished in
    // MobileConnectionIPCSerializer.
    networks.AppendElement(network.forget().take());
  }
  return SendReply(MobileConnectionReplySuccessNetworks(networks));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifySendCancelMmiSuccess(JS::Handle<JS::Value> aResult)
{
  AutoSafeJSContext cx;
  RootedDictionary<MozMMIResult> result(cx);

  if (!result.Init(cx, aResult)) {
    return NS_ERROR_TYPE_ERR;
  }

  // No additionInformation passed
  if (!result.mAdditionalInformation.WasPassed()) {
    return SendReply(MobileConnectionReplySuccessMmi(result.mServiceCode,
                                                     result.mStatusMessage,
                                                     AdditionalInformation(mozilla::void_t())));
  }

  OwningUnsignedShortOrObject& additionInformation = result.mAdditionalInformation.Value();

  if (additionInformation.IsUnsignedShort()) {
    return SendReply(MobileConnectionReplySuccessMmi(result.mServiceCode,
                                                     result.mStatusMessage,
                                                     AdditionalInformation(uint16_t(additionInformation.GetAsUnsignedShort()))));
  }

  if (additionInformation.IsObject()) {
    uint32_t length;
    JS::Rooted<JS::Value> value(cx);
    JS::Rooted<JSObject*> object(cx, additionInformation.GetAsObject());

    if (!JS_IsArrayObject(cx, object) ||
        !JS_GetArrayLength(cx, object, &length) || length <= 0 ||
        // Check first element to decide the format of array.
        !JS_GetElement(cx, object, 0, &value)) {
      return NS_ERROR_TYPE_ERR;
    }

    // Check first element to decide the format of array.
    if (value.isString()) {
      // String[]
      nsTArray<nsString> infos;
      for (uint32_t i = 0; i < length; i++) {
        if (!JS_GetElement(cx, object, i, &value) || !value.isString()) {
          return NS_ERROR_TYPE_ERR;
        }

        nsAutoJSString str;
        if (!str.init(cx, value.toString())) {
          return NS_ERROR_FAILURE;
        }
        infos.AppendElement(str);
      }

      return SendReply(MobileConnectionReplySuccessMmi(result.mServiceCode,
                                                       result.mStatusMessage,
                                                       AdditionalInformation(infos)));
    } else {
      // IPC::MozCallForwardingOptions[]
      nsTArray<IPC::MozCallForwardingOptions> infos;
      for (uint32_t i = 0; i < length; i++) {
        IPC::MozCallForwardingOptions info;
        if (!JS_GetElement(cx, object, i, &value) || !info.Init(cx, value)) {
          return NS_ERROR_TYPE_ERR;
        }

        infos.AppendElement(info);
      }

      return SendReply(MobileConnectionReplySuccessMmi(result.mServiceCode,
                                                       result.mStatusMessage,
                                                       AdditionalInformation(infos)));
    }
  }

  return NS_ERROR_TYPE_ERR;
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetCallForwardingSuccess(JS::Handle<JS::Value> aResults)
{
  uint32_t length;
  AutoSafeJSContext cx;
  JS::Rooted<JSObject*> object(cx, &aResults.toObject());
  nsTArray<IPC::MozCallForwardingOptions> results;

  if (!JS_IsArrayObject(cx, object) ||
      !JS_GetArrayLength(cx, object, &length)) {
    return NS_ERROR_TYPE_ERR;
  }

  for (uint32_t i = 0; i < length; i++) {
    JS::Rooted<JS::Value> entry(cx);
    IPC::MozCallForwardingOptions info;

    if (!JS_GetElement(cx, object, i, &entry) || !info.Init(cx, entry)) {
      return NS_ERROR_TYPE_ERR;
    }

    results.AppendElement(info);
  }

  return SendReply(MobileConnectionReplySuccessCallForwarding(results));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetCallBarringSuccess(uint16_t aProgram,
                                                           bool aEnabled,
                                                           uint16_t aServiceClass)
{
  return SendReply(MobileConnectionReplySuccessCallBarring(aProgram, aEnabled,
                                                           aServiceClass));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetClirStatusSuccess(uint16_t aN,
                                                          uint16_t aM)
{
  return SendReply(MobileConnectionReplySuccessClirStatus(aN, aM));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyError(const nsAString& aName,
                                           const nsAString& aMessage,
                                           const nsAString& aServiceCode,
                                           uint16_t aInfo,
                                           uint8_t aArgc)
{
  if (aArgc == 0) {
    nsAutoString error(aName);
    return SendReply(MobileConnectionReplyError(error));
  }

  nsAutoString name(aName);
  nsAutoString message(aMessage);
  nsAutoString serviceCode(aServiceCode);

  if (aArgc < 3) {
    return SendReply(MobileConnectionReplyErrorMmi(name, message, serviceCode,
                                                   AdditionalInformation(mozilla::void_t())));
  }

  return SendReply(MobileConnectionReplyErrorMmi(name, message, serviceCode,
                                                 AdditionalInformation(aInfo)));
}
