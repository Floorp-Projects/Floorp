/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_ipc_SocketBase_h
#define mozilla_ipc_SocketBase_h

#include "nsAutoPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace ipc {

//
// UnixSocketRawData
//

class UnixSocketRawData
{
public:
  // Number of octets in mData.
  size_t mSize;
  size_t mCurrentWriteOffset;
  nsAutoArrayPtr<uint8_t> mData;

  /**
   * Constructor for situations where only size is known beforehand
   * (for example, when being assigned strings)
   */
  UnixSocketRawData(size_t aSize);

  /**
   * Constructor for situations where size and data is known
   * beforehand (for example, when being assigned strings)
   */
  UnixSocketRawData(const void* aData, size_t aSize);
};

enum SocketConnectionStatus {
  SOCKET_DISCONNECTED = 0,
  SOCKET_LISTENING = 1,
  SOCKET_CONNECTING = 2,
  SOCKET_CONNECTED = 3
};

//
// SocketConsumerBase
//

class SocketConsumerBase
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SocketConsumerBase)

  virtual ~SocketConsumerBase();

  SocketConnectionStatus GetConnectionStatus() const;

  int GetSuggestedConnectDelayMs() const;

  /**
   * Queues the internal representation of socket for deletion. Can be called
   * from main thread.
   */
  virtual void CloseSocket() = 0;

  /**
   * Function to be called whenever data is received. This is only called on the
   * main thread.
   *
   * @param aMessage Data received from the socket.
   */
  virtual void ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage) = 0;

  /**
   * Queue data to be sent to the socket on the IO thread. Can only be called on
   * originating thread.
   *
   * @param aMessage Data to be sent to socket
   *
   * @return true if data is queued, false otherwise (i.e. not connected)
   */
  virtual bool SendSocketData(UnixSocketRawData* aMessage) = 0;

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

protected:
  SocketConsumerBase();

  void SetConnectionStatus(SocketConnectionStatus aConnectionStatus);

private:
  uint32_t CalculateConnectDelayMs() const;

  SocketConnectionStatus mConnectionStatus;
  PRIntervalTime mConnectTimestamp;
  uint32_t mConnectDelayMs;
};

//
// Socket I/O runnables
//

/* |SocketIORunnable| is a runnable for sending a message from
 * the I/O thread to the main thread.
 */
template <typename T>
class SocketIORunnable : public nsRunnable
{
public:
  virtual ~SocketIORunnable()
  { }

  T* GetIO() const
  {
    return mIO;
  }

protected:
  SocketIORunnable(T* aIO)
  : mIO(aIO)
  {
    MOZ_ASSERT(aIO);
  }

private:
  T* mIO;
};

/* |SocketIOEventRunnable| reports the connection state on the
 * I/O thrad back to the main thread.
 */
template <typename T>
class SocketIOEventRunnable MOZ_FINAL : public SocketIORunnable<T>
{
public:
  enum SocketEvent {
    CONNECT_SUCCESS,
    CONNECT_ERROR,
    DISCONNECT
  };

  SocketIOEventRunnable(T* aIO, SocketEvent e)
  : SocketIORunnable<T>(aIO)
  , mEvent(e)
  { }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    T* io = SocketIORunnable<T>::GetIO();

    if (io->IsShutdownOnMainThread()) {
      NS_WARNING("I/O consumer has already been closed!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }

    SocketConsumerBase* consumer = io->GetConsumer();
    MOZ_ASSERT(consumer);

    if (mEvent == CONNECT_SUCCESS) {
      consumer->NotifySuccess();
    } else if (mEvent == CONNECT_ERROR) {
      consumer->NotifyError();
    } else if (mEvent == DISCONNECT) {
      consumer->NotifyDisconnect();
    }

    return NS_OK;
  }

private:
  SocketEvent mEvent;
};

/* |SocketReceiveRunnable| transfers data received on the I/O thread
 * to the consumer on the main thread.
 */
template <typename T>
class SocketIOReceiveRunnable MOZ_FINAL : public SocketIORunnable<T>
{
public:
  SocketIOReceiveRunnable(T* aIO, UnixSocketRawData* aData)
  : SocketIORunnable<T>(aIO)
  , mData(aData)
  { }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    T* io = SocketIORunnable<T>::GetIO();

    if (io->IsShutdownOnMainThread()) {
      NS_WARNING("mConsumer is null, aborting receive!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }

    SocketConsumerBase* consumer = io->GetConsumer();
    MOZ_ASSERT(consumer);

    consumer->ReceiveSocketData(mData);

    return NS_OK;
  }

private:
  nsAutoPtr<UnixSocketRawData> mData;
};

template <typename T>
class SocketIORequestClosingRunnable MOZ_FINAL : public SocketIORunnable<T>
{
public:
  SocketIORequestClosingRunnable(T* aImpl)
  : SocketIORunnable<T>(aImpl)
  { }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    T* io = SocketIORunnable<T>::GetIO();

    if (io->IsShutdownOnMainThread()) {
      NS_WARNING("CloseSocket has already been called!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }

    SocketConsumerBase* consumer = io->GetConsumer();
    MOZ_ASSERT(consumer);

    consumer->CloseSocket();

    return NS_OK;
  }
};

}
}

#endif
