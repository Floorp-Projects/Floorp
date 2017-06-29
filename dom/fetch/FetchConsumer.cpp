/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"
#include "FetchConsumer.h"

#include "nsIInputStreamPump.h"
#include "nsProxyRelease.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "Workers.h"

namespace mozilla {
namespace dom {

using namespace workers;

namespace {

template <class Derived>
class FetchBodyWorkerHolder final : public workers::WorkerHolder
{
  RefPtr<FetchBodyConsumer<Derived>> mConsumer;
  bool mWasNotified;

public:
  explicit FetchBodyWorkerHolder(FetchBodyConsumer<Derived>* aConsumer)
    : mConsumer(aConsumer)
    , mWasNotified(false)
  {
    MOZ_ASSERT(aConsumer);
  }

  ~FetchBodyWorkerHolder() = default;

  bool Notify(workers::Status aStatus) override
  {
    MOZ_ASSERT(aStatus > workers::Running);
    if (!mWasNotified) {
      mWasNotified = true;
      // This will probably cause the releasing of the consumer.
      // The WorkerHolder will be released as well.
      mConsumer->ContinueConsumeBody(NS_BINDING_ABORTED, 0, nullptr);
    }

    return true;
  }
};

template <class Derived>
class BeginConsumeBodyRunnable final : public Runnable
{
  RefPtr<FetchBodyConsumer<Derived>> mFetchBodyConsumer;

public:
  explicit BeginConsumeBodyRunnable(FetchBodyConsumer<Derived>* aConsumer)
    : Runnable("BeginConsumeBodyRunnable")
    , mFetchBodyConsumer(aConsumer)
  { }

  NS_IMETHOD
  Run() override
  {
    mFetchBodyConsumer->BeginConsumeBodyMainThread();
    return NS_OK;
  }
};

/*
 * Called on successfully reading the complete stream.
 */
template <class Derived>
class ContinueConsumeBodyRunnable final : public MainThreadWorkerRunnable
{
  RefPtr<FetchBodyConsumer<Derived>> mFetchBodyConsumer;
  nsresult mStatus;
  uint32_t mLength;
  uint8_t* mResult;

public:
  ContinueConsumeBodyRunnable(FetchBodyConsumer<Derived>* aFetchBodyConsumer,
                              nsresult aStatus, uint32_t aLength,
                              uint8_t* aResult)
    : MainThreadWorkerRunnable(aFetchBodyConsumer->GetWorkerPrivate())
    , mFetchBodyConsumer(aFetchBodyConsumer)
    , mStatus(aStatus)
    , mLength(aLength)
    , mResult(aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mFetchBodyConsumer->ContinueConsumeBody(mStatus, mLength, mResult);
    return true;
  }
};

template <class Derived>
class FailConsumeBodyWorkerRunnable : public MainThreadWorkerControlRunnable
{
  RefPtr<FetchBodyConsumer<Derived>> mBodyConsumer;

public:
  explicit FailConsumeBodyWorkerRunnable(FetchBodyConsumer<Derived>* aBodyConsumer)
    : MainThreadWorkerControlRunnable(aBodyConsumer->GetWorkerPrivate())
    , mBodyConsumer(aBodyConsumer)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mBodyConsumer->ContinueConsumeBody(NS_ERROR_FAILURE, 0, nullptr);
    return true;
  }
};

/*
 * In case of failure to create a stream pump or dispatch stream completion to
 * worker, ensure we cleanup properly. Thread agnostic.
 */
template <class Derived>
class MOZ_STACK_CLASS AutoFailConsumeBody final
{
  RefPtr<FetchBodyConsumer<Derived>> mBodyConsumer;

public:
  explicit AutoFailConsumeBody(FetchBodyConsumer<Derived>* aBodyConsumer)
    : mBodyConsumer(aBodyConsumer)
  {}

  ~AutoFailConsumeBody()
  {
    AssertIsOnMainThread();

    if (mBodyConsumer) {
      if (mBodyConsumer->GetWorkerPrivate()) {
        RefPtr<FailConsumeBodyWorkerRunnable<Derived>> r =
          new FailConsumeBodyWorkerRunnable<Derived>(mBodyConsumer);
        if (!r->Dispatch()) {
          MOZ_CRASH("We are going to leak");
        }
      } else {
        mBodyConsumer->ContinueConsumeBody(NS_ERROR_FAILURE, 0, nullptr);
      }
    }
  }

  void
  DontFail()
  {
    mBodyConsumer = nullptr;
  }
};

/*
 * Called on successfully reading the complete stream for Blob.
 */
template <class Derived>
class ContinueConsumeBlobBodyRunnable final : public MainThreadWorkerRunnable
{
  RefPtr<FetchBodyConsumer<Derived>> mFetchBodyConsumer;
  RefPtr<BlobImpl> mBlobImpl;

public:
  ContinueConsumeBlobBodyRunnable(FetchBodyConsumer<Derived>* aFetchBodyConsumer,
                                  BlobImpl* aBlobImpl)
    : MainThreadWorkerRunnable(aFetchBodyConsumer->GetWorkerPrivate())
    , mFetchBodyConsumer(aFetchBodyConsumer)
    , mBlobImpl(aBlobImpl)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mBlobImpl);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mFetchBodyConsumer->ContinueConsumeBlobBody(mBlobImpl);
    return true;
  }
};

template <class Derived>
class ConsumeBodyDoneObserver : public nsIStreamLoaderObserver
                              , public MutableBlobStorageCallback
{
  RefPtr<FetchBodyConsumer<Derived>> mFetchBodyConsumer;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit ConsumeBodyDoneObserver(FetchBodyConsumer<Derived>* aFetchBodyConsumer)
    : mFetchBodyConsumer(aFetchBodyConsumer)
  { }

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader,
                   nsISupports* aCtxt,
                   nsresult aStatus,
                   uint32_t aResultLength,
                   const uint8_t* aResult) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // If the binding requested cancel, we don't need to call
    // ContinueConsumeBody, since that is the originator.
    if (aStatus == NS_BINDING_ABORTED) {
      return NS_OK;
    }

    uint8_t* nonconstResult = const_cast<uint8_t*>(aResult);
    if (mFetchBodyConsumer->GetWorkerPrivate()) {
      RefPtr<ContinueConsumeBodyRunnable<Derived>> r =
        new ContinueConsumeBodyRunnable<Derived>(mFetchBodyConsumer,
                                                 aStatus,
                                                 aResultLength,
                                                 nonconstResult);
      if (!r->Dispatch()) {
        // XXXcatalinb: The worker is shutting down, the pump will be canceled
        // by FetchBodyWorkerHolder::Notify.
        NS_WARNING("Could not dispatch ConsumeBodyRunnable");
        // Return failure so that aResult is freed.
        return NS_ERROR_FAILURE;
      }
    } else {
      mFetchBodyConsumer->ContinueConsumeBody(aStatus, aResultLength,
                                              nonconstResult);
    }

    // FetchBody is responsible for data.
    return NS_SUCCESS_ADOPTED_DATA;
  }

  virtual void BlobStoreCompleted(MutableBlobStorage* aBlobStorage,
                                  Blob* aBlob,
                                  nsresult aRv) override
  {
    // On error.
    if (NS_FAILED(aRv)) {
      OnStreamComplete(nullptr, nullptr, aRv, 0, nullptr);
      return;
    }

    MOZ_ASSERT(aBlob);

    if (mFetchBodyConsumer->GetWorkerPrivate()) {
      RefPtr<ContinueConsumeBlobBodyRunnable<Derived>> r =
        new ContinueConsumeBlobBodyRunnable<Derived>(mFetchBodyConsumer,
                                                     aBlob->Impl());

      if (!r->Dispatch()) {
        NS_WARNING("Could not dispatch ConsumeBlobBodyRunnable");
        return;
      }
    } else {
      mFetchBodyConsumer->ContinueConsumeBlobBody(aBlob->Impl());
    }
  }

private:
  virtual ~ConsumeBodyDoneObserver()
  { }
};

template <class Derived>
NS_IMPL_ADDREF(ConsumeBodyDoneObserver<Derived>)
template <class Derived>
NS_IMPL_RELEASE(ConsumeBodyDoneObserver<Derived>)
template <class Derived>
NS_INTERFACE_MAP_BEGIN(ConsumeBodyDoneObserver<Derived>)
  NS_INTERFACE_MAP_ENTRY(nsIStreamLoaderObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamLoaderObserver)
NS_INTERFACE_MAP_END

template <class Derived>
class CancelPumpRunnable final : public WorkerMainThreadRunnable
{
  RefPtr<FetchBodyConsumer<Derived>> mBodyConsumer;

public:
  explicit CancelPumpRunnable(FetchBodyConsumer<Derived>* aBodyConsumer)
    : WorkerMainThreadRunnable(aBodyConsumer->GetWorkerPrivate(),
                               NS_LITERAL_CSTRING("Fetch :: Cancel Pump"))
    , mBodyConsumer(aBodyConsumer)
  {}

