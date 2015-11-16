/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothParent.h"

#include "mozilla/Assertions.h"
#include "mozilla/unused.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"

using mozilla::Unused;
USING_BLUETOOTH_NAMESPACE

/*******************************************************************************
 * BluetoothRequestParent::ReplyRunnable
 ******************************************************************************/

class BluetoothRequestParent::ReplyRunnable final : public BluetoothReplyRunnable
{
  BluetoothRequestParent* mRequest;

public:
  ReplyRunnable(BluetoothRequestParent* aRequest)
  : BluetoothReplyRunnable(nullptr), mRequest(aRequest)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aRequest);
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mReply);

    if (mRequest) {
      // Must do this first because Send__delete__ will delete mRequest.
      mRequest->RequestComplete();

      if (!mRequest->Send__delete__(mRequest, *mReply)) {
        BT_WARNING("Failed to send response to child process!");
        return NS_ERROR_FAILURE;
      }
    }

    ReleaseMembers();
    return NS_OK;
  }

  void
  Revoke()
  {
    ReleaseMembers();
  }

  virtual bool
  ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue) override
  {
    MOZ_CRASH("This should never be called!");
  }

  virtual void
  ReleaseMembers() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mRequest = nullptr;
    BluetoothReplyRunnable::ReleaseMembers();
  }
};

/*******************************************************************************
 * BluetoothParent
 ******************************************************************************/

BluetoothParent::BluetoothParent()
: mShutdownState(Running)
{
  MOZ_COUNT_CTOR(BluetoothParent);
}

BluetoothParent::~BluetoothParent()
{
  MOZ_COUNT_DTOR(BluetoothParent);
  MOZ_ASSERT(!mService);
  MOZ_ASSERT(mShutdownState == Dead);
}

void
BluetoothParent::BeginShutdown()
{
  // Only do something here if we haven't yet begun the shutdown sequence.
  if (mShutdownState == Running) {
    Unused << SendBeginShutdown();
    mShutdownState = SentBeginShutdown;
  }
}

bool
BluetoothParent::InitWithService(BluetoothService* aService)
{
  MOZ_ASSERT(aService);
  MOZ_ASSERT(!mService);

  if (!SendEnabled(aService->IsEnabled())) {
    return false;
  }

  mService = aService;
  return true;
}

void
BluetoothParent::UnregisterAllSignalHandlers()
{
  MOZ_ASSERT(mService);
  mService->UnregisterAllSignalHandlers(this);
}

void
BluetoothParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mService) {
    UnregisterAllSignalHandlers();
#ifdef DEBUG
    mService = nullptr;
#endif
  }

#ifdef DEBUG
  mShutdownState = Dead;
#endif
}

bool
BluetoothParent::RecvRegisterSignalHandler(const nsString& aNode)
{
  MOZ_ASSERT(mService);
  mService->RegisterBluetoothSignalHandler(aNode, this);
  return true;
}

bool
BluetoothParent::RecvUnregisterSignalHandler(const nsString& aNode)
{
  MOZ_ASSERT(mService);
  mService->UnregisterBluetoothSignalHandler(aNode, this);
  return true;
}

bool
BluetoothParent::RecvStopNotifying()
{
  MOZ_ASSERT(mService);

  if (mShutdownState != Running && mShutdownState != SentBeginShutdown) {
    MOZ_ASSERT(false, "Bad state!");
    return false;
  }

  mShutdownState = ReceivedStopNotifying;

  UnregisterAllSignalHandlers();

  if (SendNotificationsStopped()) {
    mShutdownState = SentNotificationsStopped;
    return true;
  }

  return false;
}

