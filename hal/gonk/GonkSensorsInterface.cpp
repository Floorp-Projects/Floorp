/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkSensorsInterface.h"
#include "GonkSensorsPollInterface.h"
#include "GonkSensorsRegistryInterface.h"
#include "HalLog.h"
#include <mozilla/ipc/DaemonSocket.h>
#include <mozilla/ipc/DaemonSocketConnector.h>
#include <mozilla/ipc/ListenSocket.h>

namespace mozilla {
namespace hal {

using namespace mozilla::ipc;

//
// GonkSensorsResultHandler
//

void
GonkSensorsResultHandler::OnError(SensorsError aError)
{
  HAL_ERR("Received error code %d", static_cast<int>(aError));
}

void
GonkSensorsResultHandler::Connect()
{ }

void
GonkSensorsResultHandler::Disconnect()
{ }

GonkSensorsResultHandler::~GonkSensorsResultHandler()
{ }

//
// GonkSensorsNotificationHandler
//

void
GonkSensorsNotificationHandler::BackendErrorNotification(bool aCrashed)
{
  if (aCrashed) {
    HAL_ERR("Sensors backend crashed");
  } else {
    HAL_ERR("Error in sensors backend");
  }
}

GonkSensorsNotificationHandler::~GonkSensorsNotificationHandler()
{ }

//
// GonkSensorsProtocol
//

class GonkSensorsProtocol final
  : public DaemonSocketIOConsumer
  , public GonkSensorsRegistryModule
  , public GonkSensorsPollModule
{
public:
  GonkSensorsProtocol();

  void SetConnection(DaemonSocket* aConnection);

  already_AddRefed<DaemonSocketResultHandler> FetchResultHandler(
    const DaemonSocketPDUHeader& aHeader);

  // Methods for |SensorsRegistryModule| and |SensorsPollModule|
  //

  nsresult Send(DaemonSocketPDU* aPDU,
                DaemonSocketResultHandler* aRes) override;

  // Methods for |DaemonSocketIOConsumer|
  //

  void Handle(DaemonSocketPDU& aPDU) override;
  void StoreResultHandler(const DaemonSocketPDU& aPDU) override;

private:
  void HandleRegistrySvc(const DaemonSocketPDUHeader& aHeader,
                         DaemonSocketPDU& aPDU,
                         DaemonSocketResultHandler* aRes);
  void HandlePollSvc(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     DaemonSocketResultHandler* aRes);

  DaemonSocket* mConnection;
  nsTArray<RefPtr<DaemonSocketResultHandler>> mResultHandlerQ;
};

GonkSensorsProtocol::GonkSensorsProtocol()
{ }

void
GonkSensorsProtocol::SetConnection(DaemonSocket* aConnection)
{
  mConnection = aConnection;
}

already_AddRefed<DaemonSocketResultHandler>
GonkSensorsProtocol::FetchResultHandler(const DaemonSocketPDUHeader& aHeader)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (aHeader.mOpcode & 0x80) {
    return nullptr; // Ignore notifications
  }

  RefPtr<DaemonSocketResultHandler> res = mResultHandlerQ.ElementAt(0);
  mResultHandlerQ.RemoveElementAt(0);

  return res.forget();
}

void
GonkSensorsProtocol::HandleRegistrySvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  GonkSensorsRegistryModule::HandleSvc(aHeader, aPDU, aRes);
}

void
GonkSensorsProtocol::HandlePollSvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  GonkSensorsPollModule::HandleSvc(aHeader, aPDU, aRes);
}

// |SensorsRegistryModule|, |SensorsPollModule|

nsresult
GonkSensorsProtocol::Send(DaemonSocketPDU* aPDU,
                          DaemonSocketResultHandler* aRes)
{
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(aPDU);

  aPDU->SetConsumer(this);
  aPDU->SetResultHandler(aRes);
  aPDU->UpdateHeader();

  if (mConnection->GetConnectionStatus() == SOCKET_DISCONNECTED) {
    HAL_ERR("Sensors socket is disconnected");
    return NS_ERROR_FAILURE;
  }

  mConnection->SendSocketData(aPDU); // Forward PDU to data channel

  return NS_OK;
}

// |DaemonSocketIOConsumer|

