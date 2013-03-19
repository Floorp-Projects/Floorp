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

using namespace mozilla::ipc;
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

static
void get_bdaddr_as_string(const bdaddr_t *ba, char *str) {
    const uint8_t *b = (const uint8_t *)ba;
    sprintf(str, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
            b[5], b[4], b[3], b[2], b[1], b[0]);
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

bool
BluetoothUnixSocketConnector::SetUp(int aFd)
{
  int lm = 0;
  int sndbuf;
  /* kernel does not yet support LM for SCO */
  switch (mType) {
  case BluetoothSocketType::RFCOMM:
    lm |= mAuth ? RFCOMM_LM_AUTH : 0;
    lm |= mEncrypt ? RFCOMM_LM_ENCRYPT : 0;
    break;
  case BluetoothSocketType::L2CAP:
    lm |= mAuth ? L2CAP_LM_AUTH : 0;
    lm |= mEncrypt ? L2CAP_LM_ENCRYPT : 0;
    break;
  case BluetoothSocketType::SCO:
    break;
  default:
    MOZ_NOT_REACHED("Unknown socket type!");
  }

  if (lm) {
    if (setsockopt(aFd, SOL_RFCOMM, RFCOMM_LM, &lm, sizeof(lm))) {
      NS_WARNING("setsockopt(RFCOMM_LM) failed, throwing");
      return false;
    }
  }

  if (mType == BluetoothSocketType::RFCOMM) {
    sndbuf = RFCOMM_SO_SNDBUF;
    if (setsockopt(aFd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf))) {
      NS_WARNING("setsockopt(SO_SNDBUF) failed, throwing");
      return false;
    }
  }

  return true;
}

int
BluetoothUnixSocketConnector::Create()
{
  MOZ_ASSERT(!NS_IsMainThread());
  int fd = -1;

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
    MOZ_NOT_REACHED();
  }

  if (fd < 0) {
    NS_WARNING("Could not open bluetooth socket!");
    return -1;
  }

  if (!SetUp(fd)) {
    NS_WARNING("Could not set up socket!");
  }
  return fd;
}

bool
BluetoothUnixSocketConnector::CreateAddr(bool aIsServer,
                                         socklen_t& aAddrSize,
                                         sockaddr_any& aAddr,
                                         const char* aAddress)
{
  // Set to BDADDR_ANY, if it's not a server, we'll reset.
  bdaddr_t bd_address_obj = {{0, 0, 0, 0, 0, 0}};

  if (!aIsServer && aAddress && strlen(aAddress) > 0) {
    if (get_bdaddr(aAddress, &bd_address_obj)) {
      NS_WARNING("Can't get bluetooth address!");
      return false;
    }
  }

  switch (mType) {
  case BluetoothSocketType::RFCOMM:
    struct sockaddr_rc addr_rc;
    aAddrSize = sizeof(addr_rc);
    aAddr.rc.rc_family = AF_BLUETOOTH;
    aAddr.rc.rc_channel = mChannel;
    memcpy(&aAddr.rc.rc_bdaddr, &bd_address_obj, sizeof(bd_address_obj));
    break;
  case BluetoothSocketType::SCO:
    struct sockaddr_sco addr_sco;
    aAddrSize = sizeof(addr_sco);
    aAddr.sco.sco_family = AF_BLUETOOTH;
    memcpy(&aAddr.sco.sco_bdaddr, &bd_address_obj, sizeof(bd_address_obj));
    break;
  default:
    NS_WARNING("Socket type unknown!");
    return false;
  }
  return true;
}

void
BluetoothUnixSocketConnector::GetSocketAddr(const sockaddr_any& aAddr,
                                            nsAString& aAddrStr)
{
  char addr[18];
  switch (mType) {
  case BluetoothSocketType::RFCOMM:
    get_bdaddr_as_string((bdaddr_t*)(&aAddr.rc.rc_bdaddr), addr);
    break;
  case BluetoothSocketType::SCO:
    get_bdaddr_as_string((bdaddr_t*)(&aAddr.sco.sco_bdaddr), addr);
    break;
  default:
    MOZ_NOT_REACHED("Socket should be either RFCOMM or SCO!");
  }
  aAddrStr.AssignASCII(addr);
}
