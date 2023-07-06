/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BodyConsumer.h"

#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/BodyUtil.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/FileCreatorHelper.h"
#include "mozilla/dom/MutableBlobStreamListener.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TaskQueue.h"
#include "nsComponentManagerUtils.h"
#include "nsIFile.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIStreamLoader.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsIInputStream.h"

// Undefine the macro of CreateFile to avoid FileCreatorHelper#CreateFile being
// replaced by FileCreatorHelper#CreateFileW.
#ifdef CreateFile
#  undef CreateFile
#endif

namespace mozilla::dom {

namespace {

class BeginConsumeBodyRunnable final : public Runnable {
 public:
  BeginConsumeBodyRunnable(BodyConsumer* aConsumer,
                           ThreadSafeWorkerRef* aWorkerRef)
      : Runnable("BeginConsumeBodyRunnable"),
        mBodyConsumer(aConsumer),
        mWorkerRef(aWorkerRef) {}

  NS_IMETHOD
  Run() override {
    mBodyConsumer->BeginConsumeBodyMainThread(mWorkerRef);
    return NS_OK;
  }

 private:
  RefPtr<BodyConsumer> mBodyConsumer;
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

/*
 * Called on successfully reading the complete stream.
 */
class ContinueConsumeBodyRunnable final : public MainThreadWorkerRunnable {
  RefPtr<BodyConsumer> mBodyConsumer;
  nsresult mStatus;
  uint32_t mLength;
  uint8_t* mResult;

 public:
  ContinueConsumeBodyRunnable(BodyConsumer* aBodyConsumer,
                              WorkerPrivate* aWorkerPrivate, nsresult aStatus,
                              uint32_t aLength, uint8_t* aResult)
      : MainThreadWorkerRunnable(aWorkerPrivate),
        mBodyConsumer(aBodyConsumer),
        mStatus(aStatus),
        mLength(aLength),
        mResult(aResult) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mBodyConsumer->ContinueConsumeBody(mStatus, mLength, mResult);
    return true;
  }
};

// ControlRunnable used to complete the releasing of resources on the worker
// thread when already shutting down.
class AbortConsumeBodyControlRunnable final
    : public MainThreadWorkerControlRunnable {
  RefPtr<BodyConsumer> mBodyConsumer;

 public:
  AbortConsumeBodyControlRunnable(BodyConsumer* aBodyConsumer,
                                  WorkerPrivate* aWorkerPrivate)
      : MainThreadWorkerControlRunnable(aWorkerPrivate),
        mBodyConsumer(aBodyConsumer) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mBodyConsumer->ContinueConsumeBody(NS_BINDING_ABORTED, 0, nullptr,
                                       true /* shutting down */);
    return true;
  }
};

/*
 * In case of failure to create a stream pump or dispatch stream completion to
 * worker, ensure we cleanup properly. Thread agnostic.
 */
class MOZ_STACK_CLASS AutoFailConsumeBody final {
 public:
  AutoFailConsumeBody(BodyConsumer* aBodyConsumer,
                      ThreadSafeWorkerRef* aWorkerRef)
      : mBodyConsumer(aBodyConsumer), mWorkerRef(aWorkerRef) {}

  ~AutoFailConsumeBody() {
    AssertIsOnMainThread();

    if (!mBodyConsumer) {
      return;
    }

    // Web Worker
    if (mWorkerRef) {
      RefPtr<AbortConsumeBodyControlRunnable> r =
          new AbortConsumeBodyControlRunnable(mBodyConsumer,
                                              mWorkerRef->Private());
      if (!r->Dispatch()) {
        MOZ_CRASH("We are going to leak");
      }
      return;
    }

    // Main-thread
    mBodyConsumer->ContinueConsumeBody(NS_ERROR_FAILURE, 0, nullptr);
  }

  void DontFail() { mBodyConsumer = nullptr; }

 private:
  RefPtr<BodyConsumer> mBodyConsumer;
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

/*
 * Called on successfully reading the complete stream for Blob.
 */
class ContinueConsumeBlobBodyRunnable final : public MainThreadWorkerRunnable {
  RefPtr<BodyConsumer> mBodyConsumer;
  RefPtr<BlobImpl> mBlobImpl;

