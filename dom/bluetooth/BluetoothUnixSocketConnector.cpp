/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * Copyright 2009, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * NOTE: Due to being based on the dbus compatibility layer for
 * android's bluetooth implementation, this file is licensed under the
 * apache license instead of MPL.
 *
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/sco.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/l2cap.h>

#include "BluetoothUnixSocketConnector.h"
#include "nsThreadUtils.h"

USING_BLUETOOTH_NAMESPACE

static const int RFCOMM_SO_SNDBUF = 70 * 1024; // 70 KB send buffer

static
int get_bdaddr(const char *str, bdaddr_t *ba)
{
  char *d = ((char*)ba) + 5, *endp;
  for (int i = 0; i < 6; i++) {
    *d-- = strtol(str, &endp, 16);
    MOZ_ASSERT(!(*endp != ':' && i != 5));
    str = endp + 1;
  }
  return 0;
}

BluetoothUnixSocketConnector::BluetoothUnixSocketConnector(
  BluetoothSocketType aType,
  int aChannel,
  bool aAuth,
  bool aEncrypt) : mType(aType)
                 , mChannel(aChannel)
                 , mAuth(aAuth)
                 , mEncrypt(aEncrypt)
{
}

int
BluetoothUnixSocketConnector::Create()
{
  MOZ_ASSERT(!NS_IsMainThread());
  int lm = 0;
  int fd = -1;
  int sndbuf;

  switch (mType) {
  case BluetoothSocketType::RFCOMM:
    fd = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    break;
  case BluetoothSocketType::SCO:
    fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO);
    break;
  case BluetoothSocketType::L2CAP:
    fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    break;
  default:
    return -1;
  }

  if (fd < 0) {
    NS_WARNING("Could not open bluetooth socket!");
    return -1;
  }

  /* kernel does not yet support LM for SCO */
  switch (mType) {
  case BluetoothSocketType::RFCOMM:
    lm |= mAuth ? RFCOMM_LM_AUTH : 0;
    lm |= mEncrypt ? RFCOMM_LM_ENCRYPT : 0;
    lm |= (mAuth && mEncrypt) ? RFCOMM_LM_SECURE : 0;
    break;
  case BluetoothSocketType::L2CAP:
    lm |= mAuth ? L2CAP_LM_AUTH : 0;
    lm |= mEncrypt ? L2CAP_LM_ENCRYPT : 0;
    lm |= (mAuth && mEncrypt) ? L2CAP_LM_SECURE : 0;
    break;
  }

  if (lm) {
    if (setsockopt(fd, SOL_RFCOMM, RFCOMM_LM, &lm, sizeof(lm))) {
      NS_WARNING("setsockopt(RFCOMM_LM) failed, throwing");
      return -1;
    }
  }

  if (mType == BluetoothSocketType::RFCOMM) {
    sndbuf = RFCOMM_SO_SNDBUF;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf))) {
      NS_WARNING("setsockopt(SO_SNDBUF) failed, throwing");
      return -1;
    }
  }

  return fd;
}

bool
BluetoothUnixSocketConnector::ConnectInternal(int aFd, const char* aAddress)
{
  int n = 1;
  setsockopt(aFd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));
  
  socklen_t addr_sz;
  struct sockaddr *addr;
  bdaddr_t bd_address_obj;

  if (get_bdaddr(aAddress, &bd_address_obj)) {
    NS_WARNING("Can't get bluetooth address!");
    return false;
  }

  switch (mType) {
  case BluetoothSocketType::RFCOMM:
    struct sockaddr_rc addr_rc;
    addr = (struct sockaddr *)&addr_rc;
    addr_sz = sizeof(addr_rc);

    memset(addr, 0, addr_sz);
    addr_rc.rc_family = AF_BLUETOOTH;
    addr_rc.rc_channel = mChannel;
    memcpy(&addr_rc.rc_bdaddr, &bd_address_obj, sizeof(bdaddr_t));
    break;
  case BluetoothSocketType::SCO:
    struct sockaddr_sco addr_sco;
    addr = (struct sockaddr *)&addr_sco;
    addr_sz = sizeof(addr_sco);

    memset(addr, 0, addr_sz);
    addr_sco.sco_family = AF_BLUETOOTH;
    memcpy(&addr_sco.sco_bdaddr, &bd_address_obj, sizeof(bdaddr_t));
    break;
  default:
    NS_WARNING("Socket type unknown!");
    return false;
  }

  int ret = connect(aFd, addr, addr_sz);

  if (ret) {
#if DEBUG
    //LOG("Socket connect errno=%d\n", errno);
#endif
    NS_WARNING("Socket connect error!");
    return false;
  }

  return true;
}
