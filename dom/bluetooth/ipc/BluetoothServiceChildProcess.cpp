/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothServiceChildProcess.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ipc/BlobChild.h"

#include "BluetoothChild.h"
#include "MainThreadUtils.h"

USING_BLUETOOTH_NAMESPACE

namespace {

BluetoothChild* sBluetoothChild;

inline
void
SendRequest(BluetoothReplyRunnable* aRunnable, const Request& aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  NS_WARN_IF_FALSE(sBluetoothChild,
                   "Calling methods on BluetoothServiceChildProcess during "
                   "shutdown!");

  if (sBluetoothChild) {
    BluetoothRequestChild* actor = new BluetoothRequestChild(aRunnable);
    sBluetoothChild->SendPBluetoothRequestConstructor(actor, aRequest);
  }
}

} // namespace

// static
BluetoothServiceChildProcess*
BluetoothServiceChildProcess::Create()
{
  MOZ_ASSERT(!sBluetoothChild);

  mozilla::dom::ContentChild* contentChild =
    mozilla::dom::ContentChild::GetSingleton();
  MOZ_ASSERT(contentChild);

  BluetoothServiceChildProcess* btService = new BluetoothServiceChildProcess();

  sBluetoothChild = new BluetoothChild(btService);
  contentChild->SendPBluetoothConstructor(sBluetoothChild);

  return btService;
}

BluetoothServiceChildProcess::BluetoothServiceChildProcess()
{
}

BluetoothServiceChildProcess::~BluetoothServiceChildProcess()
{
  sBluetoothChild = nullptr;
}

void
BluetoothServiceChildProcess::NoteDeadActor()
{
  MOZ_ASSERT(sBluetoothChild);
  sBluetoothChild = nullptr;
}

void
BluetoothServiceChildProcess::RegisterBluetoothSignalHandler(
                                              const nsAString& aNodeName,
                                              BluetoothSignalObserver* aHandler)
{
  if (sBluetoothChild && !IsSignalRegistered(aNodeName)) {
    sBluetoothChild->SendRegisterSignalHandler(nsString(aNodeName));
  }
  BluetoothService::RegisterBluetoothSignalHandler(aNodeName, aHandler);
}

void
BluetoothServiceChildProcess::UnregisterBluetoothSignalHandler(
                                              const nsAString& aNodeName,
                                              BluetoothSignalObserver* aHandler)
{
  BluetoothService::UnregisterBluetoothSignalHandler(aNodeName, aHandler);
  if (sBluetoothChild && !IsSignalRegistered(aNodeName)) {
    sBluetoothChild->SendUnregisterSignalHandler(nsString(aNodeName));
  }
}

nsresult
BluetoothServiceChildProcess::GetAdaptersInternal(
                                              BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, GetAdaptersRequest());
  return NS_OK;
}

nsresult
BluetoothServiceChildProcess::StartInternal(BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, StartBluetoothRequest());
  return NS_OK;
}

nsresult
BluetoothServiceChildProcess::StopInternal(BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, StopBluetoothRequest());
  return NS_OK;
}

nsresult
BluetoothServiceChildProcess::GetConnectedDevicePropertiesInternal(
                                              uint16_t aServiceUuid,
                                              BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, ConnectedDevicePropertiesRequest(aServiceUuid));
  return NS_OK;
}

nsresult
BluetoothServiceChildProcess::GetPairedDevicePropertiesInternal(
                                     const nsTArray<nsString>& aDeviceAddresses,
                                     BluetoothReplyRunnable* aRunnable)
{
  PairedDevicePropertiesRequest request;
  request.addresses().AppendElements(aDeviceAddresses);

  SendRequest(aRunnable, request);
  return NS_OK;
}

nsresult
BluetoothServiceChildProcess::FetchUuidsInternal(
  const nsAString& aDeviceAddress, BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, FetchUuidsRequest(nsString(aDeviceAddress)));
  return NS_OK;
}

