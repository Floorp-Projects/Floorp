/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AesKeyAlgorithm.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/WebCryptoCommon.h"

namespace mozilla {
namespace dom {

JSObject*
AesKeyAlgorithm::WrapObject(JSContext* aCx)
{
  return AesKeyAlgorithmBinding::Wrap(aCx, this);
}

nsString
AesKeyAlgorithm::ToJwkAlg() const
{
  if (mName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC)) {
    switch (mLength) {
      case 128: return NS_LITERAL_STRING(JWK_ALG_A128CBC);
      case 192: return NS_LITERAL_STRING(JWK_ALG_A192CBC);
      case 256: return NS_LITERAL_STRING(JWK_ALG_A256CBC);
    }
  }

  if (mName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR)) {
    switch (mLength) {
      case 128: return NS_LITERAL_STRING(JWK_ALG_A128CTR);
      case 192: return NS_LITERAL_STRING(JWK_ALG_A192CTR);
      case 256: return NS_LITERAL_STRING(JWK_ALG_A256CTR);
    }
  }

  if (mName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
    switch (mLength) {
      case 128: return NS_LITERAL_STRING(JWK_ALG_A128GCM);
      case 192: return NS_LITERAL_STRING(JWK_ALG_A192GCM);
      case 256: return NS_LITERAL_STRING(JWK_ALG_A256GCM);
    }
  }

  if (mName.EqualsLiteral(WEBCRYPTO_ALG_AES_KW)) {
    switch (mLength) {
      case 128: return NS_LITERAL_STRING(JWK_ALG_A128KW);
      case 192: return NS_LITERAL_STRING(JWK_ALG_A192KW);
      case 256: return NS_LITERAL_STRING(JWK_ALG_A256KW);
    }
  }

  return nsString();
}

bool
AesKeyAlgorithm::WriteStructuredClone(JSStructuredCloneWriter* aWriter) const
{
  return JS_WriteUint32Pair(aWriter, SCTAG_AESKEYALG, 0) &&
         JS_WriteUint32Pair(aWriter, mLength, 0) &&
         WriteString(aWriter, mName);
}

KeyAlgorithm*
AesKeyAlgorithm::Create(nsIGlobalObject* aGlobal, JSStructuredCloneReader* aReader)
{
  uint32_t length, zero;
  nsString name;
  bool read = JS_ReadUint32Pair(aReader, &length, &zero) &&
              ReadString(aReader, name);
  if (!read) {
    return nullptr;
  }

  return new AesKeyAlgorithm(aGlobal, name, length);
}


} // namespace dom
} // namespace mozilla