  bool
  MainThreadRun() override
  {
    mBodyConsumer->CancelPump();
    return true;
  }
};

} // anonymous

template <class Derived>
/* static */ already_AddRefed<Promise>
FetchBodyConsumer<Derived>::Create(nsIGlobalObject* aGlobal,
                                   nsIEventTarget* aMainThreadEventTarget,
                                   FetchBody<Derived>* aBody,
                                   FetchConsumeType aType,
                                   ErrorResult& aRv)
{
  MOZ_ASSERT(aBody);
  MOZ_ASSERT(aMainThreadEventTarget);

  RefPtr<Promise> promise = Promise::Create(aGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  WorkerPrivate* workerPrivate = nullptr;
  if (!NS_IsMainThread()) {
    workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
  }

  RefPtr<FetchBodyConsumer<Derived>> consumer =
    new FetchBodyConsumer<Derived>(aMainThreadEventTarget, aGlobal,
                                   workerPrivate, aBody, promise, aType);

  if (!NS_IsMainThread()) {
    MOZ_ASSERT(workerPrivate);
    if (NS_WARN_IF(!consumer->RegisterWorkerHolder(workerPrivate))) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
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

  nsCOMPtr<nsIRunnable> r = new BeginConsumeBodyRunnable<Derived>(consumer);
  aRv = aMainThreadEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return promise.forget();
}

template <class Derived>
void
FetchBodyConsumer<Derived>::ReleaseObject()
{
  AssertIsOnTargetThread();

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
      os->RemoveObserver(this, DOM_WINDOW_FROZEN_TOPIC);
    }
  }

  mGlobal = nullptr;
  mWorkerHolder = nullptr;
  mBody = nullptr;
}

template <class Derived>
FetchBodyConsumer<Derived>::FetchBodyConsumer(nsIEventTarget* aMainThreadEventTarget,
                                              nsIGlobalObject* aGlobalObject,
                                              WorkerPrivate* aWorkerPrivate,
                                              FetchBody<Derived>* aBody,
                                              Promise* aPromise,
                                              FetchConsumeType aType)
  : mTargetThread(NS_GetCurrentThread())
  , mMainThreadEventTarget(aMainThreadEventTarget)
  , mBody(aBody)
  , mGlobal(aGlobalObject)
  , mWorkerPrivate(aWorkerPrivate)
  , mConsumeType(aType)
  , mConsumePromise(aPromise)
  , mBodyConsumed(false)
{
  MOZ_ASSERT(aMainThreadEventTarget);
  MOZ_ASSERT(aBody);
  MOZ_ASSERT(aPromise);
}

template <class Derived>
FetchBodyConsumer<Derived>::~FetchBodyConsumer()
{
  NS_ProxyRelease("FetchBodyConsumer::mBody",
                  mTargetThread, mBody.forget());
}

template <class Derived>
void
FetchBodyConsumer<Derived>::AssertIsOnTargetThread() const
{
  MOZ_ASSERT(NS_GetCurrentThread() == mTargetThread);
}

template <class Derived>
bool
FetchBodyConsumer<Derived>::RegisterWorkerHolder(WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(!mWorkerHolder);
  mWorkerHolder.reset(new FetchBodyWorkerHolder<Derived>(this));

  if (!mWorkerHolder->HoldWorker(aWorkerPrivate, Closing)) {
    NS_WARNING("Failed to add workerHolder");
    mWorkerHolder = nullptr;
    return false;
  }

  return true;
}

/*
 * BeginConsumeBodyMainThread() will automatically reject the consume promise
 * and clean up on any failures, so there is no need for callers to do so,
 * reflected in a lack of error return code.
 */
