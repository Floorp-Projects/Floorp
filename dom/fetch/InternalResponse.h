/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InternalResponse_h
#define mozilla_dom_InternalResponse_h

#include "nsIInputStream.h"
#include "nsICacheInfoChannel.h"
#include "nsISupportsImpl.h"
#include "nsProxyRelease.h"

#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/ResponseBinding.h"
#include "mozilla/dom/ChannelInfo.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace ipc {
class PrincipalInfo;
class AutoIPCStream;
} // namespace ipc

namespace dom {

class InternalHeaders;
class IPCInternalResponse;

class InternalResponse final
{
  friend class FetchDriver;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InternalResponse)

  InternalResponse(uint16_t aStatus, const nsACString& aStatusText);

  static already_AddRefed<InternalResponse>
  FromIPC(const IPCInternalResponse& aIPCResponse);

  template<typename M>
  void
  ToIPC(IPCInternalResponse* aIPCResponse,
        M* aManager,
        UniquePtr<mozilla::ipc::AutoIPCStream>& aAutoStream);

  enum CloneType
  {
    eCloneInputStream,
    eDontCloneInputStream,
  };

  already_AddRefed<InternalResponse> Clone(CloneType eCloneType);

  static already_AddRefed<InternalResponse>
  NetworkError(nsresult aRv)
  {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(aRv));
    RefPtr<InternalResponse> response = new InternalResponse(0, EmptyCString());
    ErrorResult result;
    response->Headers()->SetGuard(HeadersGuardEnum::Immutable, result);
    MOZ_ASSERT(!result.Failed());
    response->mType = ResponseType::Error;
    response->mErrorCode = aRv;
    return response.forget();
  }

  already_AddRefed<InternalResponse>
  OpaqueResponse();

  already_AddRefed<InternalResponse>
  OpaqueRedirectResponse();

  already_AddRefed<InternalResponse>
  BasicResponse();

  already_AddRefed<InternalResponse>
  CORSResponse();

  ResponseType
  Type() const
  {
    MOZ_ASSERT_IF(mType == ResponseType::Error, !mWrappedResponse);
    MOZ_ASSERT_IF(mType == ResponseType::Default, !mWrappedResponse);
    MOZ_ASSERT_IF(mType == ResponseType::Basic, mWrappedResponse);
    MOZ_ASSERT_IF(mType == ResponseType::Cors, mWrappedResponse);
    MOZ_ASSERT_IF(mType == ResponseType::Opaque, mWrappedResponse);
    MOZ_ASSERT_IF(mType == ResponseType::Opaqueredirect, mWrappedResponse);
    return mType;
  }

  bool
  IsError() const
  {
    return Type() == ResponseType::Error;
  }
  // GetUrl should return last fetch URL in response's url list and null if
  // response's url list is the empty list.
  const nsCString&
  GetURL() const
  {
    // Empty urlList when response is a synthetic response.
    if (mURLList.IsEmpty()) {
      return EmptyCString();
    }
    return mURLList.LastElement();
  }
  void
  GetURLList(nsTArray<nsCString>& aURLList) const
  {
    aURLList.Assign(mURLList);
  }
  const nsCString&
  GetUnfilteredURL() const
  {
    if (mWrappedResponse) {
      return mWrappedResponse->GetURL();
    }
    return GetURL();
  }
  void
  GetUnfilteredURLList(nsTArray<nsCString>& aURLList) const
  {
    if (mWrappedResponse) {
      return mWrappedResponse->GetURLList(aURLList);
    }

    return GetURLList(aURLList);
  }

  void
  SetURLList(const nsTArray<nsCString>& aURLList)
  {
    mURLList.Assign(aURLList);

#ifdef DEBUG
    for(uint32_t i = 0; i < mURLList.Length(); ++i) {
      MOZ_ASSERT(mURLList[i].Find(NS_LITERAL_CSTRING("#")) == kNotFound);
    }
#endif
  }

  uint16_t
  GetStatus() const
  {
    return mStatus;
  }

  uint16_t
  GetUnfilteredStatus() const
  {
    if (mWrappedResponse) {
      return mWrappedResponse->GetStatus();
    }

    return GetStatus();
  }

  const nsCString&
  GetStatusText() const
  {
    return mStatusText;
  }

  const nsCString&
  GetUnfilteredStatusText() const
  {
    if (mWrappedResponse) {
      return mWrappedResponse->GetStatusText();
    }

    return GetStatusText();
  }

  InternalHeaders*
  Headers()
  {
    return mHeaders;
  }

  InternalHeaders*
  UnfilteredHeaders()
  {
    if (mWrappedResponse) {
      return mWrappedResponse->Headers();
    };

    return Headers();
  }

  void
  GetUnfilteredBody(nsIInputStream** aStream, int64_t* aBodySize = nullptr)
  {
    if (mWrappedResponse) {
      MOZ_ASSERT(!mBody);
      return mWrappedResponse->GetBody(aStream, aBodySize);
    }
    nsCOMPtr<nsIInputStream> stream = mBody;
    stream.forget(aStream);
    if (aBodySize) {
      *aBodySize = mBodySize;
    }
  }

  void
  GetBody(nsIInputStream** aStream, int64_t* aBodySize = nullptr)
  {
    if (Type() == ResponseType::Opaque ||
        Type() == ResponseType::Opaqueredirect) {
      *aStream = nullptr;
      if (aBodySize) {
        *aBodySize = UNKNOWN_BODY_SIZE;
      }
      return;
    }

    GetUnfilteredBody(aStream, aBodySize);
  }

  void
  SetBody(nsIInputStream* aBody, int64_t aBodySize)
  {
    if (mWrappedResponse) {
      return mWrappedResponse->SetBody(aBody, aBodySize);
    }
    // A request's body may not be reset once set.
    MOZ_ASSERT(!mBody);
    MOZ_ASSERT(mBodySize == UNKNOWN_BODY_SIZE);
    // Check arguments.
    MOZ_ASSERT(aBodySize == UNKNOWN_BODY_SIZE || aBodySize >= 0);
    // If body is not given, then size must be unknown.
    MOZ_ASSERT_IF(!aBody, aBodySize == UNKNOWN_BODY_SIZE);

    mBody = aBody;
    mBodySize = aBodySize;
  }

  uint32_t
  GetPaddingInfo();

  nsresult
  GeneratePaddingInfo();

  int64_t
  GetPaddingSize();

  void
  SetPaddingSize(int64_t aPaddingSize);

  void
  SetAlternativeBody(nsIInputStream* aAlternativeBody)
  {
    if (mWrappedResponse) {
      return mWrappedResponse->SetAlternativeBody(aAlternativeBody);
    }
    // A request's body may not be reset once set.
    MOZ_DIAGNOSTIC_ASSERT(!mAlternativeBody);

    mAlternativeBody = aAlternativeBody;
  }

  already_AddRefed<nsIInputStream>
  TakeAlternativeBody()
  {
    if (mWrappedResponse) {
      return mWrappedResponse->TakeAlternativeBody();
    }

    if (!mAlternativeBody) {
      return nullptr;
    }

    // cleanup the non-alternative body here.
    // Once alternative data is used, the real body is no need anymore.
    mBody = nullptr;
    mBodySize = UNKNOWN_BODY_SIZE;
    return mAlternativeBody.forget();
  }

  void
  SetCacheInfoChannel(const nsMainThreadPtrHandle<nsICacheInfoChannel>& aCacheInfoChannel)
  {
    if (mWrappedResponse) {
      return mWrappedResponse->SetCacheInfoChannel(aCacheInfoChannel);
    }
    MOZ_ASSERT(!mCacheInfoChannel);
    mCacheInfoChannel = aCacheInfoChannel;
  }

  nsMainThreadPtrHandle<nsICacheInfoChannel>
  TakeCacheInfoChannel()
  {
    if (mWrappedResponse) {
      return mWrappedResponse->TakeCacheInfoChannel();
    }
    nsMainThreadPtrHandle<nsICacheInfoChannel> rtn = mCacheInfoChannel;
    mCacheInfoChannel = nullptr;
    return rtn;
  }

  void
  InitChannelInfo(nsIChannel* aChannel)
  {
    mChannelInfo.InitFromChannel(aChannel);
  }

  void
  InitChannelInfo(const mozilla::ipc::IPCChannelInfo& aChannelInfo)
  {
    mChannelInfo.InitFromIPCChannelInfo(aChannelInfo);
  }

  void
  InitChannelInfo(const ChannelInfo& aChannelInfo)
  {
    mChannelInfo = aChannelInfo;
  }

  const ChannelInfo&
  GetChannelInfo() const
  {
    return mChannelInfo;
  }

  const UniquePtr<mozilla::ipc::PrincipalInfo>&
  GetPrincipalInfo() const
  {
    return mPrincipalInfo;
  }

  bool
  IsRedirected() const
  {
    return mURLList.Length() > 1;
  }

  nsresult
  GetErrorCode() const
  {
    return mErrorCode;
  }

  // Takes ownership of the principal info.
  void
  SetPrincipalInfo(UniquePtr<mozilla::ipc::PrincipalInfo> aPrincipalInfo);

  LoadTainting
  GetTainting() const;

  already_AddRefed<InternalResponse>
  Unfiltered();

