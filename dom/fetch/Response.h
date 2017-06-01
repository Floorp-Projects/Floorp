/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Response_h
#define mozilla_dom_Response_h

#include "nsWrapperCache.h"
#include "nsISupportsImpl.h"

#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/ResponseBinding.h"

#include "InternalHeaders.h"
#include "InternalResponse.h"

namespace mozilla {
namespace ipc {
class PrincipalInfo;
} // namespace ipc

namespace dom {

class Headers;

class Response final : public nsISupports
                     , public FetchBody<Response>
                     , public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Response)

public:
  Response(nsIGlobalObject* aGlobal, InternalResponse* aInternalResponse);

  Response(const Response& aOther) = delete;

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return ResponseBinding::Wrap(aCx, this, aGivenProto);
  }

  ResponseType
  Type() const
  {
    return mInternalResponse->Type();
  }
  void
  GetUrl(nsAString& aUrl) const
  {
    CopyUTF8toUTF16(mInternalResponse->GetURL(), aUrl);
  }
  bool
  Redirected() const
  {
    return mInternalResponse->IsRedirected();
  }
  uint16_t
  Status() const
  {
    return mInternalResponse->GetStatus();
  }

  bool
  Ok() const
  {
    return mInternalResponse->GetStatus() >= 200 &&
           mInternalResponse->GetStatus() <= 299;
  }

  void
  GetStatusText(nsCString& aStatusText) const
  {
    aStatusText = mInternalResponse->GetStatusText();
  }

  InternalHeaders*
  GetInternalHeaders() const
  {
    return mInternalResponse->Headers();
  }

  void
  InitChannelInfo(nsIChannel* aChannel)
  {
    mInternalResponse->InitChannelInfo(aChannel);
  }

  const ChannelInfo&
  GetChannelInfo() const
  {
    return mInternalResponse->GetChannelInfo();
  }

  const UniquePtr<mozilla::ipc::PrincipalInfo>&
  GetPrincipalInfo() const
  {
    return mInternalResponse->GetPrincipalInfo();
  }

  Headers* Headers_();

  void
  GetBody(nsIInputStream** aStream) { return mInternalResponse->GetBody(aStream); }

  static already_AddRefed<Response>
  Error(const GlobalObject& aGlobal);

  static already_AddRefed<Response>
  Redirect(const GlobalObject& aGlobal, const nsAString& aUrl, uint16_t aStatus, ErrorResult& aRv);

  static already_AddRefed<Response>
  Constructor(const GlobalObject& aGlobal,
              const Optional<fetch::BodyInit>& aBody,
              const ResponseInit& aInit, ErrorResult& rv);

  nsIGlobalObject* GetParentObject() const
  {
    return mOwner;
  }

  already_AddRefed<Response>
  Clone(ErrorResult& aRv) const;

  already_AddRefed<Response>
  CloneUnfiltered(ErrorResult& aRv) const;

  void
  SetBody(nsIInputStream* aBody, int64_t aBodySize);

  already_AddRefed<InternalResponse>
  GetInternalResponse() const;

private:
  ~Response();

  RefPtr<InternalResponse> mInternalResponse;
  // Lazily created
  RefPtr<Headers> mHeaders;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Response_h
