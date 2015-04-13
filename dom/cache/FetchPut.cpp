/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/FetchPut.h"

#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FetchDriver.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ResponseBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/cache/ManagerId.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsCRT.h"
#include "nsHttp.h"

namespace mozilla {
namespace dom {
namespace cache {

class FetchPut::Runnable final : public nsRunnable
{
public:
  explicit Runnable(FetchPut* aFetchPut)
    : mFetchPut(aFetchPut)
  {
    MOZ_ASSERT(mFetchPut);
  }

  NS_IMETHOD Run() override
  {
    if (NS_IsMainThread())
    {
      mFetchPut->DoFetchOnMainThread();
      return NS_OK;
    }

    MOZ_ASSERT(mFetchPut->mInitiatingThread == NS_GetCurrentThread());

    mFetchPut->DoPutOnWorkerThread();

    // The FetchPut object must ultimately be freed on the worker thread,
    // so make sure we release our reference here.  The runnable may end
    // up getting deleted on the main thread.
    mFetchPut = nullptr;

    return NS_OK;
  }

private:
  nsRefPtr<FetchPut> mFetchPut;
};

class FetchPut::FetchObserver final : public FetchDriverObserver
{
public:
  explicit FetchObserver(FetchPut* aFetchPut)
    : mFetchPut(aFetchPut)
  {
  }

  virtual void OnResponseAvailable(InternalResponse* aResponse) override
  {
    MOZ_ASSERT(!mInternalResponse);
    mInternalResponse = aResponse;
  }

  virtual void OnResponseEnd() override
  {
    mFetchPut->FetchComplete(this, mInternalResponse);
    mFetchPut = nullptr;
  }

protected:
  virtual ~FetchObserver() { }

private:
  nsRefPtr<FetchPut> mFetchPut;
  nsRefPtr<InternalResponse> mInternalResponse;
};

// static
nsresult
FetchPut::Create(Listener* aListener, Manager* aManager, CacheId aCacheId,
                 const nsTArray<CacheRequest>& aRequests,
                 const nsTArray<nsCOMPtr<nsIInputStream>>& aRequestStreams,
                 FetchPut** aFetchPutOut)
{
  MOZ_ASSERT(aRequests.Length() == aRequestStreams.Length());

  // The FetchDriver requires that all requests have a referrer already set.
#ifdef DEBUG
  for (uint32_t i = 0; i < aRequests.Length(); ++i) {
    if (aRequests[i].referrer() == EmptyString()) {
      return NS_ERROR_UNEXPECTED;
    }
  }
#endif

  nsRefPtr<FetchPut> ref = new FetchPut(aListener, aManager, aCacheId,
                                        aRequests, aRequestStreams);

  nsresult rv = ref->DispatchToMainThread();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  ref.forget(aFetchPutOut);

  return NS_OK;
}

void
FetchPut::ClearListener()
{
  MOZ_ASSERT(mListener);
  mListener = nullptr;
}

FetchPut::FetchPut(Listener* aListener, Manager* aManager, CacheId aCacheId,
                   const nsTArray<CacheRequest>& aRequests,
                   const nsTArray<nsCOMPtr<nsIInputStream>>& aRequestStreams)
  : mListener(aListener)
  , mManager(aManager)
  , mCacheId(aCacheId)
  , mInitiatingThread(NS_GetCurrentThread())
  , mStateList(aRequests.Length())
  , mPendingCount(0)
  , mResult(NS_OK)
{
  MOZ_ASSERT(mListener);
  MOZ_ASSERT(mManager);
  MOZ_ASSERT(aRequests.Length() == aRequestStreams.Length());

  for (uint32_t i = 0; i < aRequests.Length(); ++i) {
    State* s = mStateList.AppendElement();
    s->mCacheRequest = aRequests[i];
    s->mRequestStream = aRequestStreams[i];
  }

  mManager->AddRefCacheId(mCacheId);
}

FetchPut::~FetchPut()
{
  MOZ_ASSERT(mInitiatingThread == NS_GetCurrentThread());
  MOZ_ASSERT(!mListener);
  mManager->RemoveListener(this);
  mManager->ReleaseCacheId(mCacheId);
}

nsresult
FetchPut::DispatchToMainThread()
{
  MOZ_ASSERT(!mRunnable);

  nsCOMPtr<nsIRunnable> runnable = new Runnable(this);

  nsresult rv = NS_DispatchToMainThread(runnable, nsIThread::DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!mRunnable);
  mRunnable = runnable.forget();

  return NS_OK;
}

void
FetchPut::DispatchToInitiatingThread()
{
  MOZ_ASSERT(mRunnable);

  nsresult rv = mInitiatingThread->Dispatch(mRunnable,
                                            nsIThread::DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Failed to dispatch to worker thread after fetch completion.");
  }

  mRunnable = nullptr;
}

void
FetchPut::DoFetchOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<ManagerId> managerId = mManager->GetManagerId();
  nsCOMPtr<nsIPrincipal> principal = managerId->Principal();
  mPendingCount = mStateList.Length();

  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = NS_NewLoadGroup(getter_AddRefs(loadGroup), principal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MaybeSetError(rv);
    MaybeCompleteOnMainThread();
    return;
  }

