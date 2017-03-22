/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaRequests.h"

#include "ActorsChild.h"
#include "nsIQuotaCallbacks.h"

namespace mozilla {
namespace dom {
namespace quota {

RequestBase::RequestBase()
  : mResultCode(NS_OK)
  , mHaveResultOrErrorCode(false)
{
#ifdef DEBUG
  mOwningThread = PR_GetCurrentThread();
#endif
  AssertIsOnOwningThread();
}

RequestBase::RequestBase(nsIPrincipal* aPrincipal)
  : mPrincipal(aPrincipal)
  , mResultCode(NS_OK)
  , mHaveResultOrErrorCode(false)
{
#ifdef DEBUG
  mOwningThread = PR_GetCurrentThread();
#endif
  AssertIsOnOwningThread();
}

#ifdef DEBUG

void
RequestBase::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mOwningThread);
  MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
}

#endif // DEBUG

void
RequestBase::SetError(nsresult aRv)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mResultCode == NS_OK);
  MOZ_ASSERT(!mHaveResultOrErrorCode);

  mResultCode = aRv;
  mHaveResultOrErrorCode = true;

  FireCallback();
}

NS_IMPL_CYCLE_COLLECTION_0(RequestBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RequestBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(RequestBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(RequestBase)

NS_IMETHODIMP
RequestBase::GetPrincipal(nsIPrincipal** aPrincipal)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aPrincipal);

  NS_IF_ADDREF(*aPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
RequestBase::GetResultCode(nsresult* aResultCode)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResultCode);

  if (!mHaveResultOrErrorCode) {
    return NS_ERROR_FAILURE;
  }

  *aResultCode = mResultCode;
  return NS_OK;
}

UsageRequest::UsageRequest(nsIQuotaUsageCallback* aCallback)
  : mCallback(aCallback)
  , mBackgroundActor(nullptr)
  , mCanceled(false)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);
}

UsageRequest::UsageRequest(nsIPrincipal* aPrincipal,
                           nsIQuotaUsageCallback* aCallback)
  : RequestBase(aPrincipal)
  , mCallback(aCallback)
  , mBackgroundActor(nullptr)
  , mCanceled(false)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCallback);
}

UsageRequest::~UsageRequest()
{
  AssertIsOnOwningThread();
}

void
UsageRequest::SetBackgroundActor(QuotaUsageRequestChild* aBackgroundActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!mBackgroundActor);

  mBackgroundActor = aBackgroundActor;

  if (mCanceled) {
    mBackgroundActor->SendCancel();
  }
}

void
UsageRequest::SetResult(nsIVariant* aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(!mHaveResultOrErrorCode);

  mResult = aResult;

  mHaveResultOrErrorCode = true;

  FireCallback();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(UsageRequest, RequestBase, mCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(UsageRequest)
  NS_INTERFACE_MAP_ENTRY(nsIQuotaUsageRequest)
NS_INTERFACE_MAP_END_INHERITING(RequestBase)

NS_IMPL_ADDREF_INHERITED(UsageRequest, RequestBase)
NS_IMPL_RELEASE_INHERITED(UsageRequest, RequestBase)

NS_IMETHODIMP
UsageRequest::GetResult(nsIVariant** aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResult);

  if (!mHaveResultOrErrorCode) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mResult);

  NS_ADDREF(*aResult = mResult);
  return NS_OK;
}

NS_IMETHODIMP
UsageRequest::GetCallback(nsIQuotaUsageCallback** aCallback)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);

  NS_IF_ADDREF(*aCallback = mCallback);
  return NS_OK;
}

NS_IMETHODIMP
UsageRequest::SetCallback(nsIQuotaUsageCallback* aCallback)
{
  AssertIsOnOwningThread();

  mCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
UsageRequest::Cancel()
{
  AssertIsOnOwningThread();

  if (mCanceled) {
    NS_WARNING("Canceled more than once?!");
    return NS_ERROR_UNEXPECTED;
  }

  if (mBackgroundActor) {
    mBackgroundActor->SendCancel();
  }

  mCanceled = true;

  return NS_OK;
}

void
UsageRequest::FireCallback()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCallback);

  mCallback->OnUsageResult(this);

  // Clean up.
  mCallback = nullptr;
}

Request::Request()
{
  AssertIsOnOwningThread();
}

Request::Request(nsIPrincipal* aPrincipal)
  : RequestBase(aPrincipal)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aPrincipal);
}

Request::~Request()
{
  AssertIsOnOwningThread();
}

void
Request::SetResult(nsIVariant* aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(!mHaveResultOrErrorCode);

  mResult = aResult;

  mHaveResultOrErrorCode = true;

  FireCallback();
}

NS_IMETHODIMP
Request::GetResult(nsIVariant** aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResult);

  if (!mHaveResultOrErrorCode) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mResult);

  NS_ADDREF(*aResult = mResult);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(Request, RequestBase, mCallback, mResult)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Request)
  NS_INTERFACE_MAP_ENTRY(nsIQuotaRequest)
NS_INTERFACE_MAP_END_INHERITING(RequestBase)

NS_IMPL_ADDREF_INHERITED(mozilla::dom::quota::Request, RequestBase)
NS_IMPL_RELEASE_INHERITED(mozilla::dom::quota::Request, RequestBase)

NS_IMETHODIMP
Request::GetCallback(nsIQuotaCallback** aCallback)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);

  NS_IF_ADDREF(*aCallback = mCallback);
  return NS_OK;
}

NS_IMETHODIMP
Request::SetCallback(nsIQuotaCallback* aCallback)
{
  AssertIsOnOwningThread();

  mCallback = aCallback;
  return NS_OK;
}

void
Request::FireCallback()
{
  AssertIsOnOwningThread();

  if (mCallback) {
    mCallback->OnComplete(this);

    // Clean up.
    mCallback = nullptr;
  }
}

} // namespace quota
} // namespace dom
} // namespace mozilla
