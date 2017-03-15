/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCStreamDestination.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIPipe.h"

namespace mozilla {
namespace ipc {

IPCStreamDestination::IPCStreamDestination()
{
}

IPCStreamDestination::~IPCStreamDestination()
{
}

nsresult
IPCStreamDestination::Initialize()
{
  MOZ_ASSERT(!mReader);
  MOZ_ASSERT(!mWriter);

  // use async versions for both reader and writer even though we are
  // opening the writer as an infinite stream.  We want to be able to
  // use CloseWithStatus() to communicate errors through the pipe.

  // Use an "infinite" pipe because we cannot apply back-pressure through
  // the async IPC layer at the moment.  Blocking the IPC worker thread
  // is not desirable, either.
  nsresult rv = NS_NewPipe2(getter_AddRefs(mReader),
                            getter_AddRefs(mWriter),
                            true, true,   // non-blocking
                            0,            // segment size
                            UINT32_MAX);  // "infinite" pipe
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

already_AddRefed<nsIInputStream>
IPCStreamDestination::TakeReader()
{
  MOZ_ASSERT(mReader);
  return mReader.forget();
}

void
IPCStreamDestination::ActorDestroyed()
{
  MOZ_ASSERT(mWriter);

  // If we were gracefully closed we should have gotten RecvClose().  In
  // that case, the writer will already be closed and this will have no
  // effect.  This just aborts the writer in the case where the child process
  // crashes.
  mWriter->CloseWithStatus(NS_ERROR_ABORT);
}

void
IPCStreamDestination::BufferReceived(const nsCString& aBuffer)
{
  MOZ_ASSERT(mWriter);

  uint32_t numWritten = 0;

  // This should only fail if we hit an OOM condition.
  nsresult rv = mWriter->Write(aBuffer.get(), aBuffer.Length(), &numWritten);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RequestClose(rv);
  }
}

void
IPCStreamDestination::CloseReceived(nsresult aRv)
{
  MOZ_ASSERT(mWriter);
  mWriter->CloseWithStatus(aRv);
  TerminateDestination();
}

} // namespace ipc
} // namespace mozilla
