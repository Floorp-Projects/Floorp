/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InternalResponse_h
#define mozilla_dom_InternalResponse_h

#include "nsIInputStream.h"
#include "nsISupportsImpl.h"

#include "mozilla/dom/ResponseBinding.h"
#include "mozilla/dom/ChannelInfo.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace ipc {
class PrincipalInfo;
} // namespace ipc

namespace dom {

class InternalHeaders;

class InternalResponse final
{
  friend class FetchDriver;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InternalResponse)

  InternalResponse(uint16_t aStatus, const nsACString& aStatusText);

  already_AddRefed<InternalResponse> Clone();

  static already_AddRefed<InternalResponse>
  NetworkError()
  {
    RefPtr<InternalResponse> response = new InternalResponse(0, EmptyCString());
    ErrorResult result;
    response->Headers()->SetGuard(HeadersGuardEnum::Immutable, result);
    MOZ_ASSERT(!result.Failed());
    response->mType = ResponseType::Error;
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
  void
  GetURL(nsCString& aURL) const
  {
    // Empty urlList when response is a synthetic response.
    if (mURLList.IsEmpty()) {
      aURL.Truncate();
      return;
    }

    aURL.Assign(mURLList.LastElement());
  }

  void
  GetURLList(nsTArray<nsCString>& aURLList) const
  {
    aURLList.Assign(mURLList);
  }

  void
  GetUnfilteredURL(nsCString& aURL) const
  {
    if (mWrappedResponse) {
      return mWrappedResponse->GetURL(aURL);
    }

    return GetURL(aURL);
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
  GetUnfilteredBody(nsIInputStream** aStream)
  {
    if (mWrappedResponse) {
      MOZ_ASSERT(!mBody);
      return mWrappedResponse->GetBody(aStream);
    }
    nsCOMPtr<nsIInputStream> stream = mBody;
    stream.forget(aStream);
  }

  void
  GetBody(nsIInputStream** aStream)
  {
    if (Type() == ResponseType::Opaque ||
        Type() == ResponseType::Opaqueredirect) {
      *aStream = nullptr;
      return;
    }

    return GetUnfilteredBody(aStream);
  }

  void
  SetBody(nsIInputStream* aBody)
  {
    if (mWrappedResponse) {
      return mWrappedResponse->SetBody(aBody);
    }
    // A request's body may not be reset once set.
    MOZ_ASSERT(!mBody);
    mBody = aBody;
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
