/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_unixsocket_h
#define mozilla_ipc_unixsocket_h

#include <stdlib.h>
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "mozilla/ipc/SocketBase.h"
#include "mozilla/ipc/UnixSocketWatcher.h"
#include "mozilla/RefPtr.h"
#include "UnixSocketConnector.h"

namespace mozilla {
namespace ipc {

class UnixSocketConsumerIO;

class UnixSocketConsumer : public SocketConsumerBase
{
protected:
  virtual ~UnixSocketConsumer();

public:
  UnixSocketConsumer();

  /**
   * Queue data to be sent to the socket on the IO thread. Can only be called on
   * originating thread.
   *
   * @param aMessage Data to be sent to socket
   *
   * @return true if data is queued, false otherwise (i.e. not connected)
   */
  bool SendSocketData(UnixSocketRawData* aMessage);

  /**
   * Convenience function for sending strings to the socket (common in bluetooth
   * profile usage). Converts to a UnixSocketRawData struct. Can only be called
   * on originating thread.
   *
   * @param aMessage String to be sent to socket
   *
   * @return true if data is queued, false otherwise (i.e. not connected)
   */
  bool SendSocketData(const nsACString& aMessage);

  /**
   * Starts a task on the socket that will try to connect to a socket in a
   * non-blocking manner.
   *
   * @param aConnector Connector object for socket type specific functions
   * @param aAddress Address to connect to.
   * @param aDelayMs Time delay in milli-seconds.
   *
   * @return true on connect task started, false otherwise.
   */
  bool ConnectSocket(UnixSocketConnector* aConnector,
                     const char* aAddress,
                     int aDelayMs = 0);

  /**
   * Starts a task on the socket that will try to accept a new connection in a
   * non-blocking manner.
   *
   * @param aConnector Connector object for socket type specific functions
   *
   * @return true on listen started, false otherwise
   */
  bool ListenSocket(UnixSocketConnector* aConnector);

  /**
   * Queues the internal representation of socket for deletion. Can be called
   * from main thread.
   */
  void CloseSocket();

  /**
   * Get the current sockaddr for the socket
   */
  void GetSocketAddr(nsAString& aAddrStr);

private:
  UnixSocketConsumerIO* mIO;
};

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_unixsocket_h
