/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_listensocket_h
#define mozilla_ipc_listensocket_h

#include "nsString.h"
#include "mozilla/ipc/SocketBase.h"

namespace mozilla {
namespace ipc {

class ConnectionOrientedSocket;
class ListenSocketIO;
class UnixSocketConnector;

class ListenSocket : public SocketBase
{
protected:
  virtual ~ListenSocket();

public:
  ListenSocket();

  /**
   * Starts a task on the socket that will try to accept a new connection
   * in a non-blocking manner.
   *
   * @param aConnector Connector object for socket-type-specific functions
   * @param aCOSocket The connection-oriented socket for handling the
   *                  accepted connection.
   *
   * @return true on listen started, false otherwise
   */
  bool Listen(UnixSocketConnector* aConnector,
              ConnectionOrientedSocket* aCOSocket);

  /**
   * Starts a task on the socket that will try to accept a new connection
   * in a non-blocking manner. This method re-uses a previously created
   * listen socket.
   *
   * @param aCOSocket The connection-oriented socket for handling the
   *                  accepted connection.
   *
   * @return true on listen started, false otherwise
   */
  bool Listen(ConnectionOrientedSocket* aCOSocket);

  /**
   * Queues the internal representation of socket for deletion. Can be called
   * from main thread.
   */
  void Close();

  /**
   * Get the current sockaddr for the socket
   */
  void GetSocketAddr(nsAString& aAddrStr);

  // Methods for |SocketBase|
  //

  void CloseSocket() override;

private:
  ListenSocketIO* mIO;
};

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_listensocket_h