template <class Derived>
void
FetchBodyConsumer<Derived>::BeginConsumeBodyMainThread()
{
  AssertIsOnMainThread();

  AutoFailConsumeBody<Derived> autoReject(this);

  nsresult rv;
  nsCOMPtr<nsIInputStream> stream;
  mBody->DerivedClass()->GetBody(getter_AddRefs(stream));
  if (!stream) {
    rv = NS_NewCStringInputStream(getter_AddRefs(stream), EmptyCString());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  nsCOMPtr<nsIInputStreamPump> pump;
  rv = NS_NewInputStreamPump(getter_AddRefs(pump),
                             stream, -1, -1, 0, 0, false,
                             mMainThreadEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RefPtr<ConsumeBodyDoneObserver<Derived>> p =
   new ConsumeBodyDoneObserver<Derived>(this);

  nsCOMPtr<nsIStreamListener> listener;
  if (mConsumeType == CONSUME_BLOB) {
    MutableBlobStorage::MutableBlobStorageType type =
      MutableBlobStorage::eOnlyInMemory;

    const mozilla::UniquePtr<mozilla::ipc::PrincipalInfo>& principalInfo =
      mBody->DerivedClass()->GetPrincipalInfo();
    // We support temporary file for blobs only if the principal is known and
    // it's system or content not in private Browsing.
    if (principalInfo &&
        (principalInfo->type() == mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo ||
         (principalInfo->type() == mozilla::ipc::PrincipalInfo::TContentPrincipalInfo &&
          principalInfo->get_ContentPrincipalInfo().attrs().mPrivateBrowsingId == 0))) {
      type = MutableBlobStorage::eCouldBeInTemporaryFile;
    }

    listener = new MutableBlobStreamListener(type, nullptr, mBody->MimeType(),
                                             p, mMainThreadEventTarget);
  } else {
    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), p);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    listener = loader;
  }

  rv = pump->AsyncRead(listener, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Now that everything succeeded, we can assign the pump to a pointer that
  // stays alive for the lifetime of the FetchBody.
  mConsumeBodyPump =
    new nsMainThreadPtrHolder<nsIInputStreamPump>("FetchBodyConsumer::mConsumeBodyPump",
                                                  pump, mMainThreadEventTarget);
  // It is ok for retargeting to fail and reads to happen on the main thread.
  autoReject.DontFail();

  // Try to retarget, otherwise fall back to main thread.
  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(pump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    rv = rr->RetargetDeliveryTo(sts);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      NS_WARNING("Retargeting failed");
    }
  }
}

template <class Derived>
void
FetchBodyConsumer<Derived>::ContinueConsumeBody(nsresult aStatus,
                                                uint32_t aResultLength,
                                                uint8_t* aResult)
{
  AssertIsOnTargetThread();
  // Just a precaution to ensure ContinueConsumeBody is not called out of
  // sync with a body read.
  MOZ_ASSERT(mBody->BodyUsed());

  if (mBodyConsumed) {
    return;
  }
  mBodyConsumed = true;

  auto autoFree = mozilla::MakeScopeExit([&] {
    free(aResult);
  });

  MOZ_ASSERT(mConsumePromise);
  RefPtr<Promise> localPromise = mConsumePromise.forget();

  RefPtr<FetchBodyConsumer<Derived>> self = this;
  auto autoReleaseObject = mozilla::MakeScopeExit([&] {
    self->ReleaseObject();
  });

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    localPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);

    // If binding aborted, cancel the pump. We can't assert mConsumeBodyPump.
    // In the (admittedly rare) situation that BeginConsumeBodyMainThread()
    // context switches out, and the worker thread gets canceled before the
    // pump is setup, mConsumeBodyPump will be null.
    // We've to use the !! form since non-main thread pointer access on
    // a nsMainThreadPtrHandle is not permitted.
    if (aStatus == NS_BINDING_ABORTED && !!mConsumeBodyPump) {
      if (NS_IsMainThread()) {
        CancelPump();
      } else {
        MOZ_ASSERT(mWorkerPrivate);
        // In case of worker thread, we block the worker while the request is
        // canceled on the main thread. This ensures that OnStreamComplete has
        // a valid FetchBody around to call CancelPump and we don't release the
        // FetchBody on the main thread.
        RefPtr<CancelPumpRunnable<Derived>> r =
          new CancelPumpRunnable<Derived>(this);
        ErrorResult rv;
        r->Dispatch(Terminating, rv);
        if (rv.Failed()) {
          NS_WARNING("Could not dispatch CancelPumpRunnable. Nothing we can do here");
          // None of our callers are callled directly from JS, so there is no
          // point in trying to propagate this failure out of here.  And
          // localPromise is already rejected.  Just suppress the failure.
          rv.SuppressException();
        }
      }
    }
  }

  // Release the pump and then early exit if there was an error.
  // Uses NS_ProxyRelease internally, so this is safe.
  mConsumeBodyPump = nullptr;

  // Don't warn here since we warned above.
  if (NS_FAILED(aStatus)) {
    return;
  }

  // Finish successfully consuming body according to type.
  MOZ_ASSERT(aResult);

  AutoJSAPI jsapi;
  if (!jsapi.Init(mBody->DerivedClass()->GetParentObject())) {
    localPromise->MaybeReject(NS_ERROR_UNEXPECTED);
    return;
  }

  JSContext* cx = jsapi.cx();
  ErrorResult error;

  switch (mConsumeType) {
    case CONSUME_ARRAYBUFFER: {
      JS::Rooted<JSObject*> arrayBuffer(cx);
      BodyUtil::ConsumeArrayBuffer(cx, &arrayBuffer, aResultLength, aResult,
                                   error);

      if (!error.Failed()) {
        JS::Rooted<JS::Value> val(cx);
        val.setObjectOrNull(arrayBuffer);

        localPromise->MaybeResolve(cx, val);
        // ArrayBuffer takes over ownership.
        aResult = nullptr;
      }
      break;
    }
    case CONSUME_BLOB: {
      MOZ_CRASH("This should not happen.");
      break;
    }
    case CONSUME_FORMDATA: {
      nsCString data;
      data.Adopt(reinterpret_cast<char*>(aResult), aResultLength);
      aResult = nullptr;

      RefPtr<dom::FormData> fd =
        BodyUtil::ConsumeFormData(mBody->DerivedClass()->GetParentObject(),
                                  mBody->MimeType(), data, error);
      if (!error.Failed()) {
        localPromise->MaybeResolve(fd);
      }
      break;
    }
    case CONSUME_TEXT:
      // fall through handles early exit.
    case CONSUME_JSON: {
      nsString decoded;
      if (NS_SUCCEEDED(BodyUtil::ConsumeText(aResultLength, aResult, decoded))) {
        if (mConsumeType == CONSUME_TEXT) {
          localPromise->MaybeResolve(decoded);
        } else {
          JS::Rooted<JS::Value> json(cx);
          BodyUtil::ConsumeJson(cx, &json, decoded, error);
          if (!error.Failed()) {
            localPromise->MaybeResolve(cx, json);
          }
        }
      };
      break;
    }
    default:
      NS_NOTREACHED("Unexpected consume body type");
  }

  error.WouldReportJSException();
  if (error.Failed()) {
    localPromise->MaybeReject(error);
  }
}

