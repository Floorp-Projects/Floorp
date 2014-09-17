/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnectionIPCService.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

NS_IMPL_ISUPPORTS(MobileConnectionIPCService, nsIMobileConnectionService)

StaticRefPtr<MobileConnectionIPCService> sService;

/* static */MobileConnectionIPCService*
MobileConnectionIPCService::GetSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sService) {
    return sService;
  }

  sService = new MobileConnectionIPCService();
  return sService;
}

MobileConnectionIPCService::MobileConnectionIPCService()
{
  int32_t numRil = Preferences::GetInt("ril.numRadioInterfaces", 1);
  for (int32_t i = 0; i < numRil; i++) {
    // Deallocated in ContentChild::DeallocPMobileConnectionChild().
    nsRefPtr<MobileConnectionChild> client = new MobileConnectionChild();
    NS_ASSERTION(client, "This shouldn't fail!");

    ContentChild::GetSingleton()->SendPMobileConnectionConstructor(client, i);
    client->Init();

    mClients.AppendElement(client);
  }
}

MobileConnectionIPCService::~MobileConnectionIPCService()
{
  uint32_t count = mClients.Length();
  for (uint32_t i = 0; i < count; i++) {
    mClients[i]->Shutdown();
  }

  mClients.Clear();
}

nsresult
MobileConnectionIPCService::SendRequest(uint32_t aClientId,
                                        MobileConnectionRequest aRequest,
                                        nsIMobileConnectionCallback* aRequestCallback)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  mClients[aClientId]->SendRequest(aRequest, aRequestCallback);
  return NS_OK;
}

// nsIMobileConnectionService

