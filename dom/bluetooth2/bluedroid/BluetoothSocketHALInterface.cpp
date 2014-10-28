/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothSocketHALInterface.h"
#include "BluetoothHALHelpers.h"
#include "BluetoothSocketMessageWatcher.h"
#include "mozilla/FileUtils.h"
#include "nsClassHashtable.h"
#include "nsXULAppAPI.h"

BEGIN_BLUETOOTH_NAMESPACE

typedef
  BluetoothHALInterfaceRunnable1<BluetoothSocketResultHandler, void,
                                 int, int>
  BluetoothSocketHALIntResultRunnable;

typedef
  BluetoothHALInterfaceRunnable3<BluetoothSocketResultHandler, void,
                                 int, const nsString, int,
                                 int, const nsAString_internal&, int>
  BluetoothSocketHALIntStringIntResultRunnable;

typedef
  BluetoothHALInterfaceRunnable1<BluetoothSocketResultHandler, void,
                                 BluetoothStatus, BluetoothStatus>
  BluetoothSocketHALErrorRunnable;

static nsresult
DispatchBluetoothSocketHALResult(
  BluetoothSocketResultHandler* aRes,
  void (BluetoothSocketResultHandler::*aMethod)(int), int aArg,
  BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothSocketHALIntResultRunnable(aRes, aMethod, aArg);
  } else {
    runnable = new BluetoothSocketHALErrorRunnable(aRes,
      &BluetoothSocketResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

static nsresult
DispatchBluetoothSocketHALResult(
  BluetoothSocketResultHandler* aRes,
  void (BluetoothSocketResultHandler::*aMethod)(int, const nsAString&, int),
  int aArg1, const nsAString& aArg2, int aArg3, BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothSocketHALIntStringIntResultRunnable(
      aRes, aMethod, aArg1, aArg2, aArg3);
  } else {
    runnable = new BluetoothSocketHALErrorRunnable(aRes,
      &BluetoothSocketResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

void
BluetoothSocketHALInterface::Listen(BluetoothSocketType aType,
                                    const nsAString& aServiceName,
                                    const uint8_t aServiceUuid[16],
                                    int aChannel, bool aEncrypt,
                                    bool aAuth,
                                    BluetoothSocketResultHandler* aRes)
{
  int fd;
  bt_status_t status;
  btsock_type_t type = BTSOCK_RFCOMM; // silences compiler warning

  if (NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->listen(type,
                                NS_ConvertUTF16toUTF8(aServiceName).get(),
                                aServiceUuid, aChannel, &fd,
                                (BTSOCK_FLAG_ENCRYPT * aEncrypt) |
                                (BTSOCK_FLAG_AUTH * aAuth));
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothSocketHALResult(
      aRes, &BluetoothSocketResultHandler::Listen, fd,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* |DeleteTask| deletes a class instance on the I/O thread
 */
template <typename T>
class DeleteTask MOZ_FINAL : public Task
{
public:
  DeleteTask(T* aPtr)
  : mPtr(aPtr)
  { }

  void Run() MOZ_OVERRIDE
  {
    mPtr = nullptr;
  }

private:
  nsAutoPtr<T> mPtr;
};

/* |ConnectWatcher| specializes SocketMessageWatcher for
 * connect operations by reading the socket messages from
 * Bluedroid and forwarding the connected socket to the
 * resource handler.
 */
class BluetoothSocketHALInterface::ConnectWatcher MOZ_FINAL
  : public SocketMessageWatcher
{
public:
  ConnectWatcher(int aFd, BluetoothSocketResultHandler* aRes)
    : SocketMessageWatcher(aFd, aRes)
  { }

  void Proceed(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    DispatchBluetoothSocketHALResult(
      GetResultHandler(), &BluetoothSocketResultHandler::Connect,
      GetFd(), GetBdAddress(), GetConnectionStatus(), aStatus);

    MessageLoopForIO::current()->PostTask(
      FROM_HERE, new DeleteTask<ConnectWatcher>(this));
  }
};

void
BluetoothSocketHALInterface::Connect(const nsAString& aBdAddr,
                                     BluetoothSocketType aType,
                                     const uint8_t aUuid[16],
                                     int aChannel, bool aEncrypt,
                                     bool aAuth,
                                     BluetoothSocketResultHandler* aRes)
{
  int fd;
  bt_status_t status;
  bt_bdaddr_t bdAddr;
  btsock_type_t type = BTSOCK_RFCOMM; // silences compiler warning

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr)) &&
      NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->connect(&bdAddr, type, aUuid, aChannel, &fd,
                                 (BTSOCK_FLAG_ENCRYPT * aEncrypt) |
                                 (BTSOCK_FLAG_AUTH * aAuth));
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (status == BT_STATUS_SUCCESS) {
    /* receive Bluedroid's socket-setup messages */
    Task* t = new SocketMessageWatcherTask(new ConnectWatcher(fd, aRes));
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE, t);
  } else if (aRes) {
    DispatchBluetoothSocketHALResult(
      aRes, &BluetoothSocketResultHandler::Connect, -1, EmptyString(), 0,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* |AcceptWatcher| specializes |SocketMessageWatcher| for accept
 * operations by reading the socket messages from Bluedroid and
 * forwarding the received client socket to the resource handler.
 * The first message is received immediately. When there's a new
 * connection, Bluedroid sends the 2nd message with the socket
 * info and socket file descriptor.
 */
class BluetoothSocketHALInterface::AcceptWatcher MOZ_FINAL
  : public SocketMessageWatcher
{
public:
  AcceptWatcher(int aFd, BluetoothSocketResultHandler* aRes)
    : SocketMessageWatcher(aFd, aRes)
  { }

  void Proceed(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    if ((aStatus != STATUS_SUCCESS) && (GetClientFd() != -1)) {
      mozilla::ScopedClose(GetClientFd()); // Close received socket fd on error
    }

    DispatchBluetoothSocketHALResult(
      GetResultHandler(), &BluetoothSocketResultHandler::Accept,
      GetClientFd(), GetBdAddress(), GetConnectionStatus(), aStatus);

    MessageLoopForIO::current()->PostTask(
      FROM_HERE, new DeleteTask<AcceptWatcher>(this));
  }
};

void
BluetoothSocketHALInterface::Accept(int aFd,
                                    BluetoothSocketResultHandler* aRes)
{
  /* receive Bluedroid's socket-setup messages and client fd */
  Task* t = new SocketMessageWatcherTask(new AcceptWatcher(aFd, aRes));
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, t);
}

void
BluetoothSocketHALInterface::Close(BluetoothSocketResultHandler* aRes)
{
  MOZ_ASSERT(aRes);

  /* stop the watcher corresponding to |aRes| */
  Task* t = new DeleteSocketMessageWatcherTask(aRes);
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, t);
}

BluetoothSocketHALInterface::BluetoothSocketHALInterface(
  const btsock_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothSocketHALInterface::~BluetoothSocketHALInterface()
{ }

END_BLUETOOTH_NAMESPACE
