/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AuthenticatorAttestationResponse_h
#define mozilla_dom_AuthenticatorAttestationResponse_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/AuthenticatorResponse.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class AuthenticatorAttestationResponse final : public AuthenticatorResponse
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit AuthenticatorAttestationResponse(nsPIDOMWindowInner* aParent);

protected:
  ~AuthenticatorAttestationResponse() override;

public:
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  GetAttestationObject(JSContext* aCx, JS::MutableHandle<JSObject*> aRetVal) const;

  nsresult
  SetAttestationObject(CryptoBuffer& aBuffer);

private:
  CryptoBuffer mAttestationObject;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AuthenticatorAttestationResponse_h
