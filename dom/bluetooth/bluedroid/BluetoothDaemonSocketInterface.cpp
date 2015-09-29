/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonSocketInterface.h"
#include "BluetoothSocketMessageWatcher.h"
#include "nsXULAppAPI.h"
#include "mozilla/unused.h"

BEGIN_BLUETOOTH_NAMESPACE

using namespace mozilla::ipc;

//
// Socket module
//

const int BluetoothDaemonSocketModule::MAX_NUM_CLIENTS = 1;

// Commands
//

nsresult
BluetoothDaemonSocketModule::ListenCmd(BluetoothSocketType aType,
                                       const nsAString& aServiceName,
                                       const BluetoothUuid& aServiceUuid,
                                       int aChannel, bool aEncrypt,
                                       bool aAuth,
                                       BluetoothSocketResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_LISTEN,
                        0));

  nsresult rv = PackPDU(
    aType,
    PackConversion<nsAString, BluetoothServiceName>(aServiceName),
    aServiceUuid,
    PackConversion<int, int32_t>(aChannel),
    SocketFlags(aEncrypt, aAuth), *pdu);
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

nsresult
BluetoothDaemonSocketModule::ConnectCmd(const nsAString& aBdAddr,
                                        BluetoothSocketType aType,
                                        const BluetoothUuid& aServiceUuid,
                                        int aChannel, bool aEncrypt,
                                        bool aAuth,
                                        BluetoothSocketResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CONNECT,
                        0));

  nsresult rv = PackPDU(
    PackConversion<nsAString, BluetoothAddress>(aBdAddr),
    aType,
    aServiceUuid,
    PackConversion<int, int32_t>(aChannel),
    SocketFlags(aEncrypt, aAuth), *pdu);
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

/* |DeleteTask| deletes a class instance on the I/O thread
 */
template <typename T>
class DeleteTask final : public Task
{
public:
  DeleteTask(T* aPtr)
  : mPtr(aPtr)
  { }

  void Run() override
  {
    mPtr = nullptr;
  }

private:
  nsAutoPtr<T> mPtr;
};

/* |AcceptWatcher| specializes SocketMessageWatcher for Accept
 * operations by reading the socket messages from Bluedroid and
 * forwarding the received client socket to the resource handler.
 * The first message is received immediately. When there's a new
 * connection, Bluedroid sends the 2nd message with the socket
 * info and socket file descriptor.
 */
class BluetoothDaemonSocketModule::AcceptWatcher final
  : public SocketMessageWatcher
{
public:
  AcceptWatcher(int aFd, BluetoothSocketResultHandler* aRes)
  : SocketMessageWatcher(aFd, aRes)
  { }

  void Proceed(BluetoothStatus aStatus) override
  {
    if (aStatus == STATUS_SUCCESS) {
      IntStringIntResultRunnable::Dispatch(
        GetResultHandler(), &BluetoothSocketResultHandler::Accept,
        ConstantInitOp3<int, nsString, int>(GetClientFd(), GetBdAddress(),
                                            GetConnectionStatus()));
    } else {
      ErrorRunnable::Dispatch(GetResultHandler(),
                              &BluetoothSocketResultHandler::OnError,
                              ConstantInitOp1<BluetoothStatus>(aStatus));
    }

    MessageLoopForIO::current()->PostTask(
      FROM_HERE, new DeleteTask<AcceptWatcher>(this));
  }
};

nsresult
BluetoothDaemonSocketModule::AcceptCmd(int aFd,
                                       BluetoothSocketResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  /* receive Bluedroid's socket-setup messages and client fd */
  Task* t = new SocketMessageWatcherTask(new AcceptWatcher(aFd, aRes));
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, t);

  return NS_OK;
}

nsresult
BluetoothDaemonSocketModule::CloseCmd(BluetoothSocketResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  /* stop the watcher corresponding to |aRes| */
  Task* t = new DeleteSocketMessageWatcherTask(aRes);
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, t);

  return NS_OK;
}

void
BluetoothDaemonSocketModule::HandleSvc(const DaemonSocketPDUHeader& aHeader,
                                       DaemonSocketPDU& aPDU,
                                       DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonSocketModule::* const HandleRsp[])(
    const DaemonSocketPDUHeader&,
    DaemonSocketPDU&,
    BluetoothSocketResultHandler*) = {
    [OPCODE_ERROR] = &BluetoothDaemonSocketModule::ErrorRsp,
    [OPCODE_LISTEN] = &BluetoothDaemonSocketModule::ListenRsp,
    [OPCODE_CONNECT] = &BluetoothDaemonSocketModule::ConnectRsp
  };

  if (NS_WARN_IF(MOZ_ARRAY_LENGTH(HandleRsp) <= aHeader.mOpcode) ||
      NS_WARN_IF(!HandleRsp[aHeader.mOpcode])) {
    return;
  }

  nsRefPtr<BluetoothSocketResultHandler> res =
    static_cast<BluetoothSocketResultHandler*>(aRes);

  if (!res) {
    return; // Return early if no result handler has been set
  }

  (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
}

uint8_t
BluetoothDaemonSocketModule::SocketFlags(bool aEncrypt, bool aAuth)
{
  return (0x01 * aEncrypt) | (0x02 * aAuth);
}

