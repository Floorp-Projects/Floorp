/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnection.h"

#include "mozilla/dom/CFStateChangeEvent.h"
#include "mozilla/dom/DataErrorEvent.h"
#include "mozilla/dom/MozClirModeEvent.h"
#include "mozilla/dom/MozEmergencyCbModeEvent.h"
#include "mozilla/dom/MozOtaStatusEvent.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/USSDReceivedEvent.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsIDOMDOMRequest.h"
#include "nsIPermissionManager.h"
#include "nsIVariant.h"
#include "nsJSON.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

#define CONVERT_STRING_TO_NULLABLE_ENUM(_string, _enumType, _enum)      \
{                                                                       \
  uint32_t i = 0;                                                       \
  for (const EnumEntry* entry = _enumType##Values::strings;             \
       entry->value;                                                    \
       ++entry, ++i) {                                                  \
    if (_string.EqualsASCII(entry->value)) {                            \
      _enum.SetValue(static_cast<_enumType>(i));                        \
    }                                                                   \
  }                                                                     \
}

#define CONVERT_ENUM_TO_STRING(_enumType, _enum, _string)               \
{                                                                       \
  uint32_t index = uint32_t(_enum);                                     \
  _string.AssignASCII(_enumType##Values::strings[index].value,          \
                      _enumType##Values::strings[index].length);        \
}

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

NS_IMPL_ISUPPORTS(MobileConnection::Listener, nsIMobileConnectionListener)

NS_IMPL_CYCLE_COLLECTION_CLASS(MobileConnection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MobileConnection,
                                                  DOMEventTargetHelper)
  // Don't traverse mListener because it doesn't keep any reference to
  // MobileConnection but a raw pointer instead. Neither does mProvider because
  // it's an xpcom service and is only released at shutting down.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVoice)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mData)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MobileConnection,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVoice)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mData)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MobileConnection)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MobileConnection, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MobileConnection, DOMEventTargetHelper)

MobileConnection::MobileConnection(nsPIDOMWindow* aWindow, uint32_t aClientId)
  : DOMEventTargetHelper(aWindow)
  , mClientId(aClientId)
{
  SetIsDOMBinding();

  mProvider = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);

  // Not being able to acquire the provider isn't fatal since we check
  // for it explicitly below.
  if (!mProvider) {
    NS_WARNING("Could not acquire nsIMobileConnectionProvider!");
    return;
  }

  mListener = new Listener(this);
  mVoice = new MobileConnectionInfo(GetOwner());
  mData = new MobileConnectionInfo(GetOwner());

  if (CheckPermission("mobileconnection")) {
    DebugOnly<nsresult> rv = mProvider->RegisterMobileConnectionMsg(mClientId, mListener);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "Failed registering mobile connection messages with provider");
    UpdateVoice();
    UpdateData();
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
    mVoice = nullptr;
    mData = nullptr;
  }
}

JSObject*
MobileConnection::WrapObject(JSContext* aCx)
{
  return MozMobileConnectionBinding::Wrap(aCx, this);
}

bool
MobileConnection::CheckPermission(const char* aType) const
{
  nsCOMPtr<nsIPermissionManager> permMgr =
    mozilla::services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(GetOwner(), aType, &permission);
  return permission == nsIPermissionManager::ALLOW_ACTION;
}

void
MobileConnection::UpdateVoice()
{
  if (!mProvider) {
    return;
  }

  nsCOMPtr<nsIMobileConnectionInfo> info;
  mProvider->GetVoiceConnectionInfo(mClientId, getter_AddRefs(info));
  mVoice->Update(info);
}

void
MobileConnection::UpdateData()
{
  if (!mProvider) {
    return;
  }

  nsCOMPtr<nsIMobileConnectionInfo> info;
  mProvider->GetDataConnectionInfo(mClientId, getter_AddRefs(info));
  mData->Update(info);
}

// WebIDL interface

void
MobileConnection::GetLastKnownNetwork(nsString& aRetVal) const
{
  aRetVal.SetIsVoid(true);

  if (!mProvider ||
      (!CheckPermission("mobilenetwork") &&
       !CheckPermission("mobileconnection"))) {
    return;
  }

  mProvider->GetLastKnownNetwork(mClientId, aRetVal);
}

