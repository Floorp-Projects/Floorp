/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonInterface.h"
#include <cutils/properties.h>
#include <fcntl.h>
#include <stdlib.h>
#include "BluetoothDaemonA2dpInterface.h"
#include "BluetoothDaemonAvrcpInterface.h"
#include "BluetoothDaemonCoreInterface.h"
#include "BluetoothDaemonGattInterface.h"
#include "BluetoothDaemonHandsfreeInterface.h"
#include "BluetoothDaemonHelpers.h"
#include "BluetoothDaemonSetupInterface.h"
#include "BluetoothDaemonSocketInterface.h"
#include "mozilla/ipc/DaemonRunnables.h"
#include "mozilla/ipc/DaemonSocket.h"
#include "mozilla/ipc/DaemonSocketConnector.h"
#include "mozilla/ipc/ListenSocket.h"
#include "mozilla/unused.h"

BEGIN_BLUETOOTH_NAMESPACE

using namespace mozilla::ipc;

static const int sRetryInterval = 100; // ms

BluetoothNotificationHandler*
  BluetoothDaemonInterface::sNotificationHandler;

//
// Protocol handling
//

// |BluetoothDaemonProtocol| is the central class for communicating
// with the Bluetooth daemon. It maintains both socket connections
// to the external daemon and implements the complete HAL protocol
// by inheriting from base-class modules.
//
// Each |BluetoothDaemon*Module| class implements an individual
// module of the HAL protocol. Each class contains the abstract
// method
//
//  - |Send|.
//
// Module classes use |Send| to send out command PDUs. The socket
// in |BluetoothDaemonProtocol| is required for sending. The abstract
// method hides all these internal details from the modules.
//
// |BluetoothDaemonProtocol| also handles PDU receiving. It implements
// the method |Handle| from |DaemonSocketIOConsumer|. The socket
// connections of type |DaemonSocket| invoke this method
// to forward received PDUs for processing by higher layers. The
// implementation of |Handle| checks the service id of the PDU and
// forwards it to the correct module class using the module's method
// |HandleSvc|. Further PDU processing is module-dependent.
//
// To summarize the interface between |BluetoothDaemonProtocol| and
// modules; the former implements the abstract method
//
//  - |Send|,
//
// which allow modules to send out data. Each module implements the
// method
//
//  - |HandleSvc|,
//
// which is called by |BluetoothDaemonProtcol| to hand over received
// PDUs into a module.
//
class BluetoothDaemonProtocol final
  : public DaemonSocketIOConsumer
  , public BluetoothDaemonSetupModule
  , public BluetoothDaemonCoreModule
  , public BluetoothDaemonSocketModule
  , public BluetoothDaemonHandsfreeModule
  , public BluetoothDaemonA2dpModule
  , public BluetoothDaemonAvrcpModule
  , public BluetoothDaemonGattModule
{
public:
  BluetoothDaemonProtocol();

  void SetConnection(DaemonSocket* aConnection);

  // Outgoing PDUs
  //

  nsresult Send(DaemonSocketPDU* aPDU,
                DaemonSocketResultHandler* aRes) override;

  void StoreResultHandler(const DaemonSocketPDU& aPDU) override;

  // Incoming PUDs
  //

  void Handle(DaemonSocketPDU& aPDU) override;

  already_AddRefed<DaemonSocketResultHandler> FetchResultHandler(
    const DaemonSocketPDUHeader& aHeader);

private:
  void HandleSetupSvc(const DaemonSocketPDUHeader& aHeader,
                      DaemonSocketPDU& aPDU,
                      DaemonSocketResultHandler* aRes);
  void HandleCoreSvc(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     DaemonSocketResultHandler* aRes);
  void HandleSocketSvc(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU,
                       DaemonSocketResultHandler* aRes);
  void HandleHandsfreeSvc(const DaemonSocketPDUHeader& aHeader,
                          DaemonSocketPDU& aPDU,
                          DaemonSocketResultHandler* aRes);
  void HandleA2dpSvc(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     DaemonSocketResultHandler* aUserData);
  void HandleAvrcpSvc(const DaemonSocketPDUHeader& aHeader,
                      DaemonSocketPDU& aPDU,
                      DaemonSocketResultHandler* aRes);
  void HandleGattSvc(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     DaemonSocketResultHandler* aRes);

  DaemonSocket* mConnection;
  nsTArray<RefPtr<DaemonSocketResultHandler>> mResQ;
};

