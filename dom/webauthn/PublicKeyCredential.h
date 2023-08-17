/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PublicKeyCredential_h
#define mozilla_dom_PublicKeyCredential_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Credential.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/CryptoBuffer.h"

namespace mozilla::dom {

class PublicKeyCredential final : public Credential {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PublicKeyCredential,
                                                         Credential)

  explicit PublicKeyCredential(nsPIDOMWindowInner* aParent);

 protected:
  ~PublicKeyCredential() override;

 public:
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetRawId(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal);

  already_AddRefed<AuthenticatorResponse> Response() const;

  nsresult SetRawId(CryptoBuffer& aBuffer);

  void SetResponse(RefPtr<AuthenticatorResponse>);

  static already_AddRefed<Promise>
  IsUserVerifyingPlatformAuthenticatorAvailable(GlobalObject& aGlobal,
                                                ErrorResult& aError);

  static already_AddRefed<Promise> IsExternalCTAP2SecurityKeySupported(
      GlobalObject& aGlobal, ErrorResult& aError);

  void GetClientExtensionResults(
      AuthenticationExtensionsClientOutputs& aResult);

  void SetClientExtensionResultAppId(bool aResult);

  void SetClientExtensionResultHmacSecret(bool aHmacCreateSecret);

 private:
  CryptoBuffer mRawId;
  JS::Heap<JSObject*> mRawIdCachedObj;
  RefPtr<AuthenticatorResponse> mResponse;
  AuthenticationExtensionsClientOutputs mClientExtensionOutputs;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_PublicKeyCredential_h
