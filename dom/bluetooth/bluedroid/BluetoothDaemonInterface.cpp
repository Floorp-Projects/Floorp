/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonInterface.h"
#include "BluetoothDaemonHelpers.h"
#include "BluetoothDaemonSetupInterface.h"
#include "mozilla/unused.h"

using namespace mozilla::ipc;

BEGIN_BLUETOOTH_NAMESPACE

template<class T>
struct interface_traits
{ };

//
// Protocol initialization and setup
//

class BluetoothDaemonSetupModule
{
public:
  virtual nsresult Send(BluetoothDaemonPDU* aPDU, void* aUserData) = 0;

  // Commands
  //

  nsresult RegisterModuleCmd(uint8_t aId, uint8_t aMode,
                             BluetoothSetupResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x00, 0x01, 0));

    nsresult rv = PackPDU(aId, aMode, *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult UnregisterModuleCmd(uint8_t aId,
                               BluetoothSetupResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x00, 0x02, 0));

    nsresult rv = PackPDU(aId, *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult ConfigurationCmd(const BluetoothConfigurationParameter* aParam,
                            uint8_t aLen, BluetoothSetupResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x00, 0x03, 0));

    nsresult rv = PackPDU(
      aLen, PackArray<BluetoothConfigurationParameter>(aParam, aLen), *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

protected:

  // Called to handle PDUs with Service field equal to 0x00, which
  // contains internal operations for setup and configuration.
  void HandleSvc(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU, void* aUserData)
  {
    static void (BluetoothDaemonSetupModule::* const HandleRsp[])(
      const BluetoothDaemonPDUHeader&,
      BluetoothDaemonPDU&,
      BluetoothSetupResultHandler*) = {
      [0] = &BluetoothDaemonSetupModule::ErrorRsp,
      [1] = &BluetoothDaemonSetupModule::RegisterModuleRsp,
      [2] = &BluetoothDaemonSetupModule::UnregisterModuleRsp,
      [3] = &BluetoothDaemonSetupModule::ConfigurationRsp
    };

    if (NS_WARN_IF(aHeader.mOpcode >= MOZ_ARRAY_LENGTH(HandleRsp)) ||
        NS_WARN_IF(!HandleRsp[aHeader.mOpcode])) {
      return;
    }

    nsRefPtr<BluetoothSetupResultHandler> res =
      already_AddRefed<BluetoothSetupResultHandler>(
        static_cast<BluetoothSetupResultHandler*>(aUserData));

    if (!res) {
      return; // Return early if no result handler has been set
    }

    (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
  }

  nsresult Send(BluetoothDaemonPDU* aPDU, BluetoothSetupResultHandler* aRes)
  {
    aRes->AddRef(); // Keep reference for response
    return Send(aPDU, static_cast<void*>(aRes));
  }

private:

  // Responses
  //

  typedef
    BluetoothDaemonInterfaceRunnable0<BluetoothSetupResultHandler, void>
    ResultRunnable;

  typedef
    BluetoothDaemonInterfaceRunnable1<BluetoothSetupResultHandler, void,
                                      BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void
  ErrorRsp(const BluetoothDaemonPDUHeader& aHeader,
           BluetoothDaemonPDU& aPDU,
           BluetoothSetupResultHandler* aRes)
  {
    ErrorRunnable::Dispatch<0x00, 0x00>(
      aRes, &BluetoothSetupResultHandler::OnError, aPDU);
  }

  void
  RegisterModuleRsp(const BluetoothDaemonPDUHeader& aHeader,
                    BluetoothDaemonPDU& aPDU,
                    BluetoothSetupResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothSetupResultHandler::RegisterModule);
  }

  void
  UnregisterModuleRsp(const BluetoothDaemonPDUHeader& aHeader,
                      BluetoothDaemonPDU& aPDU,
                      BluetoothSetupResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothSetupResultHandler::UnregisterModule);
  }

  void
  ConfigurationRsp(const BluetoothDaemonPDUHeader& aHeader,
                   BluetoothDaemonPDU& aPDU,
                   BluetoothSetupResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothSetupResultHandler::Configuration);
  }
};

//
// Core module
//

static BluetoothNotificationHandler* sNotificationHandler;

//
// Protocol handling
//

class BluetoothDaemonProtocol MOZ_FINAL
  : public BluetoothDaemonPDUConsumer
  , public BluetoothDaemonSetupModule
{
public:
  BluetoothDaemonProtocol(BluetoothDaemonConnection* aConnection);

  // Outgoing PDUs
  //

  nsresult Send(BluetoothDaemonPDU* aPDU, void* aUserData) MOZ_OVERRIDE;

  void StoreUserData(const BluetoothDaemonPDU& aPDU) MOZ_OVERRIDE;

  // Incoming PUDs
  //

  void Handle(BluetoothDaemonPDU& aPDU) MOZ_OVERRIDE;

  void* FetchUserData(const BluetoothDaemonPDUHeader& aHeader);

private:
  void HandleSetupSvc(const BluetoothDaemonPDUHeader& aHeader,
                      BluetoothDaemonPDU& aPDU, void* aUserData);

  BluetoothDaemonConnection* mConnection;
  nsTArray<void*> mUserDataQ;
};

BluetoothDaemonProtocol::BluetoothDaemonProtocol(
  BluetoothDaemonConnection* aConnection)
  : mConnection(aConnection)
{
  MOZ_ASSERT(mConnection);
}

nsresult
BluetoothDaemonProtocol::Send(BluetoothDaemonPDU* aPDU, void* aUserData)
{
  MOZ_ASSERT(aPDU);

  aPDU->SetUserData(aUserData);
  aPDU->UpdateHeader();
  return mConnection->Send(aPDU); // Forward PDU to command channel
}

void
BluetoothDaemonProtocol::HandleSetupSvc(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  void* aUserData)
{
  BluetoothDaemonSetupModule::HandleSvc(aHeader, aPDU, aUserData);
}

void
BluetoothDaemonProtocol::Handle(BluetoothDaemonPDU& aPDU)
{
  static void (BluetoothDaemonProtocol::* const HandleSvc[])(
    const BluetoothDaemonPDUHeader&, BluetoothDaemonPDU&, void*) = {
    [0] = &BluetoothDaemonProtocol::HandleSetupSvc
  };

  BluetoothDaemonPDUHeader header;

  if (NS_FAILED(UnpackPDU(aPDU, header)) ||
      NS_WARN_IF(!(header.mService < MOZ_ARRAY_LENGTH(HandleSvc))) ||
      NS_WARN_IF(!(HandleSvc[header.mService]))) {
    return;
  }

  (this->*(HandleSvc[header.mService]))(header, aPDU, FetchUserData(header));
}

void
BluetoothDaemonProtocol::StoreUserData(const BluetoothDaemonPDU& aPDU)
{
  MOZ_ASSERT(!NS_IsMainThread());

  mUserDataQ.AppendElement(aPDU.GetUserData());
}

void*
BluetoothDaemonProtocol::FetchUserData(const BluetoothDaemonPDUHeader& aHeader)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (aHeader.mOpcode & 0x80) {
    return nullptr; // Ignore notifications
  }

  void* userData = mUserDataQ.ElementAt(0);
  mUserDataQ.RemoveElementAt(0);

  return userData;
}

//
// Channels
//

class BluetoothDaemonChannel MOZ_FINAL : public BluetoothDaemonConnection
{
public:
  BluetoothDaemonChannel(BluetoothDaemonInterface::Channel aChannel);

  nsresult ConnectSocket(BluetoothDaemonInterface* aInterface,
                         BluetoothDaemonPDUConsumer* aConsumer);

  // Connection state
  //

