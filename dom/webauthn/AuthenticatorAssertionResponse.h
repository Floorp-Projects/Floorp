/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AuthenticatorAssertionResponse_h
#define mozilla_dom_AuthenticatorAssertionResponse_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/AuthenticatorResponse.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/WebAuthenticationBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class AuthenticatorAssertionResponse final : public AuthenticatorResponse {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      AuthenticatorAssertionResponse, AuthenticatorResponse)

  explicit AuthenticatorAssertionResponse(nsPIDOMWindowInner* aParent);

 protected:
  ~AuthenticatorAssertionResponse() override;

 public:
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetAuthenticatorData(JSContext* aCx, JS::MutableHandle<JSObject*> aValue,
                            ErrorResult& aRv);

  void SetAuthenticatorData(const nsTArray<uint8_t>& aBuffer);

  void GetSignature(JSContext* aCx, JS::MutableHandle<JSObject*> aValue,
                    ErrorResult& aRv);

  void SetSignature(const nsTArray<uint8_t>& aBuffer);

  void GetUserHandle(JSContext* aCx, JS::MutableHandle<JSObject*> aValue,
                     ErrorResult& aRv);

  void SetUserHandle(const nsTArray<uint8_t>& aBuffer);

  void ToJSON(AuthenticatorAssertionResponseJSON& aJSON, ErrorResult& aError);

 private:
  nsTArray<uint8_t> mAuthenticatorData;
  JS::Heap<JSObject*> mAuthenticatorDataCachedObj;
  nsTArray<uint8_t> mSignature;
  JS::Heap<JSObject*> mSignatureCachedObj;
  nsTArray<uint8_t> mUserHandle;
  JS::Heap<JSObject*> mUserHandleCachedObj;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AuthenticatorAssertionResponse_h
