/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_RilSocket_h
#define mozilla_ipc_RilSocket_h

#include "mozilla/ipc/ConnectionOrientedSocket.h"

class JSContext;
class MessageLoop;

namespace mozilla {
namespace dom {
namespace workers {

class WorkerCrossThreadDispatcher;

} // namespace workers
} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace ipc {

class RilSocketConsumer;
class RilSocketIO;
class UnixSocketConnector;

class RilSocket final : public ConnectionOrientedSocket
{
public:
  /**
   * Constructs an instance of |RilSocket|.
   *
   * @param aDispatcher The dispatcher class for the received messages.
   * @param aConsumer The consumer for the socket.
   * @param aIndex An arbitrary index.
   */
  RilSocket(mozilla::dom::workers::WorkerCrossThreadDispatcher* aDispatcher,
            RilSocketConsumer* aConsumer, int aIndex);

  /**
   * Method to be called whenever data is received. RIL-worker only.
   *
   * @param aCx The RIL worker's JS context.
   * @param aBuffer Data received from the socket.
   */
  void ReceiveSocketData(JSContext* aCx, nsAutoPtr<UnixSocketBuffer>& aBuffer);

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
  virtual ~RilSocket();

private:
  RilSocketIO* mIO;
  nsRefPtr<mozilla::dom::workers::WorkerCrossThreadDispatcher> mDispatcher;
  RilSocketConsumer* mConsumer;
  int mIndex;
};

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_RilSocket_h
