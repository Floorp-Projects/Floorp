/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternalResponse.h"

#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/FetchTypes.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/RandomNum.h"
#include "nsIRandomGenerator.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace dom {

namespace {

// Const variable for generate padding size
// XXX This will be tweaked to something more meaningful in Bug 1383656.
const uint32_t kMaxRandomNumber = 102400;

}  // namespace

InternalResponse::InternalResponse(uint16_t aStatus,
                                   const nsACString& aStatusText,
                                   RequestCredentials aCredentialsMode)
    : mType(ResponseType::Default),
      mStatus(aStatus),
      mStatusText(aStatusText),
      mHeaders(new InternalHeaders(HeadersGuardEnum::Response)),
      mBodySize(UNKNOWN_BODY_SIZE),
      mPaddingSize(UNKNOWN_PADDING_SIZE),
      mErrorCode(NS_OK),
      mCredentialsMode(aCredentialsMode) {}

/* static */ RefPtr<InternalResponse> InternalResponse::FromIPC(
    const IPCInternalResponse& aIPCResponse) {
  if (aIPCResponse.type() == ResponseType::Error) {
    return InternalResponse::NetworkError(aIPCResponse.errorCode());
  }

  RefPtr<InternalResponse> response =
      new InternalResponse(aIPCResponse.status(), aIPCResponse.statusText());

  response->SetURLList(aIPCResponse.urlList());
  response->mHeaders =
      new InternalHeaders(aIPCResponse.headers(), aIPCResponse.headersGuard());

  nsCOMPtr<nsIInputStream> body =
      mozilla::ipc::DeserializeIPCStream(aIPCResponse.body());
  response->SetBody(body, aIPCResponse.bodySize());

  response->SetAlternativeDataType(aIPCResponse.alternativeDataType());

  nsCOMPtr<nsIInputStream> alternativeBody =
      mozilla::ipc::DeserializeIPCStream(aIPCResponse.alternativeBody());
  response->SetAlternativeBody(alternativeBody);

  response->InitChannelInfo(aIPCResponse.channelInfo());

  if (aIPCResponse.principalInfo()) {
    response->SetPrincipalInfo(MakeUnique<mozilla::ipc::PrincipalInfo>(
        aIPCResponse.principalInfo().ref()));
  }

  switch (aIPCResponse.type()) {
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

  return response;
}

InternalResponse::~InternalResponse() = default;

template void InternalResponse::ToIPC<mozilla::ipc::PBackgroundChild>(
    IPCInternalResponse* aIPCResponse, mozilla::ipc::PBackgroundChild* aManager,
    UniquePtr<mozilla::ipc::AutoIPCStream>& aAutoBodyStream,
    UniquePtr<mozilla::ipc::AutoIPCStream>& aAutoAlternativeBodyStream);

template <typename M>
void InternalResponse::ToIPC(
    IPCInternalResponse* aIPCResponse, M* aManager,
    UniquePtr<mozilla::ipc::AutoIPCStream>& aAutoBodyStream,
    UniquePtr<mozilla::ipc::AutoIPCStream>& aAutoAlternativeBodyStream) {
  MOZ_ASSERT(aIPCResponse);

  aIPCResponse->type() = mType;
  GetUnfilteredURLList(aIPCResponse->urlList());
  aIPCResponse->status() = GetUnfilteredStatus();
  aIPCResponse->statusText() = GetUnfilteredStatusText();
  UnfilteredHeaders()->ToIPC(aIPCResponse->headers(),
                             aIPCResponse->headersGuard());

  nsCOMPtr<nsIInputStream> body;
  int64_t bodySize;
  GetUnfilteredBody(getter_AddRefs(body), &bodySize);

  if (body) {
    aAutoBodyStream.reset(
        new mozilla::ipc::AutoIPCStream(aIPCResponse->body()));
    DebugOnly<bool> ok = aAutoBodyStream->Serialize(body, aManager);
    MOZ_ASSERT(ok);
  }

  aIPCResponse->bodySize() = bodySize;
  aIPCResponse->errorCode() = mErrorCode;
  aIPCResponse->alternativeDataType() = GetAlternativeDataType();

  nsCOMPtr<nsIInputStream> alternativeBody = TakeAlternativeBody();
  if (alternativeBody) {
    aAutoAlternativeBodyStream.reset(
        new mozilla::ipc::AutoIPCStream(aIPCResponse->alternativeBody()));
    DebugOnly<bool> ok =
        aAutoAlternativeBodyStream->Serialize(alternativeBody, aManager);
    MOZ_ASSERT(ok);
  }

  aIPCResponse->channelInfo() = mChannelInfo.AsIPCChannelInfo();

  if (mPrincipalInfo) {
    aIPCResponse->principalInfo().emplace(*mPrincipalInfo);
  }
}

already_AddRefed<InternalResponse> InternalResponse::Clone(
    CloneType aCloneType) {
  RefPtr<InternalResponse> clone = CreateIncompleteCopy();

  clone->mHeaders = new InternalHeaders(*mHeaders);

  // Make sure the clone response will have the same padding size.
  clone->mPaddingInfo = mPaddingInfo;
  clone->mPaddingSize = mPaddingSize;

  clone->mCacheInfoChannel = mCacheInfoChannel;

  if (mWrappedResponse) {
    clone->mWrappedResponse = mWrappedResponse->Clone(aCloneType);
    MOZ_ASSERT(!mBody);
    return clone.forget();
  }

  if (!mBody || aCloneType == eDontCloneInputStream) {
    return clone.forget();
  }

  nsCOMPtr<nsIInputStream> clonedBody;
  nsCOMPtr<nsIInputStream> replacementBody;

  nsresult rv = NS_CloneInputStream(mBody, getter_AddRefs(clonedBody),
                                    getter_AddRefs(replacementBody));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  clone->mBody.swap(clonedBody);
  if (replacementBody) {
    mBody.swap(replacementBody);
  }

  return clone.forget();
}

already_AddRefed<InternalResponse> InternalResponse::BasicResponse() {
  MOZ_ASSERT(!mWrappedResponse,
             "Can't BasicResponse a already wrapped response");
  RefPtr<InternalResponse> basic = CreateIncompleteCopy();
  basic->mType = ResponseType::Basic;
  basic->mHeaders = InternalHeaders::BasicHeaders(Headers());
  basic->mWrappedResponse = this;
  return basic.forget();
}

already_AddRefed<InternalResponse> InternalResponse::CORSResponse() {
  MOZ_ASSERT(!mWrappedResponse,
             "Can't CORSResponse a already wrapped response");
  RefPtr<InternalResponse> cors = CreateIncompleteCopy();
  cors->mType = ResponseType::Cors;
  cors->mHeaders = InternalHeaders::CORSHeaders(Headers(), mCredentialsMode);
  cors->mWrappedResponse = this;
  return cors.forget();
}

uint32_t InternalResponse::GetPaddingInfo() {
  // If it's an opaque response, the paddingInfo should be generated only when
  // paddingSize is unknown size.
  // If it's not, the paddingInfo should be nothing and the paddingSize should
  // be unknown size.
  MOZ_DIAGNOSTIC_ASSERT(
      (mType == ResponseType::Opaque && mPaddingSize == UNKNOWN_PADDING_SIZE &&
       mPaddingInfo.isSome()) ||
      (mType == ResponseType::Opaque && mPaddingSize != UNKNOWN_PADDING_SIZE &&
       mPaddingInfo.isNothing()) ||
      (mType != ResponseType::Opaque && mPaddingSize == UNKNOWN_PADDING_SIZE &&
       mPaddingInfo.isNothing()));
  return mPaddingInfo.isSome() ? mPaddingInfo.ref() : 0;
}

nsresult InternalResponse::GeneratePaddingInfo() {
  MOZ_DIAGNOSTIC_ASSERT(mType == ResponseType::Opaque);
  MOZ_DIAGNOSTIC_ASSERT(mPaddingSize == UNKNOWN_PADDING_SIZE);

  // Utilize random generator to generator a random number
  nsresult rv;
  uint32_t randomNumber = 0;
  nsCOMPtr<nsIRandomGenerator> randomGenerator =
      do_GetService("@mozilla.org/security/random-generator;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Maybe<uint64_t> maybeRandomNum = RandomUint64();
    if (maybeRandomNum.isSome()) {
      mPaddingInfo.emplace(uint32_t(maybeRandomNum.value() % kMaxRandomNumber));
      return NS_OK;
    }
    return rv;
  }

  MOZ_DIAGNOSTIC_ASSERT(randomGenerator);

  uint8_t* buffer;
  rv = randomGenerator->GenerateRandomBytes(sizeof(randomNumber), &buffer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Maybe<uint64_t> maybeRandomNum = RandomUint64();
    if (maybeRandomNum.isSome()) {
      mPaddingInfo.emplace(uint32_t(maybeRandomNum.value() % kMaxRandomNumber));
      return NS_OK;
    }
    return rv;
  }

  memcpy(&randomNumber, buffer, sizeof(randomNumber));
  free(buffer);

  mPaddingInfo.emplace(randomNumber % kMaxRandomNumber);

  return rv;
}

int64_t InternalResponse::GetPaddingSize() {
  // We initialize padding size to an unknown size (-1). After cached, we only
  // pad opaque response. Opaque response's padding size might be unknown before
  // cached.
  MOZ_DIAGNOSTIC_ASSERT(mType == ResponseType::Opaque ||
                        mPaddingSize == UNKNOWN_PADDING_SIZE);
  MOZ_DIAGNOSTIC_ASSERT(mPaddingSize == UNKNOWN_PADDING_SIZE ||
                        mPaddingSize >= 0);

  return mPaddingSize;
}

void InternalResponse::SetPaddingSize(int64_t aPaddingSize) {
  // We should only pad the opaque response.
  MOZ_DIAGNOSTIC_ASSERT(
      (mType == ResponseType::Opaque) !=
      (aPaddingSize == InternalResponse::UNKNOWN_PADDING_SIZE));
  MOZ_DIAGNOSTIC_ASSERT(aPaddingSize == UNKNOWN_PADDING_SIZE ||
                        aPaddingSize >= 0);

  mPaddingSize = aPaddingSize;
}

void InternalResponse::SetPrincipalInfo(
    UniquePtr<mozilla::ipc::PrincipalInfo> aPrincipalInfo) {
  mPrincipalInfo = std::move(aPrincipalInfo);
}

LoadTainting InternalResponse::GetTainting() const {
  switch (mType) {
    case ResponseType::Cors:
      return LoadTainting::CORS;
    case ResponseType::Opaque:
      return LoadTainting::Opaque;
    default:
      return LoadTainting::Basic;
  }
}

already_AddRefed<InternalResponse> InternalResponse::Unfiltered() {
  RefPtr<InternalResponse> ref = mWrappedResponse;
  if (!ref) {
    ref = this;
  }
  return ref.forget();
}

already_AddRefed<InternalResponse> InternalResponse::OpaqueResponse() {
  MOZ_ASSERT(!mWrappedResponse,
             "Can't OpaqueResponse a already wrapped response");
  RefPtr<InternalResponse> response = new InternalResponse(0, EmptyCString());
  response->mType = ResponseType::Opaque;
  response->mChannelInfo = mChannelInfo;
  if (mPrincipalInfo) {
    response->mPrincipalInfo =
        MakeUnique<mozilla::ipc::PrincipalInfo>(*mPrincipalInfo);
  }
  response->mWrappedResponse = this;
  return response.forget();
}

already_AddRefed<InternalResponse> InternalResponse::OpaqueRedirectResponse() {
  MOZ_ASSERT(!mWrappedResponse,
             "Can't OpaqueRedirectResponse a already wrapped response");
  MOZ_ASSERT(!mURLList.IsEmpty(),
             "URLList should not be emtpy for internalResponse");
  RefPtr<InternalResponse> response = OpaqueResponse();
  response->mType = ResponseType::Opaqueredirect;
  response->mURLList = mURLList.Clone();
  return response.forget();
}

already_AddRefed<InternalResponse> InternalResponse::CreateIncompleteCopy() {
  RefPtr<InternalResponse> copy = new InternalResponse(mStatus, mStatusText);
  copy->mType = mType;
  copy->mURLList = mURLList.Clone();
  copy->mChannelInfo = mChannelInfo;
  if (mPrincipalInfo) {
    copy->mPrincipalInfo =
        MakeUnique<mozilla::ipc::PrincipalInfo>(*mPrincipalInfo);
  }
  return copy.forget();
}

}  // namespace dom
}  // namespace mozilla
