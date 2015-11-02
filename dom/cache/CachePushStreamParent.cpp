/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CachePushStreamParent.h"

#include "mozilla/unused.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIPipe.h"

namespace mozilla {
namespace dom {
namespace cache {

// static
CachePushStreamParent*
CachePushStreamParent::Create()
{
  // use async versions for both reader and writer even though we are
  // opening the writer as an infinite stream.  We want to be able to
  // use CloseWithStatus() to communicate errors through the pipe.
  nsCOMPtr<nsIAsyncInputStream> reader;
  nsCOMPtr<nsIAsyncOutputStream> writer;

  // Use an "infinite" pipe because we cannot apply back-pressure through
  // the async IPC layer at the moment.  Blocking the IPC worker thread
  // is not desirable, either.
  nsresult rv = NS_NewPipe2(getter_AddRefs(reader),
                            getter_AddRefs(writer),
                            true, true,   // non-blocking
                            0,            // segment size
                            UINT32_MAX);  // "infinite" pipe
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return new CachePushStreamParent(reader, writer);
}

CachePushStreamParent::~CachePushStreamParent()
{
}

already_AddRefed<nsIInputStream>
CachePushStreamParent::TakeReader()
{
  MOZ_ASSERT(mReader);
  return mReader.forget();
}

void
CachePushStreamParent::ActorDestroy(ActorDestroyReason aReason)
{
  // If we were gracefully closed we should have gotten RecvClose().  In
  // that case, the writer will already be closed and this will have no
  // effect.  This just aborts the writer in the case where the child process
  // crashes.
  mWriter->CloseWithStatus(NS_ERROR_ABORT);
}

bool
CachePushStreamParent::RecvBuffer(const nsCString& aBuffer)
{
  uint32_t numWritten = 0;

  // This should only fail if we hit an OOM condition.
  nsresult rv = mWriter->Write(aBuffer.get(), aBuffer.Length(), &numWritten);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RecvClose(rv);
  }

  return true;
}

bool
CachePushStreamParent::RecvClose(const nsresult& aRv)
{
  mWriter->CloseWithStatus(aRv);
  Unused << Send__delete__(this);
  return true;
}

CachePushStreamParent::CachePushStreamParent(nsIAsyncInputStream* aReader,
                                             nsIAsyncOutputStream* aWriter)
  : mReader(aReader)
  , mWriter(aWriter)
{
  MOZ_ASSERT(mReader);
  MOZ_ASSERT(mWriter);
}

} // namespace cache
} // namespace dom
} // namespace mozilla
