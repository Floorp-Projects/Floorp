/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Request_h
#define mozilla_dom_Request_h

#include "nsISupportsImpl.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/InternalRequest.h"
// Required here due to certain WebIDL enums/classes being declared in both
// files.
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/SafeRefPtr.h"

namespace mozilla::dom {

class Headers;
class InternalHeaders;
class RequestOrUTF8String;

class Request final : public FetchBody<Request>, public nsWrapperCache {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_INHERITED(Request,
                                                        FetchBody<Request>)

 public:
  Request(nsIGlobalObject* aOwner, SafeRefPtr<InternalRequest> aRequest,
          AbortSignal* aSignal);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return Request_Binding::Wrap(aCx, this, aGivenProto);
  }

  void GetUrl(nsACString& aUrl) const { mRequest->GetURL(aUrl); }
  void GetMethod(nsCString& aMethod) const { aMethod = mRequest->mMethod; }

  RequestMode Mode() const { return mRequest->mMode; }

  RequestCredentials Credentials() const { return mRequest->mCredentialsMode; }

  RequestCache Cache() const { return mRequest->GetCacheMode(); }

  RequestRedirect Redirect() const { return mRequest->GetRedirectMode(); }

  RequestPriority Priority() const { return mRequest->GetPriorityMode(); }

  void GetIntegrity(nsAString& aIntegrity) const {
    aIntegrity = mRequest->GetIntegrity();
  }

  bool Keepalive() const { return mRequest->GetKeepalive(); }

  bool MozErrors() const { return mRequest->MozErrors(); }

  RequestDestination Destination() const { return mRequest->Destination(); }

  void OverrideContentPolicyType(uint32_t aContentPolicyType) {
    mRequest->OverrideContentPolicyType(
        static_cast<nsContentPolicyType>(aContentPolicyType));
  }

  bool IsContentPolicyTypeOverridden() const {
    return mRequest->IsContentPolicyTypeOverridden();
  }

  void GetReferrer(nsACString& aReferrer) const {
    mRequest->GetReferrer(aReferrer);
  }

  ReferrerPolicy ReferrerPolicy_() const { return mRequest->ReferrerPolicy_(); }

  InternalHeaders* GetInternalHeaders() const { return mRequest->Headers(); }

  Headers* Headers_();

  using FetchBody::GetBody;

  void GetBody(nsIInputStream** aStream, int64_t* aBodyLength = nullptr) {
    mRequest->GetBody(aStream, aBodyLength);
  }

  void SetBody(nsIInputStream* aStream, int64_t aBodyLength) {
    mRequest->SetBody(aStream, aBodyLength);
  }

  using FetchBody::BodyBlobURISpec;

  const nsACString& BodyBlobURISpec() const {
    return mRequest->BodyBlobURISpec();
  }

  using FetchBody::BodyLocalPath;

  const nsAString& BodyLocalPath() const { return mRequest->BodyLocalPath(); }

  static SafeRefPtr<Request> Constructor(const GlobalObject& aGlobal,
                                         const RequestOrUTF8String& aInput,
                                         const RequestInit& aInit,
                                         ErrorResult& rv);

  static SafeRefPtr<Request> Constructor(nsIGlobalObject* aGlobal,
                                         JSContext* aCx,
                                         const RequestOrUTF8String& aInput,
                                         const RequestInit& aInit,
                                         const CallerType aCallerType,
                                         ErrorResult& rv);

  nsIGlobalObject* GetParentObject() const { return mOwner; }

  SafeRefPtr<Request> Clone(ErrorResult& aRv);

  SafeRefPtr<InternalRequest> GetInternalRequest();

  const UniquePtr<mozilla::ipc::PrincipalInfo>& GetPrincipalInfo() const {
    return mRequest->GetPrincipalInfo();
  }

  AbortSignal* GetOrCreateSignal();

  // This can return a null AbortSignalImpl.
  AbortSignalImpl* GetSignalImpl() const override;
  AbortSignalImpl* GetSignalImplToConsumeBody() const final;

 private:
  ~Request();

  SafeRefPtr<InternalRequest> mRequest;

  // Lazily created.
  RefPtr<Headers> mHeaders;
  RefPtr<AbortSignal> mSignal;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_Request_h
