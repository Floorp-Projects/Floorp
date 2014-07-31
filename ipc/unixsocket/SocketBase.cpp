/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "SocketBase.h"
#include <string.h>
#include "nsThreadUtils.h"

namespace mozilla {
namespace ipc {

//
// UnixSocketRawData
//

UnixSocketRawData::UnixSocketRawData(size_t aSize)
: mSize(aSize)
, mCurrentWriteOffset(0)
{
  mData = new uint8_t[mSize];
}

UnixSocketRawData::UnixSocketRawData(const void* aData, size_t aSize)
: mSize(aSize)
, mCurrentWriteOffset(0)
{
  MOZ_ASSERT(aData || !mSize);

  mData = new uint8_t[mSize];
  memcpy(mData, aData, mSize);
}

//
// SocketConsumerBase
//

SocketConsumerBase::~SocketConsumerBase()
{
  MOZ_ASSERT(mConnectionStatus == SOCKET_DISCONNECTED);
}

SocketConnectionStatus
SocketConsumerBase::GetConnectionStatus() const
{
  MOZ_ASSERT(NS_IsMainThread());

  return mConnectionStatus;
}

int
SocketConsumerBase::GetSuggestedConnectDelayMs() const
{
  MOZ_ASSERT(NS_IsMainThread());

  return mConnectDelayMs;
}

void
SocketConsumerBase::NotifySuccess()
{
  MOZ_ASSERT(NS_IsMainThread());

  mConnectionStatus = SOCKET_CONNECTED;
  mConnectTimestamp = PR_IntervalNow();
  OnConnectSuccess();
}

void
SocketConsumerBase::NotifyError()
{
  MOZ_ASSERT(NS_IsMainThread());

  mConnectionStatus = SOCKET_DISCONNECTED;
  mConnectDelayMs = CalculateConnectDelayMs();
  OnConnectError();
}

void
SocketConsumerBase::NotifyDisconnect()
{
  MOZ_ASSERT(NS_IsMainThread());

  mConnectionStatus = SOCKET_DISCONNECTED;
  mConnectDelayMs = CalculateConnectDelayMs();
  OnDisconnect();
}

uint32_t
SocketConsumerBase::CalculateConnectDelayMs() const
{
  MOZ_ASSERT(NS_IsMainThread());

  uint32_t connectDelayMs = mConnectDelayMs;

  if ((PR_IntervalNow()-mConnectTimestamp) > connectDelayMs) {
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

SocketConsumerBase::SocketConsumerBase()
: mConnectionStatus(SOCKET_DISCONNECTED)
, mConnectTimestamp(0)
, mConnectDelayMs(0)
{ }

void
SocketConsumerBase::SetConnectionStatus(
  SocketConnectionStatus aConnectionStatus)
{
  mConnectionStatus = aConnectionStatus;
}

//
// SocketIOBase
//

SocketIOBase::~SocketIOBase()
{ }

void
SocketIOBase::EnqueueData(UnixSocketRawData* aData)
{
  if (!aData->mSize) {
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
