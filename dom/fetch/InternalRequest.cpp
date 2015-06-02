/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  copy->mContext = RequestContext::Fetch;
  copy->mMode = mMode;
  copy->mCredentialsMode = mCredentialsMode;
  copy->mCacheMode = mCacheMode;
  copy->mCreatedByFetchEvent = mCreatedByFetchEvent;
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
  , mContext(aOther.mContext)
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
  , mCreatedByFetchEvent(aOther.mCreatedByFetchEvent)
{
  // NOTE: does not copy body stream... use the fallible Clone() for that
}

InternalRequest::~InternalRequest()
{
}

void
InternalRequest::SetContentPolicyType(nsContentPolicyType aContentPolicyType)
{
  mContentPolicyType = aContentPolicyType;
  switch (aContentPolicyType) {
  case nsIContentPolicy::TYPE_OTHER:
    mContext = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_SCRIPT:
    mContext = RequestContext::Script;
    break;
  case nsIContentPolicy::TYPE_IMAGE:
    mContext = RequestContext::Image;
    break;
  case nsIContentPolicy::TYPE_STYLESHEET:
    mContext = RequestContext::Style;
    break;
  case nsIContentPolicy::TYPE_OBJECT:
    mContext = RequestContext::Object;
    break;
  case nsIContentPolicy::TYPE_DOCUMENT:
    mContext = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_SUBDOCUMENT:
    mContext = RequestContext::Iframe;
    break;
  case nsIContentPolicy::TYPE_REFRESH:
    mContext = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_XBL:
    mContext = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_PING:
    mContext = RequestContext::Ping;
    break;
  case nsIContentPolicy::TYPE_XMLHTTPREQUEST:
    mContext = RequestContext::Xmlhttprequest;
    break;
  case nsIContentPolicy::TYPE_OBJECT_SUBREQUEST:
    mContext = RequestContext::Plugin;
    break;
  case nsIContentPolicy::TYPE_DTD:
    mContext = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_FONT:
    mContext = RequestContext::Font;
    break;
  case nsIContentPolicy::TYPE_MEDIA:
    mContext = RequestContext::Audio;
    break;
  case nsIContentPolicy::TYPE_WEBSOCKET:
    mContext = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_CSP_REPORT:
    mContext = RequestContext::Cspreport;
    break;
  case nsIContentPolicy::TYPE_XSLT:
    mContext = RequestContext::Xslt;
    break;
  case nsIContentPolicy::TYPE_BEACON:
    mContext = RequestContext::Beacon;
    break;
  case nsIContentPolicy::TYPE_FETCH:
    mContext = RequestContext::Fetch;
    break;
  case nsIContentPolicy::TYPE_IMAGESET:
    mContext = RequestContext::Imageset;
    break;
  case nsIContentPolicy::TYPE_WEB_MANIFEST:
    mContext = RequestContext::Manifest;
    break;
  default:
    MOZ_ASSERT(false, "Unhandled nsContentPolicyType value");
    mContext = RequestContext::Internal;
    break;
  }
}

} // namespace dom
} // namespace mozilla