template <class Derived>
void
FetchBodyConsumer<Derived>::ContinueConsumeBlobBody(BlobImpl* aBlobImpl)
{
  AssertIsOnTargetThread();
  // Just a precaution to ensure ContinueConsumeBody is not called out of
  // sync with a body read.
  MOZ_ASSERT(mBody->BodyUsed());
  MOZ_ASSERT(mConsumeType == CONSUME_BLOB);

  if (mBodyConsumed) {
    return;
  }
  mBodyConsumed = true;

  MOZ_ASSERT(mConsumePromise);
  RefPtr<Promise> localPromise = mConsumePromise.forget();

  // Release the pump and then early exit if there was an error.
  // Uses NS_ProxyRelease internally, so this is safe.
  mConsumeBodyPump = nullptr;

  RefPtr<dom::Blob> blob =
    dom::Blob::Create(mBody->DerivedClass()->GetParentObject(), aBlobImpl);
  MOZ_ASSERT(blob);

  localPromise->MaybeResolve(blob);

  ReleaseObject();
}

template <class Derived>
void
FetchBodyConsumer<Derived>::CancelPump()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mConsumeBodyPump);
  mConsumeBodyPump->Cancel(NS_BINDING_ABORTED);
}

template <class Derived>
NS_IMETHODIMP
FetchBodyConsumer<Derived>::Observe(nsISupports* aSubject,
                                    const char* aTopic,
                                    const char16_t* aData)
{
  AssertIsOnMainThread();

  MOZ_ASSERT((strcmp(aTopic, DOM_WINDOW_FROZEN_TOPIC) == 0) ||
             (strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0));

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (SameCOMIdentity(aSubject, window)) {
    ContinueConsumeBody(NS_BINDING_ABORTED, 0, nullptr);
  }

  return NS_OK;
}

template <class Derived>
NS_IMPL_ADDREF(FetchBodyConsumer<Derived>)

template <class Derived>
NS_IMPL_RELEASE(FetchBodyConsumer<Derived>)

template <class Derived>
NS_IMPL_QUERY_INTERFACE(FetchBodyConsumer<Derived>,
                        nsIObserver,
                        nsISupportsWeakReference)

} // namespace dom
} // namespace mozilla
