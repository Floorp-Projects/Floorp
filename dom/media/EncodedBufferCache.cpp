/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EncodedBufferCache.h"
#include "mozilla/dom/File.h"
#include "nsAnonymousTemporaryFile.h"
#include "prio.h"

namespace mozilla {

void
EncodedBufferCache::AppendBuffer(nsTArray<uint8_t> & aBuf)
{
  MutexAutoLock lock(mMutex);
  mDataSize += aBuf.Length();

  mEncodedBuffers.AppendElement()->SwapElements(aBuf);

  if (!mTempFileEnabled && mDataSize > mMaxMemoryStorage) {
    nsresult rv;
    PRFileDesc* tempFD = nullptr;
    {
      // Release the mMutex because there is a sync dispatch to mainthread in
      // NS_OpenAnonymousTemporaryFile.
      MutexAutoUnlock unlock(mMutex);
      rv = NS_OpenAnonymousTemporaryFile(&tempFD);
    }
    if (!NS_FAILED(rv)) {
      // Check the mDataSize again since we release the mMutex before.
      if (mDataSize > mMaxMemoryStorage) {
        mFD = tempFD;
        mTempFileEnabled = true;
      } else {
        // Close the tempFD because the data had been taken during the
        // MutexAutoUnlock.
        PR_Close(tempFD);
      }
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

already_AddRefed<dom::Blob>
EncodedBufferCache::ExtractBlob(nsISupports* aParent,
                                const nsAString &aContentType)
{
  MutexAutoLock lock(mMutex);
  nsRefPtr<dom::Blob> blob;
  if (mTempFileEnabled) {
    // generate new temporary file to write
    blob = dom::Blob::CreateTemporaryBlob(aParent, mFD, 0, mDataSize,
                                          aContentType);
    // fallback to memory blob
    mTempFileEnabled = false;
    mDataSize = 0;
    mFD = nullptr;
  } else {
    void* blobData = malloc(mDataSize);
    NS_ASSERTION(blobData, "out of memory!!");

    if (blobData) {
      for (uint32_t i = 0, offset = 0; i < mEncodedBuffers.Length(); i++) {
        memcpy((uint8_t*)blobData + offset, mEncodedBuffers.ElementAt(i).Elements(),
               mEncodedBuffers.ElementAt(i).Length());
        offset += mEncodedBuffers.ElementAt(i).Length();
      }
      blob = dom::Blob::CreateMemoryBlob(aParent, blobData, mDataSize,
                                         aContentType);
      mEncodedBuffers.Clear();
    } else
      return nullptr;
  }
  mDataSize = 0;
  return blob.forget();
}

} //end namespace
