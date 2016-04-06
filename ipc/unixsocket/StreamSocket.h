/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_streamsocket_h
#define mozilla_ipc_streamsocket_h

#include "ConnectionOrientedSocket.h"

class MessageLoop;

namespace mozilla {
namespace ipc {

class StreamSocketConsumer;
class StreamSocketIO;
class UnixSocketConnector;

class StreamSocket final : public ConnectionOrientedSocket
{
public:
  /**
   * Constructs an instance of |StreamSocket|.
   *
   * @param aConsumer The consumer for the socket.
   * @param aIndex An arbitrary index.
   */
  StreamSocket(StreamSocketConsumer* aConsumer, int aIndex);

  /**
   * Method to be called whenever data is received. Consumer-thread only.
   *
   * @param aBuffer Data received from the socket.
   */
  void ReceiveSocketData(UniquePtr<UnixSocketBuffer>& aBuffer);

  /**
   * Starts a task on the socket that will try to connect to a socket in a
   * non-blocking manner.
   *
   * @param aConnector Connector object for socket type specific functions
   * @param aDelayMs Time delay in milliseconds.
   * @param aConsumerLoop The socket's consumer thread.
   * @param aIOLoop The socket's I/O thread.
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  nsresult Connect(UnixSocketConnector* aConnector, int aDelayMs,
                   MessageLoop* aConsumerLoop, MessageLoop* aIOLoop);

  /**
   * Starts a task on the socket that will try to connect to a socket in a
   * non-blocking manner.
   *
   * @param aConnector Connector object for socket type specific functions
   * @param aDelayMs Time delay in milliseconds.
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  nsresult Connect(UnixSocketConnector* aConnector, int aDelayMs = 0);

  // Methods for |ConnectionOrientedSocket|
  //

  nsresult PrepareAccept(UnixSocketConnector* aConnector,
                         MessageLoop* aConsumerLoop,
                         MessageLoop* aIOLoop,
                         ConnectionOrientedSocketIO*& aIO) override;

  // Methods for |DataSocket|
  //

  void SendSocketData(UnixSocketIOBuffer* aBuffer) override;

  // Methods for |SocketBase|
  //

  void Close() override;
  void OnConnectSuccess() override;
  void OnConnectError() override;
  void OnDisconnect() override;

protected:
  virtual ~StreamSocket();

private:
  StreamSocketIO* mIO;
  StreamSocketConsumer* mConsumer;
  int mIndex;
};

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_streamsocket_h