BluetoothDaemonProtocol::BluetoothDaemonProtocol()
{ }

void
BluetoothDaemonProtocol::SetConnection(DaemonSocket* aConnection)
{
  mConnection = aConnection;
}

nsresult
BluetoothDaemonProtocol::Send(DaemonSocketPDU* aPDU,
                              DaemonSocketResultHandler* aRes)
{
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(aPDU);

  aPDU->SetConsumer(this);
  aPDU->SetResultHandler(aRes);
  aPDU->UpdateHeader();

  if (mConnection->GetConnectionStatus() == SOCKET_DISCONNECTED) {
    BT_LOGR("Connection to Bluetooth daemon is closed.");
    return NS_ERROR_FAILURE;
  }

  mConnection->SendSocketData(aPDU); // Forward PDU to command channel

  return NS_OK;
}

void
BluetoothDaemonProtocol::HandleSetupSvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  BluetoothDaemonSetupModule::HandleSvc(aHeader, aPDU, aRes);
}

void
BluetoothDaemonProtocol::HandleCoreSvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  BluetoothDaemonCoreModule::HandleSvc(aHeader, aPDU, aRes);
}

void
BluetoothDaemonProtocol::HandleSocketSvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  BluetoothDaemonSocketModule::HandleSvc(aHeader, aPDU, aRes);
}

void
BluetoothDaemonProtocol::HandleHandsfreeSvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  BluetoothDaemonHandsfreeModule::HandleSvc(aHeader, aPDU, aRes);
}

void
BluetoothDaemonProtocol::HandleA2dpSvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  BluetoothDaemonA2dpModule::HandleSvc(aHeader, aPDU, aRes);
}

void
BluetoothDaemonProtocol::HandleAvrcpSvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  BluetoothDaemonAvrcpModule::HandleSvc(aHeader, aPDU, aRes);
}

void
BluetoothDaemonProtocol::HandleGattSvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  BluetoothDaemonGattModule::HandleSvc(aHeader, aPDU, aRes);
}

void
BluetoothDaemonProtocol::Handle(DaemonSocketPDU& aPDU)
{
  static void (BluetoothDaemonProtocol::* const HandleSvc[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&,
    DaemonSocketResultHandler*) = {
    [BluetoothDaemonSetupModule::SERVICE_ID] =
      &BluetoothDaemonProtocol::HandleSetupSvc,
    [BluetoothDaemonCoreModule::SERVICE_ID] =
      &BluetoothDaemonProtocol::HandleCoreSvc,
    [BluetoothDaemonSocketModule::SERVICE_ID] =
      &BluetoothDaemonProtocol::HandleSocketSvc,
    [0x03] = nullptr, // HID host
    [0x04] = nullptr, // PAN
    [BluetoothDaemonHandsfreeModule::SERVICE_ID] =
      &BluetoothDaemonProtocol::HandleHandsfreeSvc,
    [BluetoothDaemonA2dpModule::SERVICE_ID] =
      &BluetoothDaemonProtocol::HandleA2dpSvc,
    [0x07] = nullptr, // Health
    [BluetoothDaemonAvrcpModule::SERVICE_ID] =
      &BluetoothDaemonProtocol::HandleAvrcpSvc,
    [BluetoothDaemonGattModule::SERVICE_ID] =
      &BluetoothDaemonProtocol::HandleGattSvc
  };

  DaemonSocketPDUHeader header;

  if (NS_FAILED(UnpackPDU(aPDU, header)) ||
      NS_WARN_IF(!(header.mService < MOZ_ARRAY_LENGTH(HandleSvc))) ||
      NS_WARN_IF(!(HandleSvc[header.mService]))) {
    return;
  }

  RefPtr<DaemonSocketResultHandler> res = FetchResultHandler(header);

  (this->*(HandleSvc[header.mService]))(header, aPDU, res);
}

void
BluetoothDaemonProtocol::StoreResultHandler(const DaemonSocketPDU& aPDU)
{
  MOZ_ASSERT(!NS_IsMainThread());

  mResQ.AppendElement(aPDU.GetResultHandler());
}

already_AddRefed<DaemonSocketResultHandler>
BluetoothDaemonProtocol::FetchResultHandler(
  const DaemonSocketPDUHeader& aHeader)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (aHeader.mOpcode & 0x80) {
    return nullptr; // Ignore notifications
  }

  RefPtr<DaemonSocketResultHandler> userData = mResQ.ElementAt(0);
  mResQ.RemoveElementAt(0);

  return userData.forget();
}