NS_IMETHODIMP
MobileConnectionIPCService::RegisterListener(uint32_t aClientId,
                                             nsIMobileConnectionListener* aListener)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  mClients[aClientId]->RegisterListener(aListener);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::UnregisterListener(uint32_t aClientId,
                                               nsIMobileConnectionListener* aListener)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  mClients[aClientId]->UnregisterListener(aListener);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::GetLastKnownNetwork(uint32_t aClientId,
                                                nsAString& aLastNetwork)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  mClients[aClientId]->GetLastNetwork(aLastNetwork);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::GetLastKnownHomeNetwork(uint32_t aClientId,
                                                    nsAString& aLastNetwork)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  mClients[aClientId]->GetLastHomeNetwork(aLastNetwork);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::GetVoiceConnectionInfo(uint32_t aClientId,
                                                   nsIMobileConnectionInfo** aInfo)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIMobileConnectionInfo> info = mClients[aClientId]->GetVoiceInfo();
  info.forget(aInfo);

  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::GetDataConnectionInfo(uint32_t aClientId,
                                                  nsIMobileConnectionInfo** aInfo)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIMobileConnectionInfo> info = mClients[aClientId]->GetDataInfo();
  info.forget(aInfo);

  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::GetIccId(uint32_t aClientId, nsAString& aIccId)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  mClients[aClientId]->GetIccId(aIccId);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::GetNetworkSelectionMode(uint32_t aClientId,
                                                    nsAString& aNetworkSelectionMode)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  mClients[aClientId]->GetNetworkSelectionMode(aNetworkSelectionMode);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::GetRadioState(uint32_t aClientId,
                                          nsAString& aRadioState)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  mClients[aClientId]->GetRadioState(aRadioState);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::GetSupportedNetworkTypes(uint32_t aClientId,
                                                     nsIVariant** aSupportedTypes)
{
  if (aClientId >= mClients.Length()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIVariant> supportedTypes = mClients[aClientId]->GetSupportedNetworkTypes();
  supportedTypes.forget(aSupportedTypes);

  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::GetNetworks(uint32_t aClientId,
                                        nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, GetNetworksRequest(), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::SelectNetwork(uint32_t aClientId,
                                          nsIMobileNetworkInfo* aNetwork,
                                          nsIMobileConnectionCallback* aRequest)
{
  nsCOMPtr<nsIMobileNetworkInfo> network = aNetwork;
  // We release the ref after serializing process is finished in
  // MobileConnectionIPCSerializer.
  return SendRequest(aClientId, SelectNetworkRequest(network.forget().take()), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::SelectNetworkAutomatically(uint32_t aClientId,
                                                       nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, SelectNetworkAutoRequest(), aRequest);
}


NS_IMETHODIMP
MobileConnectionIPCService::SetPreferredNetworkType(uint32_t aClientId,
                                                    const nsAString& aType,
                                                    nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId,
                     SetPreferredNetworkTypeRequest(nsAutoString(aType)),
                     aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::GetPreferredNetworkType(uint32_t aClientId,
                                                    nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, GetPreferredNetworkTypeRequest(), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::SetRoamingPreference(uint32_t aClientId,
                                                 const nsAString& aMode,
                                                 nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId,
                     SetRoamingPreferenceRequest(nsAutoString(aMode)),
                     aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::GetRoamingPreference(uint32_t aClientId,
                                                 nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, GetRoamingPreferenceRequest(), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::SetVoicePrivacyMode(uint32_t aClientId,
                                                bool aEnabled,
                                                nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, SetVoicePrivacyModeRequest(aEnabled), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::GetVoicePrivacyMode(uint32_t aClientId,
                                                nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, GetVoicePrivacyModeRequest(), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::SendMMI(uint32_t aClientId,
                                    const nsAString& aMmi,
                                    nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, SendMmiRequest(nsAutoString(aMmi)), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::CancelMMI(uint32_t aClientId,
                                      nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, CancelMmiRequest(), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::SetCallForwarding(uint32_t aClientId,
                                              JS::Handle<JS::Value> aOptions,
                                              nsIMobileConnectionCallback* aRequest)
{
  AutoSafeJSContext cx;
  IPC::MozCallForwardingOptions options;
  if(!options.Init(cx, aOptions)) {
    return NS_ERROR_TYPE_ERR;
  }

  return SendRequest(aClientId, SetCallForwardingRequest(options), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::GetCallForwarding(uint32_t aClientId,
                                              uint16_t aReason,
                                              nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, GetCallForwardingRequest(aReason), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::SetCallBarring(uint32_t aClientId,
                                           JS::Handle<JS::Value> aOptions,
                                           nsIMobileConnectionCallback* aRequest)
{
  AutoSafeJSContext cx;
  IPC::MozCallBarringOptions options;
  if(!options.Init(cx, aOptions)) {
    return NS_ERROR_TYPE_ERR;
  }

  return SendRequest(aClientId, SetCallBarringRequest(options), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::GetCallBarring(uint32_t aClientId,
                                           JS::Handle<JS::Value> aOptions,
                                           nsIMobileConnectionCallback* aRequest)
{
  AutoSafeJSContext cx;
  IPC::MozCallBarringOptions options;
  if(!options.Init(cx, aOptions)) {
    return NS_ERROR_TYPE_ERR;
  }

  return SendRequest(aClientId, GetCallBarringRequest(options), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::ChangeCallBarringPassword(uint32_t aClientId,
                                                      JS::Handle<JS::Value> aOptions,
                                                      nsIMobileConnectionCallback* aRequest)
{
  AutoSafeJSContext cx;
  IPC::MozCallBarringOptions options;
  if(!options.Init(cx, aOptions)) {
    return NS_ERROR_TYPE_ERR;
  }

  return SendRequest(aClientId, ChangeCallBarringPasswordRequest(options), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::SetCallWaiting(uint32_t aClientId,
                                           bool aEnabled,
                                           nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, SetCallWaitingRequest(aEnabled), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::GetCallWaiting(uint32_t aClientId,
                                           nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, GetCallWaitingRequest(), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::SetCallingLineIdRestriction(uint32_t aClientId,
                                                        uint16_t aMode,
                                                        nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, SetCallingLineIdRestrictionRequest(aMode), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::GetCallingLineIdRestriction(uint32_t aClientId,
                                                        nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, GetCallingLineIdRestrictionRequest(), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::ExitEmergencyCbMode(uint32_t aClientId,
                                                nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, ExitEmergencyCbModeRequest(), aRequest);
}

NS_IMETHODIMP
MobileConnectionIPCService::SetRadioEnabled(uint32_t aClientId,
                                            bool aEnabled,
                                            nsIMobileConnectionCallback* aRequest)
{
  return SendRequest(aClientId, SetRadioEnabledRequest(aEnabled), aRequest);
}