  void OnConnectSuccess() MOZ_OVERRIDE;
  void OnConnectError() MOZ_OVERRIDE;
  void OnDisconnect() MOZ_OVERRIDE;

private:
  BluetoothDaemonInterface* mInterface;
  BluetoothDaemonInterface::Channel mChannel;
};

BluetoothDaemonChannel::BluetoothDaemonChannel(
  BluetoothDaemonInterface::Channel aChannel)
: mInterface(nullptr)
, mChannel(aChannel)
{ }

nsresult
BluetoothDaemonChannel::ConnectSocket(BluetoothDaemonInterface* aInterface,
                                      BluetoothDaemonPDUConsumer* aConsumer)
{
  MOZ_ASSERT(aInterface);

  mInterface = aInterface;

  return BluetoothDaemonConnection::ConnectSocket(aConsumer);
}

void
BluetoothDaemonChannel::OnConnectSuccess()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInterface);

  mInterface->OnConnectSuccess(mChannel);
}

void
BluetoothDaemonChannel::OnConnectError()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInterface);

  mInterface->OnConnectError(mChannel);
  mInterface = nullptr;
}

void
BluetoothDaemonChannel::OnDisconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInterface);

  mInterface->OnDisconnect(mChannel);
  mInterface = nullptr;
}

//
// Interface
//

/* returns the container structure of a variable; _t is the container's
 * type, _v the name of the variable, and _m is _v's field within _t
 */
#define container(_t, _v, _m) \
  ( (_t*)( ((const unsigned char*)(_v)) - offsetof(_t, _m) ) )

BluetoothDaemonInterface*
BluetoothDaemonInterface::GetInstance()
{
  static BluetoothDaemonInterface* sBluetoothInterface;

  if (sBluetoothInterface) {
    return sBluetoothInterface;
  }

  // Only create channel objects here. The connection will be
  // established by |BluetoothDaemonInterface::Init|.

  BluetoothDaemonChannel* cmdChannel =
    new BluetoothDaemonChannel(BluetoothDaemonInterface::CMD_CHANNEL);

  BluetoothDaemonChannel* ntfChannel =
    new BluetoothDaemonChannel(BluetoothDaemonInterface::NTF_CHANNEL);

  // Create a new interface object with the channels and a
  // protocol handler.

  sBluetoothInterface =
    new BluetoothDaemonInterface(cmdChannel,
                                 ntfChannel,
                                 new BluetoothDaemonProtocol(cmdChannel));

  return sBluetoothInterface;
}

BluetoothDaemonInterface::BluetoothDaemonInterface(
  BluetoothDaemonChannel* aCmdChannel,
  BluetoothDaemonChannel* aNtfChannel,
  BluetoothDaemonProtocol* aProtocol)
: mCmdChannel(aCmdChannel)
, mNtfChannel(aNtfChannel)
, mProtocol(aProtocol)
{
  MOZ_ASSERT(mCmdChannel);
  MOZ_ASSERT(mNtfChannel);
  MOZ_ASSERT(mProtocol);
}

BluetoothDaemonInterface::~BluetoothDaemonInterface()
{ }

class BluetoothDaemonInterface::InitResultHandler MOZ_FINAL
  : public BluetoothSetupResultHandler
{
public:
  InitResultHandler(BluetoothDaemonInterface* aInterface,
                    BluetoothResultHandler* aRes)
    : mInterface(aInterface)
    , mRes(aRes)
    , mRegisteredSocketModule(false)
  {
    MOZ_ASSERT(mInterface);
  }

  // We need to call methods from the |BluetoothResultHandler|. Since
  // we're already on the main thread and returned from Init, we don't
  // need to dispatch a new runnable.

  void OnError(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    NS_WARNING(__func__);
    // TODO: close channels

    if (mRes) {
      mRes->OnError(aStatus);
    }
  }

  void RegisterModule() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mInterface->mProtocol);

    NS_WARNING(__func__);
    if (!mRegisteredSocketModule) {
      mRegisteredSocketModule = true;
      // Init, step 4: Register Socket module
      mInterface->mProtocol->RegisterModuleCmd(0x02, 0x00, this);
    } else if (mRes) {
      // Init, step 5: Signal success to caller
      mRes->Init();
    }
  }

