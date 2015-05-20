/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_bluetooth_BluetoothDaemonConnection_h
#define mozilla_ipc_bluetooth_BluetoothDaemonConnection_h

#include "mozilla/Attributes.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ipc/ConnectionOrientedSocket.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace ipc {

class BluetoothDaemonConnectionIO;
class BluetoothDaemonPDUConsumer;

/*
 * |BlutoothDaemonPDU| represents a single PDU that is transfered from or to
 * the Bluetooth daemon. Each PDU contains exactly one command.
 *
 * A PDU as the following format
 *
 *    |    1    |    1    |        2       |    n    |
 *    | service |  opcode | payload length | payload |
 *
 * Service and Opcode each require 1 byte, the payload length requires 2
 * bytes, and the payload requires the number of bytes as stored in the
 * payload-length field.
 *
 * Each service and opcode can have a different payload with individual
 * length. For the exact details of the Bluetooth protocol, please refer
 * to
 *
 *    https://git.kernel.org/cgit/bluetooth/bluez.git/tree/android/hal-ipc-api.txt?id=5.24
 *
 */
class BluetoothDaemonPDU final : public UnixSocketIOBuffer
{
public:
  enum {
    OFF_SERVICE = 0,
    OFF_OPCODE = 1,
    OFF_LENGTH = 2,
    OFF_PAYLOAD = 4,
    HEADER_SIZE = OFF_PAYLOAD,
    MAX_PAYLOAD_LENGTH = 1 << 16
  };

  BluetoothDaemonPDU(uint8_t aService, uint8_t aOpcode,
                     uint16_t aPayloadSize);
  BluetoothDaemonPDU(size_t aPayloadSize);

  void SetConsumer(BluetoothDaemonPDUConsumer* aConsumer)
  {
    mConsumer = aConsumer;
  }

  void SetUserData(void* aUserData)
  {
    mUserData = aUserData;
  }

  void* GetUserData() const
  {
    return mUserData;
  }

  void GetHeader(uint8_t& aService, uint8_t& aOpcode,
                 uint16_t& aPayloadSize);

  ssize_t Send(int aFd) override;
  ssize_t Receive(int aFd) override;

  int AcquireFd();

  nsresult UpdateHeader();

private:
  size_t GetPayloadSize() const;
  void OnError(const char* aFunction, int aErrno);

  BluetoothDaemonPDUConsumer* mConsumer;
  void* mUserData;
  ScopedClose mReceivedFd;
};

/*
 * |BluetoothDaemonPDUConsumer| processes incoming PDUs from the Bluetooth
 * daemon. Please note that its method |Handle| runs on a different than the
 * main thread.
 */
class BluetoothDaemonPDUConsumer
{
public:
  virtual ~BluetoothDaemonPDUConsumer();

  virtual void Handle(BluetoothDaemonPDU& aPDU) = 0;
  virtual void StoreUserData(const BluetoothDaemonPDU& aPDU) = 0;

protected:
  BluetoothDaemonPDUConsumer();
};

/*
 * |BluetoothDaemonConnection| represents the socket to connect to the
 * Bluetooth daemon. It offers connection establishment and sending
 * PDUs. PDU receiving is performed by |BluetoothDaemonPDUConsumer|.
 */
class BluetoothDaemonConnection : public ConnectionOrientedSocket
{
public:
  BluetoothDaemonConnection();
  virtual ~BluetoothDaemonConnection();

  // Methods for |ConnectionOrientedSocket|
  //

  virtual ConnectionOrientedSocketIO* GetIO() override;

  // Methods for |DataSocket|
  //

  void SendSocketData(UnixSocketIOBuffer* aBuffer) override;

  // Methods for |SocketBase|
  //

  void Close() override;

protected:

  // Prepares an instance of |BluetoothDaemonConnection| in DISCONNECTED
  // state for accepting a connection. Subclasses implementing |GetIO|
  // need to call this method.
  ConnectionOrientedSocketIO*
    PrepareAccept(BluetoothDaemonPDUConsumer* aConsumer);

private:
  BluetoothDaemonConnectionIO* mIO;
};

}
}

#endif