void
MobileConnection::GetLastKnownHomeNetwork(nsString& aRetVal) const
{
  aRetVal.SetIsVoid(true);

  if (!mProvider ||
      (!CheckPermission("mobilenetwork") &&
       !CheckPermission("mobileconnection"))) {
    return;
  }

  mProvider->GetLastKnownHomeNetwork(mClientId, aRetVal);
}

// All fields below require the "mobileconnection" permission.

MobileConnectionInfo*
MobileConnection::Voice() const
{
  if (!mProvider || !CheckPermission("mobileconnection")) {
    return nullptr;
  }

  return mVoice;
}

MobileConnectionInfo*
MobileConnection::Data() const
{
  if (!mProvider || !CheckPermission("mobileconnection")) {
    return nullptr;
  }

  return mData;
}

void
MobileConnection::GetIccId(nsString& aRetVal) const
{
  aRetVal.SetIsVoid(true);

  if (!mProvider || !CheckPermission("mobileconnection")) {
    return;
  }

  mProvider->GetIccId(mClientId, aRetVal);
}

Nullable<MobileNetworkSelectionMode>
MobileConnection::GetNetworkSelectionMode() const
{
  Nullable<MobileNetworkSelectionMode> retVal =
    Nullable<MobileNetworkSelectionMode>();

  if (!mProvider || !CheckPermission("mobileconnection")) {
    return retVal;
  }

  nsAutoString mode;
  mProvider->GetNetworkSelectionMode(mClientId, mode);
  CONVERT_STRING_TO_NULLABLE_ENUM(mode, MobileNetworkSelectionMode, retVal);

  return retVal;
}

Nullable<MobileRadioState>
MobileConnection::GetRadioState() const
{
  Nullable<MobileRadioState> retVal = Nullable<MobileRadioState>();

  if (!mProvider || !CheckPermission("mobileconnection")) {
    return retVal;
  }

  nsAutoString state;
  mProvider->GetRadioState(mClientId, state);
  CONVERT_STRING_TO_NULLABLE_ENUM(state, MobileRadioState, retVal);

  return retVal;
}

void
MobileConnection::GetSupportedNetworkTypes(nsTArray<MobileNetworkType>& aTypes) const
{
  if (!mProvider || !CheckPermission("mobileconnection")) {
    return;
  }

  nsCOMPtr<nsIVariant> variant;
  mProvider->GetSupportedNetworkTypes(mClientId,
                                      getter_AddRefs(variant));

  uint16_t type;
  nsIID iid;
  uint32_t count;
  void* data;

  // Convert the nsIVariant to an array.  We own the resulting buffer and its
  // elements.
  if (NS_FAILED(variant->GetAsArray(&type, &iid, &count, &data))) {
    return;
  }

  // We expect the element type is wstring.
  if (type == nsIDataType::VTYPE_WCHAR_STR) {
    char16_t** rawArray = reinterpret_cast<char16_t**>(data);
    for (uint32_t i = 0; i < count; ++i) {
      nsDependentString rawType(rawArray[i]);
      Nullable<MobileNetworkType> type = Nullable<MobileNetworkType>();
      CONVERT_STRING_TO_NULLABLE_ENUM(rawType, MobileNetworkType, type);

      if (!type.IsNull()) {
        aTypes.AppendElement(type.Value());
      }
    }
  }
  NS_Free(data);
}

