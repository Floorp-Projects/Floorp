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

namespace mozilla {
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
    nsRefPtr<InternalResponse> response = new InternalResponse(0, EmptyCString());
    ErrorResult result;
    response->Headers()->SetGuard(HeadersGuardEnum::Immutable, result);
    MOZ_ASSERT(!result.Failed());
    response->mType = ResponseType::Error;
    return response.forget();
  }

  already_AddRefed<InternalResponse>
  OpaqueResponse()
  {
    MOZ_ASSERT(!mWrappedResponse, "Can't OpaqueResponse a already wrapped response");
    nsRefPtr<InternalResponse> response = new InternalResponse(0, EmptyCString());
    response->mType = ResponseType::Opaque;
    response->mTerminationReason = mTerminationReason;
    response->mURL = mURL;
    response->mFinalURL = mFinalURL;
    response->mChannelInfo = mChannelInfo;
    response->mWrappedResponse = this;
    return response.forget();
  }

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
    return mType;
  }

  bool
  IsError() const
  {
    return Type() == ResponseType::Error;
  }

  // FIXME(nsm): Return with exclude fragment.
  void
  GetUrl(nsCString& aURL) const
  {
    aURL.Assign(mURL);
  }

  void
  SetUrl(const nsACString& aURL)
  {
    mURL.Assign(aURL);
  }

  bool
  FinalURL() const
  {
    return mFinalURL;
  }

  void
  SetFinalURL(bool aFinalURL)
  {
    mFinalURL = aFinalURL;
  }

  uint16_t
  GetStatus() const
  {
    return mStatus;
  }

  const nsCString&
  GetStatusText() const
  {
    return mStatusText;
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
  GetBody(nsIInputStream** aStream)
  {
    if (Type() == ResponseType::Opaque) {
      *aStream = nullptr;
      return;
    }

    if (mWrappedResponse) {
      MOZ_ASSERT(!mBody);
      return mWrappedResponse->GetBody(aStream);
    }
    nsCOMPtr<nsIInputStream> stream = mBody;
    stream.forget(aStream);
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

private:
  ~InternalResponse()
  { }

  explicit InternalResponse(const InternalResponse& aOther) = delete;
  InternalResponse& operator=(const InternalResponse&) = delete;

  // Returns an instance of InternalResponse which is a copy of this
  // InternalResponse, except headers, body and wrapped response (if any) which
  // are left uninitialized. Used for cloning and filtering.
  already_AddRefed<InternalResponse> CreateIncompleteCopy()
  {
    nsRefPtr<InternalResponse> copy = new InternalResponse(mStatus, mStatusText);
    copy->mType = mType;
    copy->mTerminationReason = mTerminationReason;
    copy->mURL = mURL;
    copy->mFinalURL = mFinalURL;
    copy->mChannelInfo = mChannelInfo;
    return copy.forget();
  }

  ResponseType mType;
  nsCString mTerminationReason;
  nsCString mURL;
  bool mFinalURL;
  const uint16_t mStatus;
  const nsCString mStatusText;
  nsRefPtr<InternalHeaders> mHeaders;
  nsCOMPtr<nsIInputStream> mBody;
  ChannelInfo mChannelInfo;

  // For filtered responses.
  // Cache, and SW interception should always serialize/access the underlying
  // unfiltered headers and when deserializing, create an InternalResponse
  // with the unfiltered headers followed by wrapping it.
  nsRefPtr<InternalResponse> mWrappedResponse;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InternalResponse_h
