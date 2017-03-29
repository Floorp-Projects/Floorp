/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryStreamParent.h"
#include "nsIInputStream.h"

namespace mozilla {
namespace dom {

MemoryStreamParent::MemoryStreamParent(uint64_t aSize)
  : mSize(aSize)
  , mCurSize(0)
{}

mozilla::ipc::IPCResult
MemoryStreamParent::RecvAddChunk(nsTArray<unsigned char>&& aData)
{
  MOZ_ASSERT(mSize);

  uint64_t dataLength = aData.Length();

  if (!dataLength || mSize < (mCurSize + dataLength)) {
    return IPC_FAIL_NO_REASON(this);
  }

  void* buffer = malloc(dataLength);
  if (NS_WARN_IF(!buffer)) {
    return IPC_FAIL_NO_REASON(this);
  }

  memcpy(buffer, aData.Elements(), dataLength);
  mData.AppendElement(new MemoryBlobImpl::DataOwner(buffer, dataLength));

  mCurSize += dataLength;
  return IPC_OK();
}

void
MemoryStreamParent::ActorDestroy(IProtocol::ActorDestroyReason)
{
}

void
MemoryStreamParent::GetStream(nsIInputStream** aInputStream)
{
  if (mCurSize != mSize) {
    *aInputStream = nullptr;
    return;
  }

  nsCOMPtr<nsIMultiplexInputStream> stream =
    do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  if (NS_WARN_IF(!stream)) {
    *aInputStream = nullptr;
    return;
  }

  for (uint32_t i = 0; i < mData.Length(); ++i) {
    nsCOMPtr<nsIInputStream> dataStream;
    nsresult rv =
      MemoryBlobImpl::DataOwnerAdapter::Create(mData[i], 0, mData[i]->mLength,
                                               getter_AddRefs(dataStream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      *aInputStream = nullptr;
      return;
    }

    stream->AppendStream(dataStream);
  }

  stream.forget(aInputStream);
}

} // namespace dom
} // namespace mozilla
