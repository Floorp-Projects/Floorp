/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/KeyAlgorithmProxy.h"
#include "mozilla/dom/WebCryptoCommon.h"

namespace mozilla {
namespace dom {

bool
KeyAlgorithmProxy::WriteStructuredClone(JSStructuredCloneWriter* aWriter) const
{
  if (!WriteString(aWriter, mName) ||
      !JS_WriteUint32Pair(aWriter, mType, KEY_ALGORITHM_SC_VERSION)) {
    return false;
  }

  switch (mType) {
    case AES:
      return JS_WriteUint32Pair(aWriter, mAes.mLength, 0);
    case HMAC:
      return JS_WriteUint32Pair(aWriter, mHmac.mLength, 0) &&
             WriteString(aWriter, mHmac.mHash.mName);
    case RSA: {
      return JS_WriteUint32Pair(aWriter, mRsa.mModulusLength, 0) &&
             WriteBuffer(aWriter, mRsa.mPublicExponent) &&
             WriteString(aWriter, mRsa.mHash.mName);
    }
    case EC:
      return WriteString(aWriter, mEc.mNamedCurve);
  }

  return false;
}

bool
KeyAlgorithmProxy::ReadStructuredClone(JSStructuredCloneReader* aReader)
{
  uint32_t type, version, dummy;
  if (!ReadString(aReader, mName) ||
      !JS_ReadUint32Pair(aReader, &type, &version)) {
    return false;
  }

  if (version != KEY_ALGORITHM_SC_VERSION) {
    return false;
  }

  mType = (KeyAlgorithmType) type;
  switch (mType) {
    case AES: {
      uint32_t length;
      if (!JS_ReadUint32Pair(aReader, &length, &dummy)) {
        return false;
      }

      mAes.mLength = length;
      mAes.mName = mName;
      return true;
    }
    case HMAC: {
      if (!JS_ReadUint32Pair(aReader, &mHmac.mLength, &dummy) ||
          !ReadString(aReader, mHmac.mHash.mName)) {
        return false;
      }

      mHmac.mName = mName;
      return true;
    }
    case RSA: {
      uint32_t modulusLength;
      nsString hashName;
      if (!JS_ReadUint32Pair(aReader, &modulusLength, &dummy) ||
          !ReadBuffer(aReader, mRsa.mPublicExponent) ||
          !ReadString(aReader, mRsa.mHash.mName)) {
        return false;
      }

      mRsa.mModulusLength = modulusLength;
      mRsa.mName = mName;
      return true;
    }
    case EC: {
      nsString namedCurve;
      if (!ReadString(aReader, mEc.mNamedCurve)) {
        return false;
      }

      mEc.mName = mName;
      return true;
    }
  }

  return false;
}

CK_MECHANISM_TYPE
KeyAlgorithmProxy::Mechanism() const
{
  if (mType == HMAC) {
    return GetMechanism(mHmac);
  }
  return MapAlgorithmNameToMechanism(mName);
}

nsString
KeyAlgorithmProxy::JwkAlg() const
{
  if (mName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC)) {
    switch (mAes.mLength) {
      case 128: return NS_LITERAL_STRING(JWK_ALG_A128CBC);
      case 192: return NS_LITERAL_STRING(JWK_ALG_A192CBC);
      case 256: return NS_LITERAL_STRING(JWK_ALG_A256CBC);
    }
  }

  if (mName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR)) {
    switch (mAes.mLength) {
      case 128: return NS_LITERAL_STRING(JWK_ALG_A128CTR);
      case 192: return NS_LITERAL_STRING(JWK_ALG_A192CTR);
      case 256: return NS_LITERAL_STRING(JWK_ALG_A256CTR);
    }
  }

  if (mName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
    switch (mAes.mLength) {
      case 128: return NS_LITERAL_STRING(JWK_ALG_A128GCM);
      case 192: return NS_LITERAL_STRING(JWK_ALG_A192GCM);
      case 256: return NS_LITERAL_STRING(JWK_ALG_A256GCM);
    }
  }

  if (mName.EqualsLiteral(WEBCRYPTO_ALG_AES_KW)) {
    switch (mAes.mLength) {
      case 128: return NS_LITERAL_STRING(JWK_ALG_A128KW);
      case 192: return NS_LITERAL_STRING(JWK_ALG_A192KW);
      case 256: return NS_LITERAL_STRING(JWK_ALG_A256KW);
    }
  }

  if (mName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
    nsString hashName = mHmac.mHash.mName;
    if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA1)) {
      return NS_LITERAL_STRING(JWK_ALG_HS1);
    } else if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA256)) {
      return NS_LITERAL_STRING(JWK_ALG_HS256);
    } else if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA384)) {
      return NS_LITERAL_STRING(JWK_ALG_HS384);
    } else if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA512)) {
      return NS_LITERAL_STRING(JWK_ALG_HS512);
    }
  }

  if (mName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
    nsString hashName = mRsa.mHash.mName;
    if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA1)) {
      return NS_LITERAL_STRING(JWK_ALG_RS1);
    } else if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA256)) {
      return NS_LITERAL_STRING(JWK_ALG_RS256);
    } else if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA384)) {
      return NS_LITERAL_STRING(JWK_ALG_RS384);
    } else if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA512)) {
      return NS_LITERAL_STRING(JWK_ALG_RS512);
    }
  }

  if (mName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
    nsString hashName = mRsa.mHash.mName;
    if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA1)) {
      return NS_LITERAL_STRING(JWK_ALG_RSA_OAEP);
    } else if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA256)) {
      return NS_LITERAL_STRING(JWK_ALG_RSA_OAEP_256);
    } else if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA384)) {
      return NS_LITERAL_STRING(JWK_ALG_RSA_OAEP_384);
    } else if (hashName.EqualsLiteral(WEBCRYPTO_ALG_SHA512)) {
      return NS_LITERAL_STRING(JWK_ALG_RSA_OAEP_512);
    }
  }

  return nsString();
}

CK_MECHANISM_TYPE
KeyAlgorithmProxy::GetMechanism(const KeyAlgorithm& aAlgorithm)
{
  // For everything but HMAC, the name determines the mechanism
  // HMAC is handled by the specialization below
  return MapAlgorithmNameToMechanism(aAlgorithm.mName);
}

CK_MECHANISM_TYPE
KeyAlgorithmProxy::GetMechanism(const HmacKeyAlgorithm& aAlgorithm)
{
  // The use of HmacKeyAlgorithm doesn't completely prevent this
  // method from being called with dictionaries that don't really
  // represent HMAC key algorithms.
  MOZ_ASSERT(aAlgorithm.mName.EqualsLiteral(WEBCRYPTO_ALG_HMAC));

  CK_MECHANISM_TYPE hashMech;
  hashMech = MapAlgorithmNameToMechanism(aAlgorithm.mHash.mName);

  switch (hashMech) {
    case CKM_SHA_1: return CKM_SHA_1_HMAC;
    case CKM_SHA256: return CKM_SHA256_HMAC;
    case CKM_SHA384: return CKM_SHA384_HMAC;
    case CKM_SHA512: return CKM_SHA512_HMAC;
  }
  return UNKNOWN_CK_MECHANISM;
}

} // namespace dom
} // namespace mozilla
