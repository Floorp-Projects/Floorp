/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MobileConnection.h"

#include "GeneratedEvents.h"
#include "mozilla/dom/CFStateChangeEvent.h"
#include "mozilla/dom/DataErrorEvent.h"
#include "mozilla/dom/MozEmergencyCbModeEvent.h"
#include "mozilla/dom/MozOtaStatusEvent.h"
#include "mozilla/dom/USSDReceivedEvent.h"
#include "mozilla/Preferences.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMDOMRequest.h"
#include "nsIPermissionManager.h"
#include "nsIVariant.h"

#include "nsJSUtils.h"
#include "nsJSON.h"
#include "mozilla/Services.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

using namespace mozilla::dom;

class MobileConnection::Listener MOZ_FINAL : public nsIMobileConnectionListener
{
  MobileConnection* mMobileConnection;

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIMOBILECONNECTIONLISTENER(mMobileConnection)

  Listener(MobileConnection* aMobileConnection)
    : mMobileConnection(aMobileConnection)
  {
    MOZ_ASSERT(mMobileConnection);
  }

  void Disconnect()
  {
    MOZ_ASSERT(mMobileConnection);
    mMobileConnection = nullptr;
  }
};

NS_IMPL_ISUPPORTS1(MobileConnection::Listener, nsIMobileConnectionListener)

DOMCI_DATA(MozMobileConnection, MobileConnection)

NS_IMPL_CYCLE_COLLECTION_CLASS(MobileConnection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MobileConnection,
                                                  DOMEventTargetHelper)
  // Don't traverse mListener because it doesn't keep any reference to
  // MobileConnection but a raw pointer instead. Neither does mProvider because
  // it's an xpcom service and is only released at shutting down.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MobileConnection,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MobileConnection)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozMobileConnection)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozMobileConnection)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MobileConnection, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MobileConnection, DOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(MobileConnection, voicechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, datachange)
NS_IMPL_EVENT_HANDLER(MobileConnection, ussdreceived)
NS_IMPL_EVENT_HANDLER(MobileConnection, dataerror)
NS_IMPL_EVENT_HANDLER(MobileConnection, cfstatechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, emergencycbmodechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, otastatuschange)
NS_IMPL_EVENT_HANDLER(MobileConnection, iccchange)
NS_IMPL_EVENT_HANDLER(MobileConnection, radiostatechange)

MobileConnection::MobileConnection(uint32_t aClientId)
: mClientId(aClientId)
{
  mProvider = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  mWindow = nullptr;

  // Not being able to acquire the provider isn't fatal since we check
  // for it explicitly below.
  if (!mProvider) {
    NS_WARNING("Could not acquire nsIMobileConnectionProvider!");
    return;
  }
}

void
MobileConnection::Init(nsPIDOMWindow* aWindow)
{
  BindToOwner(aWindow);

  mWindow = do_GetWeakReference(aWindow);
  mListener = new Listener(this);

  if (CheckPermission("mobileconnection")) {
    DebugOnly<nsresult> rv = mProvider->RegisterMobileConnectionMsg(mClientId, mListener);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "Failed registering mobile connection messages with provider");
  }
}

void
MobileConnection::Shutdown()
{
  if (mProvider && mListener) {
    mListener->Disconnect();
    mProvider->UnregisterMobileConnectionMsg(mClientId, mListener);
    mProvider = nullptr;
    mListener = nullptr;
  }
}

// nsIDOMMozMobileConnection

NS_IMETHODIMP
MobileConnection::GetLastKnownNetwork(nsAString& aNetwork)
{
  aNetwork.SetIsVoid(true);

  if (!CheckPermission("mobilenetwork")) {
    return NS_OK;
  }

  return mProvider->GetLastKnownNetwork(mClientId, aNetwork);
}

NS_IMETHODIMP
MobileConnection::GetLastKnownHomeNetwork(nsAString& aNetwork)
{
  aNetwork.SetIsVoid(true);

  if (!CheckPermission("mobilenetwork")) {
    return NS_OK;
  }

  return mProvider->GetLastKnownHomeNetwork(mClientId, aNetwork);
}

// All fields below require the "mobileconnection" permission.

bool
MobileConnection::CheckPermission(const char* aType)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, false);

  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(window, aType, &permission);
  return permission == nsIPermissionManager::ALLOW_ACTION;
}

NS_IMETHODIMP
MobileConnection::GetVoice(nsIDOMMozMobileConnectionInfo** aVoice)
{
  *aVoice = nullptr;

  if (!mProvider || !CheckPermission("mobileconnection")) {
    return NS_OK;
  }
  return mProvider->GetVoiceConnectionInfo(mClientId, aVoice);
}

