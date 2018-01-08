/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Request_h
#define mozilla_dom_Request_h

#include "nsIContentPolicy.h"
#include "nsISupportsImpl.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/InternalRequest.h"
// Required here due to certain WebIDL enums/classes being declared in both
// files.
#include "mozilla/dom/RequestBinding.h"

namespace mozilla {
namespace dom {

class Headers;
class InternalHeaders;
class RequestOrUSVString;

class Request final : public nsISupports
                    , public FetchBody<Request>
                    , public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Request)

public:
  Request(nsIGlobalObject* aOwner, InternalRequest* aRequest,
          AbortSignal* aSignal);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return RequestBinding::Wrap(aCx, this, aGivenProto);
  }

  void
  GetUrl(nsAString& aUrl) const
  {
    nsAutoCString url;
    mRequest->GetURL(url);
    CopyUTF8toUTF16(url, aUrl);
  }

  void
  GetMethod(nsCString& aMethod) const
  {
    aMethod = mRequest->mMethod;
  }

  RequestMode
  Mode() const
  {
    return mRequest->mMode;
  }

  RequestCredentials
  Credentials() const
  {
    return mRequest->mCredentialsMode;
  }

  RequestCache
  Cache() const
  {
    return mRequest->GetCacheMode();
  }

  RequestRedirect
  Redirect() const
  {
    return mRequest->GetRedirectMode();
  }

  void
  GetIntegrity(nsAString& aIntegrity) const
  {
    aIntegrity = mRequest->GetIntegrity();
  }

  bool
  MozErrors() const
  {
    return mRequest->MozErrors();
  }

  RequestContext
  Context() const
  {
    return mRequest->Context();
  }

  void
  OverrideContentPolicyType(nsContentPolicyType aContentPolicyType)
  {
    mRequest->OverrideContentPolicyType(aContentPolicyType);
  }

  bool
  IsContentPolicyTypeOverridden() const
  {
    return mRequest->IsContentPolicyTypeOverridden();
  }

  void
  GetReferrer(nsAString& aReferrer) const
  {
    mRequest->GetReferrer(aReferrer);
  }

  ReferrerPolicy
  ReferrerPolicy_() const
  {
    return mRequest->ReferrerPolicy_();
  }

  InternalHeaders*
  GetInternalHeaders() const
  {
    return mRequest->Headers();
  }

  Headers* Headers_();

  using FetchBody::GetBody;

  void
  GetBody(nsIInputStream** aStream, int64_t* aBodyLength = nullptr)
  {
    mRequest->GetBody(aStream, aBodyLength);
  }

  void
  SetBody(nsIInputStream* aStream, int64_t aBodyLength)
  {
    mRequest->SetBody(aStream, aBodyLength);
  }

  static already_AddRefed<Request>
  Constructor(const GlobalObject& aGlobal, const RequestOrUSVString& aInput,
              const RequestInit& aInit, ErrorResult& rv);

  nsIGlobalObject* GetParentObject() const
  {
    return mOwner;
  }

  already_AddRefed<Request>
  Clone(ErrorResult& aRv);

  already_AddRefed<InternalRequest>
  GetInternalRequest();

  const UniquePtr<mozilla::ipc::PrincipalInfo>&
  GetPrincipalInfo() const
  {
    return mRequest->GetPrincipalInfo();
  }

  AbortSignal*
  GetOrCreateSignal();

  // This can return a null AbortSignal.
  AbortSignal*
  GetSignal() const override;

private:
  ~Request();

  RefPtr<InternalRequest> mRequest;

  // Lazily created.
  RefPtr<Headers> mHeaders;
  RefPtr<AbortSignal> mSignal;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Request_h
