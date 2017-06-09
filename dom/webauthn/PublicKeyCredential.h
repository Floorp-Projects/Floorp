/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PublicKeyCredential_h
#define mozilla_dom_PublicKeyCredential_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Credential.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class PublicKeyCredential final : public Credential
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PublicKeyCredential,
                                                         Credential)

  explicit PublicKeyCredential(nsPIDOMWindowInner* aParent);

protected:
  ~PublicKeyCredential() override;

public:
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  GetRawId(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal) const;

  already_AddRefed<AuthenticatorResponse>
  Response() const;

  nsresult
  SetRawId(CryptoBuffer& aBuffer);

  void
  SetResponse(RefPtr<AuthenticatorResponse>);

private:
  CryptoBuffer mRawId;
  RefPtr<AuthenticatorResponse> mResponse;
  // Extensions are not supported yet.
  // <some type> mClientExtensionResults;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PublicKeyCredential_h
