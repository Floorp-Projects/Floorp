/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternalRequest.h"

#include "nsIContentPolicy.h"
#include "nsIDocument.h"

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

  copy->mBodyStream = mBodyStream;
  copy->mPreserveContentCodings = true;

  if (NS_IsMainThread()) {
    nsIPrincipal* principal = aGlobal->PrincipalOrNull();
    MOZ_ASSERT(principal);
    aRv = nsContentUtils::GetASCIIOrigin(principal, copy->mOrigin);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  } else {
    workers::WorkerPrivate* worker = workers::GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();

    workers::WorkerPrivate::LocationInfo& location = worker->GetLocationInfo();
    copy->mOrigin = NS_ConvertUTF16toUTF8(location.mOrigin);
  }

  copy->mMode = mMode;
  copy->mCredentialsMode = mCredentialsMode;
  // FIXME(nsm): Add ContentType fetch to nsIContentPolicy and friends.
  // Then set copy's mContext to that.
  return copy.forget();
}

InternalRequest::~InternalRequest()
{
}

} // namespace dom
} // namespace mozilla
