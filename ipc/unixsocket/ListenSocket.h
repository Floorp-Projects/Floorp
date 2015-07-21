/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ListenSocket_h
#define mozilla_ipc_ListenSocket_h

#include "mozilla/ipc/SocketBase.h"
#include "nsString.h"

class MessageLoop;

namespace mozilla {
namespace ipc {

class ConnectionOrientedSocket;
class ListenSocketConsumer;
class ListenSocketIO;
class UnixSocketConnector;

class ListenSocket final : public SocketBase
{
public:
  /**
   * Constructs an instance of |ListenSocket|.
   *
   * @param aConsumer The consumer for the socket.
   * @param aIndex An arbitrary index.
   */
  ListenSocket(ListenSocketConsumer* aConsumer, int aIndex);

  /**
   * Starts a task on the socket that will try to accept a new connection
   * in a non-blocking manner.
   *
   * @param aConnector Connector object for socket-type-specific functions
   * @param aConsumerLoop The socket's consumer thread.
   * @param aIOLoop The socket's I/O thread.
   * @param aCOSocket The connection-oriented socket for handling the
   *                  accepted connection.
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  nsresult Listen(UnixSocketConnector* aConnector,
                  MessageLoop* aConsumerLoop,
                  MessageLoop* aIOLoop,
                  ConnectionOrientedSocket* aCOSocket);

  /**
   * Starts a task on the socket that will try to accept a new connection
   * in a non-blocking manner.
   *
   * @param aConnector Connector object for socket-type-specific functions
   * @param aCOSocket The connection-oriented socket for handling the
   *                  accepted connection.
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  nsresult Listen(UnixSocketConnector* aConnector,
                  ConnectionOrientedSocket* aCOSocket);

  /**
   * Starts a task on the socket that will try to accept a new connection
   * in a non-blocking manner. This method re-uses a previously created
   * listen socket.
   *
   * @param aCOSocket The connection-oriented socket for handling the
   *                  accepted connection.
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  nsresult Listen(ConnectionOrientedSocket* aCOSocket);

  // Methods for |SocketBase|
  //

  void Close() override;
  void OnConnectSuccess() override;
  void OnConnectError() override;
  void OnDisconnect() override;

protected:
  virtual ~ListenSocket();

private:
  ListenSocketIO* mIO;
  ListenSocketConsumer* mConsumer;
  int mIndex;
};

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_ListenSocket_h
