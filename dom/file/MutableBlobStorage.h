/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MutableBlobStorage_h
#define mozilla_dom_MutableBlobStorage_h

#include "mozilla/RefPtr.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "prio.h"

class nsIEventTarget;
class nsIRunnable;

namespace mozilla {

class TaskQueue;

namespace dom {

class Blob;
class BlobImpl;
class MutableBlobStorage;
class TemporaryIPCBlobChild;
class TemporaryIPCBlobChildCallback;

class MutableBlobStorageCallback
{
public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void BlobStoreCompleted(MutableBlobStorage* aBlobStorage,
                                  Blob* aBlob,
                                  nsresult aRv) = 0;
};

// This class is must be created and used on main-thread, except for Append()
// that can be called on any thread.
class MutableBlobStorage final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MutableBlobStorage)

  enum MutableBlobStorageType
  {
    eOnlyInMemory,
    eCouldBeInTemporaryFile,
  };

  explicit MutableBlobStorage(MutableBlobStorageType aType,
                              nsIEventTarget* aEventTarget = nullptr,
                              uint32_t aMaxMemory = 0);

  nsresult Append(const void* aData, uint32_t aLength);

  // This method can be called just once.
  // The callback will be called when the Blob is ready.
  void GetBlobWhenReady(nsISupports* aParent,
                        const nsACString& aContentType,
                        MutableBlobStorageCallback* aCallback);

  void TemporaryFileCreated(PRFileDesc* aFD);

  void AskForBlob(TemporaryIPCBlobChildCallback* aCallback,
                  const nsACString& aContentType);

  void ErrorPropagated(nsresult aRv);

  nsIEventTarget* EventTarget()
  {
    MOZ_ASSERT(mEventTarget);
    return mEventTarget;
  }

  // Returns the heap size in bytes of our internal buffers.
  // Note that this intentionally ignores the data in the temp file.
  size_t SizeOfCurrentMemoryBuffer();

  PRFileDesc* GetFD();

  void CloseFD();

private:
  ~MutableBlobStorage();

  bool ExpandBufferSize(const MutexAutoLock& aProofOfLock,
                        uint64_t aSize);

  bool ShouldBeTemporaryStorage(const MutexAutoLock& aProofOfLock,
                                uint64_t aSize) const;

  bool MaybeCreateTemporaryFile(const MutexAutoLock& aProofOfLock);
  void MaybeCreateTemporaryFileOnMainThread();

  MOZ_MUST_USE nsresult
  DispatchToIOThread(already_AddRefed<nsIRunnable> aRunnable);

  Mutex mMutex;

  // All these variables are touched on the main thread only or in the
  // retargeted thread when used by Append(). They are protected by mMutex.

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

  RefPtr<TemporaryIPCBlobChild> mActor;

  // This value is used when we go from eInMemory to eWaitingForTemporaryFile
  // and eventually eInTemporaryFile. If the size of the buffer is >=
  // mMaxMemory, the creation of the temporary file will start.
  // It's not used if mStorageState is eKeepInMemory.
  uint32_t mMaxMemory;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MutableBlobStorage_h
