/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_ipc_SocketBase_h
#define mozilla_ipc_SocketBase_h

#include <errno.h>
#include <unistd.h>
#include "base/message_loop.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
using namespace mozilla::tasktracer;
#endif

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

/* |SocketIODeleteInstanceRunnable| deletes an object on the main thread.
 */
template<class T>
class SocketIODeleteInstanceRunnable MOZ_FINAL : public nsRunnable
{
public:
  SocketIODeleteInstanceRunnable(T* aInstance)
  : mInstance(aInstance)
  { }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    mInstance = nullptr; // delete instance

    return NS_OK;
  }

private:
  nsAutoPtr<T> mInstance;
};

//
// SocketIOBase
//

/* |SocketIOBase| is a base class for Socket I/O classes that
 * perform operations on the I/O thread. It provides methds
 * for the most common read and write scenarios.
 */
class SocketIOBase
{
public:
  virtual ~SocketIOBase();

  void EnqueueData(UnixSocketRawData* aData);
  bool HasPendingData() const;

  template <typename T>
  nsresult ReceiveData(int aFd, T* aIO)
  {
    MOZ_ASSERT(aFd >= 0);
    MOZ_ASSERT(aIO);

    do {
      nsAutoPtr<UnixSocketRawData> incoming(
        new UnixSocketRawData(mMaxReadSize));

      ssize_t res =
        TEMP_FAILURE_RETRY(read(aFd, incoming->mData, incoming->mSize));

      if (res < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return NS_OK; /* no more data available */
        }
        /* an error occored */
        nsRefPtr<nsRunnable> r = new SocketIORequestClosingRunnable<T>(aIO);
        NS_DispatchToMainThread(r);
        return NS_ERROR_FAILURE;
      } else if (!res) {
        /* EOF or peer shut down sending */
        nsRefPtr<nsRunnable> r = new SocketIORequestClosingRunnable<T>(aIO);
        NS_DispatchToMainThread(r);
        return NS_OK;
      }

#ifdef MOZ_TASK_TRACER
      // Make unix socket creation events to be the source events of TaskTracer,
      // and originate the rest correlation tasks from here.
      AutoSourceEvent taskTracerEvent(SourceEventType::UNIXSOCKET);
#endif

      incoming->mSize = res;
      nsRefPtr<nsRunnable> r =
        new SocketIOReceiveRunnable<T>(aIO, incoming.forget());
      NS_DispatchToMainThread(r);
    } while (true);

    return NS_OK;
  }

  template <typename T>
  nsresult SendPendingData(int aFd, T* aIO)
  {
    MOZ_ASSERT(aFd >= 0);
    MOZ_ASSERT(aIO);

    do {
      if (!HasPendingData()) {
        return NS_OK;
      }

      UnixSocketRawData* outgoing = mOutgoingQ.ElementAt(0);
      MOZ_ASSERT(outgoing->mSize);

      const uint8_t* data = outgoing->mData + outgoing->mCurrentWriteOffset;
      size_t size = outgoing->mSize - outgoing->mCurrentWriteOffset;

      ssize_t res = TEMP_FAILURE_RETRY(write(aFd, data, size));

      if (res < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return NS_OK; /* no more data available */
        }
        /* an error occored */
        nsRefPtr<nsRunnable> r = new SocketIORequestClosingRunnable<T>(aIO);
        NS_DispatchToMainThread(r);
        return NS_ERROR_FAILURE;
      } else if (!res) {
        return NS_OK; /* nothing written */
      }

      outgoing->mCurrentWriteOffset += res;

      if (outgoing->mCurrentWriteOffset == outgoing->mSize) {
        mOutgoingQ.RemoveElementAt(0);
        delete data;
      }
    } while (true);

    return NS_OK;
  }

protected:
  SocketIOBase(size_t aMaxReadSize);

private:
  const size_t mMaxReadSize;

  /**
   * Raw data queue. Must be pushed/popped from I/O thread only.
   */
  nsTArray<UnixSocketRawData*> mOutgoingQ;
};

//
// Socket I/O tasks
//

/* |SocketIOTask| holds a reference to a Socket I/O object. It's
 * supposed to run on the I/O thread.
 */
template <typename T>
class SocketIOTask : public CancelableTask
{
public:
  virtual ~SocketIOTask()
  { }

  T* GetIO() const
  {
    return mIO;
  }

  void Cancel() MOZ_OVERRIDE
  {
    mIO = nullptr;
  }

  bool IsCanceled() const
  {
    return !mIO;
  }

protected:
  SocketIOTask(T* aIO)
  : mIO(aIO)
  {
    MOZ_ASSERT(mIO);
  }

private:
  T* mIO;
};

/* |SocketIOSendTask| transfers an instance of |UnixSocketRawData| to
 * the I/O thread and queues it up for sending the contained data.
 */
template <typename T>
class SocketIOSendTask MOZ_FINAL : public SocketIOTask<T>
{
public:
  SocketIOSendTask(T* aIO, UnixSocketRawData* aData)
  : SocketIOTask<T>(aIO)
  , mData(aData)
  {
    MOZ_ASSERT(aData);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!SocketIOTask<T>::IsCanceled());

    T* io = SocketIOTask<T>::GetIO();
    MOZ_ASSERT(!io->IsShutdownOnIOThread());

    io->Send(mData);
  }

private:
  UnixSocketRawData* mData;
};

/* |SocketIOShutdownTask| signals shutdown to the Socket I/O object on
 * the I/O thread and sends it to the main thread for destruction.
 */
template <typename T>
class SocketIOShutdownTask MOZ_FINAL : public SocketIOTask<T>
{
public:
  SocketIOShutdownTask(T* aIO)
  : SocketIOTask<T>(aIO)
  { }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());

    T* io = SocketIOTask<T>::GetIO();

    // At this point, there should be no new events on the I/O thread
    // after this one with the possible exception of an accept task,
    // which ShutdownOnIOThread will cancel for us. We are now fully
    // shut down, so we can send a message to the main thread to delete
    // |io| safely knowing that it's not reference any longer.
    io->ShutdownOnIOThread();

    nsRefPtr<nsRunnable> r = new SocketIODeleteInstanceRunnable<T>(io);
    nsresult rv = NS_DispatchToMainThread(r);
    NS_ENSURE_SUCCESS_VOID(rv);
  }
};

}
}

#endif
