/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothUnixSocketConnector_h
#define mozilla_dom_bluetooth_BluetoothUnixSocketConnector_h

#include "BluetoothCommon.h"
#include <sys/socket.h>
#include <mozilla/ipc/UnixSocketConnector.h>

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothUnixSocketConnector : public mozilla::ipc::UnixSocketConnector
{
public:
  BluetoothUnixSocketConnector(BluetoothSocketType aType, int aChannel,
                               bool aAuth, bool aEncrypt);
  virtual ~BluetoothUnixSocketConnector()
  {}
  virtual int Create() override;
  virtual bool CreateAddr(bool aIsServer,
                          socklen_t& aAddrSize,
                          mozilla::ipc::sockaddr_any& aAddr,
                          const char* aAddress) override;
  virtual bool SetUp(int aFd) override;
  virtual bool SetUpListenSocket(int aFd) override;
  virtual void GetSocketAddr(const mozilla::ipc::sockaddr_any& aAddr,
                             nsAString& aAddrStr) override;

private:
  BluetoothSocketType mType;
  int mChannel;
  bool mAuth;
  bool mEncrypt;
};

END_BLUETOOTH_NAMESPACE

#endif
