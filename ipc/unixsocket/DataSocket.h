/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_ipc_datasocket_h
#define mozilla_ipc_datasocket_h

#include "mozilla/ipc/SocketBase.h"

namespace mozilla {
namespace ipc {

//
// DataSocket
//

/**
 * |DataSocket| represents a socket that can send or receive data. This
 * can be a stream-based socket, a datagram-based socket, or any other
 * socket that transfers data.
 */
class DataSocket : public SocketBase
{
public:
  virtual ~DataSocket();

  /**
   * Function to be called whenever data is received. This is only called on the
   * main thread.
   *
   * @param aBuffer Data received from the socket.
   */
  virtual void ReceiveSocketData(nsAutoPtr<UnixSocketBuffer>& aBuffer) = 0;

  /**
   * Queue data to be sent to the socket on the IO thread. Can only be called on
   * originating thread.
   *
   * @param aBuffer Data to be sent to socket
   */
  virtual void SendSocketData(UnixSocketIOBuffer* aBuffer) = 0;
};

//
// Runnables
//

/**
 * |SocketReceiveRunnable| transfers data received on the I/O thread
 * to an instance of |DataSocket| on the main thread.
 */
template <typename T>
class SocketIOReceiveRunnable final : public SocketIORunnable<T>
{
public:
  SocketIOReceiveRunnable(T* aIO, UnixSocketBuffer* aBuffer)
    : SocketIORunnable<T>(aIO)
    , mBuffer(aBuffer)
  { }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    T* io = SocketIORunnable<T>::GetIO();

    if (io->IsShutdownOnMainThread()) {
      NS_WARNING("mConsumer is null, aborting receive!");
      // Since we've already explicitly closed and the close happened before
      // this, this isn't really an error. Since we've warned, return OK.
      return NS_OK;
    }

    DataSocket* dataSocket = io->GetDataSocket();
    MOZ_ASSERT(dataSocket);

    dataSocket->ReceiveSocketData(mBuffer);

    return NS_OK;
  }

private:
  nsAutoPtr<UnixSocketBuffer> mBuffer;
};

//
// DataSocketIO
//

/**
 * |DataSocketIO| is a base class for Socket I/O classes that
 * transfer data on the I/O thread. It provides methods for the
 * most common read and write scenarios.
 */
class DataSocketIO : public SocketIOBase
{
public:
  virtual ~DataSocketIO();

  /**
   * Allocates a buffer for receiving data from the socket. The method
   * shall return the buffer in the arguments. The buffer is owned by the
   * I/O class. |DataSocketIO| will never ask for more than one buffer
   * at a time, so I/O classes can handout the same buffer on each invokation
   * of this method. I/O-thread only.
   *
   *  @param[out] aBuffer returns a pointer to the I/O buffer
   *  @return NS_OK on success, or an error code otherwise
   */
  virtual nsresult QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer) = 0;

  /**
   * Marks the current socket buffer to by consumed by the I/O class. The
   * class is resonsible for releasing the buffer afterwards. I/O-thread
   * only.
   *
   *  @param aIndex the socket's index
   *  @param[out] aBuffer the receive buffer
   *  @param[out] aSize the receive buffer's size
   */
  virtual void ConsumeBuffer() = 0;

  /**
   * Marks the current socket buffer to be discarded. The I/O class is
   * resonsible for releasing the buffer's memory. I/O-thread only.
   *
   *  @param aIndex the socket's index
   */
  virtual void DiscardBuffer() = 0;

  void EnqueueData(UnixSocketIOBuffer* aBuffer);
  bool HasPendingData() const;

  template <typename T>
  ssize_t ReceiveData(int aFd, T* aIO)
  {
    MOZ_ASSERT(aFd >= 0);
    MOZ_ASSERT(aIO);

    UnixSocketIOBuffer* incoming;
    nsresult rv = QueryReceiveBuffer(&incoming);
    if (NS_FAILED(rv)) {
      /* an error occured */
      nsRefPtr<nsRunnable> r = new SocketIORequestClosingRunnable<T>(aIO);
      NS_DispatchToMainThread(r);
      return -1;
    }

    ssize_t res = incoming->Receive(aFd);
    if (res < 0) {
      /* an I/O error occured */
      DiscardBuffer();
      nsRefPtr<nsRunnable> r = new SocketIORequestClosingRunnable<T>(aIO);
      NS_DispatchToMainThread(r);
      return -1;
    } else if (!res) {
      /* EOF or peer shut down sending */
      DiscardBuffer();
      nsRefPtr<nsRunnable> r = new SocketIORequestClosingRunnable<T>(aIO);
      NS_DispatchToMainThread(r);
      return 0;
    }

#ifdef MOZ_TASK_TRACER
    // Make unix socket creation events to be the source events of TaskTracer,
    // and originate the rest correlation tasks from here.
    AutoSourceEvent taskTracerEvent(SourceEventType::Unixsocket);
#endif

    ConsumeBuffer();

    return res;
  }

  template <typename T>
  nsresult SendPendingData(int aFd, T* aIO)
  {
    MOZ_ASSERT(aFd >= 0);
    MOZ_ASSERT(aIO);

    while (HasPendingData()) {
      UnixSocketIOBuffer* outgoing = mOutgoingQ.ElementAt(0);

      ssize_t res = outgoing->Send(aFd);
      if (res < 0) {
        /* an I/O error occured */
        nsRefPtr<nsRunnable> r = new SocketIORequestClosingRunnable<T>(aIO);
        NS_DispatchToMainThread(r);
        return NS_ERROR_FAILURE;
      } else if (!res && outgoing->GetSize()) {
        /* I/O is currently blocked; try again later */
        return NS_OK;
      }
      if (!outgoing->GetSize()) {
        mOutgoingQ.RemoveElementAt(0);
        delete outgoing;
      }
    }

    return NS_OK;
  }

protected:
  DataSocketIO();

private:
  /**
   * Raw data queue. Must be pushed/popped from I/O thread only.
   */
  nsTArray<UnixSocketIOBuffer*> mOutgoingQ;
};

//
// Tasks
//

/* |SocketIOSendTask| transfers an instance of |Tdata|, such as
 * |UnixSocketRawData|, to the I/O thread and queues it up for
 * sending the contained data.
 */
template<typename Tio, typename Tdata>
class SocketIOSendTask final : public SocketIOTask<Tio>
{
public:
  SocketIOSendTask(Tio* aIO, Tdata* aData)
  : SocketIOTask<Tio>(aIO)
  , mData(aData)
  {
    MOZ_ASSERT(aData);
  }

  void Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!SocketIOTask<Tio>::IsCanceled());

    Tio* io = SocketIOTask<Tio>::GetIO();
    MOZ_ASSERT(!io->IsShutdownOnIOThread());

    io->Send(mData);
  }

private:
  Tdata* mData;
};

}
}

#endif
