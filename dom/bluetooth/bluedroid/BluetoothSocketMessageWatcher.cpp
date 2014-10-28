/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothSocketMessageWatcher.h"
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "BluetoothInterface.h"
#include "nsClassHashtable.h"

BEGIN_BLUETOOTH_NAMESPACE

//
// SocketMessageWatcherWrapper
//

/* |SocketMessageWatcherWrapper| wraps SocketMessageWatcher to keep it from
 * being released by hash table's Remove() method.
 */
class SocketMessageWatcherWrapper
{
public:
  SocketMessageWatcherWrapper(SocketMessageWatcher* aSocketMessageWatcher)
    : mSocketMessageWatcher(aSocketMessageWatcher)
  {
    MOZ_ASSERT(mSocketMessageWatcher);
  }

  SocketMessageWatcher* GetSocketMessageWatcher()
  {
    return mSocketMessageWatcher;
  }

private:
  SocketMessageWatcher* mSocketMessageWatcher;
};

/* |sWatcherHashTable| maps result handlers to corresponding watchers */
static nsClassHashtable<nsRefPtrHashKey<BluetoothSocketResultHandler>,
                        SocketMessageWatcherWrapper>
  sWatcherHashtable;

//
// SocketMessageWatcher
//

SocketMessageWatcher::SocketMessageWatcher(
  int aFd, BluetoothSocketResultHandler* aRes)
  : mFd(aFd)
  , mClientFd(-1)
  , mLen(0)
  , mRes(aRes)
{
  MOZ_ASSERT(mRes);
}

SocketMessageWatcher::~SocketMessageWatcher()
{ }

void
SocketMessageWatcher::OnFileCanReadWithoutBlocking(int aFd)
{
  BluetoothStatus status;

  switch (mLen) {
    case 0:
      status = RecvMsg1();
      break;
    case MSG1_SIZE:
      status = RecvMsg2();
      break;
    default:
      /* message-size error */
      status = STATUS_FAIL;
      break;
  }

  if (IsComplete() || status != STATUS_SUCCESS) {
    StopWatching();
    Proceed(status);
  }
}

void
SocketMessageWatcher::OnFileCanWriteWithoutBlocking(int aFd)
{ }

void
SocketMessageWatcher::Watch()
{
  // add this watcher and its result handler to hash table
  sWatcherHashtable.Put(mRes, new SocketMessageWatcherWrapper(this));

  MessageLoopForIO::current()->WatchFileDescriptor(
    mFd,
    true,
    MessageLoopForIO::WATCH_READ,
    &mWatcher,
    this);
}

void
SocketMessageWatcher::StopWatching()
{
  mWatcher.StopWatchingFileDescriptor();

  // remove this watcher and its result handler from hash table
  sWatcherHashtable.Remove(mRes);
}

bool
SocketMessageWatcher::IsComplete() const
{
  return mLen == (MSG1_SIZE + MSG2_SIZE);
}

int
SocketMessageWatcher::GetFd() const
{
  return mFd;
}

int32_t
SocketMessageWatcher::GetChannel1() const
{
  return ReadInt32(OFF_CHANNEL1);
}

int32_t
SocketMessageWatcher::GetSize() const
{
  return ReadInt16(OFF_SIZE);
}

nsString
SocketMessageWatcher::GetBdAddress() const
{
  nsString bdAddress;
  ReadBdAddress(OFF_BDADDRESS, bdAddress);
  return bdAddress;
}

int32_t
SocketMessageWatcher::GetChannel2() const
{
  return ReadInt32(OFF_CHANNEL2);
}

int32_t
SocketMessageWatcher::GetConnectionStatus() const
{
  return ReadInt32(OFF_STATUS);
}

int
SocketMessageWatcher::GetClientFd() const
{
  return mClientFd;
}

BluetoothSocketResultHandler*
SocketMessageWatcher::GetResultHandler() const
{
  return mRes;
}

BluetoothStatus
SocketMessageWatcher::RecvMsg1()
{
  struct iovec iv;
  memset(&iv, 0, sizeof(iv));
  iv.iov_base = mBuf;
  iv.iov_len = MSG1_SIZE;

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iv;
  msg.msg_iovlen = 1;

  ssize_t res = TEMP_FAILURE_RETRY(recvmsg(mFd, &msg, MSG_NOSIGNAL));
  if (res <= 0) {
    return STATUS_FAIL;
  }

  mLen += res;

  return STATUS_SUCCESS;
}

