/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Fetch_h
#define mozilla_dom_Fetch_h

#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsProxyRelease.h"
#include "nsString.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/AbortSignal.h"
#include "mozilla/dom/BodyConsumer.h"
#include "mozilla/dom/BodyStream.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/FetchStreamReader.h"
#include "mozilla/dom/RequestBinding.h"

class nsIGlobalObject;
class nsIEventTarget;

namespace mozilla {
class ErrorResult;

namespace ipc {
class PrincipalInfo;
}  // namespace ipc

namespace dom {

class BlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString;
class
    BlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrReadableStreamOrUSVString;
class BlobImpl;
class InternalRequest;
class
    OwningBlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString;
struct ReadableStream;
class RequestOrUSVString;
class WorkerPrivate;

enum class CallerType : uint32_t;

already_AddRefed<Promise> FetchRequest(nsIGlobalObject* aGlobal,
                                       const RequestOrUSVString& aInput,
                                       const RequestInit& aInit,
                                       CallerType aCallerType,
                                       ErrorResult& aRv);

nsresult UpdateRequestReferrer(nsIGlobalObject* aGlobal,
                               InternalRequest* aRequest);

namespace fetch {
using BodyInit =
    BlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString;
using ResponseBodyInit =
    BlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrReadableStreamOrUSVString;
using OwningBodyInit =
    OwningBlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString;
};  // namespace fetch

/*
 * Creates an nsIInputStream based on the fetch specifications 'extract a byte
 * stream algorithm' - http://fetch.spec.whatwg.org/#concept-bodyinit-extract.
 * Stores content type in out param aContentType.
 */
nsresult ExtractByteStreamFromBody(const fetch::OwningBodyInit& aBodyInit,
                                   nsIInputStream** aStream,
                                   nsCString& aContentType,
                                   uint64_t& aContentLength);

/*
 * Non-owning version.
 */
nsresult ExtractByteStreamFromBody(const fetch::BodyInit& aBodyInit,
                                   nsIInputStream** aStream,
                                   nsCString& aContentType,
                                   uint64_t& aContentLength);

/*
 * Non-owning version. This method should go away when BodyInit will contain
 * ReadableStream.
 */
nsresult ExtractByteStreamFromBody(const fetch::ResponseBodyInit& aBodyInit,
                                   nsIInputStream** aStream,
                                   nsCString& aContentType,
                                   uint64_t& aContentLength);

/*
 * FetchBody's body consumption uses nsIInputStreamPump to read from the
 * underlying stream to a block of memory, which is then adopted by
 * ContinueConsumeBody() and converted to the right type based on the JS
 * function called.
 *
 * Use of the nsIInputStreamPump complicates things on the worker thread.
 * The solution used here is similar to WebSockets.
 * The difference is that we are only interested in completion and not data
 * events, and nsIInputStreamPump can only deliver completion on the main
 * thread.
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
class FetchBody : public BodyStreamHolder, public AbortFollower {
 public:
  using BodyStreamHolder::QueryInterface;

  NS_INLINE_DECL_REFCOUNTING_INHERITED(FetchBody, BodyStreamHolder)

  bool GetBodyUsed(ErrorResult& aRv) const;

  // For use in assertions. On success, returns true if the body is used, false
  // if not. On error, this sweeps the error under the rug and returns true.
  bool CheckBodyUsed() const;

  already_AddRefed<Promise> ArrayBuffer(JSContext* aCx, ErrorResult& aRv) {
    return ConsumeBody(aCx, BodyConsumer::CONSUME_ARRAYBUFFER, aRv);
  }

  already_AddRefed<Promise> Blob(JSContext* aCx, ErrorResult& aRv) {
    return ConsumeBody(aCx, BodyConsumer::CONSUME_BLOB, aRv);
  }

  already_AddRefed<Promise> FormData(JSContext* aCx, ErrorResult& aRv) {
    return ConsumeBody(aCx, BodyConsumer::CONSUME_FORMDATA, aRv);
  }

  already_AddRefed<Promise> Json(JSContext* aCx, ErrorResult& aRv) {
    return ConsumeBody(aCx, BodyConsumer::CONSUME_JSON, aRv);
  }

  already_AddRefed<Promise> Text(JSContext* aCx, ErrorResult& aRv) {
    return ConsumeBody(aCx, BodyConsumer::CONSUME_TEXT, aRv);
  }

  void GetBody(JSContext* aCx, JS::MutableHandle<JSObject*> aBodyOut,
               ErrorResult& aRv);

  void GetMimeType(nsACString& aMimeType);

  const nsACString& BodyBlobURISpec() const;

  const nsAString& BodyLocalPath() const;

  // If the body contains a ReadableStream body object, this method produces a
  // tee() of it.
  void MaybeTeeReadableStreamBody(JSContext* aCx,
                                  JS::MutableHandle<JSObject*> aBodyOut,
                                  FetchStreamReader** aStreamReader,
                                  nsIInputStream** aInputStream,
                                  ErrorResult& aRv);

  // Utility public methods accessed by various runnables.

  // This method _must_ be called in order to set the body as used. If the body
  // is a ReadableStream, this method will start reading the stream.
  // More in details, this method does:
  // 1) It uses an internal flag to track if the body is used.  This is tracked
  // separately from the ReadableStream disturbed state due to purely native
  // streams.
  // 2) If there is a ReadableStream reflector for the native stream it is
  // Locked.
  // 3) If there is a JS ReadableStream then we begin pumping it into the native
  // body stream.  This effectively locks and disturbs the stream.
  //
  // Note that JSContext is used only if there is a ReadableStream (this can
  // happen because the body is a ReadableStream or because attribute body has
  // already been used by content). If something goes wrong using
  // ReadableStream, errors will be reported via ErrorResult and not as JS
  // exceptions in JSContext. This is done in order to have a centralized error
  // reporting way.
  //
  // Exceptions generated when reading from the ReadableStream are directly sent
  // to the Console.
  void SetBodyUsed(JSContext* aCx, ErrorResult& aRv);

  // BodyStreamHolder
  void NullifyStream() override {
    mReadableStreamBody = nullptr;
    mReadableStreamReader = nullptr;
    mFetchStreamReader = nullptr;
  }

  void SetReadableStreamBody(JSObject* aBody) override {
    mReadableStreamBody = aBody;
  }

  JSObject* GetReadableStreamBody() override { return mReadableStreamBody; }

  void MarkAsRead() override { mBodyUsed = true; }

  virtual AbortSignalImpl* GetSignalImpl() const = 0;

  // AbortFollower
  void RunAbortAlgorithm() override;

  already_AddRefed<Promise> ConsumeBody(JSContext* aCx,
                                        BodyConsumer::ConsumeType aType,
                                        ErrorResult& aRv);

 protected:
  nsCOMPtr<nsIGlobalObject> mOwner;

  // Always set whenever the FetchBody is created on the worker thread.
  WorkerPrivate* mWorkerPrivate;

  // This is the ReadableStream exposed to content. It's underlying source is a
  // BodyStream object.
  JS::Heap<JSObject*> mReadableStreamBody;

  // This is the Reader used to retrieve data from the body.
  JS::Heap<JSObject*> mReadableStreamReader;
  RefPtr<FetchStreamReader> mFetchStreamReader;

  explicit FetchBody(nsIGlobalObject* aOwner);

  virtual ~FetchBody();

  void SetReadableStreamBody(JSContext* aCx, JSObject* aBody);

 private:
  Derived* DerivedClass() const {
    return static_cast<Derived*>(const_cast<FetchBody*>(this));
  }

  void LockStream(JSContext* aCx, JS::HandleObject aStream, ErrorResult& aRv);

  bool IsOnTargetThread() { return NS_IsMainThread() == !mWorkerPrivate; }

  void AssertIsOnTargetThread() { MOZ_ASSERT(IsOnTargetThread()); }

  // Only ever set once, always on target thread.
  bool mBodyUsed;

  // The main-thread event target for runnable dispatching.
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;
};

class EmptyBody final : public FetchBody<EmptyBody> {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(EmptyBody,
                                                         FetchBody<EmptyBody>)

 public:
  static already_AddRefed<EmptyBody> Create(
      nsIGlobalObject* aGlobal, mozilla::ipc::PrincipalInfo* aPrincipalInfo,
      AbortSignalImpl* aAbortSignalImpl, const nsACString& aMimeType,
      ErrorResult& aRv);

  nsIGlobalObject* GetParentObject() const { return mOwner; }

  AbortSignalImpl* GetSignalImpl() const override { return mAbortSignalImpl; }

  const UniquePtr<mozilla::ipc::PrincipalInfo>& GetPrincipalInfo() const {
    return mPrincipalInfo;
  }

  void GetMimeType(nsACString& aMimeType) { aMimeType = mMimeType; }

  void GetBody(nsIInputStream** aStream, int64_t* aBodyLength = nullptr);

  using FetchBody::BodyBlobURISpec;

  const nsACString& BodyBlobURISpec() const { return EmptyCString(); }

  using FetchBody::BodyLocalPath;

  const nsAString& BodyLocalPath() const { return EmptyString(); }

 private:
  EmptyBody(nsIGlobalObject* aGlobal,
            mozilla::ipc::PrincipalInfo* aPrincipalInfo,
            AbortSignalImpl* aAbortSignalImpl, const nsACString& aMimeType,
            already_AddRefed<nsIInputStream> aBodyStream);

  ~EmptyBody();

  UniquePtr<mozilla::ipc::PrincipalInfo> mPrincipalInfo;
  RefPtr<AbortSignalImpl> mAbortSignalImpl;
  nsCString mMimeType;
  nsCOMPtr<nsIInputStream> mBodyStream;
};
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Fetch_h
