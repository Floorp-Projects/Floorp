/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_UnixSocket_h
#define mozilla_ipc_UnixSocket_h

#include <sys/socket.h>
#include <stdlib.h>
#include "nsString.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace ipc {

class UnixSocketRawData
{
public:
  nsAutoArrayPtr<uint8_t> mData;

  // Number of octets in mData.
  size_t mSize;
  size_t mCurrentWriteOffset;

  /**
   * Constructor for situations where size is known beforehand (for example,
   * when being assigned strings)
   *
   */
  UnixSocketRawData(int aSize) :
    mSize(aSize),
    mCurrentWriteOffset(0)
  {
    mData = new uint8_t[aSize];
  }
private:
  UnixSocketRawData() {}
};

class UnixSocketImpl;

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
   */
  virtual void CreateAddr(bool aIsServer,
                          socklen_t& aAddrSize,
                          struct sockaddr *aAddr,
                          const char* aAddress) = 0;

  /** 
   * Does any socket type specific setup that may be needed
   *
   * @param aFd File descriptor for opened socket
   *
   * @return true is successful, false otherwise
   */
  virtual bool SetUp(int aFd) = 0;

  /** 
   * Get address of socket we're currently connected to. Return null string if
   * not connected.
   *
   * @param aAddr Address struct
   * @param aAddrStr String to store address to
   */
  virtual void GetSocketAddr(const sockaddr& aAddr,
                             nsAString& aAddrStr) = 0;

};

enum SocketConnectionStatus {
  SOCKET_DISCONNECTED = 0,
  SOCKET_LISTENING = 1,
  SOCKET_CONNECTING = 2,
  SOCKET_CONNECTED = 3
};

class UnixSocketConsumer : public RefCounted<UnixSocketConsumer>
{
public:
  UnixSocketConsumer();

  virtual ~UnixSocketConsumer();

  SocketConnectionStatus GetConnectionStatus()
  {
    return mConnectionStatus;
  }

  /**
   * Function to be called whenever data is received. This is only called on the
   * main thread.
   *
   * @param aMessage Data received from the socket.
   */
  virtual void ReceiveSocketData(UnixSocketRawData* aMessage) = 0;

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
   *
   * @return true on connect task started, false otherwise.
   */
  bool ConnectSocket(UnixSocketConnector* aConnector, const char* aAddress);

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
   * Cancels connect/accept task loop, if one is currently running.
   */
  void CancelSocketTask();

  /** 
   * Callback for socket connect/accept success. Called after connect/accept has
   * finished. Will be run on main thread, before any reads take place.
   */
  virtual void OnConnectSuccess() = 0;

  /** 
   * Callback for socket connect/accept error. Will be run on main thread.
   */
  virtual void OnConnectError() = 0;

  /** 
   * Callback for socket disconnect. Will be run on main thread.
   */
  virtual void OnDisconnect() = 0;

  /** 
   * Called by implementation to notify consumer of success.
   */
  void NotifySuccess();

  /** 
   * Called by implementation to notify consumer of error.
   */
  void NotifyError();

  /** 
   * Called by implementation to notify consumer of disconnect.
   */
  void NotifyDisconnect();

  /**
   * Get the current sockaddr for the socket
   */
  void GetSocketAddr(nsAString& aAddrStr);

private:
  UnixSocketImpl* mImpl;
  SocketConnectionStatus mConnectionStatus;
};

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_Socket_h