 public:
  ContinueConsumeBlobBodyRunnable(BodyConsumer* aBodyConsumer,
                                  WorkerPrivate* aWorkerPrivate,
                                  BlobImpl* aBlobImpl)
      : MainThreadWorkerRunnable(aWorkerPrivate),
        mBodyConsumer(aBodyConsumer),
        mBlobImpl(aBlobImpl) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mBlobImpl);
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mBodyConsumer->ContinueConsumeBlobBody(mBlobImpl);
    return true;
  }
};

// ControlRunnable used to complete the releasing of resources on the worker
// thread when already shutting down.
class AbortConsumeBlobBodyControlRunnable final
    : public MainThreadWorkerControlRunnable {
  RefPtr<BodyConsumer> mBodyConsumer;

 public:
  AbortConsumeBlobBodyControlRunnable(BodyConsumer* aBodyConsumer,
                                      WorkerPrivate* aWorkerPrivate)
      : MainThreadWorkerControlRunnable(aWorkerPrivate),
        mBodyConsumer(aBodyConsumer) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mBodyConsumer->ContinueConsumeBlobBody(nullptr, true /* shutting down */);
    return true;
  }
};

class ConsumeBodyDoneObserver final : public nsIStreamLoaderObserver,
                                      public MutableBlobStorageCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ConsumeBodyDoneObserver(BodyConsumer* aBodyConsumer,
                          ThreadSafeWorkerRef* aWorkerRef)
      : mBodyConsumer(aBodyConsumer), mWorkerRef(aWorkerRef) {}

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aCtxt,
                   nsresult aStatus, uint32_t aResultLength,
                   const uint8_t* aResult) override {
    MOZ_ASSERT(NS_IsMainThread());

    // The loading is completed. Let's nullify the pump before continuing the
    // consuming of the body.
    mBodyConsumer->NullifyConsumeBodyPump();

    uint8_t* nonconstResult = const_cast<uint8_t*>(aResult);

    // Main-thread.
    if (!mWorkerRef) {
      mBodyConsumer->ContinueConsumeBody(aStatus, aResultLength,
                                         nonconstResult);
      // The caller is responsible for data.
      return NS_SUCCESS_ADOPTED_DATA;
    }

    // Web Worker.
    {
      RefPtr<ContinueConsumeBodyRunnable> r = new ContinueConsumeBodyRunnable(
          mBodyConsumer, mWorkerRef->Private(), aStatus, aResultLength,
          nonconstResult);
      if (r->Dispatch()) {
        // The caller is responsible for data.
        return NS_SUCCESS_ADOPTED_DATA;
      }
    }

    // The worker is shutting down. Let's use a control runnable to complete the
    // shutting down procedure.

    RefPtr<AbortConsumeBodyControlRunnable> r =
        new AbortConsumeBodyControlRunnable(mBodyConsumer,
                                            mWorkerRef->Private());
    if (NS_WARN_IF(!r->Dispatch())) {
      return NS_ERROR_FAILURE;
    }

    // We haven't taken ownership of the data.
    return NS_OK;
  }

  virtual void BlobStoreCompleted(MutableBlobStorage* aBlobStorage,
                                  BlobImpl* aBlobImpl, nsresult aRv) override {
    // On error.
    if (NS_FAILED(aRv)) {
      OnStreamComplete(nullptr, nullptr, aRv, 0, nullptr);
      return;
    }

    // The loading is completed. Let's nullify the pump before continuing the
    // consuming of the body.
    mBodyConsumer->NullifyConsumeBodyPump();

    mBodyConsumer->OnBlobResult(aBlobImpl, mWorkerRef);
  }

 private:
  ~ConsumeBodyDoneObserver() = default;

  RefPtr<BodyConsumer> mBodyConsumer;
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

NS_IMPL_ISUPPORTS(ConsumeBodyDoneObserver, nsIStreamLoaderObserver)

}  // namespace

