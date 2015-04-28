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

class ListenSocketIO final : public UnixSocketWatcher
                           , protected SocketIOBase
{
public:
  class ListenTask;

  ListenSocketIO(MessageLoop* mIOLoop,
                 ListenSocket* aListenSocket,
                 UnixSocketConnector* aConnector,
                 const nsACString& aAddress);
  ~ListenSocketIO();

  void        GetSocketAddr(nsAString& aAddrStr) const;
  DataSocket* GetDataSocket();
  SocketBase* GetSocketBase();

  // Shutdown state
  //

  bool IsShutdownOnMainThread() const;
  void ShutdownOnMainThread();

  bool IsShutdownOnIOThread() const;
  void ShutdownOnIOThread();

  // Task callback methods
  //

  /**
   * Run bind/listen to prepare for further runs of accept()
   */
  void Listen(ConnectionOrientedSocketIO* aCOSocketIO);

  // I/O callback methods
  //

  void OnAccepted(int aFd, const sockaddr_any* aAddr,
                  socklen_t aAddrLen) override;
  void OnConnected() override;
  void OnError(const char* aFunction, int aErrno) override;
  void OnListening() override;

private:
  void FireSocketError();

  // Set up flags on file descriptor.
  static bool SetSocketFlags(int aFd);

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
   * Address we are connecting to, assuming we are creating a client connection.
   */
  nsCString mAddress;

  /**
   * Size of the socket address struct
   */
  socklen_t mAddrSize;

  /**
   * Address struct of the socket currently in use
   */
  sockaddr_any mAddr;

  ConnectionOrientedSocketIO* mCOSocketIO;
};

ListenSocketIO::ListenSocketIO(MessageLoop* mIOLoop,
                               ListenSocket* aListenSocket,
                               UnixSocketConnector* aConnector,
                               const nsACString& aAddress)
  : UnixSocketWatcher(mIOLoop)
  , SocketIOBase()
  , mListenSocket(aListenSocket)
  , mConnector(aConnector)
  , mShuttingDownOnIOThread(false)
  , mAddress(aAddress)
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
ListenSocketIO::GetSocketAddr(nsAString& aAddrStr) const
{
  if (!mConnector) {
    NS_WARNING("No connector to get socket address from!");
    aAddrStr.Truncate();
    return;
  }
  mConnector->GetSocketAddr(mAddr, aAddrStr);
}

DataSocket*
ListenSocketIO::GetDataSocket()
{
  MOZ_CRASH("Listen sockets cannot transfer data");

  return nullptr;
}

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

void
ListenSocketIO::ShutdownOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdownOnMainThread());

  mListenSocket = nullptr;
}

bool
ListenSocketIO::IsShutdownOnIOThread() const
{
  return mShuttingDownOnIOThread;
}

void
ListenSocketIO::ShutdownOnIOThread()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  Close(); // will also remove fd from I/O loop
  mShuttingDownOnIOThread = true;
}

void
ListenSocketIO::Listen(ConnectionOrientedSocketIO* aCOSocketIO)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(mConnector);
  MOZ_ASSERT(aCOSocketIO);

  if (!IsOpen()) {
    int fd = mConnector->Create();
    if (fd < 0) {
      NS_WARNING("Cannot create socket fd!");
      FireSocketError();
      return;
    }
    if (!SetSocketFlags(fd)) {
      NS_WARNING("Cannot set socket flags!");
      FireSocketError();
      return;
    }
    SetFd(fd);
  }

  mCOSocketIO = aCOSocketIO;

  // This will set things we don't particularly care about, but
  // it will hand back the correct structure size which is what
  // we do care about.
  if (!mConnector->CreateAddr(true, mAddrSize, mAddr, nullptr)) {
    NS_WARNING("Cannot create socket address!");
    FireSocketError();
    return;
  }

  // calls OnListening on success, or OnError otherwise
  nsresult rv = UnixSocketWatcher::Listen(
    reinterpret_cast<struct sockaddr*>(&mAddr), mAddrSize);
  NS_WARN_IF(NS_FAILED(rv));
}

void
ListenSocketIO::OnAccepted(int aFd,
                           const sockaddr_any* aAddr,
                           socklen_t aAddrLen)
{
  MOZ_ASSERT(MessageLoopForIO::current() == GetIOLoop());
  MOZ_ASSERT(GetConnectionStatus() == SOCKET_IS_LISTENING);
  MOZ_ASSERT(mCOSocketIO);

  RemoveWatchers(READ_WATCHER|WRITE_WATCHER);

  mCOSocketIO->Accept(aFd, aAddr, aAddrLen);
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

  if (!mConnector->SetUpListenSocket(GetFd())) {
    NS_WARNING("Could not set up listen socket!");
    FireSocketError();
    return;
  }

  AddWatchers(READ_WATCHER, true);

  /* We signal a successful 'connection' to a local address for listening. */
  nsRefPtr<nsRunnable> runnable =
      new SocketIOEventRunnable<ListenSocketIO>(
        this, SocketIOEventRunnable<ListenSocketIO>::CONNECT_SUCCESS);
  NS_DispatchToMainThread(runnable);
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
  nsRefPtr<nsRunnable> r =
    new SocketIOEventRunnable<ListenSocketIO>(
      this, SocketIOEventRunnable<ListenSocketIO>::CONNECT_ERROR);

  NS_DispatchToMainThread(r);
}

bool
ListenSocketIO::SetSocketFlags(int aFd)
{
  static const int reuseaddr = 1;

  // Set socket addr to be reused even if kernel is still waiting to close
  int res = setsockopt(aFd, SOL_SOCKET, SO_REUSEADDR,
                       &reuseaddr, sizeof(reuseaddr));
  if (res < 0) {
    return false;
  }

  // Set close-on-exec bit.
  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFD));
  if (-1 == flags) {
    return false;
  }
  flags |= FD_CLOEXEC;
  if (-1 == TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFD, flags))) {
    return false;
  }

  // Set non-blocking status flag.
  flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  if (-1 == flags) {
    return false;
  }
  flags |= O_NONBLOCK;
  if (-1 == TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags))) {
    return false;
  }

  return true;
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

  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE, new SocketIOShutdownTask<ListenSocketIO>(mIO));

  mIO = nullptr;

  NotifyDisconnect();
}

bool
ListenSocket::Listen(UnixSocketConnector* aConnector,
                     ConnectionOrientedSocket* aCOSocket)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aConnector);
  MOZ_ASSERT(aCOSocket);

  nsAutoPtr<UnixSocketConnector> connector(aConnector);

  if (mIO) {
    NS_WARNING("Socket already connecting/connected!");
    return false;
  }

  mIO = new ListenSocketIO(
    XRE_GetIOMessageLoop(), this, connector.forget(), EmptyCString());

  // Prepared I/O object, now start listening.
  return Listen(aCOSocket);
}

bool
ListenSocket::Listen(ConnectionOrientedSocket* aCOSocket)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mIO);
  MOZ_ASSERT(aCOSocket);

  SetConnectionStatus(SOCKET_LISTENING);

  XRE_GetIOMessageLoop()->PostTask(
    FROM_HERE, new ListenSocketIO::ListenTask(mIO, aCOSocket->GetIO()));

  return true;
}

void
ListenSocket::GetSocketAddr(nsAString& aAddrStr)
{
  aAddrStr.Truncate();
  if (!mIO || GetConnectionStatus() != SOCKET_CONNECTED) {
    NS_WARNING("No socket currently open!");
    return;
  }
  mIO->GetSocketAddr(aAddrStr);
}

} // namespace ipc
} // namespace mozilla
