/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConnectionOrientedSocket.h"

namespace mozilla {
namespace ipc {

//
// ConnectionOrientedSocketIO
//

ConnectionOrientedSocketIO::ConnectionOrientedSocketIO(
  nsIThread* aConsumerThread,
  MessageLoop* aIOLoop,
  int aFd,
  ConnectionStatus aConnectionStatus)
  : DataSocketIO(aConsumerThread)
  , UnixSocketWatcher(aIOLoop, aFd, aConnectionStatus)
{ }

ConnectionOrientedSocketIO::ConnectionOrientedSocketIO(
  nsIThread* aConsumerThread,
  MessageLoop* aIOLoop)
  : DataSocketIO(aConsumerThread)
  , UnixSocketWatcher(aIOLoop)
{ }

ConnectionOrientedSocketIO::~ConnectionOrientedSocketIO()
{ }

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

  GetConsumerThread()->Dispatch(
    new SocketIOEventRunnable(this, SocketIOEventRunnable::CONNECT_SUCCESS),
    NS_DISPATCH_NORMAL);

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
  GetConsumerThread()->Dispatch(
    new SocketIOEventRunnable(this, SocketIOEventRunnable::CONNECT_ERROR),
    NS_DISPATCH_NORMAL);
}

//
// ConnectionOrientedSocket
//

ConnectionOrientedSocket::~ConnectionOrientedSocket()
{ }

}
}