  for (uint32_t i = 0; i < mStateList.Length(); ++i) {
    nsRefPtr<InternalRequest> internalRequest =
      ToInternalRequest(mStateList[i].mCacheRequest);

    // If there is a stream we must clone it so that its still available
    // to store in the cache later;
    if (mStateList[i].mRequestStream) {
      internalRequest->SetBody(mStateList[i].mRequestStream);
      nsRefPtr<InternalRequest> clone = internalRequest->Clone();

      // The copy construction clone above can change the source stream,
      // so get it back out to use when we put this in the cache.
      internalRequest->GetBody(getter_AddRefs(mStateList[i].mRequestStream));

      internalRequest = clone;
    }

    nsRefPtr<FetchDriver> fetchDriver = new FetchDriver(internalRequest,
                                                        principal,
                                                        loadGroup);

    mStateList[i].mFetchObserver = new FetchObserver(this);
    rv = fetchDriver->Fetch(mStateList[i].mFetchObserver);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MaybeSetError(rv);
      mStateList[i].mFetchObserver = nullptr;
      mPendingCount -= 1;
      continue;
    }
  }

  // If they all failed, then we might need to complete main thread immediately
  MaybeCompleteOnMainThread();
}

void
FetchPut::FetchComplete(FetchObserver* aObserver,
                        InternalResponse* aInternalResponse)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aInternalResponse->IsError() && NS_SUCCEEDED(mResult)) {
    MaybeSetError(NS_ERROR_FAILURE);
  }

  for (uint32_t i = 0; i < mStateList.Length(); ++i) {
    if (mStateList[i].mFetchObserver == aObserver) {
      ErrorResult rv;
      ToCacheResponseWithoutBody(mStateList[i].mCacheResponse,
                                  *aInternalResponse, rv);
      if (rv.Failed()) {
        MaybeSetError(rv.ErrorCode());
        return;
      }
      aInternalResponse->GetBody(getter_AddRefs(mStateList[i].mResponseStream));
      mStateList[i].mFetchObserver = nullptr;
      MOZ_ASSERT(mPendingCount > 0);
      mPendingCount -= 1;
      MaybeCompleteOnMainThread();
      return;
    }
  }

  MOZ_ASSERT_UNREACHABLE("Should never get called by unknown fetch observer.");
}

void
FetchPut::MaybeCompleteOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mPendingCount > 0) {
    return;
  }

  DispatchToInitiatingThread();
}

void
FetchPut::DoPutOnWorkerThread()
{
  MOZ_ASSERT(mInitiatingThread == NS_GetCurrentThread());

  if (NS_FAILED(mResult)) {
    MaybeNotifyListener();
    return;
  }

  // These allocate ~4.5k combined on the stack
  nsAutoTArray<CacheRequestResponse, 16> putList;
  nsAutoTArray<nsCOMPtr<nsIInputStream>, 16> requestStreamList;
  nsAutoTArray<nsCOMPtr<nsIInputStream>, 16> responseStreamList;

  putList.SetCapacity(mStateList.Length());
  requestStreamList.SetCapacity(mStateList.Length());
  responseStreamList.SetCapacity(mStateList.Length());

  for (uint32_t i = 0; i < mStateList.Length(); ++i) {
    // The spec requires us to catch if content tries to insert a set of
    // requests that would overwrite each other.
    if (MatchInPutList(mStateList[i].mCacheRequest, putList)) {
      MaybeSetError(NS_ERROR_DOM_INVALID_STATE_ERR);
      MaybeNotifyListener();
      return;
    }

    CacheRequestResponse* entry = putList.AppendElement();
    entry->request() = mStateList[i].mCacheRequest;
    entry->response() = mStateList[i].mCacheResponse;
    requestStreamList.AppendElement(mStateList[i].mRequestStream.forget());
    responseStreamList.AppendElement(mStateList[i].mResponseStream.forget());
  }
  mStateList.Clear();

  mManager->ExecutePutAll(this, mCacheId, putList, requestStreamList,
                          responseStreamList);
}

