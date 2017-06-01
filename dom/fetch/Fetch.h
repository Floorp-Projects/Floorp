/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Fetch_h
#define mozilla_dom_Fetch_h

#include "nsAutoPtr.h"
#include "nsIInputStreamPump.h"
#include "nsIStreamLoader.h"

#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsProxyRelease.h"
#include "nsString.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/workers/bindings/WorkerHolder.h"

class nsIGlobalObject;
class nsIEventTarget;

namespace mozilla {
namespace dom {

class BlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString;
class BlobImpl;
class InternalRequest;
class OwningBlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString;
class RequestOrUSVString;
enum class CallerType : uint32_t;

namespace workers {
class WorkerPrivate;
} // namespace workers

already_AddRefed<Promise>
FetchRequest(nsIGlobalObject* aGlobal, const RequestOrUSVString& aInput,
             const RequestInit& aInit, CallerType aCallerType,
             ErrorResult& aRv);

nsresult
UpdateRequestReferrer(nsIGlobalObject* aGlobal, InternalRequest* aRequest);

namespace fetch {
typedef BlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString BodyInit;
typedef OwningBlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString OwningBodyInit;
};

/*
 * Creates an nsIInputStream based on the fetch specifications 'extract a byte
 * stream algorithm' - http://fetch.spec.whatwg.org/#concept-bodyinit-extract.
 * Stores content type in out param aContentType.
 */
nsresult
ExtractByteStreamFromBody(const fetch::OwningBodyInit& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType,
                          uint64_t& aContentLength);

/*
 * Non-owning version.
 */
nsresult
ExtractByteStreamFromBody(const fetch::BodyInit& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType,
                          uint64_t& aContentLength);

template <class Derived> class FetchBodyWorkerHolder;

/*
 * FetchBody's body consumption uses nsIInputStreamPump to read from the
 * underlying stream to a block of memory, which is then adopted by
 * ContinueConsumeBody() and converted to the right type based on the JS
 * function called.
 *
 * Use of the nsIInputStreamPump complicates things on the worker thread.
 * The solution used here is similar to WebSockets.
 * The difference is that we are only interested in completion and not data
 * events, and nsIInputStreamPump can only deliver completion on the main thread.
 *
 * Before starting the pump on the main thread, we addref the FetchBody to keep
 * it alive. Then we add a feature, to track the status of the worker.
 *
 * ContinueConsumeBody() is the function that cleans things up in both success
 * and error conditions and so all callers call it with the appropriate status.
 *
 * Once the read is initiated on the main thread there are two possibilities.
 *
 * 1) Pump finishes before worker has finished Running.
 *    In this case we adopt the data and dispatch a runnable to the worker,
 *    which derefs FetchBody and removes the feature and resolves the Promise.
 *
 * 2) Pump still working while worker has stopped Running.
 *    The feature is Notify()ed and ContinueConsumeBody() is called with
 *    NS_BINDING_ABORTED. We first Cancel() the pump using a sync runnable to
 *    ensure that mFetchBody remains alive (since mConsumeBodyPump is strongly
 *    held by it) until pump->Cancel() is called. OnStreamComplete() will not
 *    do anything if the error code is NS_BINDING_ABORTED, so we don't have to
 *    worry about keeping anything alive.
 *
 * The pump is always released on the main thread.
 */
template <class Derived>
class FetchBody {
public:
  bool
  BodyUsed() const { return mBodyUsed; }

  already_AddRefed<Promise>
  ArrayBuffer(ErrorResult& aRv)
  {
    return ConsumeBody(CONSUME_ARRAYBUFFER, aRv);
  }

  already_AddRefed<Promise>
  Blob(ErrorResult& aRv)
  {
    return ConsumeBody(CONSUME_BLOB, aRv);
  }

  already_AddRefed<Promise>
  FormData(ErrorResult& aRv)
  {
    return ConsumeBody(CONSUME_FORMDATA, aRv);
  }

  already_AddRefed<Promise>
  Json(ErrorResult& aRv)
  {
    return ConsumeBody(CONSUME_JSON, aRv);
  }

  already_AddRefed<Promise>
  Text(ErrorResult& aRv)
  {
    return ConsumeBody(CONSUME_TEXT, aRv);
  }

  // Utility public methods accessed by various runnables.
  void
  BeginConsumeBodyMainThread();

  void
  ContinueConsumeBody(nsresult aStatus, uint32_t aLength, uint8_t* aResult);

  void
  ContinueConsumeBlobBody(BlobImpl* aBlobImpl);

  void
  CancelPump();

  void
  SetBodyUsed()
  {
    mBodyUsed = true;
  }

  // Always set whenever the FetchBody is created on the worker thread.
  workers::WorkerPrivate* mWorkerPrivate;

  // Set when consuming the body is attempted on a worker.
  // Unset when consumption is done/aborted.
  nsAutoPtr<workers::WorkerHolder> mWorkerHolder;

protected:
  nsCOMPtr<nsIGlobalObject> mOwner;

  explicit FetchBody(nsIGlobalObject* aOwner);

  virtual ~FetchBody();

  void
  SetMimeType();
private:
  enum ConsumeType
  {
    CONSUME_ARRAYBUFFER,
    CONSUME_BLOB,
    CONSUME_FORMDATA,
    CONSUME_JSON,
    CONSUME_TEXT,
  };

  Derived*
  DerivedClass() const
  {
    return static_cast<Derived*>(const_cast<FetchBody*>(this));
  }

  nsresult
  BeginConsumeBody();

  already_AddRefed<Promise>
  ConsumeBody(ConsumeType aType, ErrorResult& aRv);

  bool
  AddRefObject();

  void
  ReleaseObject();

  bool
  RegisterWorkerHolder();

  void
  UnregisterWorkerHolder();

  bool
  IsOnTargetThread()
  {
    return NS_IsMainThread() == !mWorkerPrivate;
  }

  void
  AssertIsOnTargetThread()
  {
    MOZ_ASSERT(IsOnTargetThread());
  }

  // Only ever set once, always on target thread.
  bool mBodyUsed;
  nsCString mMimeType;

  // Only touched on target thread.
  ConsumeType mConsumeType;
  RefPtr<Promise> mConsumePromise;
#ifdef DEBUG
  bool mReadDone;
#endif

  nsMainThreadPtrHandle<nsIInputStreamPump> mConsumeBodyPump;

  // The main-thread event target for runnable dispatching.
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Fetch_h
