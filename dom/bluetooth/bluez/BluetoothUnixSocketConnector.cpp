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
 * NOTE: Due to being based on the D-Bus compatibility layer for
 * Android's Bluetooth implementation, this file is licensed under the
 * Apache License instead of MPL.
 */

#include "BluetoothUnixSocketConnector.h"
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sco.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include "nsThreadUtils.h" // For NS_IsMainThread.

using namespace mozilla::ipc;

BEGIN_BLUETOOTH_NAMESPACE

static const int RFCOMM_SO_SNDBUF = 70 * 1024;  // 70 KB send buffer
static const int L2CAP_SO_SNDBUF = 400 * 1024;  // 400 KB send buffer
static const int L2CAP_SO_RCVBUF = 400 * 1024;  // 400 KB receive buffer
static const int L2CAP_MAX_MTU = 65000;

BluetoothUnixSocketConnector::BluetoothUnixSocketConnector(
  const nsACString& aAddressString,
  BluetoothSocketType aType,
  int aChannel,
  bool aAuth,
  bool aEncrypt)
  : mAddressString(aAddressString)
  , mType(aType)
  , mChannel(aChannel)
  , mAuth(aAuth)
  , mEncrypt(aEncrypt)
{ }

BluetoothUnixSocketConnector::~BluetoothUnixSocketConnector()
{ }

