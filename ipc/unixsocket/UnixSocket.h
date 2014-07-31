/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_UnixSocket_h
#define mozilla_ipc_UnixSocket_h

#include <stdlib.h>
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "mozilla/ipc/UnixSocketWatcher.h"
#include "mozilla/ipc/SocketBase.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace ipc {

class UnixSocketConsumerIO;

/**
 * UnixSocketConnector defines the socket creation and connection/listening
 * functions for a UnixSocketConsumer. Due to the fact that socket setup can
 * vary between protocols (unix sockets, tcp sockets, bluetooth sockets, etc),
 * this allows the user to create whatever connection mechanism they need while
 * still depending on libevent for non-blocking communication handling.
 *
 * FIXME/Bug 793980: Currently only virtual, since we only support bluetooth.
 * Should make connection functions for other unix sockets so we can support
 * things like RIL.
 */
class UnixSocketConnector
{
public:
  UnixSocketConnector()
  {}

  virtual ~UnixSocketConnector()
  {}

  /**
   * Establishs a file descriptor for a socket.
   *
   * @return File descriptor for socket
   */
  virtual int Create() = 0;

  /**
   * Since most socket specifics are related to address formation into a
   * sockaddr struct, this function is defined by subclasses and fills in the
   * structure as needed for whatever connection it is trying to build
   *
   * @param aIsServer True is we are acting as a server socket
   * @param aAddrSize Size of the struct
   * @param aAddr Struct to fill
   * @param aAddress If aIsServer is false, Address to connect to. nullptr otherwise.
   *
   * @return True if address is filled correctly, false otherwise
   */
  virtual bool CreateAddr(bool aIsServer,
                          socklen_t& aAddrSize,
                          sockaddr_any& aAddr,
                          const char* aAddress) = 0;

  /**
   * Does any socket type specific setup that may be needed, only for socket
   * created by ConnectSocket()
   *
   * @param aFd File descriptor for opened socket
   *
   * @return true is successful, false otherwise
   */
  virtual bool SetUp(int aFd) = 0;

  /**
   * Perform socket setup for socket created by ListenSocket(), after listen().
   *
   * @param aFd File descriptor for opened socket
   *
   * @return true is successful, false otherwise
   */
  virtual bool SetUpListenSocket(int aFd) = 0;

  /**
   * Get address of socket we're currently connected to. Return null string if
   * not connected.
   *
   * @param aAddr Address struct
   * @param aAddrStr String to store address to
   */
  virtual void GetSocketAddr(const sockaddr_any& aAddr,
                             nsAString& aAddrStr) = 0;

};

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

#endif // mozilla_ipc_Socket_h