#define CMSGHDR_CONTAINS_FD(_cmsghdr) \
    ( ((_cmsghdr)->cmsg_level == SOL_SOCKET) && \
      ((_cmsghdr)->cmsg_type == SCM_RIGHTS) )

BluetoothStatus
SocketMessageWatcher::RecvMsg2()
{
  struct iovec iv;
  memset(&iv, 0, sizeof(iv));
  iv.iov_base = mBuf + MSG1_SIZE;
  iv.iov_len = MSG2_SIZE;

  struct msghdr msg;
  struct cmsghdr cmsgbuf[CMSG_SPACE(sizeof(int))];
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iv;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  ssize_t res = TEMP_FAILURE_RETRY(recvmsg(mFd, &msg, MSG_NOSIGNAL));
  if (res <= 0) {
    return STATUS_FAIL;
  }

  mLen += res;

  if (msg.msg_flags & (MSG_CTRUNC | MSG_OOB | MSG_ERRQUEUE)) {
    return STATUS_FAIL;
  }

  struct cmsghdr *cmsgptr = CMSG_FIRSTHDR(&msg);

  // Extract client fd from message header
  for (; cmsgptr; cmsgptr = CMSG_NXTHDR(&msg, cmsgptr)) {
    if (CMSGHDR_CONTAINS_FD(cmsgptr)) {
      // if multiple file descriptors have been sent, we close
      // all but the final one.
      if (mClientFd != -1) {
        TEMP_FAILURE_RETRY(close(mClientFd));
      }
      // retrieve sent client fd
      mClientFd = *(static_cast<int*>(CMSG_DATA(cmsgptr)));
    }
  }

  return STATUS_SUCCESS;
}

int16_t
SocketMessageWatcher::ReadInt16(unsigned long aOffset) const
{
  /* little-endian buffer */
  return (static_cast<int16_t>(mBuf[aOffset + 1]) << 8) |
          static_cast<int16_t>(mBuf[aOffset]);
}

int32_t
SocketMessageWatcher::ReadInt32(unsigned long aOffset) const
{
  /* little-endian buffer */
  return (static_cast<int32_t>(mBuf[aOffset + 3]) << 24) |
         (static_cast<int32_t>(mBuf[aOffset + 2]) << 16) |
         (static_cast<int32_t>(mBuf[aOffset + 1]) << 8) |
          static_cast<int32_t>(mBuf[aOffset]);
}

void
SocketMessageWatcher::ReadBdAddress(unsigned long aOffset,
                                    nsAString& aBdAddress) const
{
  char str[BLUETOOTH_ADDRESS_LENGTH + 1];

  int res = snprintf(str, sizeof(str), "%02x:%02x:%02x:%02x:%02x:%02x",
                     static_cast<int>(mBuf[aOffset + 0]),
                     static_cast<int>(mBuf[aOffset + 1]),
                     static_cast<int>(mBuf[aOffset + 2]),
                     static_cast<int>(mBuf[aOffset + 3]),
                     static_cast<int>(mBuf[aOffset + 4]),
                     static_cast<int>(mBuf[aOffset + 5]));
  if (res < 0) {
    aBdAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
  } else if ((size_t)res >= sizeof(str)) { /* string buffer too small */
    aBdAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
  } else {
    aBdAddress = NS_ConvertUTF8toUTF16(str);
  }
}

//
// SocketMessageWatcherTask
//

SocketMessageWatcherTask::SocketMessageWatcherTask(
  SocketMessageWatcher* aWatcher)
  : mWatcher(aWatcher)
{
  MOZ_ASSERT(mWatcher);
}

void
SocketMessageWatcherTask::Run()
{
  mWatcher->Watch();
}

//
// DeleteSocketMessageWatcherTask
//

DeleteSocketMessageWatcherTask::DeleteSocketMessageWatcherTask(
  BluetoothSocketResultHandler* aRes)
  : mRes(aRes)
{
  MOZ_ASSERT(mRes);
}

void
DeleteSocketMessageWatcherTask::Run()
{
  // look up hash table for the watcher corresponding to |mRes|
  SocketMessageWatcherWrapper* wrapper = sWatcherHashtable.Get(mRes);
  if (!wrapper) {
    return;
  }

  // stop the watcher if it exists
  SocketMessageWatcher* watcher = wrapper->GetSocketMessageWatcher();
  watcher->StopWatching();
  watcher->Proceed(STATUS_DONE);
}

END_BLUETOOTH_NAMESPACE
