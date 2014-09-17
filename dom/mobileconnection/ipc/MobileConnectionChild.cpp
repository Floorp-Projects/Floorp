/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnectionChild.h"

#include "MobileConnectionCallback.h"
#include "mozilla/dom/MozMobileConnectionBinding.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

void
MobileConnectionChild::Init()
{
  nsIMobileConnectionInfo* rawVoice;
  nsIMobileConnectionInfo* rawData;
  nsTArray<nsString> types;

  SendInit(&rawVoice, &rawData, &mLastNetwork, &mLastHomeNetwork, &mIccId,
           &mNetworkSelectionMode, &mRadioState, &types);

  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileConnectionInfo> voice = dont_AddRef(rawVoice);
  mVoice = new MobileConnectionInfo(nullptr);
  mVoice->Update(voice);

  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileConnectionInfo> data = dont_AddRef(rawData);
  mData = new MobileConnectionInfo(nullptr);
  mData->Update(data);


  // Initial SupportedNetworkTypes
  nsresult rv;
  mSupportedNetworkTypes = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    return;
  }

  uint32_t arrayLen = types.Length();
  if (arrayLen == 0) {
    mSupportedNetworkTypes->SetAsEmptyArray();
  } else {
    // Note: The resulting nsIVariant dupes both the array and its elements.
    const char16_t** array = reinterpret_cast<const char16_t**>
                               (NS_Alloc(arrayLen * sizeof(const char16_t***)));
    if (array) {
      for (uint32_t i = 0; i < arrayLen; ++i) {
        array[i] = types[i].get();
      }

      mSupportedNetworkTypes->SetAsArray(nsIDataType::VTYPE_WCHAR_STR,
                                         nullptr,
                                         arrayLen,
                                         reinterpret_cast<void*>(array));
      NS_Free(array);
    }
  }
}

void
MobileConnectionChild::Shutdown()
{
  if (mLive) {
    mLive = false;
    Send__delete__(this);
  }

  mListeners.Clear();
  mVoice = nullptr;
  mData = nullptr;
  mSupportedNetworkTypes = nullptr;
}

void
MobileConnectionChild::RegisterListener(nsIMobileConnectionListener* aListener)
{
  if (!mListeners.Contains(aListener)) {
    mListeners.AppendObject(aListener);
  }
}

void
MobileConnectionChild::UnregisterListener(nsIMobileConnectionListener* aListener)
{
  mListeners.RemoveObject(aListener);
}

MobileConnectionInfo*
MobileConnectionChild::GetVoiceInfo()
{
  return mVoice;
}

MobileConnectionInfo*
MobileConnectionChild::GetDataInfo()
{
  return mData;
}

void
MobileConnectionChild::GetIccId(nsAString& aIccId)
{
  aIccId = mIccId;
}

void
MobileConnectionChild::GetRadioState(nsAString& aRadioState)
{
  aRadioState = mRadioState;
}

nsIVariant*
MobileConnectionChild::GetSupportedNetworkTypes()
{
  return mSupportedNetworkTypes;
}

void
MobileConnectionChild::GetLastNetwork(nsAString& aNetwork)
{
  aNetwork = mLastNetwork;
}

void
MobileConnectionChild::GetLastHomeNetwork(nsAString& aNetwork)
{
  aNetwork = mLastHomeNetwork;
}

void
MobileConnectionChild::GetNetworkSelectionMode(nsAString& aMode)
{
  aMode = mNetworkSelectionMode;
}

bool
MobileConnectionChild::SendRequest(MobileConnectionRequest aRequest,
                                   nsIMobileConnectionCallback* aRequestCallback)
{
  NS_ENSURE_TRUE(mLive, false);

  // Deallocated in MobileConnectionChild::DeallocPMobileConnectionRequestChild().
  MobileConnectionRequestChild* actor = new MobileConnectionRequestChild(aRequestCallback);
  SendPMobileConnectionRequestConstructor(actor, aRequest);

  return true;
}

void
MobileConnectionChild::ActorDestroy(ActorDestroyReason why)
{
  mLive = false;
}

PMobileConnectionRequestChild*
MobileConnectionChild::AllocPMobileConnectionRequestChild(const MobileConnectionRequest& request)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool
MobileConnectionChild::DeallocPMobileConnectionRequestChild(PMobileConnectionRequestChild* aActor)
{
  delete aActor;
  return true;
}

bool
MobileConnectionChild::RecvNotifyVoiceInfoChanged(nsIMobileConnectionInfo* const& aInfo)
{
  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileConnectionInfo> voice = dont_AddRef(aInfo);
  mVoice->Update(voice);

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyVoiceChanged();
  }

  return true;
}

