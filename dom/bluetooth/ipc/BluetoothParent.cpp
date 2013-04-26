/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothParent.h"

#include "mozilla/Assertions.h"
#include "mozilla/unused.h"
#include "mozilla/Util.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "nsTraceRefcnt.h"

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"

using mozilla::unused;
USING_BLUETOOTH_NAMESPACE

/*******************************************************************************
 * BluetoothRequestParent::ReplyRunnable
 ******************************************************************************/

class BluetoothRequestParent::ReplyRunnable : public BluetoothReplyRunnable
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
  Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mReply);

    if (mRequest) {
      // Must do this first because Send__delete__ will delete mRequest.
      mRequest->RequestComplete();

      if (!mRequest->Send__delete__(mRequest, *mReply)) {
        NS_WARNING("Failed to send response to child process!");
        return NS_ERROR_FAILURE;
      }
    }

    ReleaseMembers();
    return NS_OK;
  }

  void
  Revoke()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mRequest = nullptr;
  }

  virtual bool
  ParseSuccessfulReply(JS::Value* aValue) MOZ_OVERRIDE
  {
    MOZ_NOT_REACHED("This should never be called!");
    return false;
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
    unused << SendBeginShutdown();
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
    case Request::TDefaultAdapterPathRequest:
      return actor->DoRequest(aRequest.get_DefaultAdapterPathRequest());
    case Request::TSetPropertyRequest:
      return actor->DoRequest(aRequest.get_SetPropertyRequest());
    case Request::TStartDiscoveryRequest:
      return actor->DoRequest(aRequest.get_StartDiscoveryRequest());
    case Request::TStopDiscoveryRequest:
      return actor->DoRequest(aRequest.get_StopDiscoveryRequest());
    case Request::TPairRequest:
      return actor->DoRequest(aRequest.get_PairRequest());
    case Request::TUnpairRequest:
      return actor->DoRequest(aRequest.get_UnpairRequest());
    case Request::TPairedDevicePropertiesRequest:
      return actor->DoRequest(aRequest.get_PairedDevicePropertiesRequest());
    case Request::TConnectedDevicePropertiesRequest:
      return actor->DoRequest(aRequest.get_ConnectedDevicePropertiesRequest());
    case Request::TSetPinCodeRequest:
      return actor->DoRequest(aRequest.get_SetPinCodeRequest());
    case Request::TSetPasskeyRequest:
      return actor->DoRequest(aRequest.get_SetPasskeyRequest());
    case Request::TConfirmPairingConfirmationRequest:
      return actor->DoRequest(aRequest.get_ConfirmPairingConfirmationRequest());
    case Request::TConfirmAuthorizationRequest:
      return actor->DoRequest(aRequest.get_ConfirmAuthorizationRequest());
    case Request::TDenyPairingConfirmationRequest:
      return actor->DoRequest(aRequest.get_DenyPairingConfirmationRequest());
    case Request::TDenyAuthorizationRequest:
      return actor->DoRequest(aRequest.get_DenyAuthorizationRequest());
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
    default:
      MOZ_NOT_REACHED("Unknown type!");
      return false;
  }

  MOZ_NOT_REACHED("Should never get here!");
  return false;
}

PBluetoothRequestParent*
BluetoothParent::AllocPBluetoothRequest(const Request& aRequest)
{
  MOZ_ASSERT(mService);
  return new BluetoothRequestParent(mService);
}

bool
BluetoothParent::DeallocPBluetoothRequest(PBluetoothRequestParent* aActor)
{
  delete aActor;
  return true;
}

void
BluetoothParent::Notify(const BluetoothSignal& aSignal)
{
  unused << SendNotify(aSignal);
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
BluetoothRequestParent::DoRequest(const DefaultAdapterPathRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDefaultAdapterPathRequest);

  nsresult rv = mService->GetDefaultAdapterPathInternal(mReplyRunnable.get());
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

  nsresult rv =
    mService->StartDiscoveryInternal(mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const StopDiscoveryRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TStopDiscoveryRequest);

  nsresult rv =
    mService->StopDiscoveryInternal(mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);

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
BluetoothRequestParent::DoRequest(const ConnectedDevicePropertiesRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConnectedDevicePropertiesRequest);
  nsresult rv =
    mService->GetConnectedDevicePropertiesInternal(aRequest.profileId(),
                                                mReplyRunnable.get());
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const SetPinCodeRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSetPinCodeRequest);

  bool result =
    mService->SetPinCodeInternal(aRequest.path(),
                                 aRequest.pincode(),
                                 mReplyRunnable.get());

  NS_ENSURE_TRUE(result, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const SetPasskeyRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSetPasskeyRequest);

  bool result =
    mService->SetPasskeyInternal(aRequest.path(),
                                 aRequest.passkey(),
                                 mReplyRunnable.get());

  NS_ENSURE_TRUE(result, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const ConfirmPairingConfirmationRequest&
                                  aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConfirmPairingConfirmationRequest);

  bool result =
    mService->SetPairingConfirmationInternal(aRequest.path(),
                                             true,
                                             mReplyRunnable.get());

  NS_ENSURE_TRUE(result, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const ConfirmAuthorizationRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConfirmAuthorizationRequest);

  bool result =
    mService->SetAuthorizationInternal(aRequest.path(),
                                       true,
                                       mReplyRunnable.get());

  NS_ENSURE_TRUE(result, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const DenyPairingConfirmationRequest&
                                  aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDenyPairingConfirmationRequest);

  bool result =
    mService->SetPairingConfirmationInternal(aRequest.path(),
                                             false,
                                             mReplyRunnable.get());

  NS_ENSURE_TRUE(result, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const DenyAuthorizationRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDenyAuthorizationRequest);

  bool result =
    mService->SetAuthorizationInternal(aRequest.path(),
                                       false,
                                       mReplyRunnable.get());

  NS_ENSURE_TRUE(result, false);

  return true;
}

bool
BluetoothRequestParent::DoRequest(const ConnectRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConnectRequest);

  mService->Connect(aRequest.address(),
                    aRequest.profileId(),
                    mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const DisconnectRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDisconnectRequest);

  mService->Disconnect(aRequest.profileId(),
                       mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const SendFileRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TSendFileRequest);

  mService->SendFile(aRequest.devicePath(),
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

  mService->StopSendingFile(aRequest.devicePath(),
                            mReplyRunnable.get());

  return true;
}

bool
BluetoothRequestParent::DoRequest(const ConfirmReceivingFileRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TConfirmReceivingFileRequest);

  mService->ConfirmReceivingFile(aRequest.devicePath(),
                                 true,
                                 mReplyRunnable.get());
  return true;
}

bool
BluetoothRequestParent::DoRequest(const DenyReceivingFileRequest& aRequest)
{
  MOZ_ASSERT(mService);
  MOZ_ASSERT(mRequestType == Request::TDenyReceivingFileRequest);

  mService->ConfirmReceivingFile(aRequest.devicePath(),
                                 false,
                                 mReplyRunnable.get());
  return true;
}
