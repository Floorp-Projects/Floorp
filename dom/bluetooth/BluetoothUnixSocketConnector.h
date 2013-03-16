/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothUnixSocketConnector_h
#define mozilla_dom_bluetooth_BluetoothUnixSocketConnector_h

#include "BluetoothCommon.h"
#include <sys/socket.h>
#include <mozilla/ipc/UnixSocket.h>

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothUnixSocketConnector : public mozilla::ipc::UnixSocketConnector
{
public:
  BluetoothUnixSocketConnector(BluetoothSocketType aType, int aChannel,
                               bool aAuth, bool aEncrypt);
  virtual ~BluetoothUnixSocketConnector()
  {}
  virtual int Create() MOZ_OVERRIDE;
  virtual bool CreateAddr(bool aIsServer,
                          socklen_t& aAddrSize,
                          mozilla::ipc::sockaddr_any& aAddr,
                          const char* aAddress) MOZ_OVERRIDE;
  virtual bool SetUp(int aFd) MOZ_OVERRIDE;
  virtual void GetSocketAddr(const mozilla::ipc::sockaddr_any& aAddr,
                             nsAString& aAddrStr) MOZ_OVERRIDE;

private:
  BluetoothSocketType mType;
  int mChannel;
  bool mAuth;
  bool mEncrypt;
};

END_BLUETOOTH_NAMESPACE

#endif
