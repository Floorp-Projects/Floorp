/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_UnixSocket_h
#define mozilla_ipc_UnixSocket_h

#include <stdlib.h>
#include "nsString.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace ipc {

struct UnixSocketRawData
{
  static const size_t MAX_DATA_SIZE = 1024;
  uint8_t mData[MAX_DATA_SIZE];

  // Number of octets in mData.
  size_t mSize;
  size_t mCurrentWriteOffset;

  /** 
   * Constructor for situations where size is not known beforehand. (for
   * example, when reading a packet)
   *
   */
  UnixSocketRawData() :
    mSize(0),
    mCurrentWriteOffset(0)
  {
  }

  /** 
   * Constructor for situations where size is known beforehand (for example,
   * when being assigned strings)
   *
   */
  UnixSocketRawData(int aSize) :
    mSize(aSize),
    mCurrentWriteOffset(0)
  {
  }

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
   * Runs connect function on a file descriptor for the address specified. Makes
   * sure socket is marked with flags expected by the UnixSocket handler
   * (non-block, etc...)
   *
   * @param aFd File descriptor created by Create() function
   * @param aAddress Address to connect to
   *
   * @return true if connected, false otherwise
   */
  bool Connect(int aFd, const char* aAddress);
  
protected:
  /** 
   * Internal type-specific connection function to be overridden by child
   * classes.
   *
   * @param aFd File descriptor created by Create() function
   * @param aAddress Address to connect to
   *
   * @return true if connected, false otherwise
   */
  virtual bool ConnectInternal(int aFd, const char* aAddress) = 0;
};

class UnixSocketConsumer : public RefCounted<UnixSocketConsumer>
{
public:
  UnixSocketConsumer()
    : mImpl(nullptr)
  {}

  virtual ~UnixSocketConsumer();
  
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
   * Connects to a socket. Due to the fact that this is a blocking connect (and
   * for things such as bluetooth, it /will/ block), it is expected to be run on
   * a thread provided by the user. It cannot run on the main thread. Runs the
   * Create() and Connect() functions in the UnixSocketConnector.
   *
   * @param aConnector Connector object for socket type specific functions
   * @param aAddress Address to connect to.
   *
   * @return true on connection, false otherwise.
   */
  bool ConnectSocket(UnixSocketConnector& aConnector, const char* aAddress);

  /** 
   * Queues the internal representation of socket for deletion. Can be called
   * from main thread.
   *
   */
  void CloseSocket();

private:
  nsAutoPtr<UnixSocketImpl> mImpl;
};

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_Socket_h
