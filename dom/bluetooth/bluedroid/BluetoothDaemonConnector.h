/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothdaemonconnector_h
#define mozilla_dom_bluetooth_bluedroid_bluetoothdaemonconnector_h

#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/ipc/UnixSocketConnector.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDaemonConnector final
  : public mozilla::ipc::UnixSocketConnector
{
public:
  BluetoothDaemonConnector(const nsACString& aSocketName);
  ~BluetoothDaemonConnector();

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

  // Deprecated

  int Create() override;
  bool CreateAddr(bool aIsServer,
             socklen_t& aAddrSize,
             mozilla::ipc::sockaddr_any& aAddr,
             const char* aAddress) override;
  bool SetUp(int aFd) override;
  bool SetUpListenSocket(int aFd) override;
  void GetSocketAddr(const mozilla::ipc::sockaddr_any& aAddr,
                     nsAString& aAddrStr) override;

private:
  nsresult CreateSocket(int& aFd) const;
  nsresult SetSocketFlags(int aFd) const;
  nsresult CreateAddress(struct sockaddr& aAddress,
                         socklen_t& aAddressLength) const;

  nsCString mSocketName;
};

END_BLUETOOTH_NAMESPACE

#endif
