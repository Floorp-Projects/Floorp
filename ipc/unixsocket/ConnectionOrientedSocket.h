/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_connectionorientedsocket_h
#define mozilla_ipc_connectionorientedsocket_h

#include <sys/socket.h>
#include "DataSocket.h"

class MessageLoop;

namespace mozilla {
namespace ipc {

class UnixSocketConnector;

/*
 * |ConnectionOrientedSocketIO| and |ConnectionOrientedSocket| define
 * interfaces for implementing stream sockets on I/O and main thread.
 * |ListenSocket| uses these classes to handle accepted sockets.
 */

class ConnectionOrientedSocketIO : public DataSocketIO
{
public:
  virtual ~ConnectionOrientedSocketIO();

  virtual nsresult Accept(int aFd,
                          const struct sockaddr* aAddress,
                          socklen_t aAddressLength) = 0;
};

class ConnectionOrientedSocket : public DataSocket
{
public:
  /**
   * Prepares an instance of |ConnectionOrientedSocket| in DISCONNECTED
   * state for accepting a connection. Main-thread only.
   *
   * @param aConnector The new connector object, owned by the
   *                   connection-oriented socket.
   * @param aIOLoop The socket's I/O thread.
   * @param[out] aIO, Returns an instance of |ConnectionOrientedSocketIO|.
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  virtual nsresult PrepareAccept(UnixSocketConnector* aConnector,
                                 MessageLoop* aIOLoop,
                                 ConnectionOrientedSocketIO*& aIO) = 0;

protected:
  virtual ~ConnectionOrientedSocket();
};

}
}

#endif
