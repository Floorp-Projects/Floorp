/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamSocket.h"
#include <fcntl.h>
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR
#include "nsXULAppAPI.h"
#include "StreamSocketConsumer.h"
#include "UnixSocketConnector.h"

static const size_t MAX_READ_SIZE = 1 << 16;

namespace mozilla {
namespace ipc {

//
// StreamSocketIO
//

class StreamSocketIO final : public ConnectionOrientedSocketIO
{
public:
  class ConnectTask;
  class DelayedConnectTask;
  class ReceiveTask;

  StreamSocketIO(MessageLoop* aConsumerLoop,
                 MessageLoop* aIOLoop,
                 StreamSocket* aStreamSocket,
                 UnixSocketConnector* aConnector);
  StreamSocketIO(MessageLoop* aConsumerLoop,
                 MessageLoop* aIOLoop,
                 int aFd, ConnectionStatus aConnectionStatus,
                 StreamSocket* aStreamSocket,
                 UnixSocketConnector* aConnector);
  ~StreamSocketIO();

  StreamSocket* GetStreamSocket();
  DataSocket* GetDataSocket();

  // Delayed-task handling
  //

  void SetDelayedConnectTask(CancelableTask* aTask);
  void ClearDelayedConnectTask();
  void CancelDelayedConnectTask();

  // Methods for |DataSocket|
  //

  nsresult QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer) override;
  void ConsumeBuffer() override;
  void DiscardBuffer() override;

  // Methods for |SocketIOBase|
  //

  SocketBase* GetSocketBase() override;

  bool IsShutdownOnConsumerThread() const override;
  bool IsShutdownOnIOThread() const override;

  void ShutdownOnConsumerThread() override;
  void ShutdownOnIOThread() override;

private:
  /**
   * Consumer pointer. Non-thread-safe pointer, so should only be manipulated
   * directly from consumer thread. All non-consumer-thread accesses should
   * happen with mIO as container.
   */
  StreamSocket* mStreamSocket;

  /**
   * If true, do not requeue whatever task we're running
   */
  bool mShuttingDownOnIOThread;

  /**
   * Task member for delayed connect task. Should only be access on consumer
   * thread.
   */
  CancelableTask* mDelayedConnectTask;

  /**
   * I/O buffer for received data
   */
  nsAutoPtr<UnixSocketRawData> mBuffer;
};

StreamSocketIO::StreamSocketIO(MessageLoop* aConsumerLoop,
                               MessageLoop* aIOLoop,
                               StreamSocket* aStreamSocket,
                               UnixSocketConnector* aConnector)
  : ConnectionOrientedSocketIO(aConsumerLoop, aIOLoop, aConnector)
  , mStreamSocket(aStreamSocket)
  , mShuttingDownOnIOThread(false)
  , mDelayedConnectTask(nullptr)
{
  MOZ_ASSERT(mStreamSocket);

  MOZ_COUNT_CTOR_INHERITED(StreamSocketIO, ConnectionOrientedSocketIO);
}

StreamSocketIO::StreamSocketIO(MessageLoop* aConsumerLoop,
                               MessageLoop* aIOLoop,
                               int aFd, ConnectionStatus aConnectionStatus,
                               StreamSocket* aStreamSocket,
                               UnixSocketConnector* aConnector)
  : ConnectionOrientedSocketIO(aConsumerLoop,
                               aIOLoop,
                               aFd,
                               aConnectionStatus,
                               aConnector)
  , mStreamSocket(aStreamSocket)
  , mShuttingDownOnIOThread(false)
  , mDelayedConnectTask(nullptr)
{
  MOZ_ASSERT(mStreamSocket);

  MOZ_COUNT_CTOR_INHERITED(StreamSocketIO, ConnectionOrientedSocketIO);
}

StreamSocketIO::~StreamSocketIO()
{
  MOZ_ASSERT(IsConsumerThread());
  MOZ_ASSERT(IsShutdownOnConsumerThread());

  MOZ_COUNT_DTOR_INHERITED(StreamSocketIO, ConnectionOrientedSocketIO);
}

StreamSocket*
StreamSocketIO::GetStreamSocket()
{
  return mStreamSocket;
}

DataSocket*
StreamSocketIO::GetDataSocket()
{
  return GetStreamSocket();
}

void
StreamSocketIO::SetDelayedConnectTask(CancelableTask* aTask)
{
  MOZ_ASSERT(IsConsumerThread());

  mDelayedConnectTask = aTask;
}

void
StreamSocketIO::ClearDelayedConnectTask()
{
  MOZ_ASSERT(IsConsumerThread());

  mDelayedConnectTask = nullptr;
}

void
StreamSocketIO::CancelDelayedConnectTask()
{
  MOZ_ASSERT(IsConsumerThread());

  if (!mDelayedConnectTask) {
    return;
  }

  mDelayedConnectTask->Cancel();
  ClearDelayedConnectTask();
}

// |DataSocketIO|

nsresult
StreamSocketIO::QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer)
{
  MOZ_ASSERT(aBuffer);

  if (!mBuffer) {
    mBuffer = new UnixSocketRawData(MAX_READ_SIZE);
  }
  *aBuffer = mBuffer.get();

  return NS_OK;
}

/**
 * |ReceiveTask| transfers data received on the I/O thread
 * to an instance of |StreamSocket| on the consumer thread.
 */
class StreamSocketIO::ReceiveTask final : public SocketTask<StreamSocketIO>
{
public:
  ReceiveTask(StreamSocketIO* aIO, UnixSocketBuffer* aBuffer)
    : SocketTask<StreamSocketIO>(aIO)
    , mBuffer(aBuffer)
  {
    MOZ_COUNT_CTOR(ReceiveTask);
  }

  ~ReceiveTask()
  {
    MOZ_COUNT_DTOR(ReceiveTask);
  }

  void Run() override
  {
    StreamSocketIO* io = SocketTask<StreamSocketIO>::GetIO();

    MOZ_ASSERT(io->IsConsumerThread());

    if (NS_WARN_IF(io->IsShutdownOnConsumerThread())) {
      // Since we've already explicitly closed and the close
      // happened before this, this isn't really an error.
      return;
    }

    StreamSocket* streamSocket = io->GetStreamSocket();
    MOZ_ASSERT(streamSocket);

    streamSocket->ReceiveSocketData(mBuffer);
  }

private:
  nsAutoPtr<UnixSocketBuffer> mBuffer;
};

void
StreamSocketIO::ConsumeBuffer()
{
  GetConsumerThread()->PostTask(FROM_HERE,
                                new ReceiveTask(this, mBuffer.forget()));
}

void
StreamSocketIO::DiscardBuffer()
{
  // Nothing to do.
}

// |SocketIOBase|

SocketBase*
StreamSocketIO::GetSocketBase()
{
  return GetDataSocket();
}

bool
StreamSocketIO::IsShutdownOnConsumerThread() const
{
  MOZ_ASSERT(IsConsumerThread());

  return mStreamSocket == nullptr;
}

bool
StreamSocketIO::IsShutdownOnIOThread() const
{
  return mShuttingDownOnIOThread;
}

void
StreamSocketIO::ShutdownOnConsumerThread()
{
  MOZ_ASSERT(IsConsumerThread());
  MOZ_ASSERT(!IsShutdownOnConsumerThread());

  mStreamSocket = nullptr;
}

void
StreamSocketIO::ShutdownOnIOThread()
{
  MOZ_ASSERT(!IsConsumerThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  Close(); // will also remove fd from I/O loop
  mShuttingDownOnIOThread = true;
}

//
// Socket tasks
//

class StreamSocketIO::ConnectTask final : public SocketIOTask<StreamSocketIO>
{
public:
  ConnectTask(StreamSocketIO* aIO)
    : SocketIOTask<StreamSocketIO>(aIO)
  {
    MOZ_COUNT_CTOR(ReceiveTask);
  }

  ~ConnectTask()
  {
    MOZ_COUNT_DTOR(ReceiveTask);
  }

  void Run() override
  {
    MOZ_ASSERT(!GetIO()->IsConsumerThread());
    MOZ_ASSERT(!IsCanceled());

    GetIO()->Connect();
  }
};

class StreamSocketIO::DelayedConnectTask final
  : public SocketIOTask<StreamSocketIO>
{
public:
  DelayedConnectTask(StreamSocketIO* aIO)
    : SocketIOTask<StreamSocketIO>(aIO)
  {
    MOZ_COUNT_CTOR(DelayedConnectTask);
  }

  ~DelayedConnectTask()
  {
    MOZ_COUNT_DTOR(DelayedConnectTask);
  }

  void Run() override
  {
    MOZ_ASSERT(GetIO()->IsConsumerThread());

    if (IsCanceled()) {
      return;
    }

    StreamSocketIO* io = GetIO();
    if (io->IsShutdownOnConsumerThread()) {
      return;
    }

    io->ClearDelayedConnectTask();
    io->GetIOLoop()->PostTask(FROM_HERE, new ConnectTask(io));
  }
};

//
// StreamSocket
//

StreamSocket::StreamSocket(StreamSocketConsumer* aConsumer, int aIndex)
  : mIO(nullptr)
  , mConsumer(aConsumer)
  , mIndex(aIndex)
{
  MOZ_ASSERT(mConsumer);

  MOZ_COUNT_CTOR_INHERITED(StreamSocket, ConnectionOrientedSocket);
}

StreamSocket::~StreamSocket()
{
  MOZ_ASSERT(!mIO);

  MOZ_COUNT_DTOR_INHERITED(StreamSocket, ConnectionOrientedSocket);
}

void
StreamSocket::ReceiveSocketData(nsAutoPtr<UnixSocketBuffer>& aBuffer)
{
  mConsumer->ReceiveSocketData(mIndex, aBuffer);
}

nsresult
StreamSocket::Connect(UnixSocketConnector* aConnector, int aDelayMs,
                      MessageLoop* aConsumerLoop, MessageLoop* aIOLoop)
{
  MOZ_ASSERT(!mIO);

  mIO = new StreamSocketIO(aConsumerLoop, aIOLoop, this, aConnector);
  SetConnectionStatus(SOCKET_CONNECTING);

  if (aDelayMs > 0) {
    StreamSocketIO::DelayedConnectTask* connectTask =
      new StreamSocketIO::DelayedConnectTask(mIO);
    mIO->SetDelayedConnectTask(connectTask);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, connectTask, aDelayMs);
  } else {
    aIOLoop->PostTask(FROM_HERE, new StreamSocketIO::ConnectTask(mIO));
  }

  return NS_OK;
}

nsresult
StreamSocket::Connect(UnixSocketConnector* aConnector, int aDelayMs)
{
  return Connect(aConnector, aDelayMs,
                 MessageLoop::current(), XRE_GetIOMessageLoop());
}

// |ConnectionOrientedSocket|

nsresult
StreamSocket::PrepareAccept(UnixSocketConnector* aConnector,
                            MessageLoop* aConsumerLoop,
                            MessageLoop* aIOLoop,
                            ConnectionOrientedSocketIO*& aIO)
{
  MOZ_ASSERT(!mIO);
  MOZ_ASSERT(aConnector);

  SetConnectionStatus(SOCKET_CONNECTING);

  mIO = new StreamSocketIO(aConsumerLoop, aIOLoop,
                           -1, UnixSocketWatcher::SOCKET_IS_CONNECTING,
                           this, aConnector);
  aIO = mIO;

  return NS_OK;
}

// |DataSocket|

void
StreamSocket::SendSocketData(UnixSocketIOBuffer* aBuffer)
{
  MOZ_ASSERT(mIO);
  MOZ_ASSERT(mIO->IsConsumerThread());
  MOZ_ASSERT(!mIO->IsShutdownOnConsumerThread());

  mIO->GetIOLoop()->PostTask(
    FROM_HERE,
    new SocketIOSendTask<StreamSocketIO, UnixSocketIOBuffer>(mIO, aBuffer));
}

// |SocketBase|

void
StreamSocket::Close()
{
  MOZ_ASSERT(mIO);
  MOZ_ASSERT(mIO->IsConsumerThread());

  mIO->CancelDelayedConnectTask();

  // From this point on, we consider |mIO| as being deleted. We sever
  // the relationship here so any future calls to |Connect| will create
  // a new I/O object.
  mIO->ShutdownOnConsumerThread();
  mIO->GetIOLoop()->PostTask(FROM_HERE, new SocketIOShutdownTask(mIO));
  mIO = nullptr;

  NotifyDisconnect();
}

void
StreamSocket::OnConnectSuccess()
{
  mConsumer->OnConnectSuccess(mIndex);
}

void
StreamSocket::OnConnectError()
{
  mConsumer->OnConnectError(mIndex);
}

void
StreamSocket::OnDisconnect()
{
  mConsumer->OnDisconnect(mIndex);
}

} // namespace ipc
} // namespace mozilla
