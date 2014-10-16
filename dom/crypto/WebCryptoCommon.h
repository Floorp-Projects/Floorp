/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebCryptoCommon_h
#define mozilla_dom_WebCryptoCommon_h

#include "pk11pub.h"
#include "nsString.h"
#include "nsContentUtils.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "js/StructuredClone.h"

// WebCrypto algorithm names
#define WEBCRYPTO_ALG_AES_CBC       "AES-CBC"
#define WEBCRYPTO_ALG_AES_CTR       "AES-CTR"
#define WEBCRYPTO_ALG_AES_GCM       "AES-GCM"
#define WEBCRYPTO_ALG_AES_KW        "AES-KW"
#define WEBCRYPTO_ALG_SHA1          "SHA-1"
#define WEBCRYPTO_ALG_SHA256        "SHA-256"
#define WEBCRYPTO_ALG_SHA384        "SHA-384"
#define WEBCRYPTO_ALG_SHA512        "SHA-512"
#define WEBCRYPTO_ALG_HMAC          "HMAC"
#define WEBCRYPTO_ALG_PBKDF2        "PBKDF2"
#define WEBCRYPTO_ALG_RSASSA_PKCS1  "RSASSA-PKCS1-v1_5"
#define WEBCRYPTO_ALG_RSA_OAEP      "RSA-OAEP"
#define WEBCRYPTO_ALG_ECDH          "ECDH"
#define WEBCRYPTO_ALG_ECDSA         "ECDSA"

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

// WebCrypto named curves
#define WEBCRYPTO_NAMED_CURVE_P256  "P-256"
#define WEBCRYPTO_NAMED_CURVE_P384  "P-384"
#define WEBCRYPTO_NAMED_CURVE_P521  "P-521"

// JWK key types
#define JWK_TYPE_SYMMETRIC          "oct"
#define JWK_TYPE_RSA                "RSA"
#define JWK_TYPE_EC                 "EC"

// JWK algorithms
#define JWK_ALG_A128CBC             "A128CBC"  // CBC
#define JWK_ALG_A192CBC             "A192CBC"
#define JWK_ALG_A256CBC             "A256CBC"
#define JWK_ALG_A128CTR             "A128CTR"  // CTR
#define JWK_ALG_A192CTR             "A192CTR"
#define JWK_ALG_A256CTR             "A256CTR"
#define JWK_ALG_A128GCM             "A128GCM"  // GCM
#define JWK_ALG_A192GCM             "A192GCM"
#define JWK_ALG_A256GCM             "A256GCM"
#define JWK_ALG_A128KW              "A128KW"   // KW
#define JWK_ALG_A192KW              "A192KW"
#define JWK_ALG_A256KW              "A256KW"
#define JWK_ALG_HS1                 "HS1"      // HMAC
#define JWK_ALG_HS256               "HS256"
#define JWK_ALG_HS384               "HS384"
#define JWK_ALG_HS512               "HS512"
#define JWK_ALG_RS1                 "RS1"      // RSASSA-PKCS1
#define JWK_ALG_RS256               "RS256"
#define JWK_ALG_RS384               "RS384"
#define JWK_ALG_RS512               "RS512"
#define JWK_ALG_RSA_OAEP            "RSA-OAEP" // RSA-OAEP
#define JWK_ALG_RSA_OAEP_256        "RSA-OAEP-256"
#define JWK_ALG_RSA_OAEP_384        "RSA-OAEP-384"
#define JWK_ALG_RSA_OAEP_512        "RSA-OAEP-512"
#define JWK_ALG_ECDSA_P_256         "ES256"
#define JWK_ALG_ECDSA_P_384         "ES384"
#define JWK_ALG_ECDSA_P_521         "ES521"

// JWK usages
#define JWK_USE_ENC                 "enc"
#define JWK_USE_SIG                 "sig"

// Define an unknown mechanism type
#define UNKNOWN_CK_MECHANISM        CKM_VENDOR_DEFINED+1

