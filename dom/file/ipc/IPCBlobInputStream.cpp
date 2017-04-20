/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStream.h"
#include "IPCBlobInputStreamChild.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF(IPCBlobInputStream);
NS_IMPL_RELEASE(IPCBlobInputStream);

NS_INTERFACE_MAP_BEGIN(IPCBlobInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsICloneableInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

IPCBlobInputStream::IPCBlobInputStream(IPCBlobInputStreamChild* aActor)
  : mActor(aActor)
{
  MOZ_ASSERT(aActor);
}

IPCBlobInputStream::~IPCBlobInputStream()
{
  Close();
}

// nsIInputStream interface

NS_IMETHODIMP
IPCBlobInputStream::Available(uint64_t* aLength)
{
  if (!mActor) {
    return NS_BASE_STREAM_CLOSED;
  }

  *aLength = mActor->Size();
  return NS_OK;
}

NS_IMETHODIMP
IPCBlobInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IPCBlobInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                 uint32_t aCount, uint32_t *aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IPCBlobInputStream::IsNonBlocking(bool* aNonBlocking)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IPCBlobInputStream::Close()
{
  if (mActor) {
    mActor->ForgetStream(this);
    mActor = nullptr;
  }

  return NS_OK;
}

// nsICloneableInputStream interface

NS_IMETHODIMP
IPCBlobInputStream::GetCloneable(bool* aCloneable)
{
  *aCloneable = !!mActor;
  return NS_OK;
}

NS_IMETHODIMP
IPCBlobInputStream::Clone(nsIInputStream** aResult)
{
  if (!mActor) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> stream = mActor->CreateStream();
  stream.forget(aResult);
  return NS_OK;
}

// nsIIPCSerializableInputStream

void
IPCBlobInputStream::Serialize(mozilla::ipc::InputStreamParams& aParams,
                              FileDescriptorArray& aFileDescriptors)
{
  IPCBlobInputStreamParams params;
  params.id() = mActor->ID();

  aParams = params;
}

bool
IPCBlobInputStream::Deserialize(const mozilla::ipc::InputStreamParams& aParams,
                                const FileDescriptorArray& aFileDescriptors)
{
  MOZ_CRASH("This should never be called.");
  return false;
}

mozilla::Maybe<uint64_t>
IPCBlobInputStream::ExpectedSerializedLength()
{
  return mozilla::Nothing();
}

} // namespace dom
} // namespace mozilla
