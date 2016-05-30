/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothSocketMessageWatcher.h"
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "BluetoothInterface.h"
#include "mozilla/EndianUtils.h"
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

BluetoothAddress
SocketMessageWatcher::GetBdAddress() const
{
  BluetoothAddress bdAddress;
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
      memcpy(&mClientFd, CMSG_DATA(cmsgptr), sizeof(mClientFd));
    }
  }

  return STATUS_SUCCESS;
}

int16_t
SocketMessageWatcher::ReadInt16(unsigned long aOffset) const
{
  /* little-endian buffer */
  return LittleEndian::readInt16(&mBuf[aOffset]);
}

int32_t
SocketMessageWatcher::ReadInt32(unsigned long aOffset) const
{
  /* little-endian buffer */
  return LittleEndian::readInt32(&mBuf[aOffset]);
}

void
SocketMessageWatcher::ReadBdAddress(unsigned long aOffset,
                                    BluetoothAddress& aBdAddress) const
{
  aBdAddress = BluetoothAddress(mBuf[aOffset + 0],
                                mBuf[aOffset + 1],
                                mBuf[aOffset + 2],
                                mBuf[aOffset + 3],
                                mBuf[aOffset + 4],
                                mBuf[aOffset + 5]);
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

NS_IMETHODIMP
SocketMessageWatcherTask::Run()
{
  mWatcher->Watch();
  return NS_OK;
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

NS_IMETHODIMP
DeleteSocketMessageWatcherTask::Run()
{
  // look up hash table for the watcher corresponding to |mRes|
  SocketMessageWatcherWrapper* wrapper = sWatcherHashtable.Get(mRes);
  if (!wrapper) {
    return NS_OK;
  }

  // stop the watcher if it exists
  SocketMessageWatcher* watcher = wrapper->GetSocketMessageWatcher();
  watcher->StopWatching();
  watcher->Proceed(STATUS_DONE);
  return NS_OK;
}

END_BLUETOOTH_NAMESPACE