nsresult
BluetoothUnixSocketConnector::CreateSocket(int& aFd) const
{
  static const int sType[] = {
    [0] = 0,
    [BluetoothSocketType::RFCOMM] = SOCK_STREAM,
    [BluetoothSocketType::SCO] = SOCK_SEQPACKET,
    [BluetoothSocketType::L2CAP] = SOCK_SEQPACKET,
    [BluetoothSocketType::EL2CAP] = SOCK_STREAM
  };
  static const int sProtocol[] = {
    [0] = 0,
    [BluetoothSocketType::RFCOMM] = BTPROTO_RFCOMM,
    [BluetoothSocketType::SCO] = BTPROTO_SCO,
    [BluetoothSocketType::L2CAP] = BTPROTO_L2CAP,
    [BluetoothSocketType::EL2CAP] = BTPROTO_L2CAP
  };

  MOZ_ASSERT(mType < MOZ_ARRAY_LENGTH(sType));
  MOZ_ASSERT(mType < MOZ_ARRAY_LENGTH(sProtocol));

  BT_LOGR("mType=%d, sType=%d sProtocol=%d",
          static_cast<int>(mType), sType[mType], sProtocol[mType]);

  aFd = socket(AF_BLUETOOTH, sType[mType], sProtocol[mType]);
  if (aFd < 0) {
    BT_LOGR("Could not open Bluetooth socket: %d(%s)",
            errno, strerror(errno));
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
BluetoothUnixSocketConnector::SetSocketFlags(int aFd) const
{
  static const int sReuseAddress = 1;

  // Set close-on-exec bit.
  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFD));
  if (flags < 0) {
    return NS_ERROR_FAILURE;
  }
  flags |= FD_CLOEXEC;
  int res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFD, flags));
  if (res < 0) {
    return NS_ERROR_FAILURE;
  }

  // Set non-blocking status flag.
  flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  if (flags < 0) {
    return NS_ERROR_FAILURE;
  }
  flags |= O_NONBLOCK;
  res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags));
  if (res < 0) {
    return NS_ERROR_FAILURE;
  }

  // Set socket addr to be reused even if kernel is still waiting to close.
  res = setsockopt(aFd, SOL_SOCKET, SO_REUSEADDR, &sReuseAddress,
                   sizeof(sReuseAddress));
  if (res < 0) {
    return NS_ERROR_FAILURE;
  }

  int lm;

  switch (mType) {
    case BluetoothSocketType::RFCOMM:
      lm |= mAuth ? RFCOMM_LM_AUTH : 0;
      lm |= mEncrypt ? RFCOMM_LM_ENCRYPT : 0;
      break;
    case BluetoothSocketType::L2CAP:
    case BluetoothSocketType::EL2CAP:
      lm |= mAuth ? L2CAP_LM_AUTH : 0;
      lm |= mEncrypt ? L2CAP_LM_ENCRYPT : 0;
      break;
    default:
      // kernel does not yet support LM for SCO
      lm = 0;
      break;
  }

  if (lm) {
    static const int sLevel[] = {
      [0] = 0,
      [BluetoothSocketType::RFCOMM] = SOL_RFCOMM,
      [BluetoothSocketType::SCO] = 0,
      [BluetoothSocketType::L2CAP] = SOL_L2CAP,
      [BluetoothSocketType::EL2CAP] = SOL_L2CAP
    };
    static const int sOptname[] = {
      [0] = 0,
      [BluetoothSocketType::RFCOMM] = RFCOMM_LM,
      [BluetoothSocketType::SCO] = 0,
      [BluetoothSocketType::L2CAP] = L2CAP_LM,
      [BluetoothSocketType::EL2CAP] = L2CAP_LM
    };

    MOZ_ASSERT(mType < MOZ_ARRAY_LENGTH(sLevel));
    MOZ_ASSERT(mType < MOZ_ARRAY_LENGTH(sOptname));

    if (setsockopt(aFd, sLevel[mType], sOptname[mType], &lm, sizeof(lm)) < 0) {
      BT_LOGR("setsockopt(RFCOMM_LM) failed, throwing");
      return NS_ERROR_FAILURE;
    }
  }

  if (mType == BluetoothSocketType::RFCOMM) {

    /* Setting RFCOMM socket options */

    int sndbuf = RFCOMM_SO_SNDBUF;
    if (setsockopt(aFd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf))) {
      BT_WARNING("setsockopt(SO_SNDBUF) failed, throwing");
      return NS_ERROR_FAILURE;
    }
  }

  if (mType == BluetoothSocketType::L2CAP ||
      mType == BluetoothSocketType::EL2CAP) {

    /* Setting L2CAP/EL2CAP socket options */

    struct l2cap_options opts;
    socklen_t optlen = sizeof(opts);
    int res = getsockopt(aFd, SOL_L2CAP, L2CAP_OPTIONS, &opts, &optlen);
    if (!res) {
      /* setting MTU for [E]L2CAP */
      opts.omtu = opts.imtu = L2CAP_MAX_MTU;

      /* Enable ERTM for [E]L2CAP */
      if (mType == BluetoothSocketType::EL2CAP) {
        opts.flush_to = 0xffff; /* infinite */
        opts.mode = L2CAP_MODE_ERTM;
        opts.fcs = 1;
        opts.txwin_size = 64;
        opts.max_tx = 10;
      }

      setsockopt(aFd, SOL_L2CAP, L2CAP_OPTIONS, &opts, optlen);
    }

    if (mType == BluetoothSocketType::EL2CAP) {

      /* Set larger SNDBUF and RCVBUF for EL2CAP connections */

      int sndbuf = L2CAP_SO_SNDBUF;
      if (setsockopt(aFd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
        BT_LOGR("setsockopt(SO_SNDBUF) failed, throwing");
        return NS_ERROR_FAILURE;
      }

      int rcvbuf = L2CAP_SO_RCVBUF;
      if (setsockopt(aFd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        BT_LOGR("setsockopt(SO_RCVBUF) failed, throwing");
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

nsresult
BluetoothUnixSocketConnector::CreateAddress(struct sockaddr& aAddress,
                                            socklen_t& aAddressLength) const
{
  switch (mType) {
    case BluetoothSocketType::RFCOMM: {
        struct sockaddr_rc* rc =
          reinterpret_cast<struct sockaddr_rc*>(&aAddress);
        rc->rc_family = AF_BLUETOOTH;
        nsresult rv = ConvertAddressString(mAddressString.get(),
                                           rc->rc_bdaddr);
        if (NS_FAILED(rv)) {
          return rv;
        }
        rc->rc_channel = mChannel;
        aAddressLength = sizeof(*rc);
      }
      break;
    case BluetoothSocketType::L2CAP:
    case BluetoothSocketType::EL2CAP: {
        struct sockaddr_l2* l2 =
          reinterpret_cast<struct sockaddr_l2*>(&aAddress);
        l2->l2_family = AF_BLUETOOTH;
        l2->l2_psm = mChannel;
        nsresult rv = ConvertAddressString(mAddressString.get(),
                                           l2->l2_bdaddr);
        if (NS_FAILED(rv)) {
          return rv;
        }
        l2->l2_cid = 0;
        aAddressLength = sizeof(*l2);
      }
      break;
    case BluetoothSocketType::SCO: {
        struct sockaddr_sco* sco =
          reinterpret_cast<struct sockaddr_sco*>(&aAddress);
        sco->sco_family = AF_BLUETOOTH;
        nsresult rv = ConvertAddressString(mAddressString.get(),
                                           sco->sco_bdaddr);
        if (NS_FAILED(rv)) {
          return rv;
        }
        sco->sco_pkt_type = 0;
        aAddressLength = sizeof(*sco);
      }
      break;
    default:
      MOZ_CRASH("Socket type unknown!");
      return NS_ERROR_ABORT;
  }

  return NS_OK;
}

nsresult
BluetoothUnixSocketConnector::ConvertAddressString(const char* aAddressString,
                                                   bdaddr_t& aAddress)
{
  char* d = reinterpret_cast<char*>(aAddress.b) + 5;

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(aAddress.b); ++i) {
    char* endp;
    *d-- = strtoul(aAddressString, &endp, 16);
    MOZ_ASSERT(!(*endp != ':' && i != 5));
    aAddressString = endp + 1;
  }
  return NS_OK;
}

// |UnixSocketConnector|

nsresult
BluetoothUnixSocketConnector::ConvertAddressToString(
  const struct sockaddr& aAddress, socklen_t aAddressLength,
  nsACString& aAddressString)
{
  MOZ_ASSERT(aAddress.sa_family == AF_BLUETOOTH);

  const uint8_t* b;

  switch (mType) {
    case BluetoothSocketType::RFCOMM: {
        const struct sockaddr_rc* rc =
          reinterpret_cast<const struct sockaddr_rc*>(&aAddress);
        b = rc->rc_bdaddr.b;
      }
      break;
    case BluetoothSocketType::SCO: {
        const struct sockaddr_sco* sco =
          reinterpret_cast<const struct sockaddr_sco*>(&aAddress);
        b = sco->sco_bdaddr.b;
      }
      break;
    case BluetoothSocketType::L2CAP:
    case BluetoothSocketType::EL2CAP: {
        const struct sockaddr_l2* l2 =
          reinterpret_cast<const struct sockaddr_l2*>(&aAddress);
        b = l2->l2_bdaddr.b;
      }
      break;
    default:
      BT_LOGR("Unknown socket type %d", static_cast<int>(mType));
      return NS_ERROR_ILLEGAL_VALUE;
  }

  char str[32];
  snprintf(str, sizeof(str), "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
           b[5], b[4], b[3], b[2], b[1], b[0]);

  aAddressString.Assign(str);

  return NS_OK;
}

nsresult
BluetoothUnixSocketConnector::CreateListenSocket(struct sockaddr* aAddress,
                                                 socklen_t* aAddressLength,
                                                 int& aListenFd)
{
  ScopedClose fd;

  nsresult rv = CreateSocket(fd.rwget());
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = SetSocketFlags(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (aAddress && aAddressLength) {
    rv = CreateAddress(*aAddress, *aAddressLength);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  aListenFd = fd.forget();

  return NS_OK;
}

nsresult
BluetoothUnixSocketConnector::AcceptStreamSocket(int aListenFd,
                                                 struct sockaddr* aAddress,
                                                 socklen_t* aAddressLength,
                                                 int& aStreamFd)
{
  ScopedClose fd(
    TEMP_FAILURE_RETRY(accept(aListenFd, aAddress, aAddressLength)));
  if (fd < 0) {
    NS_WARNING("Cannot accept file descriptor!");
    return NS_ERROR_FAILURE;
  }
  nsresult rv = SetSocketFlags(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aStreamFd = fd.forget();

  return NS_OK;
}

nsresult
BluetoothUnixSocketConnector::CreateStreamSocket(struct sockaddr* aAddress,
                                                 socklen_t* aAddressLength,
                                                 int& aStreamFd)
{
  ScopedClose fd;

  nsresult rv = CreateSocket(fd.rwget());
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = SetSocketFlags(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (aAddress && aAddressLength) {
    rv = CreateAddress(*aAddress, *aAddressLength);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  aStreamFd = fd.forget();

  return NS_OK;
}

nsresult
BluetoothUnixSocketConnector::Duplicate(UnixSocketConnector*& aConnector)
{
  aConnector = new BluetoothUnixSocketConnector(*this);

  return NS_OK;
}

END_BLUETOOTH_NAMESPACE
