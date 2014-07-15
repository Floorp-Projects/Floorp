/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebCryptoCommon_h
#define mozilla_dom_WebCryptoCommon_h

#include "pk11pub.h"
#include "nsString.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "js/StructuredClone.h"

// WebCrypto algorithm names
#define WEBCRYPTO_ALG_AES_CBC       "AES-CBC"
#define WEBCRYPTO_ALG_AES_CTR       "AES-CTR"
#define WEBCRYPTO_ALG_AES_GCM       "AES-GCM"
#define WEBCRYPTO_ALG_SHA1          "SHA-1"
#define WEBCRYPTO_ALG_SHA256        "SHA-256"
#define WEBCRYPTO_ALG_SHA384        "SHA-384"
#define WEBCRYPTO_ALG_SHA512        "SHA-512"
#define WEBCRYPTO_ALG_HMAC          "HMAC"
#define WEBCRYPTO_ALG_PBKDF2        "PBKDF2"
#define WEBCRYPTO_ALG_RSAES_PKCS1   "RSAES-PKCS1-v1_5"
#define WEBCRYPTO_ALG_RSASSA_PKCS1  "RSASSA-PKCS1-v1_5"
#define WEBCRYPTO_ALG_RSA_OAEP      "RSA-OAEP"

// WebCrypto key formats
#define WEBCRYPTO_KEY_FORMAT_RAW    "raw"
#define WEBCRYPTO_KEY_FORMAT_PKCS8  "pkcs8"
#define WEBCRYPTO_KEY_FORMAT_SPKI   "spki"
#define WEBCRYPTO_KEY_FORMAT_JWK    "jwk"

// WebCrypto key types
#define WEBCRYPTO_KEY_TYPE_PUBLIC  "public"
#define WEBCRYPTO_KEY_TYPE_PRIVATE "private"
#define WEBCRYPTO_KEY_TYPE_SECRET  "secret"

// WebCrypto key usages
#define WEBCRYPTO_KEY_USAGE_ENCRYPT     "encrypt"
#define WEBCRYPTO_KEY_USAGE_DECRYPT     "decrypt"
#define WEBCRYPTO_KEY_USAGE_SIGN        "sign"
#define WEBCRYPTO_KEY_USAGE_VERIFY      "verify"
#define WEBCRYPTO_KEY_USAGE_DERIVEKEY   "deriveKey"
#define WEBCRYPTO_KEY_USAGE_DERIVEBITS  "deriveBits"
#define WEBCRYPTO_KEY_USAGE_WRAPKEY     "wrapKey"
#define WEBCRYPTO_KEY_USAGE_UNWRAPKEY   "unwrapKey"

// JWK key types
#define JWK_TYPE_SYMMETRIC          "oct"
#define JWK_TYPE_RSA                "RSA"
#define JWK_TYPE_EC                 "EC"

// JWK algorithms
#define JWK_ALG_RS1                 "RS1"
#define JWK_ALG_RS256               "RS256"
#define JWK_ALG_RS384               "RS384"
#define JWK_ALG_RS512               "RS512"

// Define an unknown mechanism type
#define UNKNOWN_CK_MECHANISM        CKM_VENDOR_DEFINED+1

namespace mozilla {
namespace dom {

// Helper functions for structured cloning
inline bool
ReadString(JSStructuredCloneReader* aReader, nsString& aString)
{
  bool read;
  uint32_t nameLength, zero;
  read = JS_ReadUint32Pair(aReader, &nameLength, &zero);
  if (!read) {
    return false;
  }

  aString.SetLength(nameLength);
  size_t charSize = sizeof(nsString::char_type);
  read = JS_ReadBytes(aReader, (void*) aString.BeginWriting(), nameLength * charSize);
  if (!read) {
    return false;
  }

  return true;
}

inline bool
WriteString(JSStructuredCloneWriter* aWriter, const nsString& aString)
{
  size_t charSize = sizeof(nsString::char_type);
  return JS_WriteUint32Pair(aWriter, aString.Length(), 0) &&
         JS_WriteBytes(aWriter, aString.get(), aString.Length() * charSize);
}

inline bool
ReadBuffer(JSStructuredCloneReader* aReader, CryptoBuffer& aBuffer)
{
  uint32_t length, zero;
  bool ret = JS_ReadUint32Pair(aReader, &length, &zero);
  if (!ret) {
    return false;
  }

  if (length > 0) {
    if (!aBuffer.SetLength(length)) {
      return false;
    }
    ret = JS_ReadBytes(aReader, aBuffer.Elements(), aBuffer.Length());
  }
  return ret;
}

inline bool
WriteBuffer(JSStructuredCloneWriter* aWriter, const CryptoBuffer& aBuffer)
{
  bool ret = JS_WriteUint32Pair(aWriter, aBuffer.Length(), 0);
  if (ret && aBuffer.Length() > 0) {
    ret = JS_WriteBytes(aWriter, aBuffer.Elements(), aBuffer.Length());
  }
  return ret;
}

inline CK_MECHANISM_TYPE
MapAlgorithmNameToMechanism(const nsString& aName)
{
  CK_MECHANISM_TYPE mechanism(UNKNOWN_CK_MECHANISM);

  // Set mechanism based on algorithm name
  if (aName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC)) {
    mechanism = CKM_AES_CBC_PAD;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR)) {
    mechanism = CKM_AES_CTR;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
    mechanism = CKM_AES_GCM;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_SHA1)) {
    mechanism = CKM_SHA_1;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_SHA256)) {
    mechanism = CKM_SHA256;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_SHA384)) {
    mechanism = CKM_SHA384;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_SHA512)) {
    mechanism = CKM_SHA512;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_PBKDF2)) {
    mechanism = CKM_PKCS5_PBKD2;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1)) {
    mechanism = CKM_RSA_PKCS;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
    mechanism = CKM_RSA_PKCS;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
    mechanism = CKM_RSA_PKCS_OAEP;
  }

  return mechanism;
}

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebCryptoCommon_h
