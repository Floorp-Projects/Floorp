/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EncodedBufferCache.h"
#include "prio.h"
#include "nsAnonymousTemporaryFile.h"
#include "mozilla/Monitor.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/File.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

namespace mozilla {

void
EncodedBufferCache::AppendBuffer(nsTArray<uint8_t> & aBuf)
{
  MOZ_ASSERT(!NS_IsMainThread());

  MutexAutoLock lock(mMutex);
  mDataSize += aBuf.Length();

  mEncodedBuffers.AppendElement()->SwapElements(aBuf);

  if (!mTempFileEnabled && mDataSize > mMaxMemoryStorage) {
    nsresult rv;
    PRFileDesc* tempFD = nullptr;
    {
      // Release the mMutex because of the sync dispatch to the main thread.
      MutexAutoUnlock unlock(mMutex);
      if (XRE_IsParentProcess()) {
        // In case we are in the parent process, do a synchronous I/O here to open a
        // temporary file.
        rv = NS_OpenAnonymousTemporaryFile(&tempFD);
      } else {
        // In case we are in the child process, we don't have access to open a file
        // directly due to sandbox restrictions, so we need to ask the parent process
        // to do that for us.  In order to initiate the IPC, we need to first go to
        // the main thread.  This is done by dispatching a runnable to the main thread.
        // From there, we start an asynchronous IPC, and we block the current thread
        // using a monitor while this async work is in progress.  When we receive the
        // resulting file descriptor from the parent process, we notify the monitor
        // and unblock the current thread and continue.
        typedef dom::ContentChild::AnonymousTemporaryFileCallback
          AnonymousTemporaryFileCallback;
        bool done = false;
        Monitor monitor("EncodeBufferCache::AppendBuffer");
        RefPtr<dom::ContentChild> cc = dom::ContentChild::GetSingleton();
        nsCOMPtr<nsIRunnable> runnable =
          NewRunnableMethod<AnonymousTemporaryFileCallback>(
            "dom::ContentChild::AsyncOpenAnonymousTemporaryFile",
            cc,
            &dom::ContentChild::AsyncOpenAnonymousTemporaryFile,
            [&](PRFileDesc* aFile) {
              rv = aFile ? NS_OK : NS_ERROR_FAILURE;
              tempFD = aFile;
              MonitorAutoLock lock(monitor);
              done = true;
              lock.Notify();
            });
        MonitorAutoLock lock(monitor);
        rv = NS_DispatchToMainThread(runnable);
        if (NS_SUCCEEDED(rv)) {
          while (!done) {
            lock.Wait();
          }
        }
      }
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
  RefPtr<dom::Blob> blob;
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

} // namespace mozilla