NS_IMETHODIMP
MobileConnection::GetData(nsIDOMMozMobileConnectionInfo** aData)
{
  *aData = nullptr;

  if (!mProvider || !CheckPermission("mobileconnection")) {
    return NS_OK;
  }
  return mProvider->GetDataConnectionInfo(mClientId, aData);
}

NS_IMETHODIMP
MobileConnection::GetIccId(nsAString& aIccId)
{
  aIccId.SetIsVoid(true);

  if (!mProvider || !CheckPermission("mobileconnection")) {
     return NS_OK;
  }
  return mProvider->GetIccId(mClientId, aIccId);
}

NS_IMETHODIMP
MobileConnection::GetNetworkSelectionMode(nsAString& aNetworkSelectionMode)
{
  aNetworkSelectionMode.SetIsVoid(true);

  if (!mProvider || !CheckPermission("mobileconnection")) {
     return NS_OK;
  }
  return mProvider->GetNetworkSelectionMode(mClientId, aNetworkSelectionMode);
}

NS_IMETHODIMP
MobileConnection::GetRadioState(nsAString& aRadioState)
{
  aRadioState.SetIsVoid(true);

  if (!mProvider || !CheckPermission("mobileconnection")) {
     return NS_OK;
  }
  return mProvider->GetRadioState(mClientId, aRadioState);
}

NS_IMETHODIMP
MobileConnection::GetSupportedNetworkTypes(nsIVariant** aSupportedNetworkTypes)
{
  *aSupportedNetworkTypes = nullptr;

  if (!mProvider || !CheckPermission("mobileconnection")) {
     return NS_OK;
  }

  return mProvider->GetSupportedNetworkTypes(mClientId, aSupportedNetworkTypes);
}

NS_IMETHODIMP
MobileConnection::GetNetworks(nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetNetworks(mClientId, GetOwner(), aRequest);
}

NS_IMETHODIMP
MobileConnection::SelectNetwork(nsIDOMMozMobileNetworkInfo* aNetwork, nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SelectNetwork(mClientId, GetOwner(), aNetwork, aRequest);
}

NS_IMETHODIMP
MobileConnection::SelectNetworkAutomatically(nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SelectNetworkAutomatically(mClientId, GetOwner(), aRequest);
}

NS_IMETHODIMP
MobileConnection::SetPreferredNetworkType(const nsAString& aType,
                                          nsIDOMDOMRequest** aDomRequest)
{
  *aDomRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetPreferredNetworkType(mClientId, GetOwner(), aType, aDomRequest);
}

NS_IMETHODIMP
MobileConnection::GetPreferredNetworkType(nsIDOMDOMRequest** aDomRequest)
{
  *aDomRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetPreferredNetworkType(mClientId, GetOwner(), aDomRequest);
}

NS_IMETHODIMP
MobileConnection::SetRoamingPreference(const nsAString& aMode, nsIDOMDOMRequest** aDomRequest)
{
  *aDomRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetRoamingPreference(mClientId, GetOwner(), aMode, aDomRequest);
}

NS_IMETHODIMP
MobileConnection::GetRoamingPreference(nsIDOMDOMRequest** aDomRequest)
{
  *aDomRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetRoamingPreference(mClientId, GetOwner(), aDomRequest);
}

NS_IMETHODIMP
MobileConnection::SetVoicePrivacyMode(bool aEnabled, nsIDOMDOMRequest** aDomRequest)
{
  *aDomRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetVoicePrivacyMode(mClientId, GetOwner(), aEnabled, aDomRequest);
}

NS_IMETHODIMP
MobileConnection::GetVoicePrivacyMode(nsIDOMDOMRequest** aDomRequest)
{
  *aDomRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetVoicePrivacyMode(mClientId, GetOwner(), aDomRequest);
}

NS_IMETHODIMP
MobileConnection::SendMMI(const nsAString& aMMIString,
                          nsIDOMDOMRequest** aRequest)
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SendMMI(mClientId, GetOwner(), aMMIString, aRequest);
}

NS_IMETHODIMP
MobileConnection::CancelMMI(nsIDOMDOMRequest** aRequest)
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->CancelMMI(mClientId, GetOwner(),aRequest);
}

NS_IMETHODIMP
MobileConnection::GetCallForwardingOption(uint16_t aReason,
                                          nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetCallForwardingOption(mClientId, GetOwner(), aReason, aRequest);
}

NS_IMETHODIMP
MobileConnection::SetCallForwardingOption(nsIDOMMozMobileCFInfo* aCFInfo,
                                          nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetCallForwardingOption(mClientId, GetOwner(), aCFInfo, aRequest);
}

