/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_connectionorientedsocket_h
#define mozilla_ipc_connectionorientedsocket_h

#include <sys/socket.h>
#include "DataSocket.h"

namespace mozilla {
namespace ipc {

union sockaddr_any;

/*
 * |ConnectionOrientedSocketIO| and |ConnectionOrientedSocket| define
 * interfaces for implementing stream sockets on I/O and main thread.
 * |ListenSocket| uses these classes to handle accepted sockets.
 */

class ConnectionOrientedSocketIO : public DataSocketIO
{
public:
  virtual nsresult Accept(int aFd,
                          const union sockaddr_any* aAddr,
                          socklen_t aAddrLen) = 0;

protected:
  virtual ~ConnectionOrientedSocketIO();
};

class ConnectionOrientedSocket : public DataSocket
{
public:
  virtual ConnectionOrientedSocketIO* GetIO() = 0;

protected:
  virtual ~ConnectionOrientedSocket();
};

}
}

#endif