//
// Interface
//

static bool
IsDaemonRunning()
{
  char value[PROPERTY_VALUE_MAX];
  NS_WARN_IF(property_get("init.svc.bluetoothd", value, "") < 0);
  if (strcmp(value, "running")) {
    BT_LOGR("[RESTART] Bluetooth daemon state <%s>", value);
    return false;
  }

  return true;
}

BluetoothDaemonInterface*
BluetoothDaemonInterface::GetInstance()
{
  static BluetoothDaemonInterface* sBluetoothInterface;

  if (sBluetoothInterface) {
    return sBluetoothInterface;
  }

  sBluetoothInterface = new BluetoothDaemonInterface();

  return sBluetoothInterface;
}

BluetoothDaemonInterface::BluetoothDaemonInterface()
{ }

BluetoothDaemonInterface::~BluetoothDaemonInterface()
{ }

class BluetoothDaemonInterface::StartDaemonTask final : public Task
{
public:
  StartDaemonTask(BluetoothDaemonInterface* aInterface,
                  const nsACString& aCommand)
    : mInterface(aInterface)
    , mCommand(aCommand)
  {
    MOZ_ASSERT(mInterface);
  }

  void Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("Start Daemon Task");
    // Start Bluetooth daemon again
    if (NS_WARN_IF(property_set("ctl.start", mCommand.get()) < 0)) {
      mInterface->OnConnectError(CMD_CHANNEL);
    }

    // We're done if Bluetooth daemon is already running
    if (IsDaemonRunning()) {
      return;
    }

    // Otherwise try again later
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
      new StartDaemonTask(mInterface, mCommand), sRetryInterval);
  }

private:
  BluetoothDaemonInterface* mInterface;
  nsCString mCommand;
};

class BluetoothDaemonInterface::InitResultHandler final
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

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mRes) {
      mRes->OnError(aStatus);
    }
  }

  void RegisterModule() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mInterface->mProtocol);

    if (!mRegisteredSocketModule) {
      mRegisteredSocketModule = true;
      // Init, step 5: Register Socket module
      mInterface->mProtocol->RegisterModuleCmd(
        SETUP_SERVICE_ID_SOCKET,
        0x00,
        BluetoothDaemonSocketModule::MAX_NUM_CLIENTS, this);
    } else if (mRes) {
      // Init, step 6: Signal success to caller
      mRes->Init();
    }
  }

private:
  BluetoothDaemonInterface* mInterface;
  RefPtr<BluetoothResultHandler> mRes;
  bool mRegisteredSocketModule;
};

/*
 * The init procedure consists of several steps.
 *
 *  (1) Start listening for the command channel's socket connection: We
 *      do this before anything else, so that we don't miss connection
 *      requests from the Bluetooth daemon. This step will create a
 *      listen socket.
 *
 *  (2) Start the Bluetooth daemon: When the daemon starts up it will
 *      open two socket connections to Gecko and thus create the command
 *      and notification channels. Gecko already opened the listen socket
 *      in step (1). Step (2) ends with the creation of the command channel.
 *
 *  (3) Start listening for the notification channel's socket connection:
 *      At the end of step (2), the command channel was opened by the
 *      daemon. In step (3), the daemon immediately tries to open the
 *      next socket for the notification channel. Gecko will accept the
 *      incoming connection request for the notification channel. The
 *      listen socket remained open after step (2), so there's no race
 *      condition between Gecko and the Bluetooth daemon.
 *
 *  (4)(5) Register Core and Socket modules: The Core and Socket modules
 *      are always available and have to be registered after opening the
 *      socket connections during the initialization.
 *
 *  (6) Signal success to the caller.
 *
 * If any step fails, we roll-back the procedure and signal an error to the
 * caller.
 */
