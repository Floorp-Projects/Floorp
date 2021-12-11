/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SDBRequest.h"

// Local includes
#include "SDBConnection.h"

// Global includes
#include <utility>
#include "mozilla/MacroForEach.h"
#include "nsError.h"
#include "nsISDBCallbacks.h"
#include "nsISupportsUtils.h"
#include "nsIVariant.h"
#include "nsThreadUtils.h"
#include "nscore.h"

namespace mozilla::dom {

SDBRequest::SDBRequest(SDBConnection* aConnection)
    : mConnection(aConnection),
      mResultCode(NS_OK),
      mHaveResultOrErrorCode(false) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aConnection);
}

SDBRequest::~SDBRequest() { AssertIsOnOwningThread(); }

void SDBRequest::SetResult(nsIVariant* aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(!mHaveResultOrErrorCode);

  mResult = aResult;
  mHaveResultOrErrorCode = true;

  FireCallback();
}

void SDBRequest::SetError(nsresult aRv) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mResultCode == NS_OK);
  MOZ_ASSERT(!mHaveResultOrErrorCode);

  mResultCode = aRv;
  mHaveResultOrErrorCode = true;

  FireCallback();
}

void SDBRequest::FireCallback() {
  AssertIsOnOwningThread();

  if (mCallback) {
    nsCOMPtr<nsISDBCallback> callback;
    callback.swap(mCallback);

    MOZ_ALWAYS_SUCCEEDS(
        NS_DispatchToCurrentThread(NewRunnableMethod<RefPtr<SDBRequest>>(
            "nsISDBCallback::OnComplete", callback, &nsISDBCallback::OnComplete,
            this)));
  }
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(SDBRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SDBRequest)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SDBRequest)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsISDBRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(SDBRequest, mCallback, mResult)

NS_IMETHODIMP
SDBRequest::GetResult(nsIVariant** aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResult);

  if (!mHaveResultOrErrorCode) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_IF_ADDREF(*aResult = mResult);
  return NS_OK;
}

NS_IMETHODIMP
SDBRequest::GetResultCode(nsresult* aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResultCode);

  if (!mHaveResultOrErrorCode) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResultCode = mResultCode;
  return NS_OK;
}

NS_IMETHODIMP
SDBRequest::GetCallback(nsISDBCallback** aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);

  NS_IF_ADDREF(*aCallback = mCallback);
  return NS_OK;
}

NS_IMETHODIMP
SDBRequest::SetCallback(nsISDBCallback* aCallback) {
  AssertIsOnOwningThread();

  mCallback = aCallback;
  return NS_OK;
}

}  // namespace mozilla::dom