private:
  BluetoothDaemonInterface* mInterface;
  nsRefPtr<BluetoothResultHandler> mRes;
  bool mRegisteredSocketModule;
};

void
BluetoothDaemonInterface::OnConnectSuccess(enum Channel aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResultHandlerQ.IsEmpty());

  switch (aChannel) {
    case CMD_CHANNEL:
      // Init, step 2: Connect notification channel...
      if (mNtfChannel->GetConnectionStatus() != SOCKET_CONNECTED) {
        nsresult rv = mNtfChannel->ConnectSocket(this, mProtocol);
        if (NS_FAILED(rv)) {
          OnConnectError(CMD_CHANNEL);
        }
      } else {
        // ...or go to step 3 if channel is already connected.
        OnConnectSuccess(NTF_CHANNEL);
      }
      break;

    case NTF_CHANNEL: {
        nsRefPtr<BluetoothResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);

        // Init, step 3: Register Core module
        nsresult rv = mProtocol->RegisterModuleCmd(
          0x01, 0x00, new InitResultHandler(this, res));
        if (NS_FAILED(rv) && res) {
          DispatchError(res, STATUS_FAIL);
        }
      }
      break;
  }
}

void
BluetoothDaemonInterface::OnConnectError(enum Channel aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResultHandlerQ.IsEmpty());

  switch (aChannel) {
    case NTF_CHANNEL:
      // Close command channel
      mCmdChannel->CloseSocket();
    case CMD_CHANNEL: {
        // Signal error to caller
        nsRefPtr<BluetoothResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);

        if (res) {
          DispatchError(res, STATUS_FAIL);
        }
      }
      break;
  }
}

void
BluetoothDaemonInterface::OnDisconnect(enum Channel aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResultHandlerQ.IsEmpty());

  switch (aChannel) {
    case NTF_CHANNEL:
      // Cleanup, step 4: Close command channel
      mCmdChannel->CloseSocket();
      break;
    case CMD_CHANNEL: {
        nsRefPtr<BluetoothResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);

        // Cleanup, step 5: Signal success to caller
        if (res) {
          res->Cleanup();
        }
      }
      break;
  }
}

void
BluetoothDaemonInterface::Init(
  BluetoothNotificationHandler* aNotificationHandler,
  BluetoothResultHandler* aRes)
{
  sNotificationHandler = aNotificationHandler;

  mResultHandlerQ.AppendElement(aRes);

  // Init, step 1: Connect command channel...
  if (mCmdChannel->GetConnectionStatus() != SOCKET_CONNECTED) {
    nsresult rv = mCmdChannel->ConnectSocket(this, mProtocol);
    if (NS_FAILED(rv)) {
      OnConnectError(CMD_CHANNEL);
    }
  } else {
    // ...or go to step 2 if channel is already connected.
    OnConnectSuccess(CMD_CHANNEL);
  }
}

class BluetoothDaemonInterface::CleanupResultHandler MOZ_FINAL
  : public BluetoothSetupResultHandler
{
public:
  CleanupResultHandler(BluetoothDaemonInterface* aInterface)
    : mInterface(aInterface)
    , mUnregisteredCoreModule(false)
  {
    MOZ_ASSERT(mInterface);
  }

  void OnError(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    Proceed();
  }

  void UnregisterModule() MOZ_OVERRIDE
  {
    Proceed();
  }

private:
  void Proceed()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mInterface->mProtocol);

    if (!mUnregisteredCoreModule) {
      mUnregisteredCoreModule = true;
      // Cleanup, step 2: Unregister Core module
      mInterface->mProtocol->UnregisterModuleCmd(0x01, this);
    } else {
      // Cleanup, step 3: Close notification channel
      mInterface->mNtfChannel->CloseSocket();
    }
  }

  BluetoothDaemonInterface* mInterface;
  bool mUnregisteredCoreModule;
};

void
BluetoothDaemonInterface::Cleanup(BluetoothResultHandler* aRes)
{
  sNotificationHandler = nullptr;

  mResultHandlerQ.AppendElement(aRes);

  // Cleanup, step 1: Unregister Socket module
  mProtocol->UnregisterModuleCmd(0x02, new CleanupResultHandler(this));
}

void
BluetoothDaemonInterface::Enable(BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::Disable(BluetoothResultHandler* aRes)
{
}

/* Adapter Properties */

void
BluetoothDaemonInterface::GetAdapterProperties(BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::GetAdapterProperty(const nsAString& aName,
                                             BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::SetAdapterProperty(
  const BluetoothNamedValue& aProperty, BluetoothResultHandler* aRes)
{
}

/* Remote Device Properties */

void
BluetoothDaemonInterface::GetRemoteDeviceProperties(
  const nsAString& aRemoteAddr, BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::GetRemoteDeviceProperty(
  const nsAString& aRemoteAddr, const nsAString& aName,
  BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::SetRemoteDeviceProperty(
  const nsAString& aRemoteAddr, const BluetoothNamedValue& aProperty,
  BluetoothResultHandler* aRes)
{
}

/* Remote Services */

void
BluetoothDaemonInterface::GetRemoteServiceRecord(const nsAString& aRemoteAddr,
                                                 const uint8_t aUuid[16],
                                                 BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::GetRemoteServices(const nsAString& aRemoteAddr,
                                            BluetoothResultHandler* aRes)
{
}

/* Discovery */

void
BluetoothDaemonInterface::StartDiscovery(BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::CancelDiscovery(BluetoothResultHandler* aRes)
{
}

/* Bonds */

void
BluetoothDaemonInterface::CreateBond(const nsAString& aBdAddr,
                                     BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::RemoveBond(const nsAString& aBdAddr,
                                     BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::CancelBond(const nsAString& aBdAddr,
                                     BluetoothResultHandler* aRes)
{
}

/* Authentication */

void
BluetoothDaemonInterface::PinReply(const nsAString& aBdAddr, bool aAccept,
                                   const nsAString& aPinCode,
                                   BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::SspReply(const nsAString& aBdAddr,
                                   const nsAString& aVariant,
                                   bool aAccept, uint32_t aPasskey,
                                   BluetoothResultHandler* aRes)
{
}

/* DUT Mode */

void
BluetoothDaemonInterface::DutModeConfigure(bool aEnable,
                                           BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::DutModeSend(uint16_t aOpcode, uint8_t* aBuf,
                                      uint8_t aLen,
                                      BluetoothResultHandler* aRes)
{
}

/* LE Mode */

void
BluetoothDaemonInterface::LeTestMode(uint16_t aOpcode, uint8_t* aBuf,
                                     uint8_t aLen,
                                     BluetoothResultHandler* aRes)
{
}

void
BluetoothDaemonInterface::DispatchError(BluetoothResultHandler* aRes,
                                        BluetoothStatus aStatus)
{
  BluetoothDaemonInterfaceRunnable1<
    BluetoothResultHandler, void, BluetoothStatus, BluetoothStatus>::Dispatch(
    aRes, &BluetoothResultHandler::OnError, aStatus);
}

BluetoothSocketInterface*
BluetoothDaemonInterface::GetBluetoothSocketInterface()
{
  return nullptr;
}

BluetoothHandsfreeInterface*
BluetoothDaemonInterface::GetBluetoothHandsfreeInterface()
{
  return nullptr;
}

BluetoothA2dpInterface*
BluetoothDaemonInterface::GetBluetoothA2dpInterface()
{
  return nullptr;
}

BluetoothAvrcpInterface*
BluetoothDaemonInterface::GetBluetoothAvrcpInterface()
{
  return nullptr;
}

END_BLUETOOTH_NAMESPACE
