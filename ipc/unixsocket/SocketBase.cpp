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
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR

namespace mozilla {
namespace ipc {

//
// UnixSocketIOBuffer
//

UnixSocketBuffer::UnixSocketBuffer()
  : mSize(0)
  , mOffset(0)
  , mAvailableSpace(0)
  , mData(nullptr)
{
  MOZ_COUNT_CTOR(UnixSocketBuffer);
}

UnixSocketBuffer::~UnixSocketBuffer()
{
  MOZ_COUNT_DTOR(UnixSocketBuffer);

  // Make sure that the caller released the buffer's memory.
  MOZ_ASSERT(!GetBuffer());
}

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

UnixSocketIOBuffer::UnixSocketIOBuffer()
{
  MOZ_COUNT_CTOR_INHERITED(UnixSocketIOBuffer, UnixSocketBuffer);
}

UnixSocketIOBuffer::~UnixSocketIOBuffer()
{
  MOZ_COUNT_DTOR_INHERITED(UnixSocketIOBuffer, UnixSocketBuffer);
}

//
// UnixSocketRawData
//

UnixSocketRawData::UnixSocketRawData(const void* aData, size_t aSize)
{
  MOZ_ASSERT(aData || !aSize);

  MOZ_COUNT_CTOR_INHERITED(UnixSocketRawData, UnixSocketIOBuffer);

  ResetBuffer(static_cast<uint8_t*>(memcpy(new uint8_t[aSize], aData, aSize)),
              0, aSize, aSize);
}

UnixSocketRawData::UnixSocketRawData(UniquePtr<uint8_t[]> aData, size_t aSize)
{
  MOZ_ASSERT(aData || !aSize);

  MOZ_COUNT_CTOR_INHERITED(UnixSocketRawData, UnixSocketIOBuffer);

  ResetBuffer(aData.release(), 0, aSize, aSize);
}

UnixSocketRawData::UnixSocketRawData(size_t aSize)
{
  MOZ_COUNT_CTOR_INHERITED(UnixSocketRawData, UnixSocketIOBuffer);

  ResetBuffer(new uint8_t[aSize], 0, 0, aSize);
}

UnixSocketRawData::~UnixSocketRawData()
{
  MOZ_COUNT_DTOR_INHERITED(UnixSocketRawData, UnixSocketIOBuffer);

  UniquePtr<uint8_t[]> data(GetBuffer());
  ResetBuffer(nullptr, 0, 0, 0);
}

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
  return mConnectionStatus;
}

int
SocketBase::GetSuggestedConnectDelayMs() const
{
  return mConnectDelayMs;
}

void
SocketBase::NotifySuccess()
{
  mConnectionStatus = SOCKET_CONNECTED;
  mConnectTimestamp = PR_IntervalNow();
  OnConnectSuccess();
}

void
SocketBase::NotifyError()
{
  mConnectionStatus = SOCKET_DISCONNECTED;
  mConnectDelayMs = CalculateConnectDelayMs();
  mConnectTimestamp = 0;
  OnConnectError();
}

void
SocketBase::NotifyDisconnect()
{
  mConnectionStatus = SOCKET_DISCONNECTED;
  mConnectDelayMs = CalculateConnectDelayMs();
  mConnectTimestamp = 0;
  OnDisconnect();
}

uint32_t
SocketBase::CalculateConnectDelayMs() const
{
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
{
  MOZ_COUNT_CTOR(SocketBase);
}

SocketBase::~SocketBase()
{
  MOZ_ASSERT(mConnectionStatus == SOCKET_DISCONNECTED);

  MOZ_COUNT_DTOR(SocketBase);
}

void
SocketBase::SetConnectionStatus(SocketConnectionStatus aConnectionStatus)
{
  mConnectionStatus = aConnectionStatus;
}

//
// SocketIOBase
//

SocketIOBase::SocketIOBase(MessageLoop* aConsumerLoop)
  : mConsumerLoop(aConsumerLoop)
{
  MOZ_ASSERT(mConsumerLoop);

  MOZ_COUNT_CTOR(SocketIOBase);
}

SocketIOBase::~SocketIOBase()
{
  MOZ_COUNT_DTOR(SocketIOBase);
}

MessageLoop*
SocketIOBase::GetConsumerThread() const
{
  return mConsumerLoop;
}

bool
SocketIOBase::IsConsumerThread() const
{
  return GetConsumerThread() == MessageLoop::current();
}

//
// SocketEventTask
//

SocketEventTask::SocketEventTask(SocketIOBase* aIO, SocketEvent aEvent)
  : SocketTask<SocketIOBase>(aIO)
  , mEvent(aEvent)
{
  MOZ_COUNT_CTOR(SocketEventTask);
}

SocketEventTask::~SocketEventTask()
{
  MOZ_COUNT_DTOR(SocketEventTask);
}

NS_IMETHODIMP
SocketEventTask::Run()
{
  SocketIOBase* io = SocketTask<SocketIOBase>::GetIO();

  MOZ_ASSERT(io->IsConsumerThread());

  if (NS_WARN_IF(io->IsShutdownOnConsumerThread())) {
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
// SocketRequestClosingTask
//

SocketRequestClosingTask::SocketRequestClosingTask(SocketIOBase* aIO)
  : SocketTask<SocketIOBase>(aIO)
{
  MOZ_COUNT_CTOR(SocketRequestClosingTask);
}

SocketRequestClosingTask::~SocketRequestClosingTask()
{
  MOZ_COUNT_DTOR(SocketRequestClosingTask);
}

NS_IMETHODIMP
SocketRequestClosingTask::Run()
{
  SocketIOBase* io = SocketTask<SocketIOBase>::GetIO();

  MOZ_ASSERT(io->IsConsumerThread());

  if (NS_WARN_IF(io->IsShutdownOnConsumerThread())) {
    // Since we've already explicitly closed and the close
    // happened before this, this isn't really an error.
    return NS_OK;
  }

  SocketBase* socketBase = io->GetSocketBase();
  MOZ_ASSERT(socketBase);

  socketBase->Close();

  return NS_OK;
}

//
// SocketDeleteInstanceTask
//

SocketDeleteInstanceTask::SocketDeleteInstanceTask(SocketIOBase* aIO)
  : mIO(aIO)
{
  MOZ_COUNT_CTOR(SocketDeleteInstanceTask);
}

SocketDeleteInstanceTask::~SocketDeleteInstanceTask()
{
  MOZ_COUNT_DTOR(SocketDeleteInstanceTask);
}

NS_IMETHODIMP
SocketDeleteInstanceTask::Run()
{
  mIO.reset(); // delete instance
  return NS_OK;
}

//
// SocketIOShutdownTask
//

SocketIOShutdownTask::SocketIOShutdownTask(SocketIOBase* aIO)
  : SocketIOTask<SocketIOBase>(aIO)
{
  MOZ_COUNT_CTOR(SocketIOShutdownTask);
}

SocketIOShutdownTask::~SocketIOShutdownTask()
{
  MOZ_COUNT_DTOR(SocketIOShutdownTask);
}

NS_IMETHODIMP
SocketIOShutdownTask::Run()
{
  SocketIOBase* io = SocketIOTask<SocketIOBase>::GetIO();

  MOZ_ASSERT(!io->IsConsumerThread());
  MOZ_ASSERT(!io->IsShutdownOnIOThread());

  // At this point, there should be no new events on the I/O thread
  // after this one with the possible exception of an accept task,
  // which ShutdownOnIOThread will cancel for us. We are now fully
  // shut down, so we can send a message to the consumer thread to
  // delete |io| safely knowing that it's not reference any longer.
  io->ShutdownOnIOThread();
  io->GetConsumerThread()->PostTask(
    MakeAndAddRef<SocketDeleteInstanceTask>(io));
  return NS_OK;
}

}
}
