/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_unixsocketconnector_h
#define mozilla_ipc_unixsocketconnector_h

#include <sys/socket.h>
#include "mozilla/ipc/UnixSocketWatcher.h"
#include "nsString.h"

namespace mozilla {
namespace ipc {

/**
 * |UnixSocketConnector| defines the socket creation and connection/listening
 * functions for |UnixSocketConsumer|, et al. Due to the fact that socket setup
 * can vary between protocols (Unix sockets, TCP sockets, Bluetooth sockets, etc),
 * this allows the user to create whatever connection mechanism they need while
 * still depending on libevent for non-blocking communication handling.
 */
class UnixSocketConnector
{
public:
  virtual ~UnixSocketConnector();

  /**
   * Converts an address to a human-readable string.
   *
   * @param aAddress A socket address
   * @param aAddressLength The number of valid bytes in |aAddress|
   * @param[out] aAddressString The resulting string
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  virtual nsresult ConvertAddressToString(const struct sockaddr& aAddress,
                                          socklen_t aAddressLength,
                                          nsACString& aAddressString);

  /**
   * Creates a listening socket. I/O thread only.
   *
   * @param[out] aAddress The listening socket's address
   * @param[out] aAddressLength The number of valid bytes in |aAddress|
   * @param[out] aListenFd The socket's file descriptor
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  virtual nsresult CreateListenSocket(struct sockaddr* aAddress,
                                      socklen_t* aAddressLength,
                                      int& aListenFd);

  /**
   * Accepts a stream socket from a listening socket. I/O thread only.
   *
   * @param aListenFd The listening socket
   * @param[out] aAddress Returns the stream socket's address
   * @param[out] aAddressLength Returns the number of valid bytes in |aAddress|
   * @param[out] aStreamFd The stream socket's file descriptor
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  virtual nsresult AcceptStreamSocket(int aListenFd,
                                      struct sockaddr* aAddress,
                                      socklen_t* aAddressLen,
                                      int& aStreamFd);

  /**
   * Creates a stream socket. I/O thread only.
   *
   * @param[in|out] aAddress The stream socket's address
   * @param[in|out] aAddressLength The number of valid bytes in |aAddress|
   * @param[out] aStreamFd The socket's file descriptor
   * @return NS_OK on success, or an XPCOM error code otherwise.
   */
  virtual nsresult CreateStreamSocket(struct sockaddr* aAddress,
                                      socklen_t* aAddressLength,
                                      int& aStreamFd);

  /**
   * Establishs a file descriptor for a socket.
   *
   * @deprecated
   *
   * @return File descriptor for socket
   */
  virtual int Create() = 0;

  /**
   * Since most socket specifics are related to address formation into a
   * sockaddr struct, this function is defined by subclasses and fills in the
   * structure as needed for whatever connection it is trying to build
   *
   * @deprecated
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
   * @deprecated
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
   * @deprecated
   *
   * @param aAddr Address struct
   * @param aAddrStr String to store address to
   */
  virtual void GetSocketAddr(const sockaddr_any& aAddr,
                             nsAString& aAddrStr) = 0;

protected:
  UnixSocketConnector();
};

}
}

#endif
