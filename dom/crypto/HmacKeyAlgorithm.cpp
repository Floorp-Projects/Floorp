/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HmacKeyAlgorithm.h"
#include "mozilla/dom/SubtleCryptoBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(HmacKeyAlgorithm, KeyAlgorithm, mHash)
NS_IMPL_ADDREF_INHERITED(HmacKeyAlgorithm, KeyAlgorithm)
NS_IMPL_RELEASE_INHERITED(HmacKeyAlgorithm, KeyAlgorithm)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HmacKeyAlgorithm)
NS_INTERFACE_MAP_END_INHERITING(KeyAlgorithm)

JSObject*
HmacKeyAlgorithm::WrapObject(JSContext* aCx)
{
  return HmacKeyAlgorithmBinding::Wrap(aCx, this);
}

nsString
HmacKeyAlgorithm::ToJwkAlg() const
{
  switch (mMechanism) {
    case CKM_SHA_1_HMAC:  return NS_LITERAL_STRING(JWK_ALG_HS1);
    case CKM_SHA256_HMAC: return NS_LITERAL_STRING(JWK_ALG_HS256);
    case CKM_SHA384_HMAC: return NS_LITERAL_STRING(JWK_ALG_HS384);
    case CKM_SHA512_HMAC: return NS_LITERAL_STRING(JWK_ALG_HS512);
  }
  return nsString();
}

bool
HmacKeyAlgorithm::WriteStructuredClone(JSStructuredCloneWriter* aWriter) const
{
  nsString hashName;
  mHash->GetName(hashName);
  return JS_WriteUint32Pair(aWriter, SCTAG_HMACKEYALG, 0) &&
         JS_WriteUint32Pair(aWriter, mLength, 0) &&
         WriteString(aWriter, hashName) &&
         WriteString(aWriter, mName);
}

KeyAlgorithm*
HmacKeyAlgorithm::Create(nsIGlobalObject* aGlobal, JSStructuredCloneReader* aReader)
{
  uint32_t length, zero;
  nsString hash, name;
  bool read = JS_ReadUint32Pair(aReader, &length, &zero) &&
              ReadString(aReader, hash) &&
              ReadString(aReader, name);
  if (!read) {
    return nullptr;
  }

  return new HmacKeyAlgorithm(aGlobal, name, length, hash);
}

} // namespace dom
} // namespace mozilla
