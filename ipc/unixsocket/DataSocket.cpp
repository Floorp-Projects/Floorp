/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "mozilla/ipc/DataSocket.h"

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

DataSocketIO::DataSocketIO()
{ }

//
// DataSocket
//

DataSocket::~DataSocket()
{ }

}
}
