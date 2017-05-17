/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AuthenticatorResponse_h
#define mozilla_dom_AuthenticatorResponse_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class AuthenticatorResponse : public nsISupports
                            , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AuthenticatorResponse)

  explicit AuthenticatorResponse(nsPIDOMWindowInner* aParent);

protected:
  virtual ~AuthenticatorResponse();

public:
  nsISupports*
  GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  GetFormat(nsString& aRetVal) const;

  void
  GetClientDataJSON(JSContext* aCx, JS::MutableHandle<JSObject*> aRetVal) const;

  nsresult
  SetClientDataJSON(CryptoBuffer& aBuffer);

private:
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  CryptoBuffer mClientDataJSON;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AuthenticatorResponse_h
