/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SubtleCrypto.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/WebCryptoTask.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SubtleCrypto, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SubtleCrypto)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SubtleCrypto)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SubtleCrypto)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END



SubtleCrypto::SubtleCrypto(nsIGlobalObject* aParent)
  : mParent(aParent)
{
}

JSObject*
SubtleCrypto::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SubtleCryptoBinding::Wrap(aCx, this, aGivenProto);
}

#define SUBTLECRYPTO_METHOD_BODY(Operation, aRv, ...)                   \
  MOZ_ASSERT(mParent);                                                  \
  RefPtr<Promise> p = Promise::Create(mParent, aRv);                  \
  if (aRv.Failed()) {                                                   \
    return nullptr;                                                     \
  }                                                                     \
  RefPtr<WebCryptoTask> task = WebCryptoTask::Create ## Operation ## Task(__VA_ARGS__); \
  task->DispatchWithPromise(p); \
  return p.forget();

already_AddRefed<Promise>
SubtleCrypto::Encrypt(JSContext* cx,
                      const ObjectOrString& algorithm,
                      CryptoKey& key,
                      const CryptoOperationData& data,
                      ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(Encrypt, aRv, cx, algorithm, key, data)
}

already_AddRefed<Promise>
SubtleCrypto::Decrypt(JSContext* cx,
                      const ObjectOrString& algorithm,
                      CryptoKey& key,
                      const CryptoOperationData& data,
                      ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(Decrypt, aRv, cx, algorithm, key, data)
}

already_AddRefed<Promise>
SubtleCrypto::Sign(JSContext* cx,
                   const ObjectOrString& algorithm,
                   CryptoKey& key,
                   const CryptoOperationData& data,
                   ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(Sign, aRv, cx, algorithm, key, data)
}

already_AddRefed<Promise>
SubtleCrypto::Verify(JSContext* cx,
                     const ObjectOrString& algorithm,
                     CryptoKey& key,
                     const CryptoOperationData& signature,
                     const CryptoOperationData& data,
                     ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(Verify, aRv, cx, algorithm, key, signature, data)
}

already_AddRefed<Promise>
SubtleCrypto::Digest(JSContext* cx,
                     const ObjectOrString& algorithm,
                     const CryptoOperationData& data,
                     ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(Digest, aRv, cx, algorithm, data)
}


already_AddRefed<Promise>
SubtleCrypto::ImportKey(JSContext* cx,
                        const nsAString& format,
                        JS::Handle<JSObject*> keyData,
                        const ObjectOrString& algorithm,
                        bool extractable,
                        const Sequence<nsString>& keyUsages,
                        ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(ImportKey, aRv, mParent, cx, format, keyData,
                           algorithm, extractable, keyUsages)
}

already_AddRefed<Promise>
SubtleCrypto::ExportKey(const nsAString& format,
                        CryptoKey& key,
                        ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(ExportKey, aRv, format, key)
}

already_AddRefed<Promise>
SubtleCrypto::GenerateKey(JSContext* cx, const ObjectOrString& algorithm,
                          bool extractable, const Sequence<nsString>& keyUsages,
                          ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(GenerateKey, aRv, mParent, cx, algorithm,
                           extractable, keyUsages)
}

already_AddRefed<Promise>
SubtleCrypto::DeriveKey(JSContext* cx,
                        const ObjectOrString& algorithm,
                        CryptoKey& baseKey,
                        const ObjectOrString& derivedKeyType,
                        bool extractable, const Sequence<nsString>& keyUsages,
                        ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(DeriveKey, aRv, mParent, cx, algorithm, baseKey,
                           derivedKeyType, extractable, keyUsages)
}

already_AddRefed<Promise>
SubtleCrypto::DeriveBits(JSContext* cx,
                         const ObjectOrString& algorithm,
                         CryptoKey& baseKey,
                         uint32_t length,
                         ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(DeriveBits, aRv, cx, algorithm, baseKey, length)
}

already_AddRefed<Promise>
SubtleCrypto::WrapKey(JSContext* cx,
                      const nsAString& format,
                      CryptoKey& key,
                      CryptoKey& wrappingKey,
                      const ObjectOrString& wrapAlgorithm,
                      ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(WrapKey, aRv, cx, format, key, wrappingKey, wrapAlgorithm)
}

already_AddRefed<Promise>
SubtleCrypto::UnwrapKey(JSContext* cx,
                        const nsAString& format,
                        const ArrayBufferViewOrArrayBuffer& wrappedKey,
                        CryptoKey& unwrappingKey,
                        const ObjectOrString& unwrapAlgorithm,
                        const ObjectOrString& unwrappedKeyAlgorithm,
                        bool extractable,
                        const Sequence<nsString>& keyUsages,
                        ErrorResult& aRv)
{
  SUBTLECRYPTO_METHOD_BODY(UnwrapKey, aRv, mParent, cx, format, wrappedKey,
                           unwrappingKey, unwrapAlgorithm,
                           unwrappedKeyAlgorithm, extractable, keyUsages)
}

} // namespace dom
} // namespace mozilla