void
BluetoothDaemonInterface::Init(
  BluetoothNotificationHandler* aNotificationHandler,
  BluetoothResultHandler* aRes)
{
#define BASE_SOCKET_NAME "bluetoothd"
  static unsigned long POSTFIX_LENGTH = 16;

  // First of all, we set the notification handler. Backend crashes
  // will be reported this way.
  sNotificationHandler = aNotificationHandler;

  // If we could not cleanup properly before and an old
  // instance of the daemon is still running, we kill it
  // here.
  Unused << NS_WARN_IF(property_set("ctl.stop", "bluetoothd"));

  mResultHandlerQ.AppendElement(aRes);

  if (!mProtocol) {
    mProtocol = new BluetoothDaemonProtocol();
  }

  if (!mListenSocket) {
    mListenSocket = new ListenSocket(this, LISTEN_SOCKET);
  }

  // Init, step 1: Listen for command channel... */

  if (!mCmdChannel) {
    mCmdChannel = new DaemonSocket(mProtocol, this, CMD_CHANNEL);
  } else if (
    NS_WARN_IF(mCmdChannel->GetConnectionStatus() == SOCKET_CONNECTED)) {
    // Command channel should not be open; let's close it.
    mCmdChannel->Close();
  }

  // The listen socket's name is generated with a random postfix. This
  // avoids naming collisions if we still have a listen socket from a
  // previously failed cleanup. It also makes it hard for malicious
  // external programs to capture the socket name or connect before
  // the daemon can do so. If no random postfix can be generated, we
  // simply use the base name as-is.
  nsresult rv = DaemonSocketConnector::CreateRandomAddressString(
    NS_LITERAL_CSTRING(BASE_SOCKET_NAME), POSTFIX_LENGTH, mListenSocketName);
  if (NS_FAILED(rv)) {
    mListenSocketName.AssignLiteral(BASE_SOCKET_NAME);
  }

  rv = mListenSocket->Listen(new DaemonSocketConnector(mListenSocketName),
                             mCmdChannel);
  if (NS_FAILED(rv)) {
    OnConnectError(CMD_CHANNEL);
    return;
  }

  // The protocol implementation needs a command channel for
  // sending commands to the daemon. We set it here, because
  // this is the earliest time when it's available.
  mProtocol->SetConnection(mCmdChannel);
}

class BluetoothDaemonInterface::CleanupResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  CleanupResultHandler(BluetoothDaemonInterface* aInterface)
    : mInterface(aInterface)
    , mUnregisteredCoreModule(false)
  {
    MOZ_ASSERT(mInterface);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    Proceed();
  }

  void UnregisterModule() override
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
      mInterface->mProtocol->UnregisterModuleCmd(SETUP_SERVICE_ID_CORE, this);
    } else {
      // Cleanup, step 3: Close command channel
      mInterface->mCmdChannel->Close();
    }
  }

  BluetoothDaemonInterface* mInterface;
  bool mUnregisteredCoreModule;
};

/*
 * Cleaning up is inverse to initialization, except for the shutdown
 * of the socket connections in step (3)
 *
 *  (1)(2) Unregister Socket and Core modules: These modules have been
 *      registered during initialization and need to be unregistered
 *      here. We assume that all other modules are already unregistered.
 *
 *  (3) Close command socket: We only close the command socket. The
 *      daemon will then send any final notifications and close the
 *      notification socket on its side. Once we see the notification
 *      socket's disconnect, we continue with the cleanup.
 *
 *  (4) Close listen socket: The listen socket is not active any longer
 *      and we simply close it.
 *
 *  (5) Signal success to the caller.
 *
 * We don't have to stop the daemon explicitly. It will cleanup and quit
 * after it closed the notification socket.
 *
 * Rolling-back half-completed cleanups is not possible. In the case of
 * an error, we simply push forward and try to recover during the next
 * initialization.
 */