// static
bool
FetchPut::MatchInPutList(const CacheRequest& aRequest,
                         const nsTArray<CacheRequestResponse>& aPutList)
{
  // This method implements the SW spec QueryCache algorithm against an
  // in memory array of Request/Response objects.  This essentially the
  // same algorithm that is implemented in DBSchema.cpp.  Unfortunately
  // we cannot unify them because when operating against the real database
  // we don't want to load all request/response objects into memory.

  if (!aRequest.method().LowerCaseEqualsLiteral("get") &&
      !aRequest.method().LowerCaseEqualsLiteral("head")) {
    return false;
  }

  nsRefPtr<InternalHeaders> requestHeaders =
    new InternalHeaders(aRequest.headers());

  for (uint32_t i = 0; i < aPutList.Length(); ++i) {
    const CacheRequest& cachedRequest = aPutList[i].request();
    const CacheResponse& cachedResponse = aPutList[i].response();

    // If the URLs don't match, then just skip to the next entry.
    if (aRequest.url() != cachedRequest.url()) {
      continue;
    }

    nsRefPtr<InternalHeaders> cachedRequestHeaders =
      new InternalHeaders(cachedRequest.headers());

    nsRefPtr<InternalHeaders> cachedResponseHeaders =
      new InternalHeaders(cachedResponse.headers());

    nsAutoTArray<nsCString, 16> varyHeaders;
    ErrorResult rv;
    cachedResponseHeaders->GetAll(NS_LITERAL_CSTRING("vary"), varyHeaders, rv);
    MOZ_ALWAYS_TRUE(!rv.Failed());

    // Assume the vary headers match until we find a conflict
    bool varyHeadersMatch = true;

    for (uint32_t j = 0; j < varyHeaders.Length(); ++j) {
      // Extract the header names inside the Vary header value.
      nsAutoCString varyValue(varyHeaders[j]);
      char* rawBuffer = varyValue.BeginWriting();
      char* token = nsCRT::strtok(rawBuffer, NS_HTTP_HEADER_SEPS, &rawBuffer);
      bool bailOut = false;
      for (; token;
           token = nsCRT::strtok(rawBuffer, NS_HTTP_HEADER_SEPS, &rawBuffer)) {
        nsDependentCString header(token);
        if (header.EqualsLiteral("*")) {
          continue;
        }

        ErrorResult headerRv;
        nsAutoCString value;
        requestHeaders->Get(header, value, headerRv);
        if (NS_WARN_IF(headerRv.Failed())) {
          headerRv.ClearMessage();
          MOZ_ASSERT(value.IsEmpty());
        }

        nsAutoCString cachedValue;
        cachedRequestHeaders->Get(header, value, headerRv);
        if (NS_WARN_IF(headerRv.Failed())) {
          headerRv.ClearMessage();
          MOZ_ASSERT(cachedValue.IsEmpty());
        }

        if (value != cachedValue) {
          varyHeadersMatch = false;
          bailOut = true;
          break;
        }
      }

      if (bailOut) {
        break;
      }
    }

    // URL was equal and all vary headers match!
    if (varyHeadersMatch) {
      return true;
    }
  }

  return false;
}

void
FetchPut::OnOpComplete(nsresult aRv, const CacheOpResult& aResult,
                       CacheId aOpenedCacheId,
                       const nsTArray<SavedResponse>& aSavedResponseList,
                       const nsTArray<SavedRequest>& aSavedRequestList,
                       StreamList* aStreamList)
{
  MOZ_ASSERT(mInitiatingThread == NS_GetCurrentThread());
  MOZ_ASSERT(aResult.type() == CacheOpResult::TCachePutAllResult);
  MaybeSetError(aRv);
  MaybeNotifyListener();
}

void
FetchPut::MaybeSetError(nsresult aRv)
{
  if (NS_FAILED(mResult) || NS_SUCCEEDED(aRv)) {
    return;
  }
  mResult = aRv;
}

void
FetchPut::MaybeNotifyListener()
{
  MOZ_ASSERT(mInitiatingThread == NS_GetCurrentThread());
  if (!mListener) {
    return;
  }
  mListener->OnFetchPut(this, mResult);
}

nsIGlobalObject*
FetchPut::GetGlobalObject() const
{
  MOZ_CRASH("No global object in parent-size FetchPut operation!");
}

#ifdef DEBUG
void
FetchPut::AssertOwningThread() const
{
  MOZ_ASSERT(mInitiatingThread == NS_GetCurrentThread());
}
#endif

CachePushStreamChild*
FetchPut::CreatePushStream(nsIAsyncInputStream* aStream)
{
  MOZ_CRASH("FetchPut should never create a push stream!");
}

} // namespace cache
} // namespace dom
} // namespace mozilla
