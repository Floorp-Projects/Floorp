/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryStreams.h"

#include "nsStreamUtils.h"

USING_FILE_NAMESPACE

// static
already_AddRefed<MemoryOutputStream>
MemoryOutputStream::Create(PRUint64 aSize)
{
  NS_ASSERTION(aSize, "Passed zero size!");

  NS_ENSURE_TRUE(aSize <= PR_UINT32_MAX, nullptr);

  nsRefPtr<MemoryOutputStream> stream = new MemoryOutputStream();

  char* dummy;
  PRUint32 length = stream->mData.GetMutableData(&dummy, aSize, fallible_t());
  NS_ENSURE_TRUE(length == aSize, nullptr);

  return stream.forget();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(MemoryOutputStream, nsIOutputStream)

NS_IMETHODIMP
MemoryOutputStream::Close()
{
  mData.Truncate(mOffset);
  return NS_OK;
}

NS_IMETHODIMP
MemoryOutputStream::Write(const char* aBuf, PRUint32 aCount, PRUint32* _retval)
{
  return WriteSegments(NS_CopySegmentToBuffer, (char*)aBuf, aCount, _retval);
}

NS_IMETHODIMP
MemoryOutputStream::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP
MemoryOutputStream::WriteFrom(nsIInputStream* aFromStream, PRUint32 aCount,
                              PRUint32* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MemoryOutputStream::WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                                  PRUint32 aCount, PRUint32* _retval)
{
  NS_ASSERTION(mData.Length() >= mOffset, "Bad stream state!");

  PRUint32 maxCount = mData.Length() - mOffset;
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
    NS_ASSERTION(*_retval <= aCount,
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