void
BluetoothDaemonInterface::Cleanup(BluetoothResultHandler* aRes)
{
  sNotificationHandler = nullptr;

  // Cleanup, step 1: Unregister Socket module
  nsresult rv = mProtocol->UnregisterModuleCmd(
    SETUP_SERVICE_ID_SOCKET, new CleanupResultHandler(this));
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
    return;
  }

  mResultHandlerQ.AppendElement(aRes);
}

void
BluetoothDaemonInterface::DispatchError(BluetoothResultHandler* aRes,
                                        BluetoothStatus aStatus)
{
  DaemonResultRunnable1<
    BluetoothResultHandler, void, BluetoothStatus, BluetoothStatus>::Dispatch(
    aRes, &BluetoothResultHandler::OnError,
    ConstantInitOp1<BluetoothStatus>(aStatus));
}

void
BluetoothDaemonInterface::DispatchError(BluetoothResultHandler* aRes,
                                        nsresult aRv)
{
  BluetoothStatus status;

  if (NS_WARN_IF(NS_FAILED(Convert(aRv, status)))) {
    status = STATUS_FAIL;
  }
  DispatchError(aRes, status);
}

// Service Interfaces
//

BluetoothSetupInterface*
BluetoothDaemonInterface::GetBluetoothSetupInterface()
{
  if (mSetupInterface) {
    return mSetupInterface;
  }

  mSetupInterface = new BluetoothDaemonSetupInterface(mProtocol);

  return mSetupInterface;
}

BluetoothCoreInterface*
BluetoothDaemonInterface::GetBluetoothCoreInterface()
{
  if (mCoreInterface) {
    return mCoreInterface;
  }

  mCoreInterface = new BluetoothDaemonCoreInterface(mProtocol);

  return mCoreInterface;
}

BluetoothSocketInterface*
BluetoothDaemonInterface::GetBluetoothSocketInterface()
{
  if (mSocketInterface) {
    return mSocketInterface;
  }

  mSocketInterface = new BluetoothDaemonSocketInterface(mProtocol);

  return mSocketInterface;
}

BluetoothHandsfreeInterface*
BluetoothDaemonInterface::GetBluetoothHandsfreeInterface()
{
  if (mHandsfreeInterface) {
    return mHandsfreeInterface;
  }

  mHandsfreeInterface = new BluetoothDaemonHandsfreeInterface(mProtocol);

  return mHandsfreeInterface;
}

BluetoothA2dpInterface*
BluetoothDaemonInterface::GetBluetoothA2dpInterface()
{
  if (mA2dpInterface) {
    return mA2dpInterface;
  }

  mA2dpInterface = new BluetoothDaemonA2dpInterface(mProtocol);

  return mA2dpInterface;
}

BluetoothAvrcpInterface*
BluetoothDaemonInterface::GetBluetoothAvrcpInterface()
{
  if (mAvrcpInterface) {
    return mAvrcpInterface;
  }

  mAvrcpInterface = new BluetoothDaemonAvrcpInterface(mProtocol);

  return mAvrcpInterface;
}

BluetoothGattInterface*
BluetoothDaemonInterface::GetBluetoothGattInterface()
{
  if (mGattInterface) {
    return mGattInterface;
  }

  mGattInterface = new BluetoothDaemonGattInterface(mProtocol);

  return mGattInterface;
}

// |DaemonSocketConsumer|, |ListenSocketConsumer|