bool
MobileConnectionChild::RecvNotifyDataInfoChanged(nsIMobileConnectionInfo* const& aInfo)
{
  // Use dont_AddRef here because this instances is already AddRef-ed in
  // MobileConnectionIPCSerializer.h
  nsCOMPtr<nsIMobileConnectionInfo> data = dont_AddRef(aInfo);
  mData->Update(data);

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyDataChanged();
  }

  return true;
}

bool
MobileConnectionChild::RecvNotifyUssdReceived(const nsString& aMessage,
                                              const bool& aSessionEnd)
{
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyUssdReceived(aMessage, aSessionEnd);
  }

  return true;
}

bool
MobileConnectionChild::RecvNotifyDataError(const nsString& aMessage)
{
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyDataError(aMessage);
  }

  return true;
}

bool
MobileConnectionChild::RecvNotifyCFStateChanged(const bool& aSuccess,
                                                const uint16_t& aAction,
                                                const uint16_t& aReason,
                                                const nsString& aNumber,
                                                const uint16_t& aTimeSeconds,
                                                const uint16_t& aServiceClass)
{
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyCFStateChanged(aSuccess, aAction, aReason, aNumber,
                                        aTimeSeconds, aServiceClass);
  }

  return true;
}

bool
MobileConnectionChild::RecvNotifyEmergencyCbModeChanged(const bool& aActive,
                                                        const uint32_t& aTimeoutMs)
{
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyEmergencyCbModeChanged(aActive, aTimeoutMs);
  }

  return true;
}

bool
MobileConnectionChild::RecvNotifyOtaStatusChanged(const nsString& aStatus)
{
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyOtaStatusChanged(aStatus);
  }

  return true;
}

bool
MobileConnectionChild::RecvNotifyIccChanged(const nsString& aIccId)
{
  mIccId.Assign(aIccId);

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyIccChanged();
  }

  return true;
}

bool
MobileConnectionChild::RecvNotifyRadioStateChanged(const nsString& aRadioState)
{
  mRadioState.Assign(aRadioState);

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyRadioStateChanged();
  }

  return true;
}

bool
MobileConnectionChild::RecvNotifyClirModeChanged(const uint32_t& aMode)
{
  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyClirModeChanged(aMode);
  }

  return true;
}

bool
MobileConnectionChild::RecvNotifyLastNetworkChanged(const nsString& aNetwork)
{
  mLastNetwork.Assign(aNetwork);

  return true;
}

bool
MobileConnectionChild::RecvNotifyLastHomeNetworkChanged(const nsString& aNetwork)
{
  mLastHomeNetwork.Assign(aNetwork);

  return true;
}

bool
MobileConnectionChild::RecvNotifyNetworkSelectionModeChanged(const nsString& aMode)
{
  mNetworkSelectionMode.Assign(aMode);

  return true;
}

/******************************************************************************
 * MobileConnectionRequestChild
 ******************************************************************************/

void
MobileConnectionRequestChild::ActorDestroy(ActorDestroyReason why)
{
  mRequestCallback = nullptr;
}

bool
MobileConnectionRequestChild::DoReply(const MobileConnectionReplySuccess& aReply)
{
  return NS_SUCCEEDED(mRequestCallback->NotifySuccess());
}

bool
MobileConnectionRequestChild::DoReply(const MobileConnectionReplySuccessString& aReply)
{
  return NS_SUCCEEDED(mRequestCallback->NotifySuccessWithString(aReply.result()));
}

bool
MobileConnectionRequestChild::DoReply(const MobileConnectionReplySuccessBoolean& aReply)
{
  return NS_SUCCEEDED(mRequestCallback->NotifySuccessWithBoolean(aReply.result()));
}

bool
MobileConnectionRequestChild::DoReply(const MobileConnectionReplySuccessNetworks& aReply)
{
  uint32_t count = aReply.results().Length();
  nsTArray<nsCOMPtr<nsIMobileNetworkInfo>> results;
  for (uint32_t i = 0; i < count; i++) {
    // Use dont_AddRef here because these instances are already AddRef-ed in
    // MobileConnectionIPCSerializer.h
    nsCOMPtr<nsIMobileNetworkInfo> item = dont_AddRef(aReply.results()[i]);
    results.AppendElement(item);
  }

  return NS_SUCCEEDED(mRequestCallback->NotifyGetNetworksSuccess(count,
                                                                 const_cast<nsIMobileNetworkInfo**>(aReply.results().Elements())));
}

