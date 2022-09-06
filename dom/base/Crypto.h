/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Crypto_h
#define mozilla_dom_Crypto_h

#include "mozilla/dom/SubtleCrypto.h"
#include "nsIGlobalObject.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/TypedArray.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class Crypto final : public nsISupports, public nsWrapperCache {
 protected:
  virtual ~Crypto();

 public:
  explicit Crypto(nsIGlobalObject* aParent);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Crypto)

  void GetRandomValues(JSContext* aCx, const ArrayBufferView& aArray,
                       JS::MutableHandle<JSObject*> aRetval, ErrorResult& aRv);

  void RandomUUID(nsAString& aRetVal);

  SubtleCrypto* Subtle();

  nsIGlobalObject* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 private:
  nsCOMPtr<nsIGlobalObject> mParent;
  RefPtr<SubtleCrypto> mSubtle;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Crypto_h