void
GonkSensorsProtocol::Handle(DaemonSocketPDU& aPDU)
{
  static void (GonkSensorsProtocol::* const HandleSvc[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&,
    DaemonSocketResultHandler*) = {
    [GonkSensorsRegistryModule::SERVICE_ID] =
      &GonkSensorsProtocol::HandleRegistrySvc,
    [GonkSensorsPollModule::SERVICE_ID] =
      &GonkSensorsProtocol::HandlePollSvc
  };

  DaemonSocketPDUHeader header;

  if (NS_FAILED(UnpackPDU(aPDU, header))) {
    return;
  }
  if (!(header.mService < MOZ_ARRAY_LENGTH(HandleSvc)) ||
      !HandleSvc[header.mService]) {
    HAL_ERR("Sensors service %d unknown", header.mService);
    return;
  }

  RefPtr<DaemonSocketResultHandler> res = FetchResultHandler(header);

  (this->*(HandleSvc[header.mService]))(header, aPDU, res);
}

void
GonkSensorsProtocol::StoreResultHandler(const DaemonSocketPDU& aPDU)
{
  MOZ_ASSERT(!NS_IsMainThread());

  mResultHandlerQ.AppendElement(aPDU.GetResultHandler());
}

//
// GonkSensorsInterface
//

GonkSensorsInterface*
GonkSensorsInterface::GetInstance()
{
  static GonkSensorsInterface* sGonkSensorsInterface;

  if (sGonkSensorsInterface) {
    return sGonkSensorsInterface;
  }

  sGonkSensorsInterface = new GonkSensorsInterface();

  return sGonkSensorsInterface;
}

void
GonkSensorsInterface::SetNotificationHandler(
  GonkSensorsNotificationHandler* aNotificationHandler)
{
  MOZ_ASSERT(NS_IsMainThread());

  mNotificationHandler = aNotificationHandler;
}

/*
 * The connect procedure consists of several steps.
 *
 *  (1) Start listening for the command channel's socket connection: We
 *      do this before anything else, so that we don't miss connection
 *      requests from the Sensors daemon. This step will create a listen
 *      socket.
 *
 *  (2) Start the Sensors daemon: When the daemon starts up it will open
 *      a socket connection to Gecko and thus create the data channel.
 *      Gecko already opened the listen socket in step (1). Step (2) ends
 *      with the creation of the data channel.
 *
 *  (3) Signal success to the caller.
 *
 * If any step fails, we roll-back the procedure and signal an error to the
 * caller.
 */
void
GonkSensorsInterface::Connect(GonkSensorsNotificationHandler* aNotificationHandler,
                          GonkSensorsResultHandler* aRes)
{
#define BASE_SOCKET_NAME "sensorsd"
  static unsigned long POSTFIX_LENGTH = 16;

  // If we could not cleanup properly before and an old
  // instance of the daemon is still running, we kill it
  // here.
  mozilla::hal::StopSystemService("sensorsd");

  mNotificationHandler = aNotificationHandler;

  mResultHandlerQ.AppendElement(aRes);

  if (!mProtocol) {
    mProtocol = MakeUnique<GonkSensorsProtocol>();
  }

  if (!mListenSocket) {
    mListenSocket = new ListenSocket(this, LISTEN_SOCKET);
  }

  // Init, step 1: Listen for data channel... */

  if (!mDataSocket) {
    mDataSocket = new DaemonSocket(mProtocol.get(), this, DATA_SOCKET);
  } else if (mDataSocket->GetConnectionStatus() == SOCKET_CONNECTED) {
    // Command channel should not be open; let's close it.
    mDataSocket->Close();
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
                             mDataSocket);
  if (NS_FAILED(rv)) {
    OnConnectError(DATA_SOCKET);
    return;
  }

  // The protocol implementation needs a data channel for
  // sending commands to the daemon. We set it here, because
  // this is the earliest time when it's available.
  mProtocol->SetConnection(mDataSocket);
}

/*
 * Disconnecting is inverse to connecting.
 *
 *  (1) Close data socket: We close the data channel and the daemon will
 *      will notice. Once we see the socket's disconnect, we continue with
 *      the cleanup.
 *
 *  (2) Close listen socket: The listen socket is not active any longer
 *      and we simply close it.
 *
 *  (3) Signal success to the caller.
 *
 * We don't have to stop the daemon explicitly. It will cleanup and quit
 * after it noticed the closing of the data channel
 *
 * Rolling back half-completed cleanups is not possible. In the case of
 * an error, we simply push forward and try to recover during the next
 * initialization.
 */
void
GonkSensorsInterface::Disconnect(GonkSensorsResultHandler* aRes)
{
  mNotificationHandler = nullptr;

  // Cleanup, step 1: Close data channel
  mDataSocket->Close();

  mResultHandlerQ.AppendElement(aRes);
}

GonkSensorsRegistryInterface*
GonkSensorsInterface::GetSensorsRegistryInterface()
{
  if (mRegistryInterface) {
    return mRegistryInterface.get();
  }

  mRegistryInterface = MakeUnique<GonkSensorsRegistryInterface>(mProtocol.get());

  return mRegistryInterface.get();
}

GonkSensorsPollInterface*
GonkSensorsInterface::GetSensorsPollInterface()
{
  if (mPollInterface) {
    return mPollInterface.get();
  }

  mPollInterface = MakeUnique<GonkSensorsPollInterface>(mProtocol.get());

  return mPollInterface.get();
}

GonkSensorsInterface::GonkSensorsInterface()
  : mNotificationHandler(nullptr)
{ }

GonkSensorsInterface::~GonkSensorsInterface()
{ }

void
GonkSensorsInterface::DispatchError(GonkSensorsResultHandler* aRes,
                                SensorsError aError)
{
  DaemonResultRunnable1<GonkSensorsResultHandler, void,
                        SensorsError, SensorsError>::Dispatch(
    aRes, &GonkSensorsResultHandler::OnError,
    ConstantInitOp1<SensorsError>(aError));
}

void
GonkSensorsInterface::DispatchError(
  GonkSensorsResultHandler* aRes, nsresult aRv)
{
  SensorsError error;

  if (NS_FAILED(Convert(aRv, error))) {
    error = SENSORS_ERROR_FAIL;
  }
  DispatchError(aRes, error);
}

// |DaemonSocketConsumer|, |ListenSocketConsumer|

void
GonkSensorsInterface::OnConnectSuccess(int aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResultHandlerQ.IsEmpty());

  switch (aIndex) {
    case LISTEN_SOCKET: {
        // Init, step 2: Start Sensors daemon
        nsCString args("-a ");
        args.Append(mListenSocketName);
        mozilla::hal::StartSystemService("sensorsd", args.get());
      }
      break;
    case DATA_SOCKET:
      if (!mResultHandlerQ.IsEmpty()) {
        // Init, step 3: Signal success
        RefPtr<GonkSensorsResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);
        if (res) {
          res->Connect();
        }
      }
      break;
  }
}