bool
MobileConnectionRequestChild::DoReply(const MobileConnectionReplySuccessMmi& aReply)
{
  nsAutoString serviceCode(aReply.serviceCode());
  nsAutoString statusMessage(aReply.statusMessage());
  AdditionalInformation info(aReply.additionalInformation());

  nsRefPtr<MobileConnectionCallback> callback = static_cast<MobileConnectionCallback*>(mRequestCallback.get());


  // Handle union types
  switch (info.type()) {
    case AdditionalInformation::Tvoid_t:
      return NS_SUCCEEDED(callback->NotifySendCancelMmiSuccess(serviceCode,
                                                               statusMessage));
    case AdditionalInformation::Tuint16_t:
      return NS_SUCCEEDED(callback->NotifySendCancelMmiSuccess(serviceCode,
                                                               statusMessage,
                                                               info.get_uint16_t()));
    case AdditionalInformation::TArrayOfnsString:
      return NS_SUCCEEDED(callback->NotifySendCancelMmiSuccess(serviceCode,
                                                               statusMessage,
                                                               info.get_ArrayOfnsString()));
    case AdditionalInformation::TArrayOfMozCallForwardingOptions:
      return NS_SUCCEEDED(callback->NotifySendCancelMmiSuccess(serviceCode,
                                                               statusMessage,
                                                               info.get_ArrayOfMozCallForwardingOptions()));

    default:
      MOZ_CRASH("Received invalid type!");
  }

  return false;
}

bool
MobileConnectionRequestChild::DoReply(const MobileConnectionReplySuccessCallForwarding& aReply)
{
  nsRefPtr<MobileConnectionCallback> callback = static_cast<MobileConnectionCallback*>(mRequestCallback.get());
  return NS_SUCCEEDED(callback->NotifyGetCallForwardingSuccess(aReply.results()));
}

bool
MobileConnectionRequestChild::DoReply(const MobileConnectionReplySuccessCallBarring& aReply)
{
  return NS_SUCCEEDED(mRequestCallback->NotifyGetCallBarringSuccess(aReply.program(),
                                                                    aReply.enabled(),
                                                                    aReply.serviceClass()));
}

bool
MobileConnectionRequestChild::DoReply(const MobileConnectionReplySuccessClirStatus& aReply)
{
  return NS_SUCCEEDED(mRequestCallback->NotifyGetClirStatusSuccess(aReply.n(),
                                                                   aReply.m()));
}

bool
MobileConnectionRequestChild::DoReply(const MobileConnectionReplyError& aReply)
{
  return NS_SUCCEEDED(mRequestCallback->NotifyError(aReply.message()));
}

bool
MobileConnectionRequestChild::DoReply(const MobileConnectionReplyErrorMmi& aReply)
{
  nsAutoString name(aReply.name());
  nsAutoString message(aReply.message());
  nsAutoString serviceCode(aReply.serviceCode());
  AdditionalInformation info(aReply.additionalInformation());

  // Handle union types
  switch (info.type()) {
    case AdditionalInformation::Tuint16_t:
      return NS_SUCCEEDED(mRequestCallback->NotifyError(name,
                                                        message,
                                                        serviceCode,
                                                        info.get_uint16_t()));
    case AdditionalInformation::Tvoid_t:
    default:
      // If additionInfomation is not uint16_t, handle it as void_t.
      return NS_SUCCEEDED(mRequestCallback->NotifyError(name,
                                                        message,
                                                        serviceCode));
  }

  return false;
}

bool
MobileConnectionRequestChild::Recv__delete__(const MobileConnectionReply& aReply)
{
  MOZ_ASSERT(mRequestCallback);

  switch (aReply.type()) {
    case MobileConnectionReply::TMobileConnectionReplySuccess:
      return DoReply(aReply.get_MobileConnectionReplySuccess());
    case MobileConnectionReply::TMobileConnectionReplySuccessString:
      return DoReply(aReply.get_MobileConnectionReplySuccessString());
    case MobileConnectionReply::TMobileConnectionReplySuccessBoolean:
      return DoReply(aReply.get_MobileConnectionReplySuccessBoolean());
    case MobileConnectionReply::TMobileConnectionReplySuccessNetworks:
      return DoReply(aReply.get_MobileConnectionReplySuccessNetworks());
    case MobileConnectionReply::TMobileConnectionReplySuccessMmi:
      return DoReply(aReply.get_MobileConnectionReplySuccessMmi());
    case MobileConnectionReply::TMobileConnectionReplySuccessCallForwarding:
      return DoReply(aReply.get_MobileConnectionReplySuccessCallForwarding());
    case MobileConnectionReply::TMobileConnectionReplySuccessCallBarring:
      return DoReply(aReply.get_MobileConnectionReplySuccessCallBarring());
    case MobileConnectionReply::TMobileConnectionReplySuccessClirStatus:
      return DoReply(aReply.get_MobileConnectionReplySuccessClirStatus());
    case MobileConnectionReply::TMobileConnectionReplyError:
      return DoReply(aReply.get_MobileConnectionReplyError());
    case MobileConnectionReply::TMobileConnectionReplyErrorMmi:
      return DoReply(aReply.get_MobileConnectionReplyErrorMmi());
    default:
      MOZ_CRASH("Received invalid response type!");
  }

  return false;
}