/* static */ already_AddRefed<Promise> BodyConsumer::Create(
    nsIGlobalObject* aGlobal, nsISerialEventTarget* aMainThreadEventTarget,
    nsIInputStream* aBodyStream, AbortSignalImpl* aSignalImpl,
    ConsumeType aType, const nsACString& aBodyBlobURISpec,
    const nsAString& aBodyLocalPath, const nsACString& aBodyMimeType,
    const nsACString& aMixedCaseMimeType,
    MutableBlobStorage::MutableBlobStorageType aBlobStorageType,
    ErrorResult& aRv) {
  MOZ_ASSERT(aBodyStream);
  MOZ_ASSERT(aMainThreadEventTarget);

  RefPtr<Promise> promise = Promise::Create(aGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<BodyConsumer> consumer =
      new BodyConsumer(aMainThreadEventTarget, aGlobal, aBodyStream, promise,
                       aType, aBodyBlobURISpec, aBodyLocalPath, aBodyMimeType,
                       aMixedCaseMimeType, aBlobStorageType);

  RefPtr<ThreadSafeWorkerRef> workerRef;

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    RefPtr<StrongWorkerRef> strongWorkerRef =
        StrongWorkerRef::Create(workerPrivate, "BodyConsumer", [consumer]() {
          consumer->mConsumePromise = nullptr;
          consumer->mBodyConsumed = true;
          consumer->ReleaseObject();
          consumer->ShutDownMainThreadConsuming();
        });
    if (NS_WARN_IF(!strongWorkerRef)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    workerRef = new ThreadSafeWorkerRef(strongWorkerRef);
  } else {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (NS_WARN_IF(!os)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    aRv = os->AddObserver(consumer, DOM_WINDOW_DESTROYED_TOPIC, true);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    aRv = os->AddObserver(consumer, DOM_WINDOW_FROZEN_TOPIC, true);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  nsCOMPtr<nsIRunnable> r = new BeginConsumeBodyRunnable(consumer, workerRef);
  aRv = aMainThreadEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (aSignalImpl) {
    consumer->Follow(aSignalImpl);
  }

  return promise.forget();
}

void BodyConsumer::ReleaseObject() {
  AssertIsOnTargetThread();

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
      os->RemoveObserver(this, DOM_WINDOW_FROZEN_TOPIC);
    }
  }

  mGlobal = nullptr;

  Unfollow();
}

BodyConsumer::BodyConsumer(
    nsISerialEventTarget* aMainThreadEventTarget,
    nsIGlobalObject* aGlobalObject, nsIInputStream* aBodyStream,
    Promise* aPromise, ConsumeType aType, const nsACString& aBodyBlobURISpec,
    const nsAString& aBodyLocalPath, const nsACString& aBodyMimeType,
    const nsACString& aMixedCaseMimeType,
    MutableBlobStorage::MutableBlobStorageType aBlobStorageType)
    : mTargetThread(NS_GetCurrentThread()),
      mMainThreadEventTarget(aMainThreadEventTarget),
      mBodyStream(aBodyStream),
      mBlobStorageType(aBlobStorageType),
      mBodyMimeType(aBodyMimeType),
      mMixedCaseMimeType(aMixedCaseMimeType),
      mBodyBlobURISpec(aBodyBlobURISpec),
      mBodyLocalPath(aBodyLocalPath),
      mGlobal(aGlobalObject),
      mConsumeType(aType),
      mConsumePromise(aPromise),
      mBodyConsumed(false),
      mShuttingDown(false) {
  MOZ_ASSERT(aMainThreadEventTarget);
  MOZ_ASSERT(aBodyStream);
  MOZ_ASSERT(aPromise);
}

BodyConsumer::~BodyConsumer() = default;

void BodyConsumer::AssertIsOnTargetThread() const {
  MOZ_ASSERT(NS_GetCurrentThread() == mTargetThread);
}

namespace {

class FileCreationHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static void Create(Promise* aPromise, BodyConsumer* aConsumer,
                     ThreadSafeWorkerRef* aWorkerRef) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aPromise);

    RefPtr<FileCreationHandler> handler =
        new FileCreationHandler(aConsumer, aWorkerRef);
    aPromise->AppendNativeHandler(handler);
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    AssertIsOnMainThread();

    if (NS_WARN_IF(!aValue.isObject())) {
      mConsumer->OnBlobResult(nullptr, mWorkerRef);
      return;
    }

    RefPtr<Blob> blob;
    if (NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Blob, &aValue.toObject(), blob)))) {
      mConsumer->OnBlobResult(nullptr, mWorkerRef);
      return;
    }

    mConsumer->OnBlobResult(blob->Impl(), mWorkerRef);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    AssertIsOnMainThread();

    mConsumer->OnBlobResult(nullptr, mWorkerRef);
  }

 private:
  FileCreationHandler(BodyConsumer* aConsumer, ThreadSafeWorkerRef* aWorkerRef)
      : mConsumer(aConsumer), mWorkerRef(aWorkerRef) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aConsumer);
  }

  ~FileCreationHandler() = default;

  RefPtr<BodyConsumer> mConsumer;
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