already_AddRefed<DOMRequest>
MobileConnection::GetNetworks(ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->GetNetworks(mClientId, GetOwner(),
                                       getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SelectNetwork(MobileNetworkInfo& aNetwork, ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SelectNetwork(mClientId, GetOwner(), &aNetwork,
                                         getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SelectNetworkAutomatically(ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SelectNetworkAutomatically(mClientId, GetOwner(),
                                                      getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SetPreferredNetworkType(MobilePreferredNetworkType& aType,
                                          ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsAutoString type;
  CONVERT_ENUM_TO_STRING(MobilePreferredNetworkType, aType, type);

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SetPreferredNetworkType(mClientId, GetOwner(), type,
                                                   getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::GetPreferredNetworkType(ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->GetPreferredNetworkType(mClientId, GetOwner(),
                                                   getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SetRoamingPreference(MobileRoamingMode& aMode,
                                       ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsAutoString mode;
  CONVERT_ENUM_TO_STRING(MobileRoamingMode, aMode, mode);

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SetRoamingPreference(mClientId, GetOwner(), mode,
                                                getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::GetRoamingPreference(ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->GetRoamingPreference(mClientId, GetOwner(),
                                                getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SetVoicePrivacyMode(bool aEnabled, ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SetVoicePrivacyMode(mClientId, GetOwner(), aEnabled,
                                               getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::GetVoicePrivacyMode(ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->GetVoicePrivacyMode(mClientId, GetOwner(),
                                               getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SendMMI(const nsAString& aMMIString, ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SendMMI(mClientId, GetOwner(), aMMIString,
                                   getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::CancelMMI(ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->CancelMMI(mClientId, GetOwner(),
                                     getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::GetCallForwardingOption(uint16_t aReason, ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->GetCallForwarding(mClientId, GetOwner(), aReason,
                                             getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SetCallForwardingOption(const MozCallForwardingOptions& aOptions,
                                          ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(GetOwner()))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JSContext *cx = jsapi.cx();
  JS::Rooted<JS::Value> options(cx);
  if (!ToJSValue(cx, aOptions, &options)) {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SetCallForwarding(mClientId, GetOwner(), options,
                                             getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::GetCallBarringOption(const MozCallBarringOptions& aOptions,
                                       ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(GetOwner()))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JSContext *cx = jsapi.cx();
  JS::Rooted<JS::Value> options(cx);
  if (!ToJSValue(cx, aOptions, &options)) {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->GetCallBarring(mClientId, GetOwner(), options,
                                          getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SetCallBarringOption(const MozCallBarringOptions& aOptions,
                                       ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(GetOwner()))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JSContext *cx = jsapi.cx();
  JS::Rooted<JS::Value> options(cx);
  if (!ToJSValue(cx, aOptions, &options)) {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SetCallBarring(mClientId, GetOwner(), options,
                                          getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::ChangeCallBarringPassword(const MozCallBarringOptions& aOptions,
                                            ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(GetOwner()))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JSContext *cx = jsapi.cx();
  JS::Rooted<JS::Value> options(cx);
  if (!ToJSValue(cx, aOptions, &options)) {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->ChangeCallBarringPassword(mClientId,
                                                     GetOwner(),
                                                     options,
                                                     getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::GetCallWaitingOption(ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->GetCallWaiting(mClientId, GetOwner(),
                                          getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SetCallWaitingOption(bool aEnabled, ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SetCallWaiting(mClientId, GetOwner(), aEnabled,
                                          getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::GetCallingLineIdRestriction(ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->GetCallingLineIdRestriction(mClientId, GetOwner(),
                                                       getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SetCallingLineIdRestriction(uint16_t aMode,
                                              ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SetCallingLineIdRestriction(mClientId,
                                                       GetOwner(),
                                                       aMode,
                                                       getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::ExitEmergencyCbMode(ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->ExitEmergencyCbMode(mClientId, GetOwner(),
                                               getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
MobileConnection::SetRadioEnabled(bool aEnabled, ErrorResult& aRv)
{
  if (!CheckPermission("mobileconnection")) {
    return nullptr;
  }

  if (!mProvider) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = mProvider->SetRadioEnabled(mClientId, GetOwner(), aEnabled,
                                           getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}

// nsIMobileConnectionListener

NS_IMETHODIMP
MobileConnection::NotifyVoiceChanged()
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  UpdateVoice();

  return DispatchTrustedEvent(NS_LITERAL_STRING("voicechange"));
}

NS_IMETHODIMP
MobileConnection::NotifyDataChanged()
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  UpdateData();

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

NS_IMETHODIMP
MobileConnection::NotifyClirModeChanged(uint32_t aMode)
{
  if (!CheckPermission("mobileconnection")) {
    return NS_OK;
  }

  MozClirModeEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mMode = aMode;

  nsRefPtr<MozClirModeEvent> event =
    MozClirModeEvent::Constructor(this, NS_LITERAL_STRING("clirmodechange"), init);

  return DispatchTrustedEvent(event);
}