bool
BluetoothParent::RecvPBluetoothRequestConstructor(
                                                PBluetoothRequestParent* aActor,
                                                const Request& aRequest)
{
  BluetoothRequestParent* actor = static_cast<BluetoothRequestParent*>(aActor);

#ifdef DEBUG
  actor->mRequestType = aRequest.type();
#endif

  switch (aRequest.type()) {
    case Request::TGetAdaptersRequest:
      return actor->DoRequest(aRequest.get_GetAdaptersRequest());
    case Request::TStartBluetoothRequest:
      return actor->DoRequest(aRequest.get_StartBluetoothRequest());
    case Request::TStopBluetoothRequest:
      return actor->DoRequest(aRequest.get_StopBluetoothRequest());
    case Request::TSetPropertyRequest:
      return actor->DoRequest(aRequest.get_SetPropertyRequest());
    case Request::TStartDiscoveryRequest:
      return actor->DoRequest(aRequest.get_StartDiscoveryRequest());
    case Request::TStopDiscoveryRequest:
      return actor->DoRequest(aRequest.get_StopDiscoveryRequest());
    case Request::TStartLeScanRequest:
      return actor->DoRequest(aRequest.get_StartLeScanRequest());
    case Request::TStopLeScanRequest:
      return actor->DoRequest(aRequest.get_StopLeScanRequest());
    case Request::TPairRequest:
      return actor->DoRequest(aRequest.get_PairRequest());
    case Request::TUnpairRequest:
      return actor->DoRequest(aRequest.get_UnpairRequest());
    case Request::TPairedDevicePropertiesRequest:
      return actor->DoRequest(aRequest.get_PairedDevicePropertiesRequest());
    case Request::TConnectedDevicePropertiesRequest:
      return actor->DoRequest(aRequest.get_ConnectedDevicePropertiesRequest());
    case Request::TFetchUuidsRequest:
      return actor->DoRequest(aRequest.get_FetchUuidsRequest());
    case Request::TPinReplyRequest:
      return actor->DoRequest(aRequest.get_PinReplyRequest());
    case Request::TSspReplyRequest:
      return actor->DoRequest(aRequest.get_SspReplyRequest());
    case Request::TSetPinCodeRequest:
      return actor->DoRequest(aRequest.get_SetPinCodeRequest());
    case Request::TSetPasskeyRequest:
      return actor->DoRequest(aRequest.get_SetPasskeyRequest());
    case Request::TConfirmPairingConfirmationRequest:
      return actor->DoRequest(aRequest.get_ConfirmPairingConfirmationRequest());
    case Request::TDenyPairingConfirmationRequest:
      return actor->DoRequest(aRequest.get_DenyPairingConfirmationRequest());
    case Request::TConnectRequest:
      return actor->DoRequest(aRequest.get_ConnectRequest());
    case Request::TDisconnectRequest:
      return actor->DoRequest(aRequest.get_DisconnectRequest());
    case Request::TSendFileRequest:
      return actor->DoRequest(aRequest.get_SendFileRequest());
    case Request::TStopSendingFileRequest:
      return actor->DoRequest(aRequest.get_StopSendingFileRequest());
    case Request::TConfirmReceivingFileRequest:
      return actor->DoRequest(aRequest.get_ConfirmReceivingFileRequest());
    case Request::TDenyReceivingFileRequest:
      return actor->DoRequest(aRequest.get_DenyReceivingFileRequest());
    case Request::TConnectScoRequest:
      return actor->DoRequest(aRequest.get_ConnectScoRequest());
    case Request::TDisconnectScoRequest:
      return actor->DoRequest(aRequest.get_DisconnectScoRequest());
    case Request::TIsScoConnectedRequest:
      return actor->DoRequest(aRequest.get_IsScoConnectedRequest());
    case Request::TSetObexPasswordRequest:
      return actor->DoRequest(aRequest.get_SetObexPasswordRequest());
    case Request::TRejectObexAuthRequest:
      return actor->DoRequest(aRequest.get_RejectObexAuthRequest());
    case Request::TReplyTovCardPullingRequest:
      return actor->DoRequest(aRequest.get_ReplyTovCardPullingRequest());
    case Request::TReplyToPhonebookPullingRequest:
      return actor->DoRequest(aRequest.get_ReplyToPhonebookPullingRequest());
    case Request::TReplyTovCardListingRequest:
      return actor->DoRequest(aRequest.get_ReplyTovCardListingRequest());
    case Request::TReplyToFolderListingRequest:
      return actor->DoRequest(aRequest.get_ReplyToFolderListingRequest());
    case Request::TReplyToMessagesListingRequest:
      return actor->DoRequest(aRequest.get_ReplyToMessagesListingRequest());
    case Request::TReplyToGetMessageRequest:
      return actor->DoRequest(aRequest.get_ReplyToGetMessageRequest());
    case Request::TReplyToSetMessageStatusRequest:
      return actor->DoRequest(aRequest.get_ReplyToSetMessageStatusRequest());
    case Request::TReplyToSendMessageRequest:
      return actor->DoRequest(aRequest.get_ReplyToSendMessageRequest());
    case Request::TReplyToMessageUpdateRequest:
      return actor->DoRequest(aRequest.get_ReplyToMessageUpdateRequest());
#ifdef MOZ_B2G_RIL
    case Request::TAnswerWaitingCallRequest:
      return actor->DoRequest(aRequest.get_AnswerWaitingCallRequest());
    case Request::TIgnoreWaitingCallRequest:
      return actor->DoRequest(aRequest.get_IgnoreWaitingCallRequest());
    case Request::TToggleCallsRequest:
      return actor->DoRequest(aRequest.get_ToggleCallsRequest());
#endif
    case Request::TSendMetaDataRequest:
      return actor->DoRequest(aRequest.get_SendMetaDataRequest());
    case Request::TSendPlayStatusRequest:
      return actor->DoRequest(aRequest.get_SendPlayStatusRequest());
    case Request::TConnectGattClientRequest:
      return actor->DoRequest(aRequest.get_ConnectGattClientRequest());
    case Request::TDisconnectGattClientRequest:
      return actor->DoRequest(aRequest.get_DisconnectGattClientRequest());
    case Request::TDiscoverGattServicesRequest:
      return actor->DoRequest(aRequest.get_DiscoverGattServicesRequest());
    case Request::TGattClientStartNotificationsRequest:
      return actor->DoRequest(
        aRequest.get_GattClientStartNotificationsRequest());
    case Request::TGattClientStopNotificationsRequest:
      return actor->DoRequest(
        aRequest.get_GattClientStopNotificationsRequest());
    case Request::TUnregisterGattClientRequest:
      return actor->DoRequest(aRequest.get_UnregisterGattClientRequest());
    case Request::TGattClientReadRemoteRssiRequest:
      return actor->DoRequest(aRequest.get_GattClientReadRemoteRssiRequest());
    case Request::TGattClientReadCharacteristicValueRequest:
      return actor->DoRequest(
        aRequest.get_GattClientReadCharacteristicValueRequest());
    case Request::TGattClientWriteCharacteristicValueRequest:
      return actor->DoRequest(
        aRequest.get_GattClientWriteCharacteristicValueRequest());
    case Request::TGattClientReadDescriptorValueRequest:
      return actor->DoRequest(
        aRequest.get_GattClientReadDescriptorValueRequest());
    case Request::TGattClientWriteDescriptorValueRequest:
      return actor->DoRequest(
        aRequest.get_GattClientWriteDescriptorValueRequest());
    case Request::TGattServerConnectPeripheralRequest:
      return actor->DoRequest(
        aRequest.get_GattServerConnectPeripheralRequest());
    case Request::TGattServerDisconnectPeripheralRequest:
      return actor->DoRequest(
        aRequest.get_GattServerDisconnectPeripheralRequest());
    case Request::TUnregisterGattServerRequest:
      return actor->DoRequest(aRequest.get_UnregisterGattServerRequest());
    case Request::TGattServerAddServiceRequest:
      return actor->DoRequest(aRequest.get_GattServerAddServiceRequest());
    case Request::TGattServerAddIncludedServiceRequest:
      return actor->DoRequest(
               aRequest.get_GattServerAddIncludedServiceRequest());
    case Request::TGattServerAddCharacteristicRequest:
      return actor->DoRequest(
               aRequest.get_GattServerAddCharacteristicRequest());
    case Request::TGattServerAddDescriptorRequest:
      return actor->DoRequest(aRequest.get_GattServerAddDescriptorRequest());
    case Request::TGattServerRemoveServiceRequest:
      return actor->DoRequest(aRequest.get_GattServerRemoveServiceRequest());
    case Request::TGattServerStartServiceRequest:
      return actor->DoRequest(aRequest.get_GattServerStartServiceRequest());
    case Request::TGattServerStopServiceRequest:
      return actor->DoRequest(aRequest.get_GattServerStopServiceRequest());
    case Request::TGattServerSendResponseRequest:
      return actor->DoRequest(aRequest.get_GattServerSendResponseRequest());
    case Request::TGattServerSendIndicationRequest:
      return actor->DoRequest(
        aRequest.get_GattServerSendIndicationRequest());
    default:
      MOZ_CRASH("Unknown type!");
  }

  MOZ_CRASH("Should never get here!");
}