// Responses
//

void
BluetoothDaemonSocketModule::ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                                      DaemonSocketPDU& aPDU,
                                      BluetoothSocketResultHandler* aRes)
{
  ErrorRunnable::Dispatch(
    aRes, &BluetoothSocketResultHandler::OnError, UnpackPDUInitOp(aPDU));
}

class BluetoothDaemonSocketModule::ListenInitOp final : private PDUInitOp
{
public:
  ListenInitOp(DaemonSocketPDU& aPDU)
  : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (int& aArg1) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    aArg1 = pdu.AcquireFd();

    if (NS_WARN_IF(aArg1 < 0)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonSocketModule::ListenRsp(const DaemonSocketPDUHeader& aHeader,
                                       DaemonSocketPDU& aPDU,
                                       BluetoothSocketResultHandler* aRes)
{
  IntResultRunnable::Dispatch(
    aRes, &BluetoothSocketResultHandler::Listen, ListenInitOp(aPDU));
}

/* |ConnectWatcher| specializes SocketMessageWatcher for
 * connect operations by reading the socket messages from
 * Bluedroid and forwarding the connected socket to the
 * resource handler.
 */
class BluetoothDaemonSocketModule::ConnectWatcher final
  : public SocketMessageWatcher
{
public:
  ConnectWatcher(int aFd, BluetoothSocketResultHandler* aRes)
  : SocketMessageWatcher(aFd, aRes)
  { }

  void Proceed(BluetoothStatus aStatus) override
  {
    if (aStatus == STATUS_SUCCESS) {
      IntStringIntResultRunnable::Dispatch(
        GetResultHandler(), &BluetoothSocketResultHandler::Connect,
        ConstantInitOp3<int, nsString, int>(GetFd(), GetBdAddress(),
                                            GetConnectionStatus()));
    } else {
      ErrorRunnable::Dispatch(GetResultHandler(),
                              &BluetoothSocketResultHandler::OnError,
                              ConstantInitOp1<BluetoothStatus>(aStatus));
    }

    MessageLoopForIO::current()->PostTask(
      FROM_HERE, new DeleteTask<ConnectWatcher>(this));
  }
};

void
BluetoothDaemonSocketModule::ConnectRsp(const DaemonSocketPDUHeader& aHeader,
                                        DaemonSocketPDU& aPDU,
                                        BluetoothSocketResultHandler* aRes)
{
  /* the file descriptor is attached in the PDU's ancillary data */
  int fd = aPDU.AcquireFd();
  if (fd < 0) {
    ErrorRunnable::Dispatch(aRes, &BluetoothSocketResultHandler::OnError,
                            ConstantInitOp1<BluetoothStatus>(STATUS_FAIL));
    return;
  }

  /* receive Bluedroid's socket-setup messages */
  Task* t = new SocketMessageWatcherTask(new ConnectWatcher(fd, aRes));
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, t);
}

//
// Socket interface
//

BluetoothDaemonSocketInterface::BluetoothDaemonSocketInterface(
  BluetoothDaemonSocketModule* aModule)
: mModule(aModule)
{
  MOZ_ASSERT(mModule);
}

BluetoothDaemonSocketInterface::~BluetoothDaemonSocketInterface()
{ }

void
BluetoothDaemonSocketInterface::Listen(BluetoothSocketType aType,
                                       const nsAString& aServiceName,
                                       const BluetoothUuid& aServiceUuid,
                                       int aChannel, bool aEncrypt,
                                       bool aAuth,
                                       BluetoothSocketResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ListenCmd(aType, aServiceName, aServiceUuid,
                                   aChannel, aEncrypt, aAuth, aRes);
  if (NS_FAILED(rv))  {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonSocketInterface::Connect(const nsAString& aBdAddr,
                                        BluetoothSocketType aType,
                                        const BluetoothUuid& aServiceUuid,
                                        int aChannel, bool aEncrypt,
                                        bool aAuth,
                                        BluetoothSocketResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ConnectCmd(aBdAddr, aType, aServiceUuid, aChannel,
                                    aEncrypt, aAuth, aRes);
  if (NS_FAILED(rv))  {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonSocketInterface::Accept(int aFd,
                                    BluetoothSocketResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->AcceptCmd(aFd, aRes);
  if (NS_FAILED(rv))  {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonSocketInterface::Close(BluetoothSocketResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->CloseCmd(aRes);
  if (NS_FAILED(rv))  {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonSocketInterface::DispatchError(
  BluetoothSocketResultHandler* aRes, BluetoothStatus aStatus)
{
  DaemonResultRunnable1<BluetoothSocketResultHandler, void,
                        BluetoothStatus, BluetoothStatus>::Dispatch(
    aRes, &BluetoothSocketResultHandler::OnError,
    ConstantInitOp1<BluetoothStatus>(aStatus));
}

void
BluetoothDaemonSocketInterface::DispatchError(
  BluetoothSocketResultHandler* aRes, nsresult aRv)
{
  BluetoothStatus status;

  if (NS_WARN_IF(NS_FAILED(Convert(aRv, status)))) {
    status = STATUS_FAIL;
  }
  DispatchError(aRes, status);
}

END_BLUETOOTH_NAMESPACE