void
BluetoothServiceChildProcess::StopDiscoveryInternal(
   BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, StopDiscoveryRequest());
}

void
BluetoothServiceChildProcess::StartDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, StartDiscoveryRequest());
}

void
BluetoothServiceChildProcess::StopLeScanInternal(
  const nsAString& aScanUuid,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, StopLeScanRequest(nsString(aScanUuid)));
}

void
BluetoothServiceChildProcess::StartLeScanInternal(
  const nsTArray<nsString>& aServiceUuids,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, StartLeScanRequest(aServiceUuids));
}

nsresult
BluetoothServiceChildProcess::SetProperty(BluetoothObjectType aType,
                                          const BluetoothNamedValue& aValue,
                                          BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, SetPropertyRequest(aType, aValue));
  return NS_OK;
}

nsresult
BluetoothServiceChildProcess::CreatePairedDeviceInternal(
                                              const nsAString& aAddress,
                                              int aTimeout,
                                              BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              PairRequest(nsString(aAddress), aTimeout));
  return NS_OK;
}

nsresult
BluetoothServiceChildProcess::RemoveDeviceInternal(
                                              const nsAString& aObjectPath,
                                              BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              UnpairRequest(nsString(aObjectPath)));
  return NS_OK;
}

nsresult
BluetoothServiceChildProcess::GetServiceChannel(const nsAString& aDeviceAddress,
                                                const nsAString& aServiceUuid,
                                                BluetoothProfileManagerBase* aManager)
{
  MOZ_CRASH("This should never be called!");
}

bool
BluetoothServiceChildProcess::UpdateSdpRecords(const nsAString& aDeviceAddress,
                                               BluetoothProfileManagerBase* aManager)
{
  MOZ_CRASH("This should never be called!");
}

void
BluetoothServiceChildProcess::PinReplyInternal(
  const nsAString& aDeviceAddress, bool aAccept,
  const nsAString& aPinCode, BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              PinReplyRequest(nsString(aDeviceAddress),
                              aAccept,
                              nsString(aPinCode)));
}

void
BluetoothServiceChildProcess::SspReplyInternal(
  const nsAString& aDeviceAddress, BluetoothSspVariant aVariant,
  bool aAccept, BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              SspReplyRequest(nsString(aDeviceAddress),
                              aVariant,
                              aAccept));
}

void
BluetoothServiceChildProcess::SetPinCodeInternal(
                                                const nsAString& aDeviceAddress,
                                                const nsAString& aPinCode,
                                                BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              SetPinCodeRequest(nsString(aDeviceAddress), nsString(aPinCode)));
}

void
BluetoothServiceChildProcess::SetPasskeyInternal(
                                                const nsAString& aDeviceAddress,
                                                uint32_t aPasskey,
                                                BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              SetPasskeyRequest(nsString(aDeviceAddress), aPasskey));
}

void
BluetoothServiceChildProcess::SetPairingConfirmationInternal(
                                                const nsAString& aDeviceAddress,
                                                bool aConfirm,
                                                BluetoothReplyRunnable* aRunnable)
{
  if(aConfirm) {
    SendRequest(aRunnable,
                ConfirmPairingConfirmationRequest(nsString(aDeviceAddress)));
  } else {
    SendRequest(aRunnable,
                DenyPairingConfirmationRequest(nsString(aDeviceAddress)));
  }
}

void
BluetoothServiceChildProcess::Connect(
  const nsAString& aDeviceAddress,
  uint32_t aCod,
  uint16_t aServiceUuid,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              ConnectRequest(nsString(aDeviceAddress),
                             aCod,
                             aServiceUuid));
}

void
BluetoothServiceChildProcess::Disconnect(
  const nsAString& aDeviceAddress,
  uint16_t aServiceUuid,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              DisconnectRequest(nsString(aDeviceAddress), aServiceUuid));
}

void
BluetoothServiceChildProcess::SendFile(
  const nsAString& aDeviceAddress,
  BlobParent* aBlobParent,
  BlobChild* aBlobChild,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              SendFileRequest(nsString(aDeviceAddress), nullptr, aBlobChild));
}