void
GonkSensorsInterface::OnConnectError(int aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResultHandlerQ.IsEmpty());

  switch (aIndex) {
    case DATA_SOCKET:
      // Stop daemon and close listen socket
      mozilla::hal::StopSystemService("sensorsd");
      mListenSocket->Close();
      // fall through
    case LISTEN_SOCKET:
      if (!mResultHandlerQ.IsEmpty()) {
        // Signal error to caller
        RefPtr<GonkSensorsResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);
        if (res) {
          DispatchError(res, SENSORS_ERROR_FAIL);
        }
      }
      break;
  }
}

/*
 * Disconnects can happend
 *
 *  (a) during startup,
 *  (b) during regular service, or
 *  (c) during shutdown.
 *
 * For cases (a) and (c), |mResultHandlerQ| contains an element. For
 * case (b) |mResultHandlerQ| will be empty. This distinguishes a crash in
 * the daemon. The following procedure to recover from crashes consists of
 * several steps for case (b).
 *
 *  (1) Close listen socket.
 *  (2) Wait for all sockets to be disconnected and inform caller about
 *      the crash.
 *  (3) After all resources have been cleaned up, let the caller restart
 *      the daemon.
 */
void
GonkSensorsInterface::OnDisconnect(int aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());

  switch (aIndex) {
    case DATA_SOCKET:
      // Cleanup, step 2 (Recovery, step 1): Close listen socket
      mListenSocket->Close();
      break;
    case LISTEN_SOCKET:
      // Cleanup, step 3: Signal success to caller
      if (!mResultHandlerQ.IsEmpty()) {
        RefPtr<GonkSensorsResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);
        if (res) {
          res->Disconnect();
        }
      }
      break;
  }

  /* For recovery make sure all sockets disconnected, in order to avoid
   * the remaining disconnects interfere with the restart procedure.
   */
  if (mNotificationHandler && mResultHandlerQ.IsEmpty()) {
    if (mListenSocket->GetConnectionStatus() == SOCKET_DISCONNECTED &&
        mDataSocket->GetConnectionStatus() == SOCKET_DISCONNECTED) {
      // Recovery, step 2: Notify the caller to prepare the restart procedure.
      mNotificationHandler->BackendErrorNotification(true);
      mNotificationHandler = nullptr;
    }
  }
}

} // namespace hal
} // namespace mozilla
