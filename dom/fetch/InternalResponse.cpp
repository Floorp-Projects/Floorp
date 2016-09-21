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
#include "mozilla/ipc/IPCStreamUtils.h"
#include "nsIURI.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace dom {

InternalResponse::InternalResponse(uint16_t aStatus, const nsACString& aStatusText)
  : mType(ResponseType::Default)
  , mStatus(aStatus)
  , mStatusText(aStatusText)
  , mHeaders(new InternalHeaders(HeadersGuardEnum::Response))
  , mBodySize(UNKNOWN_BODY_SIZE)
{
}

already_AddRefed<InternalResponse>
InternalResponse::FromIPC(const IPCInternalResponse& aIPCResponse)
{
  if (aIPCResponse.type() == ResponseType::Error) {
    return InternalResponse::NetworkError();
  }

  RefPtr<InternalResponse> response =
    new InternalResponse(aIPCResponse.status(),
                         aIPCResponse.statusText());

  response->SetURLList(aIPCResponse.urlList());

  response->mHeaders = new InternalHeaders(aIPCResponse.headers(),
                                           aIPCResponse.headersGuard());

  response->InitChannelInfo(aIPCResponse.channelInfo());
  if (aIPCResponse.principalInfo().type() == mozilla::ipc::OptionalPrincipalInfo::TPrincipalInfo) {
    UniquePtr<mozilla::ipc::PrincipalInfo> info(new mozilla::ipc::PrincipalInfo(aIPCResponse.principalInfo().get_PrincipalInfo()));
    response->SetPrincipalInfo(Move(info));
  }

  nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(aIPCResponse.body());
  response->SetBody(stream, aIPCResponse.bodySize());

  switch (aIPCResponse.type())
  {
    case ResponseType::Basic:
      response = response->BasicResponse();
      break;
    case ResponseType::Cors:
      response = response->CORSResponse();
      break;
    case ResponseType::Default:
      break;
    case ResponseType::Opaque:
      response = response->OpaqueResponse();
      break;
    case ResponseType::Opaqueredirect:
      response = response->OpaqueRedirectResponse();
      break;
    default:
      MOZ_CRASH("Unexpected ResponseType!");
  }
  MOZ_ASSERT(response);

  return response.forget();
}

InternalResponse::~InternalResponse()
{
}

template void
InternalResponse::ToIPC<PContentParent>
  (IPCInternalResponse* aIPCResponse,
   PContentParent* aManager,
   UniquePtr<mozilla::ipc::AutoIPCStream>& aAutoStream);
template void
InternalResponse::ToIPC<nsIContentChild>
  (IPCInternalResponse* aIPCResponse,
   nsIContentChild* aManager,
   UniquePtr<mozilla::ipc::AutoIPCStream>& aAutoStream);
template void
InternalResponse::ToIPC<mozilla::ipc::PBackgroundParent>
  (IPCInternalResponse* aIPCResponse,
   mozilla::ipc::PBackgroundParent* aManager,
   UniquePtr<mozilla::ipc::AutoIPCStream>& aAutoStream);
template void
InternalResponse::ToIPC<mozilla::ipc::PBackgroundChild>
  (IPCInternalResponse* aIPCResponse,
   mozilla::ipc::PBackgroundChild* aManager,
   UniquePtr<mozilla::ipc::AutoIPCStream>& aAutoStream);

template<typename M>
void
InternalResponse::ToIPC(IPCInternalResponse* aIPCResponse,
                        M* aManager,
                        UniquePtr<mozilla::ipc::AutoIPCStream>& aAutoStream)
{
  MOZ_ASSERT(aIPCResponse);
  aIPCResponse->type() = mType;
  aIPCResponse->urlList() = mURLList;
  aIPCResponse->status() = GetUnfilteredStatus();
  aIPCResponse->statusText() = GetUnfilteredStatusText();

  mHeaders->ToIPC(aIPCResponse->headers(), aIPCResponse->headersGuard());

  aIPCResponse->channelInfo() = mChannelInfo.AsIPCChannelInfo();
  if (mPrincipalInfo) {
    aIPCResponse->principalInfo() = *mPrincipalInfo;
  } else {
    aIPCResponse->principalInfo() = void_t();
  }

  nsCOMPtr<nsIInputStream> body;
  int64_t bodySize;
  GetUnfilteredBody(getter_AddRefs(body), &bodySize);

  if (body) {
    aAutoStream.reset(new mozilla::ipc::AutoIPCStream(aIPCResponse->body()));
    aAutoStream->Serialize(body, aManager);
  } else {
    aIPCResponse->body() = void_t();
  }

  aIPCResponse->bodySize() = bodySize;
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
  MOZ_ASSERT(!mURLList.IsEmpty(), "URLList should not be emtpy for internalResponse");
  RefPtr<InternalResponse> response = OpaqueResponse();
  response->mType = ResponseType::Opaqueredirect;
  response->mURLList = mURLList;
  return response.forget();
}

already_AddRefed<InternalResponse>
InternalResponse::CreateIncompleteCopy()
{
  RefPtr<InternalResponse> copy = new InternalResponse(mStatus, mStatusText);
  copy->mType = mType;
  copy->mTerminationReason = mTerminationReason;
  copy->mURLList = mURLList;
  copy->mChannelInfo = mChannelInfo;
  if (mPrincipalInfo) {
    copy->mPrincipalInfo = MakeUnique<mozilla::ipc::PrincipalInfo>(*mPrincipalInfo);
  }
  return copy.forget();
}

} // namespace dom
} // namespace mozilla
