/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MutableBlobStorage_h
#define mozilla_dom_MutableBlobStorage_h

#include "mozilla/RefPtr.h"
#include "prio.h"

class nsIEventTarget;

namespace mozilla {

class TaskQueue;

namespace dom {

class Blob;
class BlobImpl;
class MutableBlobStorage;

class MutableBlobStorageCallback
{
public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void BlobStoreCompleted(MutableBlobStorage* aBlobStorage,
                                  Blob* aBlob,
                                  nsresult aRv) = 0;
};

// This class is main-thread only.
class MutableBlobStorage final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MutableBlobStorage);

  enum MutableBlobStorageType
  {
    eOnlyInMemory,
    eCouldBeInTemporaryFile,
  };

  explicit MutableBlobStorage(MutableBlobStorageType aType,
                              nsIEventTarget* aEventTarget = nullptr);

  nsresult Append(const void* aData, uint32_t aLength);

  // This method can be called just once.
  // The callback will be called when the Blob is ready.
  // The return value is the total size of the blob, when created.
  uint64_t GetBlobWhenReady(nsISupports* aParent,
                            const nsACString& aContentType,
                            MutableBlobStorageCallback* aCallback);

  void TemporaryFileCreated(PRFileDesc* aFD);

  void  CreateBlobAndRespond(already_AddRefed<nsISupports> aParent,
                             const nsACString& aContentType,
                             already_AddRefed<MutableBlobStorageCallback> aCallback);

  void ErrorPropagated(nsresult aRv);

  nsIEventTarget* EventTarget()
  {
    MOZ_ASSERT(mEventTarget);
    return mEventTarget;
  }

private:
  ~MutableBlobStorage();

  bool ExpandBufferSize(uint64_t aSize);

  bool ShouldBeTemporaryStorage(uint64_t aSize) const;

  nsresult MaybeCreateTemporaryFile();

  void DispatchToIOThread(already_AddRefed<nsIRunnable> aRunnable);

  // All these variables are touched on the main thread only.

  void* mData;
  uint64_t mDataLen;
  uint64_t mDataBufferLen;

  enum StorageState {
    eKeepInMemory,
    eInMemory,
    eWaitingForTemporaryFile,
    eInTemporaryFile,
    eClosed
  };

  StorageState mStorageState;

  PRFileDesc* mFD;

  nsresult mErrorResult;

  RefPtr<TaskQueue> mTaskQueue;
  nsCOMPtr<nsIEventTarget> mEventTarget;

  nsCOMPtr<nsISupports> mPendingParent;
  nsCString mPendingContentType;
  RefPtr<MutableBlobStorageCallback> mPendingCallback;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MutableBlobStorage_h
