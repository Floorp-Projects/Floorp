/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/mobileconnection/MobileConnectionParent.h"

#include "mozilla/AppProcessChecker.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/mobileconnection/MobileConnectionIPCSerializer.h"
#include "mozilla/dom/MozMobileConnectionBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsIVariant.h"
#include "nsJSUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

MobileConnectionParent::MobileConnectionParent(uint32_t aClientId)
  : mLive(true)
{
  MOZ_COUNT_CTOR(MobileConnectionParent);

  nsCOMPtr<nsIMobileConnectionService> service =
    do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
  NS_ASSERTION(service, "This shouldn't fail!");

  nsresult rv = service->GetItemByServiceId(aClientId,
                                            getter_AddRefs(mMobileConnection));
  if (NS_SUCCEEDED(rv) && mMobileConnection) {
    mMobileConnection->RegisterListener(this);
  }
}

void
MobileConnectionParent::ActorDestroy(ActorDestroyReason why)
{
  mLive = false;
  if (mMobileConnection) {
    mMobileConnection->UnregisterListener(this);
    mMobileConnection = nullptr;
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

  MobileConnectionRequestParent* actor =
    new MobileConnectionRequestParent(mMobileConnection);
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
                                 int32_t* aNetworkSelectionMode,
                                 int32_t* aRadioState,
                                 nsTArray<nsString>* aSupportedNetworkTypes)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  NS_ENSURE_SUCCESS(mMobileConnection->GetVoice(aVoice), false);
  NS_ENSURE_SUCCESS(mMobileConnection->GetData(aData), false);
  NS_ENSURE_SUCCESS(mMobileConnection->GetLastKnownNetwork(*aLastKnownNetwork), false);
  NS_ENSURE_SUCCESS(mMobileConnection->GetLastKnownHomeNetwork(*aLastKnownHomeNetwork), false);
  NS_ENSURE_SUCCESS(mMobileConnection->GetIccId(*aIccId), false);
  NS_ENSURE_SUCCESS(mMobileConnection->GetNetworkSelectionMode(aNetworkSelectionMode), false);
  NS_ENSURE_SUCCESS(mMobileConnection->GetRadioState(aRadioState), false);

  char16_t** types = nullptr;
  uint32_t length = 0;

  nsresult rv = mMobileConnection->GetSupportedNetworkTypes(&types, &length);
  NS_ENSURE_SUCCESS(rv, false);

  for (uint32_t i = 0; i < length; ++i) {
    nsDependentString type(types[i]);
    aSupportedNetworkTypes->AppendElement(type);
  }

  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(length, types);

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
  rv = mMobileConnection->GetVoice(getter_AddRefs(info));
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
  rv = mMobileConnection->GetData(getter_AddRefs(info));
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
MobileConnectionParent::NotifyCFStateChanged(uint16_t aAction,
                                             uint16_t aReason,
                                             const nsAString &aNumber,
                                             uint16_t aTimeSeconds,
                                             uint16_t aServiceClass)
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  return SendNotifyCFStateChanged(aAction, aReason, nsAutoString(aNumber),
                                  aTimeSeconds, aServiceClass) ? NS_OK : NS_ERROR_FAILURE;
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
  mMobileConnection->GetIccId(iccId);

  return SendNotifyIccChanged(iccId) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyRadioStateChanged()
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  int32_t radioState;
  rv = mMobileConnection->GetRadioState(&radioState);
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
  rv = mMobileConnection->GetLastKnownNetwork(network);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyLastNetworkChanged(network) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyLastKnownHomeNetworkChanged()
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  nsAutoString network;
  rv = mMobileConnection->GetLastKnownHomeNetwork(network);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyLastHomeNetworkChanged(network) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileConnectionParent::NotifyNetworkSelectionModeChanged()
{
  NS_ENSURE_TRUE(mLive, NS_ERROR_FAILURE);

  nsresult rv;
  int32_t mode;
  rv = mMobileConnection->GetNetworkSelectionMode(&mode);
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
  mMobileConnection = nullptr;
}

bool
MobileConnectionRequestParent::DoRequest(const GetNetworksRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetNetworks(this));
}

bool
MobileConnectionRequestParent::DoRequest(const SelectNetworkRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileNetworkInfo> network = dont_AddRef(aRequest.network());
  return NS_SUCCEEDED(mMobileConnection->SelectNetwork(network, this));
}

