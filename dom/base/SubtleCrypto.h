/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SubtleCrypto_h
#define mozilla_dom_SubtleCrypto_h

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsIGlobalObject.h"
#include "mozilla/dom/CryptoKey.h"
#include "js/TypeDecls.h"

namespace mozilla::dom {

class ObjectOrString;
class Promise;

typedef ArrayBufferViewOrArrayBuffer CryptoOperationData;

class SubtleCrypto final : public nsISupports, public nsWrapperCache {
  ~SubtleCrypto() = default;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(SubtleCrypto)

 public:
  explicit SubtleCrypto(nsIGlobalObject* aParent);

  nsIGlobalObject* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Encrypt(JSContext* cx,
                                    const ObjectOrString& algorithm,
                                    CryptoKey& key,
                                    const CryptoOperationData& data,
                                    ErrorResult& aRv);

  already_AddRefed<Promise> Decrypt(JSContext* cx,
                                    const ObjectOrString& algorithm,
                                    CryptoKey& key,
                                    const CryptoOperationData& data,
                                    ErrorResult& aRv);

  already_AddRefed<Promise> Sign(JSContext* cx, const ObjectOrString& algorithm,
                                 CryptoKey& key,
                                 const CryptoOperationData& data,
                                 ErrorResult& aRv);

  already_AddRefed<Promise> Verify(JSContext* cx,
                                   const ObjectOrString& algorithm,
                                   CryptoKey& key,
                                   const CryptoOperationData& signature,
                                   const CryptoOperationData& data,
                                   ErrorResult& aRv);

  already_AddRefed<Promise> Digest(JSContext* cx,
                                   const ObjectOrString& aAlgorithm,
                                   const CryptoOperationData& aData,
                                   ErrorResult& aRv);

  already_AddRefed<Promise> ImportKey(JSContext* cx, const nsAString& format,
                                      JS::Handle<JSObject*> keyData,
                                      const ObjectOrString& algorithm,
                                      bool extractable,
                                      const Sequence<nsString>& keyUsages,
                                      ErrorResult& aRv);

  already_AddRefed<Promise> ExportKey(const nsAString& format, CryptoKey& key,
                                      ErrorResult& aRv);

  already_AddRefed<Promise> GenerateKey(JSContext* cx,
                                        const ObjectOrString& algorithm,
                                        bool extractable,
                                        const Sequence<nsString>& keyUsages,
                                        ErrorResult& aRv);

  already_AddRefed<Promise> DeriveKey(
      JSContext* cx, const ObjectOrString& algorithm, CryptoKey& baseKey,
      const ObjectOrString& derivedKeyType, bool extractable,
      const Sequence<nsString>& keyUsages, ErrorResult& aRv);

  already_AddRefed<Promise> DeriveBits(JSContext* cx,
                                       const ObjectOrString& algorithm,
                                       CryptoKey& baseKey, uint32_t length,
                                       ErrorResult& aRv);

  already_AddRefed<Promise> WrapKey(JSContext* cx, const nsAString& format,
                                    CryptoKey& key, CryptoKey& wrappingKey,
                                    const ObjectOrString& wrapAlgorithm,
                                    ErrorResult& aRv);

  already_AddRefed<Promise> UnwrapKey(
      JSContext* cx, const nsAString& format,
      const ArrayBufferViewOrArrayBuffer& wrappedKey, CryptoKey& unwrappingKey,
      const ObjectOrString& unwrapAlgorithm,
      const ObjectOrString& unwrappedKeyAlgorithm, bool extractable,
      const Sequence<nsString>& keyUsages, ErrorResult& aRv);

 private:
  nsCOMPtr<nsIGlobalObject> mParent;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_SubtleCrypto_h
