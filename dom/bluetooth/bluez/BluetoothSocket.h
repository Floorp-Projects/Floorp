/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothSocket_h
#define mozilla_dom_bluetooth_BluetoothSocket_h

#include "BluetoothCommon.h"
#include <stdlib.h>
#include "mozilla/ipc/DataSocket.h"
#include "mozilla/ipc/UnixSocketWatcher.h"
#include "mozilla/RefPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocketObserver;
class BluetoothUnixSocketConnector;

class BluetoothSocket final : public mozilla::ipc::DataSocket
{
public:
  BluetoothSocket(BluetoothSocketObserver* aObserver);
  ~BluetoothSocket();

  nsresult Connect(const nsAString& aDeviceAddress,
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
   * main thread.
   *
   * @param aBuffer Data received from the socket.
   */
  void ReceiveSocketData(nsAutoPtr<mozilla::ipc::UnixSocketBuffer>& aBuffer);

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
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  nsresult Connect(BluetoothUnixSocketConnector* aConnector,
                   int aDelayMs = 0);

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
   * @param[out] aDeviceAddress Returns the address string.
   */
  void GetAddress(nsAString& aDeviceAddress);

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

#endif
