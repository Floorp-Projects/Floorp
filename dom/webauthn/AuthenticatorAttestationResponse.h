/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AuthenticatorAttestationResponse_h
#define mozilla_dom_AuthenticatorAttestationResponse_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/AuthenticatorResponse.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class AuthenticatorAttestationResponse final : public AuthenticatorResponse {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      AuthenticatorAttestationResponse, AuthenticatorResponse)

  explicit AuthenticatorAttestationResponse(nsPIDOMWindowInner* aParent);

 protected:
  ~AuthenticatorAttestationResponse() override;

 public:
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetAttestationObject(JSContext* aCx,
                            JS::MutableHandle<JSObject*> aRetVal);

  nsresult SetAttestationObject(CryptoBuffer& aBuffer);

 private:
  CryptoBuffer mAttestationObject;
  JS::Heap<JSObject*> mAttestationObjectCachedObj;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AuthenticatorAttestationResponse_h
