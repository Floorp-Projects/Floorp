/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ConnectionOrientedSocket_h
#define mozilla_ipc_ConnectionOrientedSocket_h

#include <sys/socket.h>
#include "DataSocket.h"
#include "mozilla/ipc/UnixSocketWatcher.h"

class MessageLoop;

namespace mozilla {
namespace ipc {

class UnixSocketConnector;

/*
 * |ConnectionOrientedSocketIO| and |ConnectionOrientedSocket| define
 * interfaces for implementing stream sockets on I/O and consumer thread.
 * |ListenSocket| uses these classes to handle accepted sockets.
 */

class ConnectionOrientedSocketIO
  : public DataSocketIO
  , public UnixSocketWatcher
{
public:
  virtual ~ConnectionOrientedSocketIO();

  nsresult Accept(int aFd,
                  const struct sockaddr* aAddress,
                  socklen_t aAddressLength);

  nsresult Connect();

  void Send(UnixSocketIOBuffer* aBuffer);

  // Methods for |UnixSocketWatcher|
  //

  void OnSocketCanReceiveWithoutBlocking() final;
  void OnSocketCanSendWithoutBlocking() final;

  void OnListening() final;
  void OnConnected() final;
  void OnError(const char* aFunction, int aErrno) final;

protected:
  /**
   * Constructs an instance of |ConnectionOrientedSocketIO|
   *
   * @param aConsumerLoop The socket's consumer thread.
   * @param aIOLoop The socket's I/O loop.
   * @param aFd The socket file descriptor.
   * @param aConnectionStatus The connection status for |aFd|.
   * @param aConnector Connector object for socket-type-specific methods.
   */
  ConnectionOrientedSocketIO(MessageLoop* aConsumerLoop,
                             MessageLoop* aIOLoop,
                             int aFd, ConnectionStatus aConnectionStatus,
                             UnixSocketConnector* aConnector);

  /**
   * Constructs an instance of |ConnectionOrientedSocketIO|
   *
   * @param aConsumerLoop The socket's consumer thread.
   * @param aIOLoop The socket's I/O loop.
   * @param aConnector Connector object for socket-type-specific methods.
   */
  ConnectionOrientedSocketIO(MessageLoop* aConsumerLoop,
                             MessageLoop* aIOLoop,
                             UnixSocketConnector* aConnector);

private:
  /**
   * Connector object used to create the connection we are currently using.
   */
  UniquePtr<UnixSocketConnector> mConnector;

  /**
   * Number of valid bytes in |mPeerAddress|.
   */
  socklen_t mPeerAddressLength;

  /**
   * Address of the socket's current peer.
   */
  struct sockaddr_storage mPeerAddress;
};

class ConnectionOrientedSocket : public DataSocket
{
public:
  /**
   * Prepares an instance of |ConnectionOrientedSocket| in DISCONNECTED
   * state for accepting a connection. Consumer-thread only.
   *
   * @param aConnector The new connector object, owned by the
   *                   connection-oriented socket.
   * @param aConsumerLoop The socket's consumer thread.
   * @param aIOLoop The socket's I/O thread.
   * @param[out] aIO, Returns an instance of |ConnectionOrientedSocketIO|.
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  virtual nsresult PrepareAccept(UnixSocketConnector* aConnector,
                                 MessageLoop* aConsumerLoop,
                                 MessageLoop* aIOLoop,
                                 ConnectionOrientedSocketIO*& aIO) = 0;

protected:
  ConnectionOrientedSocket();
  virtual ~ConnectionOrientedSocket();
};

}
}

#endif // mozilla_ipc_ConnectionOrientedSocket
