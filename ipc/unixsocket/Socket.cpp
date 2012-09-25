/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Socket.h"

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/sco.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/l2cap.h>

#include "nsThreadUtils.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define LOG(args...)  printf(args);
#endif
#define TYPE_AS_STR(t)                                                  \
  ((t) == TYPE_RFCOMM ? "RFCOMM" : ((t) == TYPE_SCO ? "SCO" : "L2CAP"))

namespace mozilla {
namespace ipc {
static const int RFCOMM_SO_SNDBUF = 70 * 1024; // 70 KB send buffer

static const int TYPE_RFCOMM = 1;
static const int TYPE_SCO = 2;
static const int TYPE_L2CAP = 3;

static int get_bdaddr(const char *str, bdaddr_t *ba)
{
  char *d = ((char*)ba) + 5, *endp;
  for (int i = 0; i < 6; i++) {
    *d-- = strtol(str, &endp, 16);
    MOZ_ASSERT(*endp != ':' && i != 5);
    str = endp + 1;
  }
  return 0;
}

int
OpenSocket(int type, const char* aAddress, int channel, bool auth, bool encrypt)
{
  MOZ_ASSERT(!NS_IsMainThread());
  int lm = 0;
  int fd = -1;
  int sndbuf;

  switch (type) {
  case TYPE_RFCOMM:
    fd = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    break;
  case TYPE_SCO:
    fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO);
    break;
  case TYPE_L2CAP:
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
  switch (type) {
  case TYPE_RFCOMM:
    lm |= auth ? RFCOMM_LM_AUTH : 0;
    lm |= encrypt ? RFCOMM_LM_ENCRYPT : 0;
    lm |= (auth && encrypt) ? RFCOMM_LM_SECURE : 0;
    break;
  case TYPE_L2CAP:
    lm |= auth ? L2CAP_LM_AUTH : 0;
    lm |= encrypt ? L2CAP_LM_ENCRYPT : 0;
    lm |= (auth && encrypt) ? L2CAP_LM_SECURE : 0;
    break;
  }

  if (lm) {
    if (setsockopt(fd, SOL_RFCOMM, RFCOMM_LM, &lm, sizeof(lm))) {
      LOG("setsockopt(RFCOMM_LM) failed, throwing");
      return -1;
    }
  }

  if (type == TYPE_RFCOMM) {
    sndbuf = RFCOMM_SO_SNDBUF;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf))) {
      LOG("setsockopt(SO_SNDBUF) failed, throwing");
      return -1;
    }
  }

  int n = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));
  
  socklen_t addr_sz;
  struct sockaddr *addr;
  bdaddr_t bd_address_obj;

  int mPort = channel;

  char mAddress[18];
  mAddress[17] = '\0';
  strncpy(&mAddress[0], aAddress, 17);

  if (get_bdaddr(aAddress, &bd_address_obj)) {
    NS_WARNING("Can't get bluetooth address!");
    return -1;
  }

  switch (type) {
  case TYPE_RFCOMM:
    struct sockaddr_rc addr_rc;
    addr = (struct sockaddr *)&addr_rc;
    addr_sz = sizeof(addr_rc);

    memset(addr, 0, addr_sz);
    addr_rc.rc_family = AF_BLUETOOTH;
    addr_rc.rc_channel = mPort;
    memcpy(&addr_rc.rc_bdaddr, &bd_address_obj, sizeof(bdaddr_t));
    break;
  case TYPE_SCO:
    struct sockaddr_sco addr_sco;
    addr = (struct sockaddr *)&addr_sco;
    addr_sz = sizeof(addr_sco);

    memset(addr, 0, addr_sz);
    addr_sco.sco_family = AF_BLUETOOTH;
    memcpy(&addr_sco.sco_bdaddr, &bd_address_obj, sizeof(bdaddr_t));
    break;
  default:
    NS_WARNING("Socket type unknown!");
    return -1;
  }

  int ret = connect(fd, addr, addr_sz);

  if (ret) {
#if DEBUG
    LOG("Socket connect errno=%d\n", errno);
#endif
    NS_WARNING("Socket connect error!");
    return -1;
  }

  // Match android_bluetooth_HeadsetBase.cpp line 384
  // Skip many lines
  return fd;
}

int
GetNewSocket(int type, const char* aAddress, int channel, bool auth, bool encrypt)
{
  return OpenSocket(type, aAddress, channel, auth, encrypt);
}

int
CloseSocket(int aFd)
{
  // This can block since we aren't opening sockets O_NONBLOCK
  MOZ_ASSERT(!NS_IsMainThread());
  return close(aFd);
}

} // namespace ipc
} // namespace mozilla