void
BluetoothServiceChildProcess::SendFile(
  const nsAString& aDeviceAddress,
  Blob* aBlobChild,
  BluetoothReplyRunnable* aRunnable)
{
  // Parent-process-only method
  MOZ_CRASH("This should never be called!");
}

void
BluetoothServiceChildProcess::StopSendingFile(
  const nsAString& aDeviceAddress,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              StopSendingFileRequest(nsString(aDeviceAddress)));
}

void
BluetoothServiceChildProcess::ConfirmReceivingFile(
  const nsAString& aDeviceAddress,
  bool aConfirm,
  BluetoothReplyRunnable* aRunnable)
{
  if(aConfirm) {
    SendRequest(aRunnable,
                ConfirmReceivingFileRequest(nsString(aDeviceAddress)));
    return;
  }

  SendRequest(aRunnable,
              DenyReceivingFileRequest(nsString(aDeviceAddress)));
}

void
BluetoothServiceChildProcess::ConnectSco(BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, ConnectScoRequest());
}

void
BluetoothServiceChildProcess::DisconnectSco(BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, DisconnectScoRequest());
}

void
BluetoothServiceChildProcess::IsScoConnected(BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, IsScoConnectedRequest());
}

void
BluetoothServiceChildProcess::ReplyTovCardPulling(
  BlobParent* aBlobParent,
  BlobChild* aBlobChild,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, ReplyTovCardPullingRequest(nullptr, aBlobChild));
}

void
BluetoothServiceChildProcess::ReplyTovCardPulling(
  Blob* aBlobChild,
  BluetoothReplyRunnable* aRunnable)
{
  // Parent-process-only method
  MOZ_CRASH("This should never be called!");
}

void
BluetoothServiceChildProcess::ReplyToPhonebookPulling(
  BlobParent* aBlobParent,
  BlobChild* aBlobChild,
  uint16_t aPhonebookSize,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    ReplyToPhonebookPullingRequest(nullptr, aBlobChild, aPhonebookSize));
}

void
BluetoothServiceChildProcess::ReplyToPhonebookPulling(
  Blob* aBlobChild,
  uint16_t aPhonebookSize,
  BluetoothReplyRunnable* aRunnable)
{
  // Parent-process-only method
  MOZ_CRASH("This should never be called!");
}

void
BluetoothServiceChildProcess::ReplyTovCardListing(
  BlobParent* aBlobParent,
  BlobChild* aBlobChild,
  uint16_t aPhonebookSize,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    ReplyTovCardListingRequest(nullptr, aBlobChild, aPhonebookSize));
}

void
BluetoothServiceChildProcess::ReplyTovCardListing(
  Blob* aBlobChild,
  uint16_t aPhonebookSize,
  BluetoothReplyRunnable* aRunnable)
{
  // Parent-process-only method
  MOZ_CRASH("This should never be called!");
}

#ifdef MOZ_B2G_RIL
void
BluetoothServiceChildProcess::AnswerWaitingCall(
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, AnswerWaitingCallRequest());
}

void
BluetoothServiceChildProcess::IgnoreWaitingCall(
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, IgnoreWaitingCallRequest());
}

void
BluetoothServiceChildProcess::ToggleCalls(
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, ToggleCallsRequest());
}
#endif // MOZ_B2G_RIL

void
BluetoothServiceChildProcess::SendMetaData(const nsAString& aTitle,
                                           const nsAString& aArtist,
                                           const nsAString& aAlbum,
                                           int64_t aMediaNumber,
                                           int64_t aTotalMediaCount,
                                           int64_t aDuration,
                                           BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              SendMetaDataRequest(nsString(aTitle), nsString(aArtist),
                                  nsString(aAlbum), aMediaNumber,
                                  aTotalMediaCount, aDuration));
}