NS_IMPL_ISUPPORTS0(FileCreationHandler)

}  // namespace

nsresult BodyConsumer::GetBodyLocalFile(nsIFile** aFile) const {
  AssertIsOnMainThread();

  if (!mBodyLocalPath.Length()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = file->InitWithPath(mBodyLocalPath);
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  bool isDir;
  rv = file->IsDirectory(&isDir);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isDir) {
    return NS_ERROR_FILE_IS_DIRECTORY;
  }

  file.forget(aFile);
  return NS_OK;
}

/*
 * BeginConsumeBodyMainThread() will automatically reject the consume promise
 * and clean up on any failures, so there is no need for callers to do so,
 * reflected in a lack of error return code.
 */
void BodyConsumer::BeginConsumeBodyMainThread(ThreadSafeWorkerRef* aWorkerRef) {
  AssertIsOnMainThread();

  AutoFailConsumeBody autoReject(this, aWorkerRef);

  if (mShuttingDown) {
    // We haven't started yet, but we have been terminated. AutoFailConsumeBody
    // will dispatch a runnable to release resources.
    return;
  }

  if (mConsumeType == CONSUME_BLOB) {
    nsresult rv;

    // If we're trying to consume a blob, and the request was for a blob URI,
    // then just consume that URI's blob instance.
    if (!mBodyBlobURISpec.IsEmpty()) {
      RefPtr<BlobImpl> blobImpl;
      rv = NS_GetBlobForBlobURISpec(mBodyBlobURISpec, getter_AddRefs(blobImpl));
      if (NS_WARN_IF(NS_FAILED(rv)) || !blobImpl) {
        return;
      }
      autoReject.DontFail();
      DispatchContinueConsumeBlobBody(blobImpl, aWorkerRef);
      return;
    }

    // If we're trying to consume a blob, and the request was for a local
    // file, then generate and return a File blob.
    nsCOMPtr<nsIFile> file;
    rv = GetBodyLocalFile(getter_AddRefs(file));
    if (!NS_WARN_IF(NS_FAILED(rv)) && file && !aWorkerRef) {
      ChromeFilePropertyBag bag;
      CopyUTF8toUTF16(mBodyMimeType, bag.mType);

      ErrorResult error;
      RefPtr<Promise> promise =
          FileCreatorHelper::CreateFile(mGlobal, file, bag, true, error);
      if (NS_WARN_IF(error.Failed())) {
        return;
      }

      autoReject.DontFail();
      FileCreationHandler::Create(promise, this, aWorkerRef);
      return;
    }
  }

  nsCOMPtr<nsIInputStreamPump> pump;
  nsresult rv =
      NS_NewInputStreamPump(getter_AddRefs(pump), mBodyStream.forget(), 0, 0,
                            false, mMainThreadEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RefPtr<ConsumeBodyDoneObserver> p =
      new ConsumeBodyDoneObserver(this, aWorkerRef);

  nsCOMPtr<nsIStreamListener> listener;
  if (mConsumeType == CONSUME_BLOB) {
    listener = new MutableBlobStreamListener(mBlobStorageType, mBodyMimeType, p,
                                             mMainThreadEventTarget);
  } else {
    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), p);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    listener = loader;
  }

  rv = pump->AsyncRead(listener);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Now that everything succeeded, we can assign the pump to a pointer that
  // stays alive for the lifetime of the BodyConsumer.
  mConsumeBodyPump = pump;

  // It is ok for retargeting to fail and reads to happen on the main thread.
  autoReject.DontFail();

  // Try to retarget, otherwise fall back to main thread.
  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(pump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    RefPtr<TaskQueue> queue =
        TaskQueue::Create(sts.forget(), "BodyConsumer STS Delivery Queue");
    rv = rr->RetargetDeliveryTo(queue);
    if (NS_FAILED(rv)) {
      NS_WARNING("Retargeting failed");
    }
  }
}

