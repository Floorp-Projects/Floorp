/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EncodedBufferCache.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsDOMFile.h"
#include "prio.h"

namespace mozilla {

void
EncodedBufferCache::AppendBuffer(nsTArray<uint8_t> & aBuf)
{
  MutexAutoLock lock(mMutex);
  mDataSize += aBuf.Length();

  mEncodedBuffers.AppendElement()->SwapElements(aBuf);

  if (!mTempFileEnabled && mDataSize > mMaxMemoryStorage) {
    nsresult rv = NS_OpenAnonymousTemporaryFile(&mFD);
    if (!NS_FAILED(rv)) {
      mTempFileEnabled = true;
    }
  }

  if (mTempFileEnabled) {
    // has created temporary file, write buffer in it
    for (uint32_t i = 0; i < mEncodedBuffers.Length(); i++) {
      int32_t amount = PR_Write(mFD, mEncodedBuffers.ElementAt(i).Elements(), mEncodedBuffers.ElementAt(i).Length());
      if (amount < 0 || size_t(amount) < mEncodedBuffers.ElementAt(i).Length()) {
        NS_WARNING("Failed to write media cache block!");
      }
    }
    mEncodedBuffers.Clear();
  }

}

already_AddRefed<nsIDOMBlob>
EncodedBufferCache::ExtractBlob(const nsAString &aContentType)
{
  MutexAutoLock lock(mMutex);
  nsCOMPtr<nsIDOMBlob> blob;
  if (mTempFileEnabled) {
    // generate new temporary file to write
    blob = new nsDOMTemporaryFileBlob(mFD, 0, mDataSize, aContentType);
    // fallback to memory blob
    mTempFileEnabled = false;
    mDataSize = 0;
    mFD = nullptr;
  } else {
    void* blobData = moz_malloc(mDataSize);
    NS_ASSERTION(blobData, "out of memory!!");

    if (blobData) {
      for (uint32_t i = 0, offset = 0; i < mEncodedBuffers.Length(); i++) {
        memcpy((uint8_t*)blobData + offset, mEncodedBuffers.ElementAt(i).Elements(),
               mEncodedBuffers.ElementAt(i).Length());
        offset += mEncodedBuffers.ElementAt(i).Length();
      }
      blob = new nsDOMMemoryFile(blobData, mDataSize,
                                 aContentType);
      mEncodedBuffers.Clear();
    } else
      return nullptr;
  }
  mDataSize = 0;
  return blob.forget();
}

} //end namespace