NS_IMETHODIMP
MobileConnection::GetCallBarringOption(JS::Handle<JS::Value> aOption,
                                       nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetCallBarringOption(mClientId, GetOwner(), aOption, aRequest);
}

NS_IMETHODIMP
MobileConnection::SetCallBarringOption(JS::Handle<JS::Value> aOption,
                                       nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetCallBarringOption(mClientId, GetOwner(), aOption, aRequest);
}

NS_IMETHODIMP
MobileConnection::ChangeCallBarringPassword(JS::Handle<JS::Value> aInfo,
                                            nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->ChangeCallBarringPassword(mClientId, GetOwner(), aInfo, aRequest);
}

NS_IMETHODIMP
MobileConnection::GetCallWaitingOption(nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetCallWaitingOption(mClientId, GetOwner(), aRequest);
}

NS_IMETHODIMP
MobileConnection::SetCallWaitingOption(bool aEnabled,
                                       nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetCallWaitingOption(mClientId, GetOwner(), aEnabled, aRequest);
}

NS_IMETHODIMP
MobileConnection::GetCallingLineIdRestriction(nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetCallingLineIdRestriction(mClientId, GetOwner(), aRequest);
}

NS_IMETHODIMP
MobileConnection::SetCallingLineIdRestriction(unsigned short aClirMode,
                                              nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetCallingLineIdRestriction(mClientId, GetOwner(), aClirMode, aRequest);
}

NS_IMETHODIMP
MobileConnection::ExitEmergencyCbMode(nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->ExitEmergencyCbMode(mClientId, GetOwner(), aRequest);
}

NS_IMETHODIMP
MobileConnection::SetRadioEnabled(bool aEnabled,
                                  nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetRadioEnabled(mClientId, GetOwner(), aEnabled, aRequest);
}

// nsIMobileConnectionListener

NS_IMETHODIMP
MobileConnection::NotifyVoiceChanged()
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  return DispatchTrustedEvent(NS_LITERAL_STRING("voicechange"));
}

NS_IMETHODIMP
MobileConnection::NotifyDataChanged()
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  return DispatchTrustedEvent(NS_LITERAL_STRING("datachange"));
}

NS_IMETHODIMP
MobileConnection::NotifyUssdReceived(const nsAString& aMessage,
                                     bool aSessionEnded)
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  USSDReceivedEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mMessage = aMessage;
  init.mSessionEnded = aSessionEnded;

  nsRefPtr<USSDReceivedEvent> event =
    USSDReceivedEvent::Constructor(this, NS_LITERAL_STRING("ussdreceived"), init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileConnection::NotifyDataError(const nsAString& aMessage)
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  DataErrorEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mMessage = aMessage;

  nsRefPtr<DataErrorEvent> event =
    DataErrorEvent::Constructor(this, NS_LITERAL_STRING("dataerror"), init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileConnection::NotifyCFStateChange(bool aSuccess,
                                      unsigned short aAction,
                                      unsigned short aReason,
                                      const nsAString& aNumber,
                                      unsigned short aSeconds,
                                      unsigned short aServiceClass)
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  CFStateChangeEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mSuccess = aSuccess;
  init.mAction = aAction;
  init.mReason = aReason;
  init.mNumber = aNumber;
  init.mTimeSeconds = aSeconds;
  init.mServiceClass = aServiceClass;

  nsRefPtr<CFStateChangeEvent> event =
    CFStateChangeEvent::Constructor(this, NS_LITERAL_STRING("cfstatechange"), init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileConnection::NotifyEmergencyCbModeChanged(bool aActive,
                                               uint32_t aTimeoutMs)
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  MozEmergencyCbModeEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mActive = aActive;
  init.mTimeoutMs = aTimeoutMs;

  nsRefPtr<MozEmergencyCbModeEvent> event =
    MozEmergencyCbModeEvent::Constructor(this, NS_LITERAL_STRING("emergencycbmodechange"), init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileConnection::NotifyOtaStatusChanged(const nsAString& aStatus)
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  MozOtaStatusEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mStatus = aStatus;

  nsRefPtr<MozOtaStatusEvent> event =
    MozOtaStatusEvent::Constructor(this, NS_LITERAL_STRING("otastatuschange"), init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileConnection::NotifyIccChanged()
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  return DispatchTrustedEvent(NS_LITERAL_STRING("iccchange"));
}

NS_IMETHODIMP
MobileConnection::NotifyRadioStateChanged()
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  return DispatchTrustedEvent(NS_LITERAL_STRING("radiostatechange"));
}
