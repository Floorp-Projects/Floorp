/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include "UnixSocketWatcher.h"

namespace mozilla {
namespace ipc {

UnixSocketWatcher::~UnixSocketWatcher()
{
}

void UnixSocketWatcher::Close()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  mConnectionStatus = SOCKET_IS_DISCONNECTED;
  UnixFdWatcher::Close();
}

nsresult
UnixSocketWatcher::Connect(const struct sockaddr* aAddr, socklen_t aAddrLen)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(IsOpen());
  MOZ_ASSERT(aAddr || !aAddrLen);

  if (connect(GetFd(), aAddr, aAddrLen) < 0) {
    if (errno == EINPROGRESS) {
      mConnectionStatus = SOCKET_IS_CONNECTING;
      // Set up a write watch to receive the connect signal
      AddWatchers(WRITE_WATCHER, false);
    } else {
      OnError("connect", errno);
    }
    return NS_ERROR_FAILURE;
  }

  mConnectionStatus = SOCKET_IS_CONNECTED;
  OnConnected();

  return NS_OK;
}

nsresult
UnixSocketWatcher::Listen(const struct sockaddr* aAddr, socklen_t aAddrLen)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(IsOpen());
  MOZ_ASSERT(aAddr || !aAddrLen);

  if (bind(GetFd(), aAddr, aAddrLen) < 0) {
    OnError("bind", errno);
    return NS_ERROR_FAILURE;
  }
  if (listen(GetFd(), 1) < 0) {
    OnError("listen", errno);
    return NS_ERROR_FAILURE;
  }
  mConnectionStatus = SOCKET_IS_LISTENING;
  OnListening();

  return NS_OK;
}

UnixSocketWatcher::UnixSocketWatcher(MessageLoop* aIOLoop)
: UnixFdWatcher(aIOLoop)
, mConnectionStatus(SOCKET_IS_DISCONNECTED)
{
}

UnixSocketWatcher::UnixSocketWatcher(MessageLoop* aIOLoop, int aFd,
                                     ConnectionStatus aConnectionStatus)
: UnixFdWatcher(aIOLoop, aFd)
, mConnectionStatus(aConnectionStatus)
{
}

void
UnixSocketWatcher::SetSocket(int aFd, ConnectionStatus aConnectionStatus)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  SetFd(aFd);
  mConnectionStatus = aConnectionStatus;
}

void
UnixSocketWatcher::OnFileCanReadWithoutBlocking(int aFd)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(aFd == GetFd());

  if (mConnectionStatus == SOCKET_IS_CONNECTED) {
    OnSocketCanReceiveWithoutBlocking();
  } else if (mConnectionStatus == SOCKET_IS_LISTENING) {
    sockaddr_any addr;
    socklen_t addrLen = sizeof(addr);
    int fd = TEMP_FAILURE_RETRY(accept(GetFd(),
      reinterpret_cast<struct sockaddr*>(&addr), &addrLen));
    if (fd < 0) {
      OnError("accept", errno);
    } else {
      OnAccepted(fd, &addr, addrLen);
    }
  } else {
    NS_NOTREACHED("invalid connection state for reading");
  }
}

void
UnixSocketWatcher::OnFileCanWriteWithoutBlocking(int aFd)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(aFd == GetFd());

  if (mConnectionStatus == SOCKET_IS_CONNECTED) {
    OnSocketCanSendWithoutBlocking();
  } else if (mConnectionStatus == SOCKET_IS_CONNECTING) {
    RemoveWatchers(WRITE_WATCHER);
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(GetFd(), SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
      OnError("getsockopt", errno);
    } else if (error) {
      OnError("connect", error);
    } else {
      mConnectionStatus = SOCKET_IS_CONNECTED;
      OnConnected();
    }
  } else {
    NS_NOTREACHED("invalid connection state for writing");
  }
}

}
}