// python security/pkix/tools/DottedOIDToCode.py id-ecDH 1.3.132.112
static const uint8_t id_ecDH[] = { 0x2b, 0x81, 0x04, 0x70 };
const SECItem SEC_OID_DATA_EC_DH = { siBuffer, (unsigned char*)id_ecDH,
                                     PR_ARRAY_SIZE(id_ecDH) };

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
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_AES_KW)) {
    mechanism = CKM_NSS_AES_KEY_WRAP;
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
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
    mechanism = CKM_RSA_PKCS;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
    mechanism = CKM_RSA_PKCS_OAEP;
  } else if (aName.EqualsLiteral(WEBCRYPTO_ALG_ECDH)) {
    mechanism = CKM_ECDH1_DERIVE;
  }

  return mechanism;
}

#define NORMALIZED_EQUALS(aTest, aConst) \
        nsContentUtils::EqualsIgnoreASCIICase(aTest, NS_LITERAL_STRING(aConst))

inline bool
NormalizeToken(const nsString& aName, nsString& aDest)
{
  // Algorithm names
  if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_AES_CBC)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_AES_CBC);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_AES_CTR)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_AES_CTR);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_AES_GCM)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_AES_GCM);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_AES_KW)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_AES_KW);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_SHA1)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_SHA1);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_SHA256)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_SHA256);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_SHA384)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_SHA384);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_SHA512)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_SHA512);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_HMAC)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_HMAC);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_PBKDF2)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_PBKDF2);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_RSASSA_PKCS1)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_RSA_OAEP)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_RSA_OAEP);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_ECDH)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_ECDH);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_ALG_ECDSA)) {
    aDest.AssignLiteral(WEBCRYPTO_ALG_ECDSA);
  // Named curve values
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_NAMED_CURVE_P256)) {
    aDest.AssignLiteral(WEBCRYPTO_NAMED_CURVE_P256);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_NAMED_CURVE_P384)) {
    aDest.AssignLiteral(WEBCRYPTO_NAMED_CURVE_P384);
  } else if (NORMALIZED_EQUALS(aName, WEBCRYPTO_NAMED_CURVE_P521)) {
    aDest.AssignLiteral(WEBCRYPTO_NAMED_CURVE_P521);
  } else {
    return false;
  }

  return true;
}

inline bool
CheckEncodedECParameters(const SECItem* aEcParams)
{
  // Need at least two bytes for a valid ASN.1 encoding.
  if (aEcParams->len < 2) {
    return false;
  }

  // Check the ASN.1 tag.
  if (aEcParams->data[0] != SEC_ASN1_OBJECT_ID) {
    return false;
  }

  // OID tags are short, we never need more than one length byte.
  if (aEcParams->data[1] >= 128) {
    return false;
  }

  // Check that the SECItem's length is correct.
  if (aEcParams->len != (unsigned)aEcParams->data[1] + 2) {
    return false;
  }

  return true;
}

inline SECItem*
CreateECParamsForCurve(const nsString& aNamedCurve, PLArenaPool* aArena)
{
  SECOidTag curveOIDTag;

  if (aNamedCurve.EqualsLiteral(WEBCRYPTO_NAMED_CURVE_P256)) {
    curveOIDTag = SEC_OID_SECG_EC_SECP256R1;
  } else if (aNamedCurve.EqualsLiteral(WEBCRYPTO_NAMED_CURVE_P384)) {
    curveOIDTag = SEC_OID_SECG_EC_SECP384R1;
  } else if (aNamedCurve.EqualsLiteral(WEBCRYPTO_NAMED_CURVE_P521)) {
    curveOIDTag = SEC_OID_SECG_EC_SECP521R1;
  } else {
    return nullptr;
  }

  // Retrieve curve data by OID tag.
  SECOidData* oidData = SECOID_FindOIDByTag(curveOIDTag);
  if (!oidData) {
    return nullptr;
  }

  // Create parameters.
  SECItem* params = ::SECITEM_AllocItem(aArena, nullptr, 2 + oidData->oid.len);
  if (!params) {
    return nullptr;
  }

  // Set parameters.
  params->data[0] = SEC_ASN1_OBJECT_ID;
  params->data[1] = oidData->oid.len;
  memcpy(params->data + 2, oidData->oid.data, oidData->oid.len);

  // Sanity check the params we just created.
  if (!CheckEncodedECParameters(params)) {
    return nullptr;
  }

  return params;
}

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebCryptoCommon_h