void
BluetoothServiceChildProcess::SendPlayStatus(int64_t aDuration,
                                             int64_t aPosition,
                                             const nsAString& aPlayStatus,
                                             BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              SendPlayStatusRequest(aDuration, aPosition,
                                    nsString(aPlayStatus)));
}

void
BluetoothServiceChildProcess::ConnectGattClientInternal(
  const nsAString& aAppUuid, const nsAString& aDeviceAddress,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, ConnectGattClientRequest(nsString(aAppUuid),
                                                  nsString(aDeviceAddress)));
}

void
BluetoothServiceChildProcess::DisconnectGattClientInternal(
  const nsAString& aAppUuid, const nsAString& aDeviceAddress,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    DisconnectGattClientRequest(nsString(aAppUuid), nsString(aDeviceAddress)));
}

void
BluetoothServiceChildProcess::DiscoverGattServicesInternal(
  const nsAString& aAppUuid, BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    DiscoverGattServicesRequest(nsString(aAppUuid)));
}

void
BluetoothServiceChildProcess::GattClientStartNotificationsInternal(
  const nsAString& aAppUuid, const BluetoothGattServiceId& aServId,
  const BluetoothGattId& aCharId, BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattClientStartNotificationsRequest(nsString(aAppUuid), aServId, aCharId));
}

void
BluetoothServiceChildProcess::GattClientStopNotificationsInternal(
  const nsAString& aAppUuid, const BluetoothGattServiceId& aServId,
  const BluetoothGattId& aCharId, BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattClientStopNotificationsRequest(nsString(aAppUuid), aServId, aCharId));
}

void
BluetoothServiceChildProcess::UnregisterGattClientInternal(
  int aClientIf, BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, UnregisterGattClientRequest(aClientIf));
}

void
BluetoothServiceChildProcess::GattClientReadRemoteRssiInternal(
  int aClientIf, const nsAString& aDeviceAddress,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
              GattClientReadRemoteRssiRequest(aClientIf,
                                              nsString(aDeviceAddress)));
}

void
BluetoothServiceChildProcess::GattClientReadCharacteristicValueInternal(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattClientReadCharacteristicValueRequest(nsString(aAppUuid),
                                             aServiceId,
                                             aCharacteristicId));
}

void
BluetoothServiceChildProcess::GattClientWriteCharacteristicValueInternal(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattWriteType& aWriteType,
  const nsTArray<uint8_t>& aValue,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattClientWriteCharacteristicValueRequest(nsString(aAppUuid),
                                              aServiceId,
                                              aCharacteristicId,
                                              aWriteType,
                                              aValue));
}

void
BluetoothServiceChildProcess::GattClientReadDescriptorValueInternal(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattId& aDescriptorId,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattClientReadDescriptorValueRequest(nsString(aAppUuid),
                                         aServiceId,
                                         aCharacteristicId,
                                         aDescriptorId));
}

void
BluetoothServiceChildProcess::GattClientWriteDescriptorValueInternal(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattId& aDescriptorId,
  const nsTArray<uint8_t>& aValue,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattClientWriteDescriptorValueRequest(nsString(aAppUuid),
                                          aServiceId,
                                          aCharacteristicId,
                                          aDescriptorId,
                                          aValue));
}

void
BluetoothServiceChildProcess::GattServerConnectPeripheralInternal(
  const nsAString& aAppUuid,
  const nsAString& aAddress,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerConnectPeripheralRequest(nsString(aAppUuid),
                                       nsString(aAddress)));
}

void
BluetoothServiceChildProcess::GattServerDisconnectPeripheralInternal(
  const nsAString& aAppUuid,
  const nsAString& aAddress,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerDisconnectPeripheralRequest(nsString(aAppUuid),
                                          nsString(aAddress)));
}

void
BluetoothServiceChildProcess::UnregisterGattServerInternal(
  int aServerIf, BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable, UnregisterGattServerRequest(aServerIf));
}

