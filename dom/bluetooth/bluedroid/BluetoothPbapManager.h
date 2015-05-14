/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothpbapmanager_h__
#define mozilla_dom_bluetooth_bluetoothpbapmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothProfileManagerBase.h"
#include "BluetoothSocketObserver.h"
#include "mozilla/ipc/SocketBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;
class ObexHeaderSet;

class BluetoothPbapManager : public BluetoothSocketObserver
                          , public BluetoothProfileManagerBase
{
public:
  BT_DECL_PROFILE_MGR_BASE
  BT_DECL_SOCKET_OBSERVER
  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("PBAP");
  }

  static const int MAX_PACKET_LENGTH = 0xFFFE;

  static BluetoothPbapManager* Get();
  bool Listen();

protected:
  virtual ~BluetoothPbapManager();

private:
  BluetoothPbapManager();
  bool Init();
  void HandleShutdown();

  void ReplyToConnect();
  void ReplyToDisconnectOrAbort();
  void ReplyError(uint8_t aError);
  void SendObexData(uint8_t* aData, uint8_t aOpcode, int aSize);

  bool CompareHeaderTarget(const ObexHeaderSet& aHeader);
  void AfterPbapConnected();
  void AfterPbapDisconnected();

  /**
   * OBEX session status. Set when OBEX session is established.
   */
  bool mConnected;
  nsString mDeviceAddress;

  // If a connection has been established, mSocket will be the socket
  // communicating with the remote socket. We maintain the invariant that if
  // mSocket is non-null, mServerSocket must be null (and vice versa).
  nsRefPtr<BluetoothSocket> mSocket;

  // Server socket. Once an inbound connection is established, it will hand
  // over the ownership to mSocket, and get a new server socket while Listen()
  // is called.
  nsRefPtr<BluetoothSocket> mServerSocket;
};

END_BLUETOOTH_NAMESPACE

#endif
