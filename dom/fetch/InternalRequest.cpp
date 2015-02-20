/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternalRequest.h"

#include "nsIContentPolicy.h"
#include "nsIDocument.h"
#include "nsStreamUtils.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/workers/Workers.h"

#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

// The global is used to extract the principal.
already_AddRefed<InternalRequest>
InternalRequest::GetRequestConstructorCopy(nsIGlobalObject* aGlobal, ErrorResult& aRv) const
{
  nsRefPtr<InternalRequest> copy = new InternalRequest();
  copy->mURL.Assign(mURL);
  copy->SetMethod(mMethod);
  copy->mHeaders = new InternalHeaders(*mHeaders);
  copy->SetUnsafeRequest();

  copy->mBodyStream = mBodyStream;
  copy->mForceOriginHeader = true;
  // The "client" is not stored in our implementation. Fetch API users should
  // use the appropriate window/document/principal and other Gecko security
  // mechanisms as appropriate.
  copy->mSameOriginDataURL = true;
  copy->mPreserveContentCodings = true;
  // The default referrer is already about:client.

  copy->mContentPolicyType = nsIContentPolicy::TYPE_FETCH;
  copy->mMode = mMode;
  copy->mCredentialsMode = mCredentialsMode;
  copy->mCacheMode = mCacheMode;
  return copy.forget();
}

already_AddRefed<InternalRequest>
InternalRequest::Clone()
{
  nsRefPtr<InternalRequest> clone = new InternalRequest(*this);

  if (!mBodyStream) {
    return clone.forget();
  }

  nsCOMPtr<nsIInputStream> clonedBody;
  nsCOMPtr<nsIInputStream> replacementBody;

  nsresult rv = NS_CloneInputStream(mBodyStream, getter_AddRefs(clonedBody),
                                    getter_AddRefs(replacementBody));
  if (NS_WARN_IF(NS_FAILED(rv))) { return nullptr; }

  clone->mBodyStream.swap(clonedBody);
  if (replacementBody) {
    mBodyStream.swap(replacementBody);
  }

  return clone.forget();
}

InternalRequest::InternalRequest(const InternalRequest& aOther)
  : mMethod(aOther.mMethod)
  , mURL(aOther.mURL)
  , mHeaders(new InternalHeaders(*aOther.mHeaders))
  , mContentPolicyType(aOther.mContentPolicyType)
  , mReferrer(aOther.mReferrer)
  , mMode(aOther.mMode)
  , mCredentialsMode(aOther.mCredentialsMode)
  , mResponseTainting(aOther.mResponseTainting)
  , mCacheMode(aOther.mCacheMode)
  , mAuthenticationFlag(aOther.mAuthenticationFlag)
  , mForceOriginHeader(aOther.mForceOriginHeader)
  , mPreserveContentCodings(aOther.mPreserveContentCodings)
  , mSameOriginDataURL(aOther.mSameOriginDataURL)
  , mSandboxedStorageAreaURLs(aOther.mSandboxedStorageAreaURLs)
  , mSkipServiceWorker(aOther.mSkipServiceWorker)
  , mSynchronous(aOther.mSynchronous)
  , mUnsafeRequest(aOther.mUnsafeRequest)
  , mUseURLCredentials(aOther.mUseURLCredentials)
{
  // NOTE: does not copy body stream... use the fallible Clone() for that
}

InternalRequest::~InternalRequest()
{
}

} // namespace dom
} // namespace mozilla