void
BluetoothServiceChildProcess::GattServerAddServiceInternal(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  uint16_t aHandleCount,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerAddServiceRequest(nsString(aAppUuid), aServiceId, aHandleCount));
}

void
BluetoothServiceChildProcess::GattServerAddIncludedServiceInternal(
  const nsAString& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  const BluetoothAttributeHandle& aIncludedServiceHandle,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerAddIncludedServiceRequest(nsString(aAppUuid),
                                        aServiceHandle,
                                        aIncludedServiceHandle));
}

void
BluetoothServiceChildProcess::GattServerAddCharacteristicInternal(
  const nsAString& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  const BluetoothUuid& aCharacteristicUuid,
  BluetoothGattAttrPerm aPermissions,
  BluetoothGattCharProp aProperties,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerAddCharacteristicRequest(nsString(aAppUuid),
                                       aServiceHandle,
                                       aCharacteristicUuid,
                                       aPermissions,
                                       aProperties));
}

void
BluetoothServiceChildProcess::GattServerAddDescriptorInternal(
  const nsAString& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  const BluetoothAttributeHandle& aCharacteristicHandle,
  const BluetoothUuid& aDescriptorUuid,
  BluetoothGattAttrPerm aPermissions,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerAddDescriptorRequest(nsString(aAppUuid),
                                   aServiceHandle,
                                   aCharacteristicHandle,
                                   aDescriptorUuid,
                                   aPermissions));
}

void
BluetoothServiceChildProcess::GattServerRemoveServiceInternal(
  const nsAString& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerRemoveServiceRequest(nsString(aAppUuid), aServiceHandle));
}

void
BluetoothServiceChildProcess::GattServerStartServiceInternal(
  const nsAString& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerStartServiceRequest(nsString(aAppUuid), aServiceHandle));
}

void
BluetoothServiceChildProcess::GattServerStopServiceInternal(
  const nsAString& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerStopServiceRequest(nsString(aAppUuid), aServiceHandle));
}

void
BluetoothServiceChildProcess::GattServerSendResponseInternal(
  const nsAString& aAppUuid,
  const nsAString& aAddress,
  uint16_t aStatus,
  int32_t aRequestId,
  const BluetoothGattResponse& aRsp,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerSendResponseRequest(
      nsString(aAppUuid), nsString(aAddress), aStatus, aRequestId, aRsp));
}

void
BluetoothServiceChildProcess::GattServerSendIndicationInternal(
  const nsAString& aAppUuid,
  const nsAString& aAddress,
  const BluetoothAttributeHandle& aCharacteristicHandle,
  bool aConfirm,
  const nsTArray<uint8_t>& aValue,
  BluetoothReplyRunnable* aRunnable)
{
  SendRequest(aRunnable,
    GattServerSendIndicationRequest(nsString(aAppUuid),
                                    nsString(aAddress),
                                    aCharacteristicHandle,
                                    aConfirm,
                                    aValue));
}

nsresult
BluetoothServiceChildProcess::HandleStartup()
{
  // Don't need to do anything here for startup since our Create function takes
  // care of the actor machinery.
  return NS_OK;
}

nsresult
BluetoothServiceChildProcess::HandleShutdown()
{
  // If this process is shutting down then we need to disconnect ourselves from
  // the parent.
  if (sBluetoothChild) {
    sBluetoothChild->BeginShutdown();
  }
  return NS_OK;
}

nsresult
BluetoothServiceChildProcess::SendSinkMessage(const nsAString& aDeviceAddresses,
                                              const nsAString& aMessage)
{
  MOZ_CRASH("This should never be called!");
}

nsresult
BluetoothServiceChildProcess::SendInputMessage(const nsAString& aDeviceAddresses,
                                               const nsAString& aMessage)
{
  MOZ_CRASH("This should never be called!");
}

void
BluetoothServiceChildProcess::UpdatePlayStatus(uint32_t aDuration,
                                               uint32_t aPosition,
                                               ControlPlayStatus aPlayStatus)
{
  MOZ_CRASH("This should never be called!");
}