/*
 * OnBlobResult() is called when a blob body is ready to be consumed (when its
 * network transfer completes in BeginConsumeBodyRunnable or its local File has
 * been wrapped by FileCreationHandler). The blob is sent to the target thread
 * and ContinueConsumeBody is called.
 */
void BodyConsumer::OnBlobResult(BlobImpl* aBlobImpl,
                                ThreadSafeWorkerRef* aWorkerRef) {
  AssertIsOnMainThread();

  DispatchContinueConsumeBlobBody(aBlobImpl, aWorkerRef);
}

void BodyConsumer::DispatchContinueConsumeBlobBody(
    BlobImpl* aBlobImpl, ThreadSafeWorkerRef* aWorkerRef) {
  AssertIsOnMainThread();

  // Main-thread.
  if (!aWorkerRef) {
    if (aBlobImpl) {
      ContinueConsumeBlobBody(aBlobImpl);
    } else {
      ContinueConsumeBody(NS_ERROR_DOM_ABORT_ERR, 0, nullptr);
    }
    return;
  }

  // Web Worker.
  if (aBlobImpl) {
    RefPtr<ContinueConsumeBlobBodyRunnable> r =
        new ContinueConsumeBlobBodyRunnable(this, aWorkerRef->Private(),
                                            aBlobImpl);

    if (r->Dispatch()) {
      return;
    }
  } else {
    RefPtr<ContinueConsumeBodyRunnable> r = new ContinueConsumeBodyRunnable(
        this, aWorkerRef->Private(), NS_ERROR_DOM_ABORT_ERR, 0, nullptr);

    if (r->Dispatch()) {
      return;
    }
  }

  // The worker is shutting down. Let's use a control runnable to complete the
  // shutting down procedure.

  RefPtr<AbortConsumeBlobBodyControlRunnable> r =
      new AbortConsumeBlobBodyControlRunnable(this, aWorkerRef->Private());

  Unused << NS_WARN_IF(!r->Dispatch());
}

/*
 * ContinueConsumeBody() is to be called on the target thread whenever the
 * final result of the fetch is known. The fetch promise is resolved or
 * rejected based on whether the fetch succeeded, and the body can be
 * converted into the expected type of JS object.
 */
