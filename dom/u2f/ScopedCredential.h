/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScopedCredential_h
#define mozilla_dom_ScopedCredential_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/WebAuthenticationBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class ScopedCredential final : public nsISupports
                             , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ScopedCredential)

public:
  explicit ScopedCredential(WebAuthentication* aParent);

protected:
  ~ScopedCredential();

public:
  WebAuthentication*
  GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  ScopedCredentialType
  Type() const;

  void
  GetId(JSContext* aCx, JS::MutableHandle<JSObject*> aRetVal) const;

  nsresult
  SetType(ScopedCredentialType aType);

  nsresult
  SetId(CryptoBuffer& aBuffer);

private:
  RefPtr<WebAuthentication> mParent;
  CryptoBuffer mIdBuffer;
  ScopedCredentialType mType;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScopedCredential_h