bool
MobileConnectionRequestParent::DoRequest(const SelectNetworkAutoRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SelectNetworkAutomatically(this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetPreferredNetworkTypeRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetPreferredNetworkType(aRequest.type(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetPreferredNetworkTypeRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetPreferredNetworkType(this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetRoamingPreferenceRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetRoamingPreference(aRequest.mode(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetRoamingPreferenceRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetRoamingPreference(this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetVoicePrivacyModeRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetVoicePrivacyMode(aRequest.enabled(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetVoicePrivacyModeRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetVoicePrivacyMode(this));
}

bool
MobileConnectionRequestParent::DoRequest(const SendMmiRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SendMMI(aRequest.mmi(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const CancelMmiRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->CancelMMI(this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetCallForwardingRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetCallForwarding(aRequest.action(),
                                                           aRequest.reason(),
                                                           aRequest.number(),
                                                           aRequest.timeSeconds(),
                                                           aRequest.serviceClass(),
                                                           this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetCallForwardingRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetCallForwarding(aRequest.reason(),
                                                           this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetCallBarringRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetCallBarring(aRequest.program(),
                                                        aRequest.enabled(),
                                                        aRequest.password(),
                                                        aRequest.serviceClass(),
                                                        this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetCallBarringRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetCallBarring(aRequest.program(),
                                                        aRequest.password(),
                                                        aRequest.serviceClass(),
                                                        this));
}

bool
MobileConnectionRequestParent::DoRequest(const ChangeCallBarringPasswordRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->ChangeCallBarringPassword(aRequest.pin(),
                                                                   aRequest.newPin(),
                                                                   this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetCallWaitingRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetCallWaiting(aRequest.enabled(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetCallWaitingRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetCallWaiting(this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetCallingLineIdRestrictionRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetCallingLineIdRestriction(aRequest.mode(), this));
}

bool
MobileConnectionRequestParent::DoRequest(const GetCallingLineIdRestrictionRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->GetCallingLineIdRestriction(this));
}

bool
MobileConnectionRequestParent::DoRequest(const ExitEmergencyCbModeRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->ExitEmergencyCbMode(this));
}

bool
MobileConnectionRequestParent::DoRequest(const SetRadioEnabledRequest& aRequest)
{
  NS_ENSURE_TRUE(mMobileConnection, false);

  return NS_SUCCEEDED(mMobileConnection->SetRadioEnabled(aRequest.enabled(), this));
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
MobileConnectionRequestParent::NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                                                          const nsAString& aStatusMessage)
{
  return SendReply(MobileConnectionReplySuccessMmi(nsString(aServiceCode),
                                                   nsString(aStatusMessage),
                                                   AdditionalInformation(mozilla::void_t())));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifySendCancelMmiSuccessWithInteger(const nsAString& aServiceCode,
                                                                     const nsAString& aStatusMessage,
                                                                     uint16_t aAdditionalInformation)
{
  return SendReply(MobileConnectionReplySuccessMmi(nsString(aServiceCode),
                                                   nsString(aStatusMessage),
                                                   AdditionalInformation(aAdditionalInformation)));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifySendCancelMmiSuccessWithStrings(const nsAString& aServiceCode,
                                                                     const nsAString& aStatusMessage,
                                                                     uint32_t aCount,
                                                                     const char16_t** aAdditionalInformation)
{
  nsTArray<nsString> additionalInformation;
  for (uint32_t i = 0; i < aCount; i++) {
    additionalInformation.AppendElement(nsDependentString(aAdditionalInformation[i]));
  }

  return SendReply(MobileConnectionReplySuccessMmi(nsString(aServiceCode),
                                                   nsString(aStatusMessage),
                                                   AdditionalInformation(additionalInformation)));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifySendCancelMmiSuccessWithCallForwardingOptions(const nsAString& aServiceCode,
                                                                                   const nsAString& aStatusMessage,
                                                                                   uint32_t aCount,
                                                                                   nsIMobileCallForwardingOptions** aAdditionalInformation)
{
  nsTArray<nsIMobileCallForwardingOptions*> additionalInformation;
  for (uint32_t i = 0; i < aCount; i++) {
    additionalInformation.AppendElement(aAdditionalInformation[i]);
  }

  return SendReply(MobileConnectionReplySuccessMmi(nsString(aServiceCode),
                                                   nsString(aStatusMessage),
                                                   AdditionalInformation(additionalInformation)));
}

NS_IMETHODIMP
MobileConnectionRequestParent::NotifyGetCallForwardingSuccess(uint32_t aCount,
                                                              nsIMobileCallForwardingOptions** aResults)
{
  nsTArray<nsIMobileCallForwardingOptions*> results;
  for (uint32_t i = 0; i < aCount; i++) {
    results.AppendElement(aResults[i]);
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
MobileConnectionRequestParent::NotifyGetPreferredNetworkTypeSuccess(int32_t aType)
{
  return SendReply(MobileConnectionReplySuccessPreferredNetworkType(aType));
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