private:
  ~InternalResponse();

  explicit InternalResponse(const InternalResponse& aOther) = delete;
  InternalResponse& operator=(const InternalResponse&) = delete;

  // Returns an instance of InternalResponse which is a copy of this
  // InternalResponse, except headers, body and wrapped response (if any) which
  // are left uninitialized. Used for cloning and filtering.
  already_AddRefed<InternalResponse> CreateIncompleteCopy();

  ResponseType mType;
  nsCString mTerminationReason;
  // A response has an associated url list (a list of zero or more fetch URLs).
  // Unless stated otherwise, it is the empty list. The current url is the last
  // element in mURLlist
  nsTArray<nsCString> mURLList;
  const uint16_t mStatus;
  const nsCString mStatusText;
  RefPtr<InternalHeaders> mHeaders;
  nsCOMPtr<nsIInputStream> mBody;
  int64_t mBodySize;
  // It's used to passed to the CacheResponse to generate padding size. Once, we
  // generate the padding size for resposne, we don't need it anymore.
  Maybe<uint32_t> mPaddingInfo;
  int64_t mPaddingSize;
  nsresult mErrorCode;

  // For alternative data such as JS Bytecode cached in the HTTP cache.
  nsCOMPtr<nsIInputStream> mAlternativeBody;
  nsMainThreadPtrHandle<nsICacheInfoChannel> mCacheInfoChannel;

public:
  static const int64_t UNKNOWN_BODY_SIZE = -1;
  static const int64_t UNKNOWN_PADDING_SIZE = -1;
private:
  ChannelInfo mChannelInfo;
  UniquePtr<mozilla::ipc::PrincipalInfo> mPrincipalInfo;

  // For filtered responses.
  // Cache, and SW interception should always serialize/access the underlying
  // unfiltered headers and when deserializing, create an InternalResponse
  // with the unfiltered headers followed by wrapping it.
  RefPtr<InternalResponse> mWrappedResponse;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InternalResponse_h
