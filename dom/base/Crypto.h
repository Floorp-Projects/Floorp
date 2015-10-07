/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_Crypto_h
#define mozilla_dom_Crypto_h

#include "nsIDOMCrypto.h"
#include "mozilla/dom/SubtleCrypto.h"
#include "nsIGlobalObject.h"

#include "nsWrapperCache.h"
#include "mozilla/dom/TypedArray.h"
#define NS_DOMCRYPTO_CID \
  {0x929d9320, 0x251e, 0x11d4, { 0x8a, 0x7c, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3} }

namespace mozilla {

class ErrorResult;

namespace dom {

class Crypto : public nsIDOMCrypto,
               public nsWrapperCache
{
protected:
  virtual ~Crypto();

public:
  Crypto();

  NS_DECL_NSIDOMCRYPTO

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Crypto)

  void
  GetRandomValues(JSContext* aCx, const ArrayBufferView& aArray,
                  JS::MutableHandle<JSObject*> aRetval,
                  ErrorResult& aRv);

  SubtleCrypto*
  Subtle();

  // WebIDL

  nsIGlobalObject*
  GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static uint8_t*
  GetRandomValues(uint32_t aLength);

private:
  nsCOMPtr<nsIGlobalObject> mParent;
  nsRefPtr<SubtleCrypto> mSubtle;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Crypto_h