void
BluetoothDaemonInterface::OnConnectSuccess(int aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResultHandlerQ.IsEmpty());

  switch (aIndex) {
    case LISTEN_SOCKET: {
        // Init, step 2: Start Bluetooth daemon */
        nsCString value("bluetoothd:-a ");
        value.Append(mListenSocketName);
        if (NS_WARN_IF(property_set("ctl.start", value.get()) < 0)) {
          OnConnectError(CMD_CHANNEL);
        }

        /*
         * If Bluetooth daemon is not running, retry to start it later.
         *
         * This condition happens when when we restart Bluetooth daemon
         * immediately after it crashed, as the daemon state remains 'stopping'
         * instead of 'stopped'. Due to the limitation of property service,
         * hereby add delay. See Bug 1143925 Comment 41.
         */
        if (!IsDaemonRunning()) {
          MessageLoop::current()->PostDelayedTask(FROM_HERE,
              new StartDaemonTask(this, value), sRetryInterval);
        }
      }
      break;
    case CMD_CHANNEL:
      // Init, step 3: Listen for notification channel...
      if (!mNtfChannel) {
        mNtfChannel = new DaemonSocket(mProtocol, this, NTF_CHANNEL);
      } else if (
        NS_WARN_IF(mNtfChannel->GetConnectionStatus() == SOCKET_CONNECTED)) {
        /* Notification channel should not be open; let's close it. */
        mNtfChannel->Close();
      }
      if (NS_FAILED(mListenSocket->Listen(mNtfChannel))) {
        OnConnectError(NTF_CHANNEL);
      }
      break;
    case NTF_CHANNEL: {
        RefPtr<BluetoothResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);

        // Init, step 4: Register Core module
        nsresult rv = mProtocol->RegisterModuleCmd(
          SETUP_SERVICE_ID_CORE,
          0x00,
          BluetoothDaemonCoreModule::MAX_NUM_CLIENTS,
          new InitResultHandler(this, res));
        if (NS_FAILED(rv) && res) {
          DispatchError(res, STATUS_FAIL);
        }
      }
      break;
  }
}

void
BluetoothDaemonInterface::OnConnectError(int aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResultHandlerQ.IsEmpty());

  switch (aIndex) {
    case NTF_CHANNEL:
      // Close command channel
      mCmdChannel->Close();
    case CMD_CHANNEL:
      // Stop daemon and close listen socket
      Unused << NS_WARN_IF(property_set("ctl.stop", "bluetoothd"));
      mListenSocket->Close();
    case LISTEN_SOCKET:
      if (!mResultHandlerQ.IsEmpty()) {
        // Signal error to caller
        RefPtr<BluetoothResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);

        if (res) {
          DispatchError(res, STATUS_FAIL);
        }
      }
      break;
  }
}

/*
 * Three cases for restarting:
 * a) during startup
 * b) during regular service
 * c) during shutdown
 * For (a)/(c) cases, mResultHandlerQ contains an element, but case (b)
 * mResultHandlerQ shall be empty. The following procedure to recover from crashed
 * consists of several steps for case (b).
 * 1) Close listen socket.
 * 2) Wait for all sockets disconnected and inform BluetoothServiceBluedroid to
 * perform the regular stop bluetooth procedure.
 * 3) When stop bluetooth procedures complete, fire
 * AdapterStateChangedNotification to cleanup all necessary data members and
 * deinit ProfileManagers.
 * 4) After all resources cleanup, call |StartBluetooth|
 */
void
BluetoothDaemonInterface::OnDisconnect(int aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());

  switch (aIndex) {
    case CMD_CHANNEL:
      // We don't have to do anything here. Step 4 is triggered
      // by the daemon.
      break;
    case NTF_CHANNEL:
      // Cleanup, step 4 (Recovery, step 1): Close listen socket
      mListenSocket->Close();
      break;
    case LISTEN_SOCKET:
      if (!mResultHandlerQ.IsEmpty()) {
        RefPtr<BluetoothResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);
        // Cleanup, step 5: Signal success to caller
        if (res) {
          res->Cleanup();
        }
      }
      break;
  }

  /* For recovery make sure all sockets disconnected, in order to avoid
   * the remaining disconnects interfere with the restart procedure.
   */
  if (sNotificationHandler && mResultHandlerQ.IsEmpty()) {
    if (mListenSocket->GetConnectionStatus() == SOCKET_DISCONNECTED &&
        mCmdChannel->GetConnectionStatus() == SOCKET_DISCONNECTED &&
        mNtfChannel->GetConnectionStatus() == SOCKET_DISCONNECTED) {
      // Assume daemon crashed during regular service; notify
      // BluetoothServiceBluedroid to prepare restart-daemon procedure
      sNotificationHandler->BackendErrorNotification(true);
      sNotificationHandler = nullptr;
    }
  }
}

END_BLUETOOTH_NAMESPACE
