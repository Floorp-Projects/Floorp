/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SubtleCrypto_h
#define mozilla_dom_SubtleCrypto_h

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/CryptoKey.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class Promise;

typedef ArrayBufferViewOrArrayBuffer CryptoOperationData;
typedef ArrayBufferViewOrArrayBuffer KeyData;

class SubtleCrypto MOZ_FINAL : public nsISupports,
                               public nsWrapperCache
{
  ~SubtleCrypto() {}

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(SubtleCrypto)

public:
  SubtleCrypto(nsPIDOMWindow* aWindow);

  nsPIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  already_AddRefed<Promise> Encrypt(JSContext* cx,
                                    const ObjectOrString& algorithm,
                                    CryptoKey& key,
                                    const CryptoOperationData& data);

  already_AddRefed<Promise> Decrypt(JSContext* cx,
                                    const ObjectOrString& algorithm,
                                    CryptoKey& key,
                                    const CryptoOperationData& data);

  already_AddRefed<Promise> Sign(JSContext* cx,
                                 const ObjectOrString& algorithm,
                                 CryptoKey& key,
                                 const CryptoOperationData& data);

  already_AddRefed<Promise> Verify(JSContext* cx,
                                   const ObjectOrString& algorithm,
                                   CryptoKey& key,
                                   const CryptoOperationData& signature,
                                   const CryptoOperationData& data);

  already_AddRefed<Promise> Digest(JSContext* cx,
                                   const ObjectOrString& aAlgorithm,
                                   const CryptoOperationData& aData);

  already_AddRefed<Promise> ImportKey(JSContext* cx,
                                      const nsAString& format,
                                      const KeyData& keyData,
                                      const ObjectOrString& algorithm,
                                      bool extractable,
                                      const Sequence<nsString>& keyUsages);

  already_AddRefed<Promise> ExportKey(const nsAString& format, CryptoKey& key);

  already_AddRefed<Promise> GenerateKey(JSContext* cx,
                                        const ObjectOrString& algorithm,
                                        bool extractable,
                                        const Sequence<nsString>& keyUsages);

  already_AddRefed<Promise> DeriveKey(JSContext* cx,
                                      const ObjectOrString& algorithm,
                                      CryptoKey& baseKey,
                                      const ObjectOrString& derivedKeyType,
                                      bool extractable,
                                      const Sequence<nsString>& keyUsages);

  already_AddRefed<Promise> DeriveBits(JSContext* cx,
                                       const ObjectOrString& algorithm,
                                       CryptoKey& baseKey,
                                       uint32_t length);

private:
  nsCOMPtr<nsPIDOMWindow> mWindow;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SubtleCrypto_h
