/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConnectionOrientedSocket.h"
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR
#include "UnixSocketConnector.h"

namespace mozilla {
namespace ipc {

//
// ConnectionOrientedSocketIO
//

ConnectionOrientedSocketIO::ConnectionOrientedSocketIO(
  MessageLoop* aConsumerLoop,
  MessageLoop* aIOLoop,
  int aFd, ConnectionStatus aConnectionStatus,
  UnixSocketConnector* aConnector)
  : DataSocketIO(aConsumerLoop)
  , UnixSocketWatcher(aIOLoop, aFd, aConnectionStatus)
  , mConnector(aConnector)
  , mPeerAddressLength(0)
{
  MOZ_ASSERT(mConnector);

  MOZ_COUNT_CTOR_INHERITED(ConnectionOrientedSocketIO, DataSocketIO);
}

ConnectionOrientedSocketIO::ConnectionOrientedSocketIO(
  MessageLoop* aConsumerLoop,
  MessageLoop* aIOLoop,
  UnixSocketConnector* aConnector)
  : DataSocketIO(aConsumerLoop)
  , UnixSocketWatcher(aIOLoop)
  , mConnector(aConnector)
  , mPeerAddressLength(0)
{
  MOZ_ASSERT(mConnector);

  MOZ_COUNT_CTOR_INHERITED(ConnectionOrientedSocketIO, DataSocketIO);
}

ConnectionOrientedSocketIO::~ConnectionOrientedSocketIO()
{
  MOZ_COUNT_DTOR_INHERITED(ConnectionOrientedSocketIO, DataSocketIO);
}

nsresult
ConnectionOrientedSocketIO::Accept(int aFd,
                                   const struct sockaddr* aPeerAddress,
                                   socklen_t aPeerAddressLength)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTING);

  SetSocket(aFd, SOCKET_IS_CONNECTED);

  // Address setup
  mPeerAddressLength = aPeerAddressLength;
  memcpy(&mPeerAddress, aPeerAddress, mPeerAddressLength);

  // Signal success and start data transfer
  OnConnected();

  return NS_OK;
}

nsresult
ConnectionOrientedSocketIO::Connect()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(!IsOpen());

  struct sockaddr* peerAddress =
    reinterpret_cast<struct sockaddr*>(&mPeerAddress);
  mPeerAddressLength = sizeof(mPeerAddress);

  int fd;
  nsresult rv = mConnector->CreateStreamSocket(peerAddress,
                                               &mPeerAddressLength,
                                               fd);
  if (NS_FAILED(rv)) {
    // Tell the consumer thread we've errored
    GetConsumerThread()->PostTask(
      MakeAndAddRef<SocketEventTask>(this, SocketEventTask::CONNECT_ERROR));
    return NS_ERROR_FAILURE;
  }

  SetFd(fd);

  // calls OnConnected() on success, or OnError() otherwise
  rv = UnixSocketWatcher::Connect(peerAddress, mPeerAddressLength);

  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

void
ConnectionOrientedSocketIO::Send(UnixSocketIOBuffer* aBuffer)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  EnqueueData(aBuffer);
  AddWatchers(WRITE_WATCHER, false);
}

// |UnixSocketWatcher|

void
ConnectionOrientedSocketIO::OnSocketCanReceiveWithoutBlocking()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED); // see bug 990984

  ssize_t res = ReceiveData(GetFd());
  if (res < 0) {
    /* I/O error */
    RemoveWatchers(READ_WATCHER|WRITE_WATCHER);
  } else if (!res) {
    /* EOF or peer shutdown */
    RemoveWatchers(READ_WATCHER);
  }
}

void
ConnectionOrientedSocketIO::OnSocketCanSendWithoutBlocking()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED); // see bug 990984
  MOZ_ASSERT(!IsShutdownOnIOThread());

  nsresult rv = SendPendingData(GetFd());
  if (NS_FAILED(rv)) {
    return;
  }

  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
ConnectionOrientedSocketIO::OnConnected()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_CONNECTED);

  GetConsumerThread()->PostTask(
    MakeAndAddRef<SocketEventTask>(this, SocketEventTask::CONNECT_SUCCESS));

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void
ConnectionOrientedSocketIO::OnListening()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  NS_NOTREACHED("Invalid call to |ConnectionOrientedSocketIO::OnListening|");
}

void
ConnectionOrientedSocketIO::OnError(const char* aFunction, int aErrno)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  UnixFdWatcher::OnError(aFunction, aErrno);

  // Clean up watchers, status, fd
  Close();

  // Tell the consumer thread we've errored
  GetConsumerThread()->PostTask(
    MakeAndAddRef<SocketEventTask>(this, SocketEventTask::CONNECT_ERROR));
}

//
// ConnectionOrientedSocket
//

ConnectionOrientedSocket::ConnectionOrientedSocket()
{
  MOZ_COUNT_CTOR_INHERITED(ConnectionOrientedSocket, DataSocket);
}

ConnectionOrientedSocket::~ConnectionOrientedSocket()
{
  MOZ_COUNT_DTOR_INHERITED(ConnectionOrientedSocket, DataSocket);
}

}
}
