/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MutableBlobStreamListener.h"
#include "MutableBlobStorage.h"
#include "nsIInputStream.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

MutableBlobStreamListener::MutableBlobStreamListener(MutableBlobStorage::MutableBlobStorageType aStorageType,
                                                     nsISupports* aParent,
                                                     const nsACString& aContentType,
                                                     MutableBlobStorageCallback* aCallback,
                                                     nsIEventTarget* aEventTarget)
  : mCallback(aCallback)
  , mParent(aParent)
  , mStorageType(aStorageType)
  , mContentType(aContentType)
  , mEventTarget(aEventTarget)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);

  if (!mEventTarget) {
    mEventTarget = GetMainThreadEventTarget();
  }

  MOZ_ASSERT(mEventTarget);
}

MutableBlobStreamListener::~MutableBlobStreamListener()
{
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMPL_ISUPPORTS(MutableBlobStreamListener,
                  nsIStreamListener,
                  nsIThreadRetargetableStreamListener,
                  nsIRequestObserver)

NS_IMETHODIMP
MutableBlobStreamListener::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mStorage);
  MOZ_ASSERT(mEventTarget);

  mStorage = new MutableBlobStorage(mStorageType, mEventTarget);
  return NS_OK;
}

NS_IMETHODIMP
MutableBlobStreamListener::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                                         nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStorage);

  // Resetting mStorage to nullptr.
  RefPtr<MutableBlobStorage> storage;
  storage.swap(mStorage);

  // Let's propagate the error simulating a failure of the storage.
  if (NS_FAILED(aStatus)) {
    mCallback->BlobStoreCompleted(storage, nullptr, aStatus);
    return NS_OK;
  }

  storage->GetBlobWhenReady(mParent, mContentType, mCallback);
  return NS_OK;
}

NS_IMETHODIMP
MutableBlobStreamListener::OnDataAvailable(nsIRequest* aRequest,
                                           nsISupports* aContext,
                                           nsIInputStream* aStream,
                                           uint64_t aSourceOffset,
                                           uint32_t aCount)
{
  // This method could be called on any thread.
  MOZ_ASSERT(mStorage);

  uint32_t countRead;
  return aStream->ReadSegments(WriteSegmentFun, this, aCount, &countRead);
}

nsresult
MutableBlobStreamListener::WriteSegmentFun(nsIInputStream* aWriterStream,
                                           void* aClosure,
                                           const char* aFromSegment,
                                           uint32_t aToOffset,
                                           uint32_t aCount,
                                           uint32_t* aWriteCount)
{
  // This method could be called on any thread.

  MutableBlobStreamListener* self = static_cast<MutableBlobStreamListener*>(aClosure);
  MOZ_ASSERT(self->mStorage);

  nsresult rv = self->mStorage->Append(aFromSegment, aCount);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aWriteCount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
MutableBlobStreamListener::CheckListenerChain()
{
  return NS_OK;
}

} // namespace net
} // namespace mozilla
