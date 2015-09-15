/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DaemonSocketPDU.h"
#include "mozilla/ipc/DaemonSocketConsumer.h"
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR

#ifdef CHROMIUM_LOG
#undef CHROMIUM_LOG
#endif

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define CHROMIUM_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "I/O", args);
#else
#include <stdio.h>
#define IODEBUG true
#define CHROMIUM_LOG(args...) if (IODEBUG) printf(args);
#endif

namespace mozilla {
namespace ipc {

//
// DaemonSocketPDU
//

DaemonSocketPDU::DaemonSocketPDU(uint8_t aService, uint8_t aOpcode,
                                 uint16_t aPayloadSize)
  : mConsumer(nullptr)
{
  MOZ_COUNT_CTOR_INHERITED(DaemonSocketPDU, UnixSocketIOBuffer);

  // Allocate memory
  size_t availableSpace = HEADER_SIZE + aPayloadSize;
  ResetBuffer(new uint8_t[availableSpace], 0, 0, availableSpace);

  // Reserve PDU header
  uint8_t* data = Append(HEADER_SIZE);
  MOZ_ASSERT(data);

  // Setup PDU header
  data[OFF_SERVICE] = aService;
  data[OFF_OPCODE] = aOpcode;
  memcpy(data + OFF_LENGTH, &aPayloadSize, sizeof(aPayloadSize));
}

DaemonSocketPDU::DaemonSocketPDU(size_t aPayloadSize)
  : mConsumer(nullptr)
{
  MOZ_COUNT_CTOR_INHERITED(DaemonSocketPDU, UnixSocketIOBuffer);

  size_t availableSpace = HEADER_SIZE + aPayloadSize;
  ResetBuffer(new uint8_t[availableSpace], 0, 0, availableSpace);
}

DaemonSocketPDU::~DaemonSocketPDU()
{
  MOZ_COUNT_DTOR_INHERITED(DaemonSocketPDU, UnixSocketIOBuffer);

  nsAutoArrayPtr<uint8_t> data(GetBuffer());
  ResetBuffer(nullptr, 0, 0, 0);
}

void
DaemonSocketPDU::GetHeader(uint8_t& aService, uint8_t& aOpcode,
                              uint16_t& aPayloadSize)
{
  memcpy(&aService, GetData(OFF_SERVICE), sizeof(aService));
  memcpy(&aOpcode, GetData(OFF_OPCODE), sizeof(aOpcode));
  memcpy(&aPayloadSize, GetData(OFF_LENGTH), sizeof(aPayloadSize));
}

ssize_t
DaemonSocketPDU::Send(int aFd)
{
  struct iovec iv;
  memset(&iv, 0, sizeof(iv));
  iv.iov_base = GetData(GetLeadingSpace());
  iv.iov_len = GetSize();

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iv;
  msg.msg_iovlen = 1;
  msg.msg_control = nullptr;
  msg.msg_controllen = 0;

  ssize_t res = TEMP_FAILURE_RETRY(sendmsg(aFd, &msg, 0));
  if (res < 0) {
    MOZ_ASSERT(errno != EBADF); /* internal error */
    OnError("sendmsg", errno);
    return -1;
  }

  Consume(res);

  if (mConsumer) {
    // We successfully sent a PDU, now store the
    // result handler in the consumer.
    mConsumer->StoreResultHandler(*this);
  }

  return res;
}

#define CMSGHDR_CONTAINS_FD(_cmsghdr) \
    ( ((_cmsghdr)->cmsg_level == SOL_SOCKET) && \
      ((_cmsghdr)->cmsg_type == SCM_RIGHTS) )

ssize_t
DaemonSocketPDU::Receive(int aFd)
{
  struct iovec iv;
  memset(&iv, 0, sizeof(iv));
  iv.iov_base = GetData(0);
  iv.iov_len = GetAvailableSpace();

  uint8_t cmsgbuf[CMSG_SPACE(sizeof(int))];

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iv;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  ssize_t res = TEMP_FAILURE_RETRY(recvmsg(aFd, &msg, MSG_NOSIGNAL));
  if (res < 0) {
    MOZ_ASSERT(errno != EBADF); /* internal error */
    OnError("recvmsg", errno);
    return -1;
  }
  if (msg.msg_flags & (MSG_CTRUNC | MSG_OOB | MSG_ERRQUEUE)) {
    return -1;
  }

  SetRange(0, res);

  struct cmsghdr *chdr = CMSG_FIRSTHDR(&msg);

  for (; chdr; chdr = CMSG_NXTHDR(&msg, chdr)) {
    if (NS_WARN_IF(!CMSGHDR_CONTAINS_FD(chdr))) {
      continue;
    }
    // Retrieve sent file descriptor. If multiple file descriptors
    // have been sent, we close all but the final one.
    mReceivedFd = *(static_cast<int*>(CMSG_DATA(chdr)));
  }

  return res;
}

int
DaemonSocketPDU::AcquireFd()
{
  return mReceivedFd.forget();
}

nsresult
DaemonSocketPDU::UpdateHeader()
{
  size_t len = GetPayloadSize();
  if (len >= MAX_PAYLOAD_LENGTH) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  uint16_t len16 = static_cast<uint16_t>(len);

  memcpy(GetData(OFF_LENGTH), &len16, sizeof(len16));

  return NS_OK;
}

size_t
DaemonSocketPDU::GetPayloadSize() const
{
  MOZ_ASSERT(GetSize() >= HEADER_SIZE);

  return GetSize() - HEADER_SIZE;
}

void
DaemonSocketPDU::OnError(const char* aFunction, int aErrno)
{
  CHROMIUM_LOG("%s failed with error %d (%s)",
               aFunction, aErrno, strerror(aErrno));
}

}
}
