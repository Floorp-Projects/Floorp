/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternalResponse.h"

#include "nsIDOMFile.h"

#include "mozilla/dom/InternalHeaders.h"

namespace mozilla {
namespace dom {

InternalResponse::InternalResponse(uint16_t aStatus, const nsACString& aStatusText)
  : mType(ResponseType::Default)
  , mStatus(aStatus)
  , mStatusText(aStatusText)
  , mHeaders(new InternalHeaders(HeadersGuardEnum::Response))
{
}

// Headers are not copied since BasicResponse and CORSResponse both need custom
// header handling.
InternalResponse::InternalResponse(const InternalResponse& aOther)
  : mType(aOther.mType)
  , mTerminationReason(aOther.mTerminationReason)
  , mURL(aOther.mURL)
  , mStatus(aOther.mStatus)
  , mStatusText(aOther.mStatusText)
  , mBody(aOther.mBody)
  , mContentType(aOther.mContentType)
{
}

// static
already_AddRefed<InternalResponse>
InternalResponse::BasicResponse(InternalResponse* aInner)
{
  MOZ_ASSERT(aInner);
  nsRefPtr<InternalResponse> basic = new InternalResponse(*aInner);
  basic->mType = ResponseType::Basic;
  basic->mHeaders = InternalHeaders::BasicHeaders(aInner->mHeaders);
  return basic.forget();
}

// static
already_AddRefed<InternalResponse>
InternalResponse::CORSResponse(InternalResponse* aInner)
{
  MOZ_ASSERT(aInner);
  nsRefPtr<InternalResponse> cors = new InternalResponse(*aInner);
  cors->mType = ResponseType::Cors;
  cors->mHeaders = InternalHeaders::CORSHeaders(aInner->mHeaders);
  return cors.forget();
}

} // namespace dom
} // namespace mozilla
