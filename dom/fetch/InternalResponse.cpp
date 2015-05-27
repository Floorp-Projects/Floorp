/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternalResponse.h"

#include "mozilla/dom/InternalHeaders.h"
#include "nsStreamUtils.h"
#include "nsSerializationHelper.h"

namespace mozilla {
namespace dom {

InternalResponse::InternalResponse(uint16_t aStatus, const nsACString& aStatusText)
  : mType(ResponseType::Default)
  , mFinalURL(false)
  , mStatus(aStatus)
  , mStatusText(aStatusText)
  , mHeaders(new InternalHeaders(HeadersGuardEnum::Response))
{
}

already_AddRefed<InternalResponse>
InternalResponse::Clone()
{
  nsRefPtr<InternalResponse> clone = CreateIncompleteCopy();

  clone->mHeaders = new InternalHeaders(*mHeaders);
  if (mWrappedResponse) {
    clone->mWrappedResponse = mWrappedResponse->Clone();
    MOZ_ASSERT(!mBody);
    return clone.forget();
  }

  if (!mBody) {
    return clone.forget();
  }

  nsCOMPtr<nsIInputStream> clonedBody;
  nsCOMPtr<nsIInputStream> replacementBody;

  nsresult rv = NS_CloneInputStream(mBody, getter_AddRefs(clonedBody),
                                    getter_AddRefs(replacementBody));
  if (NS_WARN_IF(NS_FAILED(rv))) { return nullptr; }

  clone->mBody.swap(clonedBody);
  if (replacementBody) {
    mBody.swap(replacementBody);
  }

  return clone.forget();
}

already_AddRefed<InternalResponse>
InternalResponse::BasicResponse()
{
  MOZ_ASSERT(!mWrappedResponse, "Can't BasicResponse a already wrapped response");
  nsRefPtr<InternalResponse> basic = CreateIncompleteCopy();
  basic->mType = ResponseType::Basic;
  basic->mHeaders = InternalHeaders::BasicHeaders(Headers());
  basic->mWrappedResponse = this;
  return basic.forget();
}

already_AddRefed<InternalResponse>
InternalResponse::CORSResponse()
{
  MOZ_ASSERT(!mWrappedResponse, "Can't CORSResponse a already wrapped response");
  nsRefPtr<InternalResponse> cors = CreateIncompleteCopy();
  cors->mType = ResponseType::Cors;
  cors->mHeaders = InternalHeaders::CORSHeaders(Headers());
  cors->mWrappedResponse = this;
  return cors.forget();
}

void
InternalResponse::SetSecurityInfo(nsISupports* aSecurityInfo)
{
  MOZ_ASSERT(mSecurityInfo.IsEmpty(), "security info should only be set once");
  nsCOMPtr<nsISerializable> serializable = do_QueryInterface(aSecurityInfo);
  if (!serializable) {
    NS_WARNING("A non-serializable object was passed to InternalResponse::SetSecurityInfo");
    return;
  }
  NS_SerializeToString(serializable, mSecurityInfo);
}

void
InternalResponse::SetSecurityInfo(const nsCString& aSecurityInfo)
{
  MOZ_ASSERT(mSecurityInfo.IsEmpty(), "security info should only be set once");
  mSecurityInfo = aSecurityInfo;
}

} // namespace dom
} // namespace mozilla
