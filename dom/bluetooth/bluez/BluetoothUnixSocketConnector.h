/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluez_BluetoothUnixSocketConnector_h
#define mozilla_dom_bluetooth_bluez_BluetoothUnixSocketConnector_h

#include <bluetooth/bluetooth.h>
#include "BluetoothCommon.h"
#include "mozilla/ipc/UnixSocketConnector.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothUnixSocketConnector final
  : public mozilla::ipc::UnixSocketConnector
{
public:
  BluetoothUnixSocketConnector(const BluetoothAddress& aAddress,
                               BluetoothSocketType aType,
                               int aChannel, bool aAuth, bool aEncrypt);
  ~BluetoothUnixSocketConnector();

  nsresult ConvertAddress(const struct sockaddr& aAddress,
                          socklen_t aAddressLength,
                          BluetoothAddress& aAddressOut);

  // Methods for |UnixSocketConnector|
  //

  nsresult ConvertAddressToString(const struct sockaddr& aAddress,
                                  socklen_t aAddressLength,
                                  nsACString& aAddressString) override;

  nsresult CreateListenSocket(struct sockaddr* aAddress,
                              socklen_t* aAddressLength,
                              int& aListenFd) override;

  nsresult AcceptStreamSocket(int aListenFd,
                              struct sockaddr* aAddress,
                              socklen_t* aAddressLen,
                              int& aStreamFd) override;

  nsresult CreateStreamSocket(struct sockaddr* aAddress,
                              socklen_t* aAddressLength,
                              int& aStreamFd) override;

  nsresult Duplicate(UnixSocketConnector*& aConnector) override;

private:
  static nsresult ConvertAddress(const BluetoothAddress& aAddress,
                                 bdaddr_t& aBdAddr);

  static nsresult ConvertAddress(const bdaddr_t& aBdAddr,
                                 BluetoothAddress& aAddress);

  nsresult CreateSocket(int& aFd) const;
  nsresult SetSocketFlags(int aFd) const;
  nsresult CreateAddress(struct sockaddr& aAddress,
                         socklen_t& aAddressLength) const;

  BluetoothAddress mAddress;
  BluetoothSocketType mType;
  int mChannel;
  bool mAuth;
  bool mEncrypt;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluez_BluetoothUnixSocketConnector_h
