/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BodyConsumer_h
#define mozilla_dom_BodyConsumer_h

#include "mozilla/dom/AbortSignal.h"
#include "mozilla/dom/MutableBlobStorage.h"
#include "nsIInputStreamPump.h"
#include "nsNetUtil.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

class nsIThread;

namespace mozilla {
namespace dom {

class Promise;
class ThreadSafeWorkerRef;

// In order to keep alive the object all the time, we use a ThreadSafeWorkerRef,
// if created on workers.
class BodyConsumer final : public nsIObserver,
                           public nsSupportsWeakReference,
                           public AbortFollower {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  enum ConsumeType {
    CONSUME_ARRAYBUFFER,
    CONSUME_BLOB,
    CONSUME_FORMDATA,
    CONSUME_JSON,
    CONSUME_TEXT,
  };

  /**
   * Returns a promise which will be resolved when the body is completely
   * consumed and converted to the wanted type (See ConsumeType).
   *
   * @param aGlobal the global to construct the Promise.
   * @param aMainThreadEventTarget the main-thread event target. The reading
   *          needs to start on the main-thread because of nsIInputStreamPump.
   * @param aBodyStream the stream to read.
   * @param aSignalImpl an AbortSignal object. Optional.
   * @param aType the consume type.
   * @param aBodyBlobURISpec this is used only if the consume type is
   *          CONSUME_BLOB. Optional.
   * @param aBodyLocalPath local path in case the blob is created from a local
   *          file. Used only by CONSUME_BLOB. Optional.
   * @param aBodyMimeType the mime-type for blob. Used only by CONSUME_BLOB.
   *          Optional.
   * @param aBlobStorageType Blobs can be saved in temporary file. This is the
   *          type of blob storage to use. Used only by CONSUME_BLOB.
   * @param aRv An ErrorResult.
   */
  static already_AddRefed<Promise> Create(
      nsIGlobalObject* aGlobal, nsIEventTarget* aMainThreadEventTarget,
      nsIInputStream* aBodyStream, AbortSignalImpl* aSignalImpl,
      ConsumeType aType, const nsACString& aBodyBlobURISpec,
      const nsAString& aBodyLocalPath, const nsACString& aBodyMimeType,
      MutableBlobStorage::MutableBlobStorageType aBlobStorageType,
      ErrorResult& aRv);

  void ReleaseObject();

  void BeginConsumeBodyMainThread(ThreadSafeWorkerRef* aWorkerRef);

  void OnBlobResult(BlobImpl* aBlobImpl,
                    ThreadSafeWorkerRef* aWorkerRef = nullptr);

  void ContinueConsumeBody(nsresult aStatus, uint32_t aLength, uint8_t* aResult,
                           bool aShuttingDown = false);

  void ContinueConsumeBlobBody(BlobImpl* aBlobImpl, bool aShuttingDown = false);

  void DispatchContinueConsumeBlobBody(BlobImpl* aBlobImpl,
                                       ThreadSafeWorkerRef* aWorkerRef);

  void ShutDownMainThreadConsuming();

  void NullifyConsumeBodyPump() {
    mShuttingDown = true;
    mConsumeBodyPump = nullptr;
  }

  // AbortFollower
  void Abort() override;

 private:
  BodyConsumer(nsIEventTarget* aMainThreadEventTarget,
               nsIGlobalObject* aGlobalObject, nsIInputStream* aBodyStream,
               Promise* aPromise, ConsumeType aType,
               const nsACString& aBodyBlobURISpec,
               const nsAString& aBodyLocalPath, const nsACString& aBodyMimeType,
               MutableBlobStorage::MutableBlobStorageType aBlobStorageType);

  ~BodyConsumer();

  nsresult GetBodyLocalFile(nsIFile** aFile) const;

  void AssertIsOnTargetThread() const;

  nsCOMPtr<nsIThread> mTargetThread;
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;

  // This is nullified when the consuming of the body starts.
  nsCOMPtr<nsIInputStream> mBodyStream;

  MutableBlobStorage::MutableBlobStorageType mBlobStorageType;
  nsCString mBodyMimeType;

  nsCString mBodyBlobURISpec;
  nsString mBodyLocalPath;

  nsCOMPtr<nsIGlobalObject> mGlobal;

  // Touched on the main-thread only.
  nsCOMPtr<nsIInputStreamPump> mConsumeBodyPump;

  // Only ever set once, always on target thread.
  ConsumeType mConsumeType;
  RefPtr<Promise> mConsumePromise;

  // touched only on the target thread.
  bool mBodyConsumed;

  // touched only on the main-thread.
  bool mShuttingDown;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BodyConsumer_h
