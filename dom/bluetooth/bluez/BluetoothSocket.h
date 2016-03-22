/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluez_BluetoothSocket_h
#define mozilla_dom_bluetooth_bluez_BluetoothSocket_h

#include "BluetoothCommon.h"
#include "mozilla/ipc/DataSocket.h"
#include "mozilla/ipc/UnixSocketWatcher.h"
#include "mozilla/UniquePtr.h"
#include "nsString.h"

class MessageLoop;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocketObserver;
class BluetoothUnixSocketConnector;

class BluetoothSocket final : public mozilla::ipc::DataSocket
{
public:
  BluetoothSocket(BluetoothSocketObserver* aObserver);
  ~BluetoothSocket();

  nsresult Connect(const BluetoothAddress& aDeviceAddress,
                   const BluetoothUuid& aServiceUuid,
                   BluetoothSocketType aType,
                   int aChannel,
                   bool aAuth, bool aEncrypt);

  nsresult Listen(const nsAString& aServiceName,
                  const BluetoothUuid& aServiceUuid,
                  BluetoothSocketType aType,
                  int aChannel,
                  bool aAuth, bool aEncrypt);

  /**
   * Method to be called whenever data is received. This is only called on the
   * consumer thread.
   *
   * @param aBuffer Data received from the socket.
   */
  void ReceiveSocketData(UniquePtr<mozilla::ipc::UnixSocketBuffer>& aBuffer);

  /**
   * Convenience function for sending strings to the socket (common in bluetooth
   * profile usage). Converts to a UnixSocketRawData struct. Can only be called
   * on originating thread.
   *
   * @param aMessage String to be sent to socket
   *
   * @return true if data is queued, false otherwise (i.e. not connected)
   */
  bool SendSocketData(const nsACString& aMessage);

  /**
   * Starts a task on the socket that will try to connect to a socket in a
   * non-blocking manner.
   *
   * @param aConnector Connector object for socket type specific functions
   * @param aDelayMs Time delay in milli-seconds.
   * @param aConsumerLoop The socket's consumer thread.
   * @param aIOLoop The socket's I/O thread.
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  nsresult Connect(BluetoothUnixSocketConnector* aConnector, int aDelayMs,
                   MessageLoop* aConsumerLoop, MessageLoop* aIOLoop);

  /**
   * Starts a task on the socket that will try to connect to a socket in a
   * non-blocking manner.
   *
   * @param aConnector Connector object for socket type specific functions
   * @param aDelayMs Time delay in milli-seconds.
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  nsresult Connect(BluetoothUnixSocketConnector* aConnector,
                   int aDelayMs = 0);

  /**
   * Starts a task on the socket that will try to accept a new connection in a
   * non-blocking manner.
   *
   * @param aConnector Connector object for socket type specific functions
   * @param aConsumerLoop The socket's consumer thread.
   * @param aIOLoop The socket's I/O thread.
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  nsresult Listen(BluetoothUnixSocketConnector* aConnector,
                  MessageLoop* aConsumerLoop, MessageLoop* aIOLoop);

  /**
   * Starts a task on the socket that will try to accept a new connection in a
   * non-blocking manner.
   *
   * @param aConnector Connector object for socket type specific functions
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  nsresult Listen(BluetoothUnixSocketConnector* aConnector);

  /**
   * Get the current socket address.
   *
   * @param[out] aDeviceAddress Returns the address.
   */
  void GetAddress(BluetoothAddress& aDeviceAddress);

  // Methods for |DataSocket|
  //

  void SendSocketData(mozilla::ipc::UnixSocketIOBuffer* aBuffer) override;

  // Methods for |SocketBase|
  //

  void Close() override;

  void OnConnectSuccess() override;
  void OnConnectError() override;
  void OnDisconnect() override;

private:
  class BluetoothSocketIO;
  class ConnectTask;
  class DelayedConnectTask;
  class ListenTask;

  BluetoothSocketObserver* mObserver;
  BluetoothSocketIO* mIO;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluez_BluetoothSocket_h
