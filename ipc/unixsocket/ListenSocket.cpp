/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ListenSocket.h"
#include <fcntl.h>
#include "ConnectionOrientedSocket.h"
#include "DataSocket.h"
#include "mozilla/RefPtr.h"
#include "nsXULAppAPI.h"
#include "UnixSocketConnector.h"

namespace mozilla {
namespace ipc {

//
// ListenSocketIO
//

class ListenSocketIO final
  : public UnixSocketWatcher
  , public SocketIOBase
{
public:
  class ListenTask;

  ListenSocketIO(MessageLoop* mIOLoop,
                 ListenSocket* aListenSocket,
                 UnixSocketConnector* aConnector);
  ~ListenSocketIO();

  // Task callback methods
  //

  /**
   * Run bind/listen to prepare for further runs of accept()
   */
  void Listen(ConnectionOrientedSocketIO* aCOSocketIO);

  // I/O callback methods
  //

  void OnConnected() override;
  void OnError(const char* aFunction, int aErrno) override;
  void OnListening() override;
  void OnSocketCanAcceptWithoutBlocking() override;

  // Methods for |SocketIOBase|
  //

  SocketBase* GetSocketBase() override;

  bool IsShutdownOnMainThread() const override;
  bool IsShutdownOnIOThread() const override;

  void ShutdownOnMainThread() override;
  void ShutdownOnIOThread() override;

private:
  void FireSocketError();

  /**
   * Consumer pointer. Non-thread safe RefPtr, so should only be manipulated
   * directly from main thread. All non-main-thread accesses should happen with
   * mIO as container.
   */
  RefPtr<ListenSocket> mListenSocket;

  /**
   * Connector object used to create the connection we are currently using.
   */
  nsAutoPtr<UnixSocketConnector> mConnector;

  /**
   * If true, do not requeue whatever task we're running
   */
  bool mShuttingDownOnIOThread;

  /**
   * Number of valid bytes in |mAddress|
   */
  socklen_t mAddressLength;

  /**
   * Address structure of the socket currently in use
   */
  struct sockaddr_storage mAddress;

  ConnectionOrientedSocketIO* mCOSocketIO;
};

ListenSocketIO::ListenSocketIO(MessageLoop* mIOLoop,
                               ListenSocket* aListenSocket,
                               UnixSocketConnector* aConnector)
  : UnixSocketWatcher(mIOLoop)
  , SocketIOBase()
  , mListenSocket(aListenSocket)
  , mConnector(aConnector)
  , mShuttingDownOnIOThread(false)
  , mAddressLength(0)
  , mCOSocketIO(nullptr)
{
  MOZ_ASSERT(mListenSocket);
  MOZ_ASSERT(mConnector);
}

ListenSocketIO::~ListenSocketIO()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsShutdownOnMainThread());
}

void
ListenSocketIO::Listen(ConnectionOrientedSocketIO* aCOSocketIO)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(mConnector);
  MOZ_ASSERT(aCOSocketIO);

  struct sockaddr* address = reinterpret_cast<struct sockaddr*>(&mAddress);
  mAddressLength = sizeof(mAddress);

  if (!IsOpen()) {
    int fd;
    nsresult rv = mConnector->CreateListenSocket(address, &mAddressLength,
                                                 fd);
    if (NS_FAILED(rv)) {
      FireSocketError();
      return;
    }
    SetFd(fd);
  }

  mCOSocketIO = aCOSocketIO;

  // calls OnListening on success, or OnError otherwise
  nsresult rv = UnixSocketWatcher::Listen(address, mAddressLength);
  NS_WARN_IF(NS_FAILED(rv));
}

void
ListenSocketIO::OnConnected()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  NS_NOTREACHED("Invalid call to |ListenSocketIO::OnConnected|");
}

void
ListenSocketIO::OnListening()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_LISTENING);

  AddWatchers(READ_WATCHER, true);

  /* We signal a successful 'connection' to a local address for listening. */
  NS_DispatchToMainThread(
    new SocketIOEventRunnable(this, SocketIOEventRunnable::CONNECT_SUCCESS));
}

void
ListenSocketIO::OnError(const char* aFunction, int aErrno)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  UnixFdWatcher::OnError(aFunction, aErrno);
  FireSocketError();
}

void
ListenSocketIO::FireSocketError()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());

  // Clean up watchers, statuses, fds
  Close();

  // Tell the main thread we've errored
  NS_DispatchToMainThread(
    new SocketIOEventRunnable(this, SocketIOEventRunnable::CONNECT_ERROR));
}

void
ListenSocketIO::OnSocketCanAcceptWithoutBlocking()
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_LISTENING);
  MOZ_ASSERT(mCOSocketIO);

  RemoveWatchers(READ_WATCHER|WRITE_WATCHER);

  struct sockaddr_storage storage;
  socklen_t addressLength = sizeof(storage);

  int fd;
  nsresult rv = mConnector->AcceptStreamSocket(
    GetFd(),
    reinterpret_cast<struct sockaddr*>(&storage), &addressLength,
    fd);
  if (NS_FAILED(rv)) {
    FireSocketError();
    return;
  }

  mCOSocketIO->Accept(fd,
                      reinterpret_cast<union sockaddr_any*>(&storage),
                      addressLength);
}

// |SocketIOBase|

SocketBase*
ListenSocketIO::GetSocketBase()
{
  return mListenSocket.get();
}

bool
ListenSocketIO::IsShutdownOnMainThread() const
{
  MOZ_ASSERT(NS_IsMainThread());

  return mListenSocket == nullptr;
}

bool
ListenSocketIO::IsShutdownOnIOThread() const
{
  return mShuttingDownOnIOThread;
}

void
ListenSocketIO::ShutdownOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdownOnMainThread());

  mListenSocket = nullptr;
}

void
ListenSocketIO::ShutdownOnIOThread()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  Close(); // will also remove fd from I/O loop
  mShuttingDownOnIOThread = true;
}

//
// Socket tasks
//

class ListenSocketIO::ListenTask final
  : public SocketIOTask<ListenSocketIO>
{
public:
  ListenTask(ListenSocketIO* aIO, ConnectionOrientedSocketIO* aCOSocketIO)
  : SocketIOTask<ListenSocketIO>(aIO)
  , mCOSocketIO(aCOSocketIO)
  {
    MOZ_ASSERT(mCOSocketIO);
  }

  void Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());

    if (!IsCanceled()) {
      GetIO()->Listen(mCOSocketIO);
    }
  }

private:
  ConnectionOrientedSocketIO* mCOSocketIO;
};

//
// UnixSocketConsumer
//

ListenSocket::ListenSocket()
: mIO(nullptr)
{ }

ListenSocket::~ListenSocket()
{
  MOZ_ASSERT(!mIO);
}

nsresult
ListenSocket::Listen(UnixSocketConnector* aConnector,
                     ConnectionOrientedSocket* aCOSocket)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIO);

  mIO = new ListenSocketIO(XRE_GetIOMessageLoop(), this, aConnector);

  // Prepared I/O object, now start listening.
  return Listen(aCOSocket);
}

nsresult
ListenSocket::Listen(ConnectionOrientedSocket* aCOSocket)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCOSocket);
  MOZ_ASSERT(mIO);

  SetConnectionStatus(SOCKET_LISTENING);

  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE, new ListenSocketIO::ListenTask(mIO, aCOSocket->GetIO()));

  return NS_OK;
}

// |SocketBase|

void
ListenSocket::Close()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mIO) {
    return;
  }

  // From this point on, we consider mIO as being deleted. We sever
  // the relationship here so any future calls to listen or connect
  // will create a new implementation.
  mIO->ShutdownOnMainThread();

  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new SocketIOShutdownTask(mIO));

  mIO = nullptr;

  NotifyDisconnect();
}

} // namespace ipc
} // namespace mozilla
