/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryOutputStream.h"

#include "nsStreamUtils.h"

namespace mozilla {
namespace dom {
namespace quota {

// static
already_AddRefed<MemoryOutputStream>
MemoryOutputStream::Create(uint64_t aSize)
{
  MOZ_ASSERT(aSize, "Passed zero size!");

  if (NS_WARN_IF(aSize > UINT32_MAX)) {
    return nullptr;
  }

  RefPtr<MemoryOutputStream> stream = new MemoryOutputStream();

  if (NS_WARN_IF(!stream->mData.SetLength(aSize, fallible))) {
    return nullptr;
  }

  return stream.forget();
}

NS_IMPL_ISUPPORTS(MemoryOutputStream, nsIOutputStream)

NS_IMETHODIMP
MemoryOutputStream::Close()
{
  mData.Truncate(mOffset);
  return NS_OK;
}

NS_IMETHODIMP
MemoryOutputStream::Write(const char* aBuf, uint32_t aCount, uint32_t* _retval)
{
  return WriteSegments(NS_CopySegmentToBuffer, (char*)aBuf, aCount, _retval);
}

NS_IMETHODIMP
MemoryOutputStream::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP
MemoryOutputStream::WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                              uint32_t* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MemoryOutputStream::WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                                  uint32_t aCount, uint32_t* _retval)
{
  MOZ_ASSERT(mData.Length() >= mOffset, "Bad stream state!");

  uint32_t maxCount = mData.Length() - mOffset;
  if (maxCount == 0) {
    *_retval = 0;
    return NS_OK;
  }

  if (aCount > maxCount) {
    aCount = maxCount;
  }

  nsresult rv = aReader(this, aClosure, mData.BeginWriting() + mOffset, 0,
                        aCount, _retval);
  if (NS_SUCCEEDED(rv)) {
    MOZ_ASSERT(*_retval <= aCount,
               "Reader should not read more than we asked it to read!");
    mOffset += *_retval;
  }

  return NS_OK;
}

NS_IMETHODIMP
MemoryOutputStream::IsNonBlocking(bool* _retval)
{
  *_retval = false;
  return NS_OK;
}

} // namespace quota
} // namespace dom
} // namespace mozilla
