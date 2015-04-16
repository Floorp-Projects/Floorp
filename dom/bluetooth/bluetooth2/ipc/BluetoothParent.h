/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_ipc_bluetoothparent_h__
#define mozilla_dom_bluetooth_ipc_bluetoothparent_h__

#include "mozilla/dom/bluetooth/BluetoothCommon.h"

#include "mozilla/dom/bluetooth/PBluetoothParent.h"
#include "mozilla/dom/bluetooth/PBluetoothRequestParent.h"

#include "mozilla/Attributes.h"
#include "mozilla/Observer.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

template <class T>
class nsRevocableEventPtr;

namespace mozilla {
namespace dom {

class ContentParent;

} // namespace dom
} // namespace mozilla

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothService;

/*******************************************************************************
 * BluetoothParent
 ******************************************************************************/

class BluetoothParent : public PBluetoothParent,
                        public mozilla::Observer<BluetoothSignal>
{
  friend class mozilla::dom::ContentParent;

  enum ShutdownState
  {
    Running = 0,
    SentBeginShutdown,
    ReceivedStopNotifying,
    SentNotificationsStopped,
    Dead
  };

  nsRefPtr<BluetoothService> mService;
  ShutdownState mShutdownState;
  bool mReceivedStopNotifying;
  bool mSentBeginShutdown;

public:
  void
  BeginShutdown();

protected:
  BluetoothParent();
  virtual ~BluetoothParent();

  bool
  InitWithService(BluetoothService* aService);

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  RecvRegisterSignalHandler(const nsString& aNode) override;

  virtual bool
  RecvUnregisterSignalHandler(const nsString& aNode) override;

  virtual bool
  RecvStopNotifying() override;

  virtual bool
  RecvPBluetoothRequestConstructor(PBluetoothRequestParent* aActor,
                                   const Request& aRequest) override;

  virtual PBluetoothRequestParent*
  AllocPBluetoothRequestParent(const Request& aRequest) override;

  virtual bool
  DeallocPBluetoothRequestParent(PBluetoothRequestParent* aActor) override;

  virtual void
  Notify(const BluetoothSignal& aSignal) override;

private:
  void
  UnregisterAllSignalHandlers();
};

/*******************************************************************************
 * BluetoothAdapterRequestParent
 ******************************************************************************/

class BluetoothRequestParent : public PBluetoothRequestParent
{
  class ReplyRunnable;
  friend class BluetoothParent;

  friend class ReplyRunnable;

  nsRefPtr<BluetoothService> mService;
  nsRevocableEventPtr<ReplyRunnable> mReplyRunnable;

#ifdef DEBUG
  Request::Type mRequestType;
#endif

protected:
  BluetoothRequestParent(BluetoothService* aService);
  virtual ~BluetoothRequestParent();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  void
  RequestComplete();

  bool
  DoRequest(const GetAdaptersRequest& aRequest);

  bool
  DoRequest(const StartBluetoothRequest& aRequest);

  bool
  DoRequest(const StopBluetoothRequest& aRequest);

  bool
  DoRequest(const SetPropertyRequest& aRequest);

  bool
  DoRequest(const GetPropertyRequest& aRequest);

  bool
  DoRequest(const StartDiscoveryRequest& aRequest);

  bool
  DoRequest(const StopDiscoveryRequest& aRequest);

  bool
  DoRequest(const PairRequest& aRequest);

  bool
  DoRequest(const UnpairRequest& aRequest);

  bool
  DoRequest(const PairedDevicePropertiesRequest& aRequest);

  bool
  DoRequest(const ConnectedDevicePropertiesRequest& aRequest);

  bool
  DoRequest(const FetchUuidsRequest& aRequest);

  bool
  DoRequest(const SetPinCodeRequest& aRequest);

  bool
  DoRequest(const SetPasskeyRequest& aRequest);

  bool
  DoRequest(const ConfirmPairingConfirmationRequest& aRequest);

  bool
  DoRequest(const DenyPairingConfirmationRequest& aRequest);

  bool
  DoRequest(const PinReplyRequest& aRequest);

  bool
  DoRequest(const SspReplyRequest& aRequest);

  bool
  DoRequest(const ConnectRequest& aRequest);

  bool
  DoRequest(const DisconnectRequest& aRequest);

  bool
  DoRequest(const SendFileRequest& aRequest);

  bool
  DoRequest(const StopSendingFileRequest& aRequest);

  bool
  DoRequest(const ConfirmReceivingFileRequest& aRequest);

  bool
  DoRequest(const DenyReceivingFileRequest& aRequest);

  bool
  DoRequest(const ConnectScoRequest& aRequest);

  bool
  DoRequest(const DisconnectScoRequest& aRequest);

  bool
  DoRequest(const IsScoConnectedRequest& aRequest);

#ifdef MOZ_B2G_RIL
  bool
  DoRequest(const AnswerWaitingCallRequest& aRequest);

  bool
  DoRequest(const IgnoreWaitingCallRequest& aRequest);

  bool
  DoRequest(const ToggleCallsRequest& aRequest);
#endif

  bool
  DoRequest(const SendMetaDataRequest& aRequest);

  bool
  DoRequest(const SendPlayStatusRequest& aRequest);

  bool
  DoRequest(const ConnectGattClientRequest& aRequest);

  bool
  DoRequest(const DisconnectGattClientRequest& aRequest);

  bool
  DoRequest(const DiscoverGattServicesRequest& aRequest);

  bool
  DoRequest(const GattClientStartNotificationsRequest& aRequest);

  bool
  DoRequest(const GattClientStopNotificationsRequest& aRequest);

  bool
  DoRequest(const UnregisterGattClientRequest& aRequest);

  bool
  DoRequest(const GattClientReadRemoteRssiRequest& aRequest);

  bool
  DoRequest(const GattClientReadCharacteristicValueRequest& aRequest);

  bool
  DoRequest(const GattClientWriteCharacteristicValueRequest& aRequest);

  bool
  DoRequest(const GattClientReadDescriptorValueRequest& aRequest);

  bool
  DoRequest(const GattClientWriteDescriptorValueRequest& aRequest);
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_ipc_bluetoothparent_h__
