/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchConsumer_h
#define mozilla_dom_FetchConsumer_h

#include "Fetch.h"
#include "mozilla/dom/AbortSignal.h"
#include "mozilla/dom/MutableBlobStorage.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

class nsIThread;

namespace mozilla {
namespace dom {

class Promise;
class ThreadSafeWorkerRef;

template <class Derived> class FetchBody;

// FetchBody is not thread-safe but we need to move it around threads.  In order
// to keep it alive all the time, we use a ThreadSafeWorkerRef, if created on
// workers.
template <class Derived>
class FetchBodyConsumer final : public nsIObserver
                              , public nsSupportsWeakReference
                              , public AbortFollower
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static already_AddRefed<Promise>
  Create(nsIGlobalObject* aGlobal,
         nsIEventTarget* aMainThreadEventTarget,
         FetchBody<Derived>* aBody,
         AbortSignalImpl* aSignalImpl,
         FetchConsumeType aType,
         ErrorResult& aRv);

  void
  ReleaseObject();

  void
  BeginConsumeBodyMainThread(ThreadSafeWorkerRef* aWorkerRef);

  void
  OnBlobResult(Blob* aBlob, ThreadSafeWorkerRef* aWorkerRef = nullptr);

  void
  ContinueConsumeBody(nsresult aStatus, uint32_t aLength, uint8_t* aResult,
                      bool aShuttingDown = false);

  void
  ContinueConsumeBlobBody(BlobImpl* aBlobImpl, bool aShuttingDown = false);

  void
  ShutDownMainThreadConsuming();

  void
  NullifyConsumeBodyPump()
  {
    mShuttingDown = true;
    mConsumeBodyPump = nullptr;
  }

  // AbortFollower
  void Abort() override;

private:
  FetchBodyConsumer(nsIEventTarget* aMainThreadEventTarget,
                    nsIGlobalObject* aGlobalObject,
                    FetchBody<Derived>* aBody,
                    nsIInputStream* aBodyStream,
                    Promise* aPromise,
                    FetchConsumeType aType);

  ~FetchBodyConsumer();

  nsresult
  GetBodyLocalFile(nsIFile** aFile) const;

  void
  AssertIsOnTargetThread() const;

  nsCOMPtr<nsIThread> mTargetThread;
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;

#ifdef DEBUG
  // This is used only to check if the body has been correctly consumed.
  RefPtr<FetchBody<Derived>> mBody;
#endif

  // This is nullified when the consuming of the body starts.
  nsCOMPtr<nsIInputStream> mBodyStream;

  MutableBlobStorage::MutableBlobStorageType mBlobStorageType;
  nsCString mBodyMimeType;

  PathString mBodyLocalPath;

  nsCOMPtr<nsIGlobalObject> mGlobal;

  // Touched on the main-thread only.
  nsCOMPtr<nsIInputStreamPump> mConsumeBodyPump;

  // Only ever set once, always on target thread.
  FetchConsumeType mConsumeType;
  RefPtr<Promise> mConsumePromise;

  // touched only on the target thread.
  bool mBodyConsumed;

  // touched only on the main-thread.
  bool mShuttingDown;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FetchConsumer_h
