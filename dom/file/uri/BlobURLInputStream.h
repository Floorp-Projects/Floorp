/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BlobURLInputStream_h
#define mozilla_dom_BlobURLInputStream_h

#include "mozilla/dom/BlobImpl.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsIInputStreamLength.h"

namespace mozilla::dom {

class BlobURL;
class BlobURLChannel;
class BlobURLInputStream final : public nsIAsyncInputStream,
                                 public nsIInputStreamLength,
                                 public nsIAsyncInputStreamLength,
                                 public nsIInputStreamCallback,
                                 public nsIInputStreamLengthCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMLENGTH
  NS_DECL_NSIASYNCINPUTSTREAMLENGTH
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIINPUTSTREAMLENGTHCALLBACK

  static already_AddRefed<nsIInputStream> Create(BlobURLChannel* const aChannel,
                                                 BlobURL* const aBlobURL);

  BlobURLInputStream(BlobURLChannel* const aChannel, nsACString& aBlobURLSpec);

 private:
  enum class State { INITIAL, READY, WAITING, CLOSED, ERROR };

  ~BlobURLInputStream();

  void WaitOnUnderlyingStream(const MutexAutoLock& aProofOfLock);

  // This method should only be used to call RetrieveBlobData in a different
  // thread
  void CallRetrieveBlobData();

  void RetrieveBlobData(const MutexAutoLock& aProofOfLock);

  nsresult StoreBlobImplStream(already_AddRefed<BlobImpl> aBlobImpl,
                               const MutexAutoLock& aProofOfLock);
  void NotifyWaitTargets(const MutexAutoLock& aProofOfLock);
  void ReleaseUnderlyingStream(const MutexAutoLock& aProofOfLock);

  RefPtr<BlobURLChannel> mChannel;
  const nsCString mBlobURLSpec;

  // Non-recursive mutex introduced in order to guard access to mState, mError
  // and mAsyncInputStream
  Mutex mStateMachineMutex MOZ_UNANNOTATED;
  State mState;
  // Stores the error code if stream is in error state
  nsresult mError;

  int64_t mBlobSize;

  nsCOMPtr<nsIAsyncInputStream> mAsyncInputStream;
  nsCOMPtr<nsIInputStreamCallback> mAsyncWaitCallback;
  nsCOMPtr<nsIEventTarget> mAsyncWaitTarget;
  uint32_t mAsyncWaitFlags;
  uint32_t mAsyncWaitRequestedCount;

  nsCOMPtr<nsIInputStreamLengthCallback> mAsyncLengthWaitCallback;
  nsCOMPtr<nsIEventTarget> mAsyncLengthWaitTarget;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_BlobURLInputStream_h */
