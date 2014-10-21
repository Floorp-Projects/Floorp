/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "SocketBase.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

namespace mozilla {
namespace ipc {

//
// UnixSocketRawData
//

UnixSocketRawData::UnixSocketRawData(const void* aData, size_t aSize)
: mSize(aSize)
, mOffset(0)
, mAvailableSpace(aSize)
{
  MOZ_ASSERT(aData || !mSize);

  mData = new uint8_t[mAvailableSpace];
  memcpy(mData, aData, mSize);
}

UnixSocketRawData::UnixSocketRawData(size_t aSize)
: mSize(0)
, mOffset(0)
, mAvailableSpace(aSize)
{
  mData = new uint8_t[mAvailableSpace];
}

ssize_t
UnixSocketRawData::Receive(int aFd)
{
  if (!GetTrailingSpace()) {
    if (!GetLeadingSpace()) {
      return -1; /* buffer is full */
    }
    /* free up space at the end of data buffer */
    if (GetSize() <= GetLeadingSpace()) {
      memcpy(mData, GetData(), GetSize());
    } else {
      memmove(mData, GetData(), GetSize());
    }
    mOffset = 0;
  }

  ssize_t res =
    TEMP_FAILURE_RETRY(read(aFd, GetTrailingBytes(), GetTrailingSpace()));

  if (res < 0) {
    /* I/O error */
    return -1;
  } else if (!res) {
    /* EOF or peer shutdown sending */
    return 0;
  }

  mSize += res;

  return res;
}

ssize_t
UnixSocketRawData::Send(int aFd)
{
  if (!GetSize()) {
    return 0;
  }

  ssize_t res = TEMP_FAILURE_RETRY(write(aFd, GetData(), GetSize()));

  if (res < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0; /* socket is blocked; try again later */
    }
    return -1;
  } else if (!res) {
    /* nothing written */
    return 0;
  }

  Consume(res);

  return res;
}

//
// SocketBase
//

SocketBase::~SocketBase()
{
  MOZ_ASSERT(mConnectionStatus == SOCKET_DISCONNECTED);
}

SocketConnectionStatus
SocketBase::GetConnectionStatus() const
{
  MOZ_ASSERT(NS_IsMainThread());

  return mConnectionStatus;
}

int
SocketBase::GetSuggestedConnectDelayMs() const
{
  MOZ_ASSERT(NS_IsMainThread());

  return mConnectDelayMs;
}

void
SocketBase::NotifySuccess()
{
  MOZ_ASSERT(NS_IsMainThread());

  mConnectionStatus = SOCKET_CONNECTED;
  mConnectTimestamp = PR_IntervalNow();
  OnConnectSuccess();
}

void
SocketBase::NotifyError()
{
  MOZ_ASSERT(NS_IsMainThread());

  mConnectionStatus = SOCKET_DISCONNECTED;
  mConnectDelayMs = CalculateConnectDelayMs();
  mConnectTimestamp = 0;
  OnConnectError();
}

void
SocketBase::NotifyDisconnect()
{
  MOZ_ASSERT(NS_IsMainThread());

  mConnectionStatus = SOCKET_DISCONNECTED;
  mConnectDelayMs = CalculateConnectDelayMs();
  mConnectTimestamp = 0;
  OnDisconnect();
}

uint32_t
SocketBase::CalculateConnectDelayMs() const
{
  MOZ_ASSERT(NS_IsMainThread());

  uint32_t connectDelayMs = mConnectDelayMs;

  if (mConnectTimestamp && (PR_IntervalNow()-mConnectTimestamp) > connectDelayMs) {
    // reset delay if connection has been opened for a while, or...
    connectDelayMs = 0;
  } else if (!connectDelayMs) {
    // ...start with a delay of ~1 sec, or...
    connectDelayMs = 1<<10;
  } else if (connectDelayMs < (1<<16)) {
    // ...otherwise increase delay by a factor of 2
    connectDelayMs <<= 1;
  }
  return connectDelayMs;
}

SocketBase::SocketBase()
: mConnectionStatus(SOCKET_DISCONNECTED)
, mConnectTimestamp(0)
, mConnectDelayMs(0)
{ }

void
SocketBase::SetConnectionStatus(SocketConnectionStatus aConnectionStatus)
{
  mConnectionStatus = aConnectionStatus;
}

//
// SocketConsumerBase
//

SocketConsumerBase::~SocketConsumerBase()
{ }

//
// SocketIOBase
//

SocketIOBase::~SocketIOBase()
{ }

void
SocketIOBase::EnqueueData(UnixSocketRawData* aData)
{
  if (!aData->GetSize()) {
    delete aData; // delete empty data immediately
    return;
  }
  mOutgoingQ.AppendElement(aData);
}

bool
SocketIOBase::HasPendingData() const
{
  return !mOutgoingQ.IsEmpty();
}

SocketIOBase::SocketIOBase(size_t aMaxReadSize)
  : mMaxReadSize(aMaxReadSize)
{
  MOZ_ASSERT(mMaxReadSize);
}

}
}