void BodyConsumer::ContinueConsumeBody(nsresult aStatus, uint32_t aResultLength,
                                       uint8_t* aResult, bool aShuttingDown) {
  AssertIsOnTargetThread();

  // This makes sure that we free the data correctly.
  UniquePtr<uint8_t[], JS::FreePolicy> resultPtr{aResult};

  if (mBodyConsumed) {
    return;
  }
  mBodyConsumed = true;

  MOZ_ASSERT(mConsumePromise);
  RefPtr<Promise> localPromise = std::move(mConsumePromise);

  RefPtr<BodyConsumer> self = this;
  auto autoReleaseObject =
      mozilla::MakeScopeExit([self] { self->ReleaseObject(); });

  if (aShuttingDown) {
    // If shutting down, we don't want to resolve any promise.
    return;
  }

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    // Per
    // https://fetch.spec.whatwg.org/#concept-read-all-bytes-from-readablestream
    // Decoding errors should reject with a TypeError
    if (aStatus == NS_ERROR_INVALID_CONTENT_ENCODING) {
      localPromise->MaybeRejectWithTypeError<MSG_DOM_DECODING_FAILED>();
    } else if (aStatus == NS_ERROR_DOM_WRONG_TYPE_ERR) {
      localPromise->MaybeRejectWithTypeError<MSG_FETCH_BODY_WRONG_TYPE>();
    } else {
      localPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }
  }

  // Don't warn here since we warned above.
  if (NS_FAILED(aStatus)) {
    return;
  }

  // Finish successfully consuming body according to type.
  MOZ_ASSERT(resultPtr);

  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    localPromise->MaybeReject(NS_ERROR_UNEXPECTED);
    return;
  }

  JSContext* cx = jsapi.cx();
  ErrorResult error;

  switch (mConsumeType) {
    case CONSUME_ARRAYBUFFER: {
      JS::Rooted<JSObject*> arrayBuffer(cx);
      BodyUtil::ConsumeArrayBuffer(cx, &arrayBuffer, aResultLength,
                                   std::move(resultPtr), error);

      if (!error.Failed()) {
        JS::Rooted<JS::Value> val(cx);
        val.setObjectOrNull(arrayBuffer);

        localPromise->MaybeResolve(val);
      }
      break;
    }
    case CONSUME_BLOB: {
      MOZ_CRASH("This should not happen.");
      break;
    }
    case CONSUME_FORMDATA: {
      nsCString data;
      data.Adopt(reinterpret_cast<char*>(resultPtr.release()), aResultLength);

      RefPtr<dom::FormData> fd = BodyUtil::ConsumeFormData(
          mGlobal, mBodyMimeType, mMixedCaseMimeType, data, error);
      if (!error.Failed()) {
        localPromise->MaybeResolve(fd);
      }
      break;
    }
    case CONSUME_TEXT:
      // fall through handles early exit.
    case CONSUME_JSON: {
      nsString decoded;
      if (NS_SUCCEEDED(
              BodyUtil::ConsumeText(aResultLength, resultPtr.get(), decoded))) {
        if (mConsumeType == CONSUME_TEXT) {
          localPromise->MaybeResolve(decoded);
        } else {
          JS::Rooted<JS::Value> json(cx);
          BodyUtil::ConsumeJson(cx, &json, decoded, error);
          if (!error.Failed()) {
            localPromise->MaybeResolve(json);
          }
        }
      };
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected consume body type");
  }

  error.WouldReportJSException();
  if (error.Failed()) {
    localPromise->MaybeReject(std::move(error));
  }
}

void BodyConsumer::ContinueConsumeBlobBody(BlobImpl* aBlobImpl,
                                           bool aShuttingDown) {
  AssertIsOnTargetThread();
  MOZ_ASSERT(mConsumeType == CONSUME_BLOB);

  if (mBodyConsumed) {
    return;
  }
  mBodyConsumed = true;

  MOZ_ASSERT(mConsumePromise);
  RefPtr<Promise> localPromise = std::move(mConsumePromise);

  if (!aShuttingDown) {
    RefPtr<dom::Blob> blob = dom::Blob::Create(mGlobal, aBlobImpl);
    if (NS_WARN_IF(!blob)) {
      localPromise->MaybeReject(NS_ERROR_FAILURE);
      return;
    }

    localPromise->MaybeResolve(blob);
  }

  ReleaseObject();
}

void BodyConsumer::ShutDownMainThreadConsuming() {
  if (!NS_IsMainThread()) {
    RefPtr<BodyConsumer> self = this;

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "BodyConsumer::ShutDownMainThreadConsuming",
        [self]() { self->ShutDownMainThreadConsuming(); });

    mMainThreadEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
    return;
  }

  // We need this because maybe, mConsumeBodyPump has not been created yet. We
  // must be sure that we don't try to do it.
  mShuttingDown = true;

  if (mConsumeBodyPump) {
    mConsumeBodyPump->CancelWithReason(
        NS_BINDING_ABORTED, "BodyConsumer::ShutDownMainThreadConsuming"_ns);
    mConsumeBodyPump = nullptr;
  }
}

NS_IMETHODIMP BodyConsumer::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* aData) {
  AssertIsOnMainThread();

  MOZ_ASSERT((strcmp(aTopic, DOM_WINDOW_FROZEN_TOPIC) == 0) ||
             (strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0));

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (SameCOMIdentity(aSubject, window)) {
    ContinueConsumeBody(NS_BINDING_ABORTED, 0, nullptr);
  }

  return NS_OK;
}

void BodyConsumer::RunAbortAlgorithm() {
  AssertIsOnTargetThread();
  ShutDownMainThreadConsuming();
  ContinueConsumeBody(NS_ERROR_DOM_ABORT_ERR, 0, nullptr);
}

NS_IMPL_ISUPPORTS(BodyConsumer, nsIObserver, nsISupportsWeakReference)

}  // namespace mozilla::dom
