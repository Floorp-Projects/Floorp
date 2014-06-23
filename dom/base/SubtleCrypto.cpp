/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SubtleCrypto.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/WebCryptoTask.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SubtleCrypto, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SubtleCrypto)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SubtleCrypto)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SubtleCrypto)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END



SubtleCrypto::SubtleCrypto(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
  SetIsDOMBinding();
}

JSObject*
SubtleCrypto::WrapObject(JSContext* aCx)
{
  return SubtleCryptoBinding::Wrap(aCx, this);
}

#define SUBTLECRYPTO_METHOD_BODY(Operation, ...) \
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow); \
  MOZ_ASSERT(global); \
  nsRefPtr<Promise> p = new Promise(global); \
  nsRefPtr<WebCryptoTask> task = WebCryptoTask::Operation ## Task(__VA_ARGS__); \
  task->DispatchWithPromise(p); \
  return p.forget();

already_AddRefed<Promise>
SubtleCrypto::Encrypt(JSContext* cx,
                      const ObjectOrString& algorithm,
                      CryptoKey& key,
                      const CryptoOperationData& data)
{
  SUBTLECRYPTO_METHOD_BODY(Encrypt, cx, algorithm, key, data)
}

already_AddRefed<Promise>
SubtleCrypto::Decrypt(JSContext* cx,
                      const ObjectOrString& algorithm,
                      CryptoKey& key,
                      const CryptoOperationData& data)
{
  SUBTLECRYPTO_METHOD_BODY(Decrypt, cx, algorithm, key, data)
}

already_AddRefed<Promise>
SubtleCrypto::Sign(JSContext* cx,
                   const ObjectOrString& algorithm,
                   CryptoKey& key,
                   const CryptoOperationData& data)
{
  SUBTLECRYPTO_METHOD_BODY(Sign, cx, algorithm, key, data)
}

already_AddRefed<Promise>
SubtleCrypto::Verify(JSContext* cx,
                     const ObjectOrString& algorithm,
                     CryptoKey& key,
                     const CryptoOperationData& signature,
                     const CryptoOperationData& data)
{
  SUBTLECRYPTO_METHOD_BODY(Verify, cx, algorithm, key, signature, data)
}

already_AddRefed<Promise>
SubtleCrypto::Digest(JSContext* cx,
                     const ObjectOrString& algorithm,
                     const CryptoOperationData& data)
{
  SUBTLECRYPTO_METHOD_BODY(Digest, cx, algorithm, data)
}


already_AddRefed<Promise>
SubtleCrypto::ImportKey(JSContext* cx,
                        const nsAString& format,
                        const KeyData& keyData,
                        const ObjectOrString& algorithm,
                        bool extractable,
                        const Sequence<nsString>& keyUsages)
{
  SUBTLECRYPTO_METHOD_BODY(ImportKey, cx, format, keyData, algorithm,
                                      extractable, keyUsages)
}

already_AddRefed<Promise>
SubtleCrypto::ExportKey(const nsAString& format,
                        CryptoKey& key)
{
  SUBTLECRYPTO_METHOD_BODY(ExportKey, format, key)
}

already_AddRefed<Promise>
SubtleCrypto::GenerateKey(JSContext* cx, const ObjectOrString& algorithm,
                          bool extractable, const Sequence<nsString>& keyUsages)
{
  SUBTLECRYPTO_METHOD_BODY(GenerateKey, cx, algorithm, extractable, keyUsages)
}

already_AddRefed<Promise>
SubtleCrypto::DeriveKey(JSContext* cx,
                        const ObjectOrString& algorithm,
                        CryptoKey& baseKey,
                        const ObjectOrString& derivedKeyType,
                        bool extractable, const Sequence<nsString>& keyUsages)
{
  SUBTLECRYPTO_METHOD_BODY(DeriveKey, cx, algorithm, baseKey, derivedKeyType,
                                      extractable, keyUsages)
}

already_AddRefed<Promise>
SubtleCrypto::DeriveBits(JSContext* cx,
                         const ObjectOrString& algorithm,
                         CryptoKey& baseKey,
                         uint32_t length)
{
  SUBTLECRYPTO_METHOD_BODY(DeriveBits, cx, algorithm, baseKey, length)
}

} // namespace dom
} // namespace mozilla
