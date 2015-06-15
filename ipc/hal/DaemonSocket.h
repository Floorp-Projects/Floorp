/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DaemonSocket_h
#define mozilla_ipc_DaemonSocket_h

#include "mozilla/ipc/ConnectionOrientedSocket.h"

namespace mozilla {
namespace ipc {

class DaemonSocketConsumer;
class DaemonSocketIO;
class DaemonSocketIOConsumer;

/**
 * |DaemonSocket| represents the socket to connect to the HAL daemon. It
 * offers connection establishment and sending PDUs. PDU receiving is
 * performed by |DaemonSocketIOConsumer|.
 */
class DaemonSocket : public ConnectionOrientedSocket
{
public:
  DaemonSocket(DaemonSocketIOConsumer* aIOConsumer,
               DaemonSocketConsumer* aConsumer,
               int aIndex);
  virtual ~DaemonSocket();

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

private:
  DaemonSocketIO* mIO;
  DaemonSocketIOConsumer* mIOConsumer;
  DaemonSocketConsumer* mConsumer;
  int mIndex;
};

}
}

#endif
