/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternalResponse.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIURI.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace dom {

InternalResponse::InternalResponse(uint16_t aStatus, const nsACString& aStatusText)
  : mType(ResponseType::Default)
  , mStatus(aStatus)
  , mStatusText(aStatusText)
  , mHeaders(new InternalHeaders(HeadersGuardEnum::Response))
{
}

InternalResponse::~InternalResponse()
{
}

already_AddRefed<InternalResponse>
InternalResponse::Clone()
{
  RefPtr<InternalResponse> clone = CreateIncompleteCopy();

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
  RefPtr<InternalResponse> basic = CreateIncompleteCopy();
  basic->mType = ResponseType::Basic;
  basic->mHeaders = InternalHeaders::BasicHeaders(Headers());
  basic->mWrappedResponse = this;
  return basic.forget();
}

already_AddRefed<InternalResponse>
InternalResponse::CORSResponse()
{
  MOZ_ASSERT(!mWrappedResponse, "Can't CORSResponse a already wrapped response");
  RefPtr<InternalResponse> cors = CreateIncompleteCopy();
  cors->mType = ResponseType::Cors;
  cors->mHeaders = InternalHeaders::CORSHeaders(Headers());
  cors->mWrappedResponse = this;
  return cors.forget();
}

void
InternalResponse::SetPrincipalInfo(UniquePtr<mozilla::ipc::PrincipalInfo> aPrincipalInfo)
{
  mPrincipalInfo = Move(aPrincipalInfo);
}

nsresult
InternalResponse::StripFragmentAndSetUrl(const nsACString& aUrl)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> iuri;
  nsresult rv;

  rv = NS_NewURI(getter_AddRefs(iuri), aUrl);
  if(NS_WARN_IF(NS_FAILED(rv))){
    return rv;
  }

  nsCOMPtr<nsIURI> iuriClone;
  // We use CloneIgnoringRef to strip away the fragment even if the original URI
  // is immutable.
  rv = iuri->CloneIgnoringRef(getter_AddRefs(iuriClone));
  if(NS_WARN_IF(NS_FAILED(rv))){
    return rv;
  }

  nsCString spec;
  rv = iuriClone->GetSpec(spec);
  if(NS_WARN_IF(NS_FAILED(rv))){
    return rv;
  }

  SetUrl(spec);
  return NS_OK;
}

LoadTainting
InternalResponse::GetTainting() const
{
  switch (mType) {
    case ResponseType::Cors:
      return LoadTainting::CORS;
    case ResponseType::Opaque:
      return LoadTainting::Opaque;
    default:
      return LoadTainting::Basic;
  }
}

already_AddRefed<InternalResponse>
InternalResponse::Unfiltered()
{
  RefPtr<InternalResponse> ref = mWrappedResponse;
  if (!ref) {
    ref = this;
  }
  return ref.forget();
}

already_AddRefed<InternalResponse>
InternalResponse::OpaqueResponse()
{
  MOZ_ASSERT(!mWrappedResponse, "Can't OpaqueResponse a already wrapped response");
  RefPtr<InternalResponse> response = new InternalResponse(0, EmptyCString());
  response->mType = ResponseType::Opaque;
  response->mTerminationReason = mTerminationReason;
  response->mChannelInfo = mChannelInfo;
  if (mPrincipalInfo) {
    response->mPrincipalInfo = MakeUnique<mozilla::ipc::PrincipalInfo>(*mPrincipalInfo);
  }
  response->mWrappedResponse = this;
  return response.forget();
}

already_AddRefed<InternalResponse>
InternalResponse::OpaqueRedirectResponse()
{
  MOZ_ASSERT(!mWrappedResponse, "Can't OpaqueRedirectResponse a already wrapped response");
  RefPtr<InternalResponse> response = OpaqueResponse();
  response->mType = ResponseType::Opaqueredirect;
  response->mURL = mURL;
  return response.forget();
}

already_AddRefed<InternalResponse>
InternalResponse::CreateIncompleteCopy()
{
  RefPtr<InternalResponse> copy = new InternalResponse(mStatus, mStatusText);
  copy->mType = mType;
  copy->mTerminationReason = mTerminationReason;
  copy->mURL = mURL;
  copy->mChannelInfo = mChannelInfo;
  if (mPrincipalInfo) {
    copy->mPrincipalInfo = MakeUnique<mozilla::ipc::PrincipalInfo>(*mPrincipalInfo);
  }
  return copy.forget();
}

} // namespace dom
} // namespace mozilla
