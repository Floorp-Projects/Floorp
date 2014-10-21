/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InternalResponse_h
#define mozilla_dom_InternalResponse_h

#include "nsIInputStream.h"
#include "nsISupportsImpl.h"

#include "mozilla/dom/ResponseBinding.h"

namespace mozilla {
namespace dom {

class InternalHeaders;

class InternalResponse MOZ_FINAL
{
  friend class FetchDriver;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InternalResponse)

  InternalResponse(uint16_t aStatus, const nsACString& aStatusText);

  static already_AddRefed<InternalResponse>
  NetworkError()
  {
    nsRefPtr<InternalResponse> response = new InternalResponse(0, EmptyCString());
    response->mType = ResponseType::Error;
    return response.forget();
  }

  static already_AddRefed<InternalResponse>
  OpaqueResponse()
  {
    nsRefPtr<InternalResponse> response = new InternalResponse(0, EmptyCString());
    response->mType = ResponseType::Opaque;
    return response.forget();
  }

  // DO NOT use the inner response after filtering it since the filtered
  // response will adopt the inner response's body.
  static already_AddRefed<InternalResponse>
  BasicResponse(InternalResponse* aInner);

  // DO NOT use the inner response after filtering it since the filtered
  // response will adopt the inner response's body.
  static already_AddRefed<InternalResponse>
  CORSResponse(InternalResponse* aInner);

  ResponseType
  Type() const
  {
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

  void
  GetBody(nsIInputStream** aStream)
  {
    nsCOMPtr<nsIInputStream> stream = mBody;
    stream.forget(aStream);
  }

  void
  SetBody(nsIInputStream* aBody)
  {
    mBody = aBody;
  }

private:
  ~InternalResponse()
  { }

  // Used to create filtered responses.
  // Does not copy headers.
  explicit InternalResponse(const InternalResponse& aOther);

  ResponseType mType;
  nsCString mTerminationReason;
  nsCString mURL;
  const uint16_t mStatus;
  const nsCString mStatusText;
  nsRefPtr<InternalHeaders> mHeaders;
  nsCOMPtr<nsIInputStream> mBody;
  nsCString mContentType;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InternalResponse_h
