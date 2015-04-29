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
// UnixSocketIOBuffer
//

UnixSocketBuffer::UnixSocketBuffer(const void* aData, size_t aSize)
  : mSize(aSize)
  , mOffset(0)
  , mAvailableSpace(aSize)
{
  MOZ_ASSERT(aData || !mSize);

  mData = new uint8_t[mAvailableSpace];
  memcpy(mData, aData, mSize);
}

UnixSocketBuffer::UnixSocketBuffer(size_t aAvailableSpace)
  : mSize(0)
  , mOffset(0)
  , mAvailableSpace(aAvailableSpace)
{
  mData = new uint8_t[mAvailableSpace];
}

UnixSocketBuffer::~UnixSocketBuffer()
{ }

const uint8_t*
UnixSocketBuffer::Consume(size_t aLen)
{
  if (NS_WARN_IF(GetSize() < aLen)) {
    return nullptr;
  }
  uint8_t* data = mData + mOffset;
  mOffset += aLen;
  return data;
}

nsresult
UnixSocketBuffer::Read(void* aValue, size_t aLen)
{
  const uint8_t* data = Consume(aLen);
  if (!data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memcpy(aValue, data, aLen);
  return NS_OK;
}

uint8_t*
UnixSocketBuffer::Append(size_t aLen)
{
  if (((mAvailableSpace - mSize) < aLen)) {
    size_t availableSpace = mAvailableSpace + std::max(mAvailableSpace, aLen);
    uint8_t* data = new uint8_t[availableSpace];
    memcpy(data, mData, mSize);
    mData = data;
    mAvailableSpace = availableSpace;
  }
  uint8_t* data = mData + mSize;
  mSize += aLen;
  return data;
}

nsresult
UnixSocketBuffer::Write(const void* aValue, size_t aLen)
{
  uint8_t* data = Append(aLen);
  if (!data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memcpy(data, aValue, aLen);
  return NS_OK;
}

void
UnixSocketBuffer::CleanupLeadingSpace()
{
  if (GetLeadingSpace()) {
    if (GetSize() <= GetLeadingSpace()) {
      memcpy(mData, GetData(), GetSize());
    } else {
      memmove(mData, GetData(), GetSize());
    }
    mOffset = 0;
  }
}

//
// UnixSocketIOBuffer
//

UnixSocketIOBuffer::UnixSocketIOBuffer(const void* aData, size_t aSize)
  : UnixSocketBuffer(aData, aSize)
{ }

UnixSocketIOBuffer::UnixSocketIOBuffer(size_t aAvailableSpace)
  : UnixSocketBuffer(aAvailableSpace)
{ }

UnixSocketIOBuffer::~UnixSocketIOBuffer()
{ }

//
// UnixSocketRawData
//

UnixSocketRawData::UnixSocketRawData(const void* aData, size_t aSize)
: UnixSocketIOBuffer(aData, aSize)
{ }

UnixSocketRawData::UnixSocketRawData(size_t aSize)
: UnixSocketIOBuffer(aSize)
{ }

ssize_t
UnixSocketRawData::Receive(int aFd)
{
  if (!GetTrailingSpace()) {
    if (!GetLeadingSpace()) {
      return -1; /* buffer is full */
    }
    /* free up space at the end of data buffer */
    CleanupLeadingSpace();
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

  Append(res); /* mark read data as 'valid' */

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

SocketBase::~SocketBase()
{
  MOZ_ASSERT(mConnectionStatus == SOCKET_DISCONNECTED);
}

void
SocketBase::SetConnectionStatus(SocketConnectionStatus aConnectionStatus)
{
  mConnectionStatus = aConnectionStatus;
}

//
// SocketIOBase
//

SocketIOBase::SocketIOBase()
{ }

SocketIOBase::~SocketIOBase()
{ }

//
// SocketIOEventRunnable
//

SocketIOEventRunnable::SocketIOEventRunnable(SocketIOBase* aIO,
                                             SocketEvent aEvent)
  : SocketIORunnable<SocketIOBase>(aIO)
  , mEvent(aEvent)
{ }

NS_METHOD
SocketIOEventRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  SocketIOBase* io = SocketIORunnable<SocketIOBase>::GetIO();

  if (NS_WARN_IF(io->IsShutdownOnMainThread())) {
    // Since we've already explicitly closed and the close
    // happened before this, this isn't really an error.
    return NS_OK;
  }

  SocketBase* socketBase = io->GetSocketBase();
  MOZ_ASSERT(socketBase);

  if (mEvent == CONNECT_SUCCESS) {
    socketBase->NotifySuccess();
  } else if (mEvent == CONNECT_ERROR) {
    socketBase->NotifyError();
  } else if (mEvent == DISCONNECT) {
    socketBase->NotifyDisconnect();
  }

  return NS_OK;
}

//
// SocketIORequestClosingRunnable
//

SocketIORequestClosingRunnable::SocketIORequestClosingRunnable(
  SocketIOBase* aIO)
  : SocketIORunnable<SocketIOBase>(aIO)
{ }

NS_METHOD
SocketIORequestClosingRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  SocketIOBase* io = SocketIORunnable<SocketIOBase>::GetIO();

  if (NS_WARN_IF(io->IsShutdownOnMainThread())) {
    // Since we've already explicitly closed and the close
    // happened before this, this isn't really an error.
    return NS_OK;
  }

  SocketBase* socketBase = io->GetSocketBase();
  MOZ_ASSERT(socketBase);

  socketBase->CloseSocket();

  return NS_OK;
}

//
// SocketIODeleteInstanceRunnable
//

SocketIODeleteInstanceRunnable::SocketIODeleteInstanceRunnable(
  SocketIOBase* aIO)
  : mIO(aIO)
{ }

NS_METHOD
SocketIODeleteInstanceRunnable::Run()
{
  mIO = nullptr; // delete instance

  return NS_OK;
}

//
// SocketIOShutdownTask
//

SocketIOShutdownTask::SocketIOShutdownTask(SocketIOBase* aIO)
  : SocketIOTask<SocketIOBase>(aIO)
{ }

void
SocketIOShutdownTask::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  SocketIOBase* io = SocketIOTask<SocketIOBase>::GetIO();

  // At this point, there should be no new events on the I/O thread
  // after this one with the possible exception of an accept task,
  // which ShutdownOnIOThread will cancel for us. We are now fully
  // shut down, so we can send a message to the main thread to delete
  // |io| safely knowing that it's not reference any longer.
  MOZ_ASSERT(!io->IsShutdownOnIOThread());
  io->ShutdownOnIOThread();

  NS_DispatchToMainThread(new SocketIODeleteInstanceRunnable(io));
}

}
}
