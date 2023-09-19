/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AuthenticatorResponse_h
#define mozilla_dom_AuthenticatorResponse_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"

namespace mozilla::dom {

class AuthenticatorResponse : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AuthenticatorResponse)

  explicit AuthenticatorResponse(nsPIDOMWindowInner* aParent);

 protected:
  virtual ~AuthenticatorResponse();

 public:
  nsISupports* GetParentObject() const;

  void GetFormat(nsString& aRetVal) const;

  void GetClientDataJSON(JSContext* aCx, JS::MutableHandle<JSObject*> aValue,
                         ErrorResult& aRv);

  void SetClientDataJSON(const nsCString& aBuffer);

 protected:
  nsCString mClientDataJSON;

 private:
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  JS::Heap<JSObject*> mClientDataJSONCachedObj;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AuthenticatorResponse_h