PBluetoothRequestParent*
BluetoothParent::AllocPBluetoothRequestParent(const Request& aRequest)
{
  MOZ_ASSERT(mService);
  return new BluetoothRequestParent(mService);
}

bool
BluetoothParent::DeallocPBluetoothRequestParent(PBluetoothRequestParent* aActor)
{
  delete aActor;
  return true;
}

void
BluetoothParent::Notify(const BluetoothSignal& aSignal)
{
  Unused << SendNotify(aSignal);
}

/*******************************************************************************
 * BluetoothRequestParent
 ******************************************************************************/

BluetoothRequestParent::BluetoothRequestParent(BluetoothService* aService)
: mService(aService)
#ifdef DEBUG
  , mRequestType(Request::T__None)
#endif
{
  MOZ_COUNT_CTOR(BluetoothRequestParent);
  MOZ_ASSERT(aService);

  mReplyRunnable = new ReplyRunnable(this);
}

BluetoothRequestParent::~BluetoothRequestParent()
{
  MOZ_COUNT_DTOR(BluetoothRequestParent);

  // mReplyRunnable will be automatically revoked.
}

void
BluetoothRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mReplyRunnable.Revoke();
}

void
BluetoothRequestParent::RequestComplete()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mReplyRunnable.IsPending());

  mReplyRunnable.Forget();
}

bool
BluetoothRequestParent::DoRequest(const GetAdaptersRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TGetAdaptersRequest);

  nsresult rv = mService->GetAdaptersInternal(mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const StartBluetoothRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TStartBluetoothRequest);

  nsresult rv = mService->StartInternal(mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const StopBluetoothRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TStopBluetoothRequest);

  nsresult rv = mService->StopInternal(mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const SetPropertyRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSetPropertyRequest);

  nsresult rv =
    mService->SetProperty(aRequest.type(), aRequest.value(),
                          mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const StartDiscoveryRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TStartDiscoveryRequest);

  mService->StartDiscoveryInternal(mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const StopDiscoveryRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TStopDiscoveryRequest);

  mService->StopDiscoveryInternal(mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const StartLeScanRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TStartLeScanRequest);

  mService->StartLeScanInternal(aRequest.serviceUuids(), mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const StopLeScanRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TStopLeScanRequest);

  mService->StopLeScanInternal(aRequest.scanUuid(), mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const PairRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TPairRequest);

  nsresult rv =
    mService->CreatePairedDeviceInternal(aRequest.address(),
                                         aRequest.timeoutMS(),
                                         mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const UnpairRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TUnpairRequest);

  nsresult rv =
    mService->RemoveDeviceInternal(aRequest.address(),
                                   mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const PairedDevicePropertiesRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TPairedDevicePropertiesRequest);

  nsresult rv =
    mService->GetPairedDevicePropertiesInternal(aRequest.addresses(),
                                                mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);
  return true;
}

bool
BluetoothRequestParent::DoRequest(
    const ConnectedDevicePropertiesRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConnectedDevicePropertiesRequest);
  nsresult rv =
    mService->GetConnectedDevicePropertiesInternal(aRequest.serviceUuid(),
                                                   mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const FetchUuidsRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TFetchUuidsRequest);
  nsresult rv =
    mService->FetchUuidsInternal(aRequest.address(), mReplyRunnable.get());

  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const PinReplyRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TPinReplyRequest);

  mService->PinReplyInternal(aRequest.address(),
                             aRequest.accept(),
                             aRequest.pinCode(),
                             mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const SspReplyRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSspReplyRequest);

  mService->SspReplyInternal(aRequest.address(),
                             aRequest.variant(),
                             aRequest.accept(),
                             mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const SetPinCodeRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSetPinCodeRequest);

  mService->SetPinCodeInternal(aRequest.address(),
                               aRequest.pincode(),
                               mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const SetPasskeyRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSetPasskeyRequest);

  mService->SetPasskeyInternal(aRequest.address(),
                               aRequest.passkey(),
                               mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const ConfirmPairingConfirmationRequest&
                                  aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConfirmPairingConfirmationRequest);

  mService->SetPairingConfirmationInternal(aRequest.address(),
                                           true,
                                           mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const DenyPairingConfirmationRequest&
                                  aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDenyPairingConfirmationRequest);

  mService->SetPairingConfirmationInternal(aRequest.address(),
                                           false,
                                           mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const ConnectRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConnectRequest);

  mService->Connect(aRequest.address(),
                    aRequest.cod(),
                    aRequest.serviceUuid(),
                    mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const DisconnectRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDisconnectRequest);

  mService->Disconnect(aRequest.address(),
                       aRequest.serviceUuid(),
                       mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const SendFileRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSendFileRequest);

  mService->SendFile(aRequest.address(),
                     (BlobParent*)aRequest.blobParent(),
                     (BlobChild*)aRequest.blobChild(),
                     mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const StopSendingFileRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TStopSendingFileRequest);

  mService->StopSendingFile(aRequest.address(),
                            mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const ConfirmReceivingFileRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConfirmReceivingFileRequest);

  mService->ConfirmReceivingFile(aRequest.address(),
                                 true,
                                 mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const DenyReceivingFileRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDenyReceivingFileRequest);

  mService->ConfirmReceivingFile(aRequest.address(),
                                 false,
                                 mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const ConnectScoRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConnectScoRequest);

  mService->ConnectSco(mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const DisconnectScoRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDisconnectScoRequest);

  mService->DisconnectSco(mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const IsScoConnectedRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TIsScoConnectedRequest);

  mService->IsScoConnected(mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const SetObexPasswordRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSetObexPasswordRequest);

  mService->SetObexPassword(aRequest.password(), mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const RejectObexAuthRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TRejectObexAuthRequest);

  mService->RejectObexAuth(mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const ReplyTovCardPullingRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TReplyTovCardPullingRequest);

  mService->ReplyTovCardPulling((BlobParent*)aRequest.blobParent(),
                                (BlobChild*)aRequest.blobChild(),
                                mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const ReplyToPhonebookPullingRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TReplyToPhonebookPullingRequest);

  mService->ReplyToPhonebookPulling((BlobParent*)aRequest.blobParent(),
                                    (BlobChild*)aRequest.blobChild(),
                                    aRequest.phonebookSize(),
                                    mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const ReplyTovCardListingRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TReplyTovCardListingRequest);

  mService->ReplyTovCardListing((BlobParent*)aRequest.blobParent(),
                                (BlobChild*)aRequest.blobChild(),
                                aRequest.phonebookSize(),
                                mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const ReplyToFolderListingRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TReplyToFolderListingRequest);

  mService->ReplyToMapFolderListing(aRequest.masId(),
                                           aRequest.folderList(),
                                           mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const ReplyToMessagesListingRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TReplyToMessagesListingRequest);

  mService->ReplyToMapMessagesListing((BlobParent*)aRequest.blobParent(),
                                             (BlobChild*)aRequest.blobChild(),
                                             aRequest.masId(),
                                             aRequest.newMessage(),
                                             aRequest.timeStamp(),
                                             aRequest.size(),
                                             mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const ReplyToGetMessageRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TReplyToGetMessageRequest);

  mService->ReplyToMapGetMessage((BlobParent*)aRequest.blobParent(),
                                     (BlobChild*)aRequest.blobChild(),
                                     aRequest.masId(),
                                     mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const ReplyToSetMessageStatusRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TReplyToSetMessageStatusRequest);

  mService->ReplyToMapSetMessageStatus(aRequest.masId(),
                                       aRequest.messageStatus(),
                                       mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const ReplyToSendMessageRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TReplyToSendMessageRequest);

  mService->ReplyToMapSendMessage(aRequest.masId(),
                                  aRequest.handleId(),
                                  aRequest.messageStatus(),
                                  mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const ReplyToMessageUpdateRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TReplyToMessageUpdateRequest);

  mService->ReplyToMapMessageUpdate(aRequest.masId(),
                                    aRequest.messageStatus(),
                                    mReplyRunnable.get());
  return true;
}

#ifdef MOZ_B2G_RIL
bool
BluetoothRequestParent::DoRequest(const AnswerWaitingCallRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TAnswerWaitingCallRequest);

  mService->AnswerWaitingCall(mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const IgnoreWaitingCallRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TAnswerWaitingCallRequest);

  mService->IgnoreWaitingCall(mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const ToggleCallsRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TAnswerWaitingCallRequest);

  mService->ToggleCalls(mReplyRunnable.get());

  return true;
}
#endif // MOZ_B2G_RIL

bool
BluetoothRequestParent::DoRequest(const SendMetaDataRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSendMetaDataRequest);

  mService->SendMetaData(aRequest.title(),
                         aRequest.artist(),
                         aRequest.album(),
                         aRequest.mediaNumber(),
                         aRequest.totalMediaCount(),
                         aRequest.duration(),
                         mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const SendPlayStatusRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSendPlayStatusRequest);

  mService->SendPlayStatus(aRequest.duration(),
                           aRequest.position(),
                           aRequest.playStatus(),
                           mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const ConnectGattClientRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConnectGattClientRequest);

  mService->ConnectGattClientInternal(aRequest.appUuid(),
                                      aRequest.deviceAddress(),
                                      mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const DisconnectGattClientRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDisconnectGattClientRequest);

  mService->DisconnectGattClientInternal(aRequest.appUuid(),
                                         aRequest.deviceAddress(),
                                         mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const DiscoverGattServicesRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDiscoverGattServicesRequest);

  mService->DiscoverGattServicesInternal(aRequest.appUuid(),
                                         mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattClientStartNotificationsRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TGattClientStartNotificationsRequest);

  mService->GattClientStartNotificationsInternal(aRequest.appUuid(),
                                                 aRequest.servId(),
                                                 aRequest.charId(),
                                                 mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattClientStopNotificationsRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TGattClientStopNotificationsRequest);

  mService->GattClientStopNotificationsInternal(aRequest.appUuid(),
                                                aRequest.servId(),
                                                aRequest.charId(),
                                                mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const UnregisterGattClientRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TUnregisterGattClientRequest);

  mService->UnregisterGattClientInternal(aRequest.clientIf(),
                                         mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattClientReadRemoteRssiRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TGattClientReadRemoteRssiRequest);

  mService->GattClientReadRemoteRssiInternal(aRequest.clientIf(),
                                             aRequest.deviceAddress(),
                                             mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattClientReadCharacteristicValueRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattClientReadCharacteristicValueRequest);

  mService->GattClientReadCharacteristicValueInternal(aRequest.appUuid(),
                                                      aRequest.serviceId(),
                                                      aRequest.charId(),
                                                      mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattClientWriteCharacteristicValueRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattClientWriteCharacteristicValueRequest);

  mService->GattClientWriteCharacteristicValueInternal(aRequest.appUuid(),
                                                       aRequest.serviceId(),
                                                       aRequest.charId(),
                                                       aRequest.writeType(),
                                                       aRequest.value(),
                                                       mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattClientReadDescriptorValueRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattClientReadDescriptorValueRequest);

  mService->GattClientReadDescriptorValueInternal(aRequest.appUuid(),
                                                  aRequest.serviceId(),
                                                  aRequest.charId(),
                                                  aRequest.descId(),
                                                  mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattClientWriteDescriptorValueRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattClientWriteDescriptorValueRequest);

  mService->GattClientWriteDescriptorValueInternal(aRequest.appUuid(),
                                                   aRequest.serviceId(),
                                                   aRequest.charId(),
                                                   aRequest.descId(),
                                                   aRequest.value(),
                                                   mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattServerConnectPeripheralRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattServerConnectPeripheralRequest);

  mService->GattServerConnectPeripheralInternal(aRequest.appUuid(),
                                                aRequest.address(),
                                                mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattServerDisconnectPeripheralRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattServerDisconnectPeripheralRequest);

  mService->GattServerDisconnectPeripheralInternal(aRequest.appUuid(),
                                                   aRequest.address(),
                                                   mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const UnregisterGattServerRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TUnregisterGattServerRequest);

  mService->UnregisterGattServerInternal(aRequest.serverIf(),
                                         mReplyRunnable.get());

  return true;
}
bool
BluetoothRequestParent::DoRequest(
  const GattServerAddServiceRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattServerAddServiceRequest);

  mService->GattServerAddServiceInternal(aRequest.appUuid(),
                                         aRequest.serviceId(),
                                         aRequest.handleCount(),
                                         mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattServerAddIncludedServiceRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattServerAddIncludedServiceRequest);

  mService->GattServerAddIncludedServiceInternal(
    aRequest.appUuid(),
    aRequest.serviceHandle(),
    aRequest.includedServiceHandle(),
    mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattServerAddCharacteristicRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattServerAddCharacteristicRequest);

  mService->GattServerAddCharacteristicInternal(
    aRequest.appUuid(),
    aRequest.serviceHandle(),
    aRequest.characteristicUuid(),
    aRequest.permissions(),
    aRequest.properties(),
    mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattServerAddDescriptorRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattServerAddDescriptorRequest);

  mService->GattServerAddDescriptorInternal(
    aRequest.appUuid(),
    aRequest.serviceHandle(),
    aRequest.characteristicHandle(),
    aRequest.descriptorUuid(),
    aRequest.permissions(),
    mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattServerRemoveServiceRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattServerRemoveServiceRequest);

  mService->GattServerRemoveServiceInternal(
    aRequest.appUuid(),
    aRequest.serviceHandle(),
    mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattServerStartServiceRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattServerStartServiceRequest);

  mService->GattServerStartServiceInternal(
    aRequest.appUuid(),
    aRequest.serviceHandle(),
    mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattServerStopServiceRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattServerStopServiceRequest);

  mService->GattServerStopServiceInternal(
    aRequest.appUuid(),
    aRequest.serviceHandle(),
    mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattServerSendIndicationRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TGattServerSendIndicationRequest);

  mService->GattServerSendIndicationInternal(
    aRequest.appUuid(),
    aRequest.address(),
    aRequest.characteristicHandle(),
    aRequest.confirm(),
    aRequest.value(),
    mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(
  const GattServerSendResponseRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType ==
             Request::TGattServerSendResponseRequest);

  mService->GattServerSendResponseInternal(
    aRequest.appUuid(),
    aRequest.address(),
    aRequest.status(),
    aRequest.requestId(),
    aRequest.response(),
    mReplyRunnable.get());

  return true;
}
