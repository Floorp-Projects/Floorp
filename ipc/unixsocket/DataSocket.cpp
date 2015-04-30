/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "mozilla/ipc/DataSocket.h"
#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

#ifdef MOZ_TASK_TRACER
using namespace mozilla::tasktracer;
#endif

namespace mozilla {
namespace ipc {

//
// DataSocketIO
//

DataSocketIO::~DataSocketIO()
{ }

void
DataSocketIO::EnqueueData(UnixSocketIOBuffer* aBuffer)
{
  if (!aBuffer->GetSize()) {
    delete aBuffer; // delete empty data immediately
    return;
  }
  mOutgoingQ.AppendElement(aBuffer);
}

bool
DataSocketIO::HasPendingData() const
{
  return !mOutgoingQ.IsEmpty();
}

ssize_t
DataSocketIO::ReceiveData(int aFd)
{
  MOZ_ASSERT(aFd >= 0);

  UnixSocketIOBuffer* incoming;
  nsresult rv = QueryReceiveBuffer(&incoming);
  if (NS_FAILED(rv)) {
    /* an error occured */
    NS_DispatchToMainThread(new SocketIORequestClosingRunnable(this));
    return -1;
  }

  ssize_t res = incoming->Receive(aFd);
  if (res < 0) {
    /* an I/O error occured */
    DiscardBuffer();
    NS_DispatchToMainThread(new SocketIORequestClosingRunnable(this));
    return -1;
  } else if (!res) {
    /* EOF or peer shut down sending */
    DiscardBuffer();
    NS_DispatchToMainThread(new SocketIORequestClosingRunnable(this));
    return 0;
  }

#ifdef MOZ_TASK_TRACER
  /* Make unix socket creation events to be the source events of TaskTracer,
   * and originate the rest correlation tasks from here.
   */
  AutoSourceEvent taskTracerEvent(SourceEventType::Unixsocket);
#endif

  ConsumeBuffer();

  return res;
}

nsresult
DataSocketIO::SendPendingData(int aFd)
{
  MOZ_ASSERT(aFd >= 0);

  while (HasPendingData()) {
    UnixSocketIOBuffer* outgoing = mOutgoingQ.ElementAt(0);

    ssize_t res = outgoing->Send(aFd);
    if (res < 0) {
      /* an I/O error occured */
      NS_DispatchToMainThread(new SocketIORequestClosingRunnable(this));
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

DataSocketIO::DataSocketIO()
{ }

//
// DataSocket
//

DataSocket::~DataSocket()
{ }

}
}
