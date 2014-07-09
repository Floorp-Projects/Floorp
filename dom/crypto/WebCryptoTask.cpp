/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pk11pub.h"
#include "cryptohi.h"
#include "secerr.h"
#include "ScopedNSSTypes.h"

#include "mozilla/dom/WebCryptoTask.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/CryptoKey.h"
#include "mozilla/dom/KeyAlgorithm.h"
#include "mozilla/dom/CryptoKeyPair.h"
#include "mozilla/dom/AesKeyAlgorithm.h"
#include "mozilla/dom/HmacKeyAlgorithm.h"
#include "mozilla/dom/RsaKeyAlgorithm.h"
#include "mozilla/dom/RsaHashedKeyAlgorithm.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/WebCryptoCommon.h"

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace dom {

// Pre-defined identifiers for telemetry histograms

enum TelemetryMethod {
  TM_ENCRYPT      = 0,
  TM_DECRYPT      = 1,
  TM_SIGN         = 2,
  TM_VERIFY       = 3,
  TM_DIGEST       = 4,
  TM_GENERATEKEY  = 5,
  TM_DERIVEKEY    = 6,
  TM_DERIVEBITS   = 7,
  TM_IMPORTKEY    = 8,
  TM_EXPORTKEY    = 9,
  TM_WRAPKEY      = 10,
  TM_UNWRAPKEY    = 11
};

enum TelemetryAlgorithm {
  TA_UNKNOWN         = 0,
  // encrypt / decrypt
  TA_AES_CBC         = 1,
  TA_AES_CFB         = 2,
  TA_AES_CTR         = 3,
  TA_AES_GCM         = 4,
  TA_RSAES_PKCS1     = 5,
  TA_RSA_OAEP        = 6,
  // sign/verify
  TA_RSASSA_PKCS1    = 7,
  TA_RSA_PSS         = 8,
  TA_HMAC_SHA_1      = 9,
  TA_HMAC_SHA_224    = 10,
  TA_HMAC_SHA_256    = 11,
  TA_HMAC_SHA_384    = 12,
  TA_HMAC_SHA_512    = 13,
  // digest
  TA_SHA_1           = 14,
  TA_SHA_224         = 15,
  TA_SHA_256         = 16,
  TA_SHA_384         = 17,
  TA_SHA_512         = 18
};

// Convenience functions for extracting / converting information

// OOM-safe CryptoBuffer initialization, suitable for constructors
#define ATTEMPT_BUFFER_INIT(dst, src) \
  if (!dst.Assign(src)) { \
    mEarlyRv = NS_ERROR_DOM_UNKNOWN_ERR; \
    return; \
  }

// OOM-safe CryptoBuffer-to-SECItem copy, suitable for DoCrypto
#define ATTEMPT_BUFFER_TO_SECITEM(dst, src) \
  dst = src.ToSECItem(); \
  if (!dst) { \
    return NS_ERROR_DOM_UNKNOWN_ERR; \
  }

// OOM-safe CryptoBuffer copy, suitable for DoCrypto
#define ATTEMPT_BUFFER_ASSIGN(dst, src) \
  if (!dst.Assign(src)) { \
    return NS_ERROR_DOM_UNKNOWN_ERR; \
  }

class ClearException
{
public:
  ClearException(JSContext* aCx)
    : mCx(aCx)
  {}

  ~ClearException()
  {
    JS_ClearPendingException(mCx);
  }

private:
  JSContext* mCx;
};

template<class OOS>
static nsresult
GetAlgorithmName(JSContext* aCx, const OOS& aAlgorithm, nsString& aName)
{
  ClearException ce(aCx);

  if (aAlgorithm.IsString()) {
    // If string, then treat as algorithm name
    aName.Assign(aAlgorithm.GetAsString());
  } else {
    // Coerce to algorithm and extract name
    JS::RootedValue value(aCx, JS::ObjectValue(*aAlgorithm.GetAsObject()));
    Algorithm alg;

    if (!alg.Init(aCx, value) || !alg.mName.WasPassed()) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    aName.Assign(alg.mName.Value());
  }

  return NS_OK;
}

template<class T, class OOS>
static nsresult
Coerce(JSContext* aCx, T& aTarget, const OOS& aAlgorithm)
{
  ClearException ce(aCx);

  if (!aAlgorithm.IsObject()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  JS::RootedValue value(aCx, JS::ObjectValue(*aAlgorithm.GetAsObject()));
  if (!aTarget.Init(aCx, value)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  return NS_OK;
}

// Implementation of WebCryptoTask methods

void
WebCryptoTask::FailWithError(nsresult aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_RESOLVED, false);

  // Blindly convert nsresult to DOMException
  // Individual tasks must ensure they pass the right values
  mResultPromise->MaybeReject(aRv);
  // Manually release mResultPromise while we're on the main thread
  mResultPromise = nullptr;
  Cleanup();
}

nsresult
WebCryptoTask::CalculateResult()
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (NS_FAILED(mEarlyRv)) {
    return mEarlyRv;
  }

  if (isAlreadyShutDown()) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  return DoCrypto();
}

void
WebCryptoTask::CallCallback(nsresult rv)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_FAILED(rv)) {
    FailWithError(rv);
    return;
  }

  nsresult rv2 = AfterCrypto();
  if (NS_FAILED(rv2)) {
    FailWithError(rv2);
    return;
  }

  Resolve();
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_RESOLVED, true);

  // Manually release mResultPromise while we're on the main thread
  mResultPromise = nullptr;
  Cleanup();
}

// Some generic utility classes

class FailureTask : public WebCryptoTask
{
public:
  FailureTask(nsresult rv) {
    mEarlyRv = rv;
  }
};

class ReturnArrayBufferViewTask : public WebCryptoTask
{
protected:
  CryptoBuffer mResult;

private:
  // Returns mResult as an ArrayBufferView, or an error
  virtual void Resolve() MOZ_OVERRIDE
  {
    TypedArrayCreator<Uint8Array> ret(mResult);
    mResultPromise->MaybeResolve(ret);
  }
};

class AesTask : public ReturnArrayBufferViewTask
{
public:
  AesTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
          mozilla::dom::CryptoKey& aKey, const CryptoOperationData& aData,
          bool aEncrypt)
    : mSymKey(aKey.GetSymKey())
    , mEncrypt(aEncrypt)
  {
    ATTEMPT_BUFFER_INIT(mData, aData);

    nsString algName;
    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, algName);
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    // Check that we got a reasonable key
    if ((mSymKey.Length() != 16) &&
        (mSymKey.Length() != 24) &&
        (mSymKey.Length() != 32))
    {
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }

    // Cache parameters depending on the specific algorithm
    TelemetryAlgorithm telemetryAlg;
    if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC)) {
      mMechanism = CKM_AES_CBC_PAD;
      telemetryAlg = TA_AES_CBC;
      AesCbcParams params;
      nsresult rv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(rv) || !params.mIv.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }

      ATTEMPT_BUFFER_INIT(mIv, params.mIv.Value())
      if (mIv.Length() != 16) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR)) {
      mMechanism = CKM_AES_CTR;
      telemetryAlg = TA_AES_CTR;
      AesCtrParams params;
      nsresult rv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(rv) || !params.mCounter.WasPassed() ||
          !params.mLength.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      ATTEMPT_BUFFER_INIT(mIv, params.mCounter.Value())
      if (mIv.Length() != 16) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }

      mCounterLength = params.mLength.Value();
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
      mMechanism = CKM_AES_GCM;
      telemetryAlg = TA_AES_GCM;
      AesGcmParams params;
      nsresult rv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(rv) || !params.mIv.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      ATTEMPT_BUFFER_INIT(mIv, params.mIv.Value())

      if (params.mAdditionalData.WasPassed()) {
        ATTEMPT_BUFFER_INIT(mAad, params.mAdditionalData.Value())
      }

      // 32, 64, 96, 104, 112, 120 or 128
      mTagLength = 128;
      if (params.mTagLength.WasPassed()) {
        mTagLength = params.mTagLength.Value();
        if ((mTagLength > 128) ||
            !(mTagLength == 32 || mTagLength == 64 ||
              (mTagLength >= 96 && mTagLength % 8 == 0))) {
          mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
          return;
        }
      }
    } else {
      mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      return;
    }
    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, telemetryAlg);
  }

private:
  CK_MECHANISM_TYPE mMechanism;
  CryptoBuffer mSymKey;
  CryptoBuffer mIv;   // Initialization vector
  CryptoBuffer mData;
  CryptoBuffer mAad;  // Additional Authenticated Data
  uint8_t mTagLength;
  uint8_t mCounterLength;
  bool mEncrypt;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsresult rv;

    // Construct the parameters object depending on algorithm
    SECItem param;
    ScopedSECItem cbcParam;
    CK_AES_CTR_PARAMS ctrParams;
    CK_GCM_PARAMS gcmParams;
    switch (mMechanism) {
      case CKM_AES_CBC_PAD:
        ATTEMPT_BUFFER_TO_SECITEM(cbcParam, mIv);
        param = *cbcParam;
        break;
      case CKM_AES_CTR:
        ctrParams.ulCounterBits = mCounterLength;
        MOZ_ASSERT(mIv.Length() == 16);
        memcpy(&ctrParams.cb, mIv.Elements(), 16);
        param.type = siBuffer;
        param.data = (unsigned char*) &ctrParams;
        param.len  = sizeof(ctrParams);
        break;
      case CKM_AES_GCM:
        gcmParams.pIv = mIv.Elements();
        gcmParams.ulIvLen = mIv.Length();
        gcmParams.pAAD = mAad.Elements();
        gcmParams.ulAADLen = mAad.Length();
        gcmParams.ulTagBits = mTagLength;
        param.type = siBuffer;
        param.data = (unsigned char*) &gcmParams;
        param.len  = sizeof(gcmParams);
        break;
      default:
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    // Import the key
    ScopedSECItem keyItem;
    ATTEMPT_BUFFER_TO_SECITEM(keyItem, mSymKey);
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    MOZ_ASSERT(slot.get());
    ScopedPK11SymKey symKey(PK11_ImportSymKey(slot, mMechanism, PK11_OriginUnwrap,
                                              CKA_ENCRYPT, keyItem.get(), nullptr));
    if (!symKey) {
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
    }

    // Initialize the output buffer (enough space for padding / a full tag)
    uint32_t dataLen = mData.Length();
    uint32_t maxLen = dataLen + 16;
    if (!mResult.SetLength(maxLen)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }
    uint32_t outLen = 0;

    // Perform the encryption/decryption
    if (mEncrypt) {
      rv = MapSECStatus(PK11_Encrypt(symKey.get(), mMechanism, &param,
                                     mResult.Elements(), &outLen, maxLen,
                                     mData.Elements(), mData.Length()));
    } else {
      rv = MapSECStatus(PK11_Decrypt(symKey.get(), mMechanism, &param,
                                     mResult.Elements(), &outLen, maxLen,
                                     mData.Elements(), mData.Length()));
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

    mResult.SetLength(outLen);
    return rv;
  }
};

class RsaesPkcs1Task : public ReturnArrayBufferViewTask
{
public:
  RsaesPkcs1Task(JSContext* aCx, const ObjectOrString& aAlgorithm,
                 mozilla::dom::CryptoKey& aKey, const CryptoOperationData& aData,
                 bool aEncrypt)
    : mPrivKey(aKey.GetPrivateKey())
    , mPubKey(aKey.GetPublicKey())
    , mEncrypt(aEncrypt)
  {
    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, TA_RSAES_PKCS1);

    ATTEMPT_BUFFER_INIT(mData, aData);

    if (mEncrypt) {
      if (!mPubKey) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }
      mStrength = SECKEY_PublicKeyStrength(mPubKey);

      // Verify that the data input is not too big
      // (as required by PKCS#1 / RFC 3447, Section 7.2)
      // http://tools.ietf.org/html/rfc3447#section-7.2
      if (mData.Length() > mStrength - 11) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }
    } else {
      if (!mPrivKey) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }
      mStrength = PK11_GetPrivateModulusLen(mPrivKey);
    }
  }

private:
  ScopedSECKEYPrivateKey mPrivKey;
  ScopedSECKEYPublicKey mPubKey;
  CryptoBuffer mData;
  uint32_t mStrength;
  bool mEncrypt;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsresult rv;

    // Ciphertext is an integer mod the modulus, so it will be
    // no longer than mStrength octets
    if (!mResult.SetLength(mStrength)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    if (mEncrypt) {
      rv = MapSECStatus(PK11_PubEncryptPKCS1(
              mPubKey.get(), mResult.Elements(),
              mData.Elements(), mData.Length(),
              nullptr));
    } else {
      uint32_t outLen;
      rv = MapSECStatus(PK11_PrivDecryptPKCS1(
              mPrivKey.get(), mResult.Elements(),
              &outLen, mResult.Length(),
              mData.Elements(), mData.Length()));
      mResult.SetLength(outLen);
    }

    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);
    return NS_OK;
  }
};

class HmacTask : public WebCryptoTask
{
public:
  HmacTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
           mozilla::dom::CryptoKey& aKey,
           const CryptoOperationData& aSignature,
           const CryptoOperationData& aData,
           bool aSign)
    : mMechanism(aKey.Algorithm()->Mechanism())
    , mSymKey(aKey.GetSymKey())
    , mSign(aSign)
  {
    ATTEMPT_BUFFER_INIT(mData, aData);
    if (!aSign) {
      ATTEMPT_BUFFER_INIT(mSignature, aSignature);
    }

    // Check that we got a symmetric key
    if (mSymKey.Length() == 0) {
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }

    TelemetryAlgorithm telemetryAlg;
    switch (mMechanism) {
      case CKM_SHA_1_HMAC:  telemetryAlg = TA_HMAC_SHA_1; break;
      case CKM_SHA224_HMAC: telemetryAlg = TA_HMAC_SHA_224; break;
      case CKM_SHA256_HMAC: telemetryAlg = TA_HMAC_SHA_256; break;
      case CKM_SHA384_HMAC: telemetryAlg = TA_HMAC_SHA_384; break;
      case CKM_SHA512_HMAC: telemetryAlg = TA_HMAC_SHA_512; break;
      default:              telemetryAlg = TA_UNKNOWN;
    }
    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, telemetryAlg);
  }

private:
  CK_MECHANISM_TYPE mMechanism;
  CryptoBuffer mSymKey;
  CryptoBuffer mData;
  CryptoBuffer mSignature;
  CryptoBuffer mResult;
  bool mSign;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    // Initialize the output buffer
    if (!mResult.SetLength(HASH_LENGTH_MAX)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }
    uint32_t outLen;

    // Import the key
    ScopedSECItem keyItem;
    ATTEMPT_BUFFER_TO_SECITEM(keyItem, mSymKey);
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    MOZ_ASSERT(slot.get());
    ScopedPK11SymKey symKey(PK11_ImportSymKey(slot, mMechanism, PK11_OriginUnwrap,
                                              CKA_SIGN, keyItem.get(), nullptr));
    if (!symKey) {
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
    }

    // Compute the MAC
    SECItem param = { siBuffer, nullptr, 0 };
    ScopedPK11Context ctx(PK11_CreateContextBySymKey(mMechanism, CKA_SIGN,
                                                     symKey.get(), &param));
    if (!ctx.get()) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }
    nsresult rv = MapSECStatus(PK11_DigestBegin(ctx.get()));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);
    rv = MapSECStatus(PK11_DigestOp(ctx.get(), mData.Elements(), mData.Length()));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);
    rv = MapSECStatus(PK11_DigestFinal(ctx.get(), mResult.Elements(),
                                       &outLen, HASH_LENGTH_MAX));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

    mResult.SetLength(outLen);
    return rv;
  }

  // Returns mResult as an ArrayBufferView, or an error
  virtual void Resolve() MOZ_OVERRIDE
  {
    if (mSign) {
      // Return the computed MAC
      TypedArrayCreator<Uint8Array> ret(mResult);
      mResultPromise->MaybeResolve(ret);
    } else {
      // Compare the MAC to the provided signature
      // No truncation allowed
      bool equal = (mResult.Length() == mSignature.Length());
      if (equal) {
        int cmp = NSS_SecureMemcmp(mSignature.Elements(),
                                   mResult.Elements(),
                                   mSignature.Length());
        equal = (cmp == 0);
      }
      mResultPromise->MaybeResolve(equal);
    }
  }
};

class RsassaPkcs1Task : public WebCryptoTask
{
public:
  RsassaPkcs1Task(JSContext* aCx, const ObjectOrString& aAlgorithm,
                  mozilla::dom::CryptoKey& aKey,
                  const CryptoOperationData& aSignature,
                  const CryptoOperationData& aData,
                  bool aSign)
    : mOidTag(SEC_OID_UNKNOWN)
    , mPrivKey(aKey.GetPrivateKey())
    , mPubKey(aKey.GetPublicKey())
    , mSign(aSign)
    , mVerified(false)
  {
    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, TA_RSASSA_PKCS1);

    ATTEMPT_BUFFER_INIT(mData, aData);
    if (!aSign) {
      ATTEMPT_BUFFER_INIT(mSignature, aSignature);
    }

    // Look up the SECOidTag based on the KeyAlgorithm
    // static_cast is safe because we only get here if the algorithm name
    // is RSASSA-PKCS1-v1_5, and that only happens if we've constructed
    // an RsaHashedKeyAlgorithm
    nsRefPtr<RsaHashedKeyAlgorithm> rsaAlg = static_cast<RsaHashedKeyAlgorithm*>(aKey.Algorithm());
    nsRefPtr<KeyAlgorithm> hashAlg = rsaAlg->Hash();

    switch (hashAlg->Mechanism()) {
      case CKM_SHA_1:
        mOidTag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION; break;
      case CKM_SHA256:
        mOidTag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION; break;
      case CKM_SHA384:
        mOidTag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION; break;
      case CKM_SHA512:
        mOidTag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION; break;
      default: {
        mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        return;
      }
    }

    // Check that we have the appropriate key
    if ((mSign && !mPrivKey) || (!mSign && !mPubKey)) {
      mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
      return;
    }
  }

private:
  SECOidTag mOidTag;
  ScopedSECKEYPrivateKey mPrivKey;
  ScopedSECKEYPublicKey mPubKey;
  CryptoBuffer mSignature;
  CryptoBuffer mData;
  bool mSign;
  bool mVerified;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsresult rv;
    if (mSign) {
      ScopedSECItem signature((SECItem*) PORT_Alloc(sizeof(SECItem)));
      ScopedSGNContext ctx(SGN_NewContext(mOidTag, mPrivKey));
      if (!ctx) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      rv = MapSECStatus(SGN_Begin(ctx));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

      rv = MapSECStatus(SGN_Update(ctx, mData.Elements(), mData.Length()));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

      rv = MapSECStatus(SGN_End(ctx, signature));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

      ATTEMPT_BUFFER_ASSIGN(mSignature, signature);
    } else {
      ScopedSECItem signature(mSignature.ToSECItem());
      if (!signature) {
        return NS_ERROR_DOM_UNKNOWN_ERR;
      }

      ScopedVFYContext ctx(VFY_CreateContext(mPubKey, signature,
                                             mOidTag, nullptr));
      if (!ctx) {
        int err = PORT_GetError();
        if (err == SEC_ERROR_BAD_SIGNATURE) {
          mVerified = false;
          return NS_OK;
        }
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      rv = MapSECStatus(VFY_Begin(ctx));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

      rv = MapSECStatus(VFY_Update(ctx, mData.Elements(), mData.Length()));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

      rv = MapSECStatus(VFY_End(ctx));
      mVerified = NS_SUCCEEDED(rv);
    }

    return NS_OK;
  }

  virtual void Resolve() MOZ_OVERRIDE
  {
    if (mSign) {
      TypedArrayCreator<Uint8Array> ret(mSignature);
      mResultPromise->MaybeResolve(ret);
    } else {
      mResultPromise->MaybeResolve(mVerified);
    }
  }
};

class SimpleDigestTask : public ReturnArrayBufferViewTask
{
public:
  SimpleDigestTask(JSContext* aCx,
                   const ObjectOrString& aAlgorithm,
                   const CryptoOperationData& aData)
  {
    ATTEMPT_BUFFER_INIT(mData, aData);

    nsString algName;
    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, algName);
    if (NS_FAILED(mEarlyRv)) {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }

    TelemetryAlgorithm telemetryAlg;
    if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA1))   {
      mOidTag = SEC_OID_SHA1;
      telemetryAlg = TA_SHA_1;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA256)) {
      mOidTag = SEC_OID_SHA256;
      telemetryAlg = TA_SHA_224;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA384)) {
      mOidTag = SEC_OID_SHA384;
      telemetryAlg = TA_SHA_256;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA512)) {
      mOidTag = SEC_OID_SHA512;
      telemetryAlg = TA_SHA_384;
    } else {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }
    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, telemetryAlg);
  }

private:
  SECOidTag mOidTag;
  CryptoBuffer mData;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    // Resize the result buffer
    uint32_t hashLen = HASH_ResultLenByOidTag(mOidTag);
    if (!mResult.SetLength(hashLen)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    // Compute the hash
    nsresult rv = MapSECStatus(PK11_HashBuf(mOidTag, mResult.Elements(),
                                            mData.Elements(), mData.Length()));
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    return rv;
  }
};

class ImportKeyTask : public WebCryptoTask
{
public:
  ImportKeyTask(JSContext* aCx,
      const nsAString& aFormat, const KeyData& aKeyData,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    // Get the current global object from the context
    nsIGlobalObject *global = xpc::GetNativeForGlobal(JS::CurrentGlobalOrNull(aCx));
    if (!global) {
      mEarlyRv = NS_ERROR_DOM_UNKNOWN_ERR;
      return;
    }

    // This stuff pretty much always happens, so we'll do it here
    mKey = new CryptoKey(global);
    mKey->SetExtractable(aExtractable);
    mKey->ClearUsages();
    for (uint32_t i = 0; i < aKeyUsages.Length(); ++i) {
      mEarlyRv = mKey->AddUsage(aKeyUsages[i]);
      if (NS_FAILED(mEarlyRv)) {
        return;
      }
    }

    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, mAlgName);
    if (NS_FAILED(mEarlyRv)) {
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }
  }

protected:
  nsRefPtr<CryptoKey> mKey;
  nsString mAlgName;

private:
  virtual void Resolve() MOZ_OVERRIDE
  {
    mResultPromise->MaybeResolve(mKey);
  }

  virtual void Cleanup() MOZ_OVERRIDE
  {
    mKey = nullptr;
  }
};


class ImportSymmetricKeyTask : public ImportKeyTask
{
public:
  ImportSymmetricKeyTask(JSContext* aCx,
      const nsAString& aFormat, const KeyData& aKeyData,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
    : ImportKeyTask(aCx, aFormat, aKeyData, aAlgorithm, aExtractable, aKeyUsages)
  {
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    // Import the key data
    if (aFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_RAW)) {
      if (aKeyData.IsArrayBufferView()) {
        mKeyData.Assign(aKeyData.GetAsArrayBufferView());
      } else if (aKeyData.IsArrayBuffer()) {
        mKeyData.Assign(aKeyData.GetAsArrayBuffer());
      } else {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }
    } else if (aFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK)) {
      mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      return;
    } else {
      // Invalid key format
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }

    // If this is an HMAC key, import the hash name
    if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
      RootedDictionary<HmacImportParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mHash.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }
      mEarlyRv = GetAlgorithmName(aCx, params.mHash.Value(), mHashName);
      if (NS_FAILED(mEarlyRv)) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }
    }
  }

  virtual nsresult BeforeCrypto() MOZ_OVERRIDE
  {
    // Construct an appropriate KeyAlorithm,
    // and verify that usages are appropriate
    nsRefPtr<KeyAlgorithm> algorithm;
    nsIGlobalObject* global = mKey->GetParentObject();
    uint32_t length = 8 * mKeyData.Length(); // bytes to bits
    if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
        mAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
        mAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
      if (mKey->HasUsageOtherThan(CryptoKey::ENCRYPT | CryptoKey::DECRYPT)) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      if ( (length != 128) && (length != 192) && (length != 256) ) {
        return NS_ERROR_DOM_DATA_ERR;
      }
      algorithm = new AesKeyAlgorithm(global, mAlgName, length);
    } else if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
      if (mKey->HasUsageOtherThan(CryptoKey::SIGN | CryptoKey::VERIFY)) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      algorithm = new HmacKeyAlgorithm(global, mAlgName, length, mHashName);
      if (algorithm->Mechanism() == UNKNOWN_CK_MECHANISM) {
        return NS_ERROR_DOM_SYNTAX_ERR;
      }
    } else {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    mKey->SetAlgorithm(algorithm);
    mKey->SetSymKey(mKeyData);
    mKey->SetType(CryptoKey::SECRET);
    mEarlyComplete = true;
    return NS_OK;
  }

private:
  CryptoBuffer mKeyData;
  nsString mHashName;
};

class ImportRsaKeyTask : public ImportKeyTask
{
public:
  ImportRsaKeyTask(JSContext* aCx,
      const nsAString& aFormat, const KeyData& aKeyData,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
    : ImportKeyTask(aCx, aFormat, aKeyData, aAlgorithm, aExtractable, aKeyUsages)
  {
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    mFormat = aFormat;

    // Import the key data
    if (aKeyData.IsArrayBufferView()) {
      mKeyData.Assign(aKeyData.GetAsArrayBufferView());
    } else if (aKeyData.IsArrayBuffer()) {
      mKeyData.Assign(aKeyData.GetAsArrayBuffer());
    } else {
      // TODO This will need to be changed for JWK (Bug 1005220)
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }

    // If this is RSA with a hash, cache the hash name
    if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
      RootedDictionary<RsaHashedImportParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mHash.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }

      mEarlyRv = GetAlgorithmName(aCx, params.mHash.Value(), mHashName);
      if (NS_FAILED(mEarlyRv)) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }
    }
  }

private:
  CryptoBuffer mKeyData;
  nsString mFormat;
  nsString mHashName;
  uint32_t mModulusLength;
  CryptoBuffer mPublicExponent;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsNSSShutDownPreventionLock locker;

    // Import the key data itself
    ScopedSECKEYPublicKey pubKey;
    if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_PKCS8)) {
      ScopedSECKEYPrivateKey privKey(CryptoKey::PrivateKeyFromPkcs8(mKeyData, locker));
      if (!privKey.get()) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      mKey->SetPrivateKey(privKey.get());
      mKey->SetType(CryptoKey::PRIVATE);
      pubKey = SECKEY_ConvertToPublicKey(privKey.get());
      if (!pubKey) {
        return NS_ERROR_DOM_UNKNOWN_ERR;
      }
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_SPKI)) {
      pubKey = CryptoKey::PublicKeyFromSpki(mKeyData, locker);
      if (!pubKey.get()) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      if (pubKey->keyType != rsaKey) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      mKey->SetPublicKey(pubKey.get());
      mKey->SetType(CryptoKey::PUBLIC);
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK)) {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    } else {
      // Invalid key format
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    // Extract relevant information from the public key
    mModulusLength = 8 * pubKey->u.rsa.modulus.len;
    mPublicExponent.Assign(&pubKey->u.rsa.publicExponent);

    return NS_OK;
  }

  virtual nsresult AfterCrypto() MOZ_OVERRIDE
  {
    // Construct an appropriate KeyAlgorithm
    nsIGlobalObject* global = mKey->GetParentObject();
    if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1)) {
      if ((mKey->GetKeyType() == CryptoKey::PUBLIC &&
           mKey->HasUsageOtherThan(CryptoKey::ENCRYPT)) ||
          (mKey->GetKeyType() == CryptoKey::PRIVATE &&
           mKey->HasUsageOtherThan(CryptoKey::DECRYPT))) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      mKey->SetAlgorithm(new RsaKeyAlgorithm(global, mAlgName, mModulusLength, mPublicExponent));
    } else if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
      if ((mKey->GetKeyType() == CryptoKey::PUBLIC &&
           mKey->HasUsageOtherThan(CryptoKey::VERIFY)) ||
          (mKey->GetKeyType() == CryptoKey::PRIVATE &&
           mKey->HasUsageOtherThan(CryptoKey::SIGN))) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      nsRefPtr<RsaHashedKeyAlgorithm> algorithm = new RsaHashedKeyAlgorithm(
                                                          global,
                                                          mAlgName,
                                                          mModulusLength,
                                                          mPublicExponent,
                                                          mHashName);
      if (algorithm->Mechanism() == UNKNOWN_CK_MECHANISM) {
        return NS_ERROR_DOM_SYNTAX_ERR;
      }
      mKey->SetAlgorithm(algorithm);
    }

    return NS_OK;
  }
};


class UnifiedExportKeyTask : public ReturnArrayBufferViewTask
{
public:
  UnifiedExportKeyTask(const nsAString& aFormat, CryptoKey& aKey)
    : mFormat(aFormat)
    , mSymKey(aKey.GetSymKey())
    , mPrivateKey(aKey.GetPrivateKey())
    , mPublicKey(aKey.GetPublicKey())
  {
    if (!aKey.Extractable()) {
      mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
      return;
    }
  }


private:
  nsString mFormat;
  CryptoBuffer mSymKey;
  ScopedSECKEYPrivateKey mPrivateKey;
  ScopedSECKEYPublicKey mPublicKey;

  virtual void ReleaseNSSResources() MOZ_OVERRIDE
  {
    mPrivateKey.dispose();
    mPublicKey.dispose();
  }

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsNSSShutDownPreventionLock locker;

    if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_RAW)) {
      mResult = mSymKey;
      if (mResult.Length() == 0) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }

      return NS_OK;
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_PKCS8)) {
      if (!mPrivateKey) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }

      switch (mPrivateKey->keyType) {
        case rsaKey:
          CryptoKey::PrivateKeyToPkcs8(mPrivateKey.get(), mResult, locker);
          return NS_OK;
        default:
          return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_SPKI)) {
      if (!mPublicKey) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }

      return CryptoKey::PublicKeyToSpki(mPublicKey.get(), mResult, locker);
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK)) {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    return NS_ERROR_DOM_SYNTAX_ERR;
  }
};

class GenerateSymmetricKeyTask : public WebCryptoTask
{
public:
  GenerateSymmetricKeyTask(JSContext* aCx,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    nsIGlobalObject* global = xpc::GetNativeForGlobal(JS::CurrentGlobalOrNull(aCx));
    if (!global) {
      mEarlyRv = NS_ERROR_DOM_UNKNOWN_ERR;
      return;
    }

    // Create an empty key and set easy attributes
    mKey = new CryptoKey(global);
    mKey->SetExtractable(aExtractable);
    mKey->SetType(CryptoKey::SECRET);

    // Extract algorithm name
    nsString algName;
    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, algName);
    if (NS_FAILED(mEarlyRv)) {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }

    // Construct an appropriate KeyAlorithm
    nsRefPtr<KeyAlgorithm> algorithm;
    uint32_t allowedUsages = 0;
    if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
        algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
        algName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
      RootedDictionary<AesKeyGenParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mLength.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      mLength = params.mLength.Value();
      if (mLength != 128 && mLength != 192 && mLength != 256) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }
      algorithm = new AesKeyAlgorithm(global, algName, mLength);
      allowedUsages = CryptoKey::ENCRYPT | CryptoKey::DECRYPT;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
      RootedDictionary<HmacKeyGenParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mHash.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      nsString hashName;
      if (params.mHash.Value().IsString()) {
        hashName.Assign(params.mHash.Value().GetAsString());
      } else {
        Algorithm hashAlg;
        mEarlyRv = Coerce(aCx, hashAlg, params.mHash.Value());
        if (NS_FAILED(mEarlyRv) || !hashAlg.mName.WasPassed()) {
          mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
          return;
        }
        hashName.Assign(hashAlg.mName.Value());
      }

      if (params.mLength.WasPassed()) {
        mLength = params.mLength.Value();
      } else {
        nsRefPtr<KeyAlgorithm> hashAlg = new KeyAlgorithm(global, hashName);
        switch (hashAlg->Mechanism()) {
          case CKM_SHA_1:
          case CKM_SHA256:
            mLength = 512;
            break;
          case CKM_SHA384:
          case CKM_SHA512:
            mLength = 1024;
            break;
          default:
            mLength = 0;
            break;
        }
      }

      if (mLength == 0) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }

      algorithm = new HmacKeyAlgorithm(global, algName, mLength, hashName);
      allowedUsages = CryptoKey::SIGN | CryptoKey::VERIFY;
    } else {
      mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      return;
    }

    // Add key usages
    mKey->ClearUsages();
    for (uint32_t i = 0; i < aKeyUsages.Length(); ++i) {
      mEarlyRv = mKey->AddUsageIntersecting(aKeyUsages[i], allowedUsages);
      if (NS_FAILED(mEarlyRv)) {
        return;
      }
    }

    mLength = mLength >> 3; // bits to bytes
    mMechanism = algorithm->Mechanism();
    mKey->SetAlgorithm(algorithm);
    // SetSymKey done in Resolve, after we've done the keygen
  }

private:
  nsRefPtr<CryptoKey> mKey;
  size_t mLength;
  CK_MECHANISM_TYPE mMechanism;
  CryptoBuffer mKeyData;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    MOZ_ASSERT(slot.get());

    ScopedPK11SymKey symKey(PK11_KeyGen(slot.get(), mMechanism, nullptr,
                                        mLength, nullptr));
    if (!symKey) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    nsresult rv = MapSECStatus(PK11_ExtractKeyValue(symKey));
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    // This doesn't leak, because the SECItem* returned by PK11_GetKeyData
    // just refers to a buffer managed by symKey.  The assignment copies the
    // data, so mKeyData manages one copy, while symKey manages another.
    ATTEMPT_BUFFER_ASSIGN(mKeyData, PK11_GetKeyData(symKey));
    return NS_OK;
  }

  virtual void Resolve() {
    mKey->SetSymKey(mKeyData);
    mResultPromise->MaybeResolve(mKey);
  }

  virtual void Cleanup() {
    mKey = nullptr;
  }
};

class GenerateAsymmetricKeyTask : public WebCryptoTask
{
public:
  GenerateAsymmetricKeyTask(JSContext* aCx,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    nsIGlobalObject* global = xpc::GetNativeForGlobal(JS::CurrentGlobalOrNull(aCx));
    if (!global) {
      mEarlyRv = NS_ERROR_DOM_UNKNOWN_ERR;
      return;
    }

    // Create an empty key and set easy attributes
    mKeyPair = new CryptoKeyPair(global);

    // Extract algorithm name
    nsString algName;
    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, algName);
    if (NS_FAILED(mEarlyRv)) {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }

    // Construct an appropriate KeyAlorithm
    KeyAlgorithm* algorithm;
    uint32_t privateAllowedUsages = 0, publicAllowedUsages = 0;
    if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
      RootedDictionary<RsaHashedKeyGenParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mModulusLength.WasPassed() ||
          !params.mPublicExponent.WasPassed() ||
          !params.mHash.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      // Pull relevant info
      uint32_t modulusLength = params.mModulusLength.Value();
      CryptoBuffer publicExponent;
      ATTEMPT_BUFFER_INIT(publicExponent, params.mPublicExponent.Value());
      nsString hashName;
      mEarlyRv = GetAlgorithmName(aCx, params.mHash.Value(), hashName);
      if (NS_FAILED(mEarlyRv)) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      // Create algorithm
      algorithm = new RsaHashedKeyAlgorithm(global, algName, modulusLength,
                                            publicExponent, hashName);
      mKeyPair->PublicKey()->SetAlgorithm(algorithm);
      mKeyPair->PrivateKey()->SetAlgorithm(algorithm);
      mMechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;

      // Set up params struct
      mRsaParams.keySizeInBits = modulusLength;
      bool converted = publicExponent.GetBigIntValue(mRsaParams.pe);
      if (!converted) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }

      privateAllowedUsages = CryptoKey::SIGN;
      publicAllowedUsages = CryptoKey::VERIFY;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1)) {
      RootedDictionary<RsaKeyGenParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mModulusLength.WasPassed() ||
          !params.mPublicExponent.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      // Pull relevant info
      uint32_t modulusLength = params.mModulusLength.Value();
      CryptoBuffer publicExponent;
      ATTEMPT_BUFFER_INIT(publicExponent, params.mPublicExponent.Value());

      // Create algorithm and note the mechanism
      algorithm = new RsaKeyAlgorithm(global, algName, modulusLength,
                                      publicExponent);
      mKeyPair->PublicKey()->SetAlgorithm(algorithm);
      mKeyPair->PrivateKey()->SetAlgorithm(algorithm);
      mMechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;

      // Set up params struct
      mRsaParams.keySizeInBits = modulusLength;
      bool converted = publicExponent.GetBigIntValue(mRsaParams.pe);
      if (!converted) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }

      privateAllowedUsages = CryptoKey::DECRYPT | CryptoKey::UNWRAPKEY;
      publicAllowedUsages = CryptoKey::ENCRYPT | CryptoKey::WRAPKEY;
    } else {
      mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      return;
    }

    mKeyPair->PrivateKey()->SetExtractable(aExtractable);
    mKeyPair->PrivateKey()->SetType(CryptoKey::PRIVATE);

    mKeyPair->PublicKey()->SetExtractable(true);
    mKeyPair->PublicKey()->SetType(CryptoKey::PUBLIC);

    mKeyPair->PrivateKey()->ClearUsages();
    mKeyPair->PublicKey()->ClearUsages();
    for (uint32_t i=0; i < aKeyUsages.Length(); ++i) {
      mEarlyRv = mKeyPair->PrivateKey()->AddUsageIntersecting(aKeyUsages[i],
                                                              privateAllowedUsages);
      if (NS_FAILED(mEarlyRv)) {
        return;
      }

      mEarlyRv = mKeyPair->PublicKey()->AddUsageIntersecting(aKeyUsages[i],
                                                             publicAllowedUsages);
      if (NS_FAILED(mEarlyRv)) {
        return;
      }
    }
  }

private:
  nsRefPtr<CryptoKeyPair> mKeyPair;
  CK_MECHANISM_TYPE mMechanism;
  PK11RSAGenParams mRsaParams;
  ScopedSECKEYPublicKey mPublicKey;
  ScopedSECKEYPrivateKey mPrivateKey;

  virtual void ReleaseNSSResources() MOZ_OVERRIDE
  {
    mPublicKey.dispose();
    mPrivateKey.dispose();
  }

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    MOZ_ASSERT(slot.get());

    void* param;
    switch (mMechanism) {
      case CKM_RSA_PKCS_KEY_PAIR_GEN: param = &mRsaParams; break;
      default: return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    SECKEYPublicKey* pubKey = nullptr;
    mPrivateKey = PK11_GenerateKeyPair(slot.get(), mMechanism, param, &pubKey,
                                       PR_FALSE, PR_FALSE, nullptr);
    mPublicKey = pubKey;
    if (!mPrivateKey.get() || !mPublicKey.get()) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    mKeyPair->PrivateKey()->SetPrivateKey(mPrivateKey);
    mKeyPair->PublicKey()->SetPublicKey(mPublicKey);
    return NS_OK;
  }

  virtual void Resolve() MOZ_OVERRIDE
  {
    mResultPromise->MaybeResolve(mKeyPair);
  }

  virtual void Cleanup() MOZ_OVERRIDE
  {
    mKeyPair = nullptr;
  }
};


// Task creation methods for WebCryptoTask

WebCryptoTask*
WebCryptoTask::EncryptDecryptTask(JSContext* aCx,
                                  const ObjectOrString& aAlgorithm,
                                  CryptoKey& aKey,
                                  const CryptoOperationData& aData,
                                  bool aEncrypt)
{
  TelemetryMethod method = (aEncrypt)? TM_ENCRYPT : TM_DECRYPT;
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, method);
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_EXTRACTABLE_ENC, aKey.Extractable());

  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  // Ensure key is usable for this operation
  if ((aEncrypt  && !aKey.HasUsage(CryptoKey::ENCRYPT)) ||
      (!aEncrypt && !aKey.HasUsage(CryptoKey::DECRYPT))) {
    return new FailureTask(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  }

  if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
    return new AesTask(aCx, aAlgorithm, aKey, aData, aEncrypt);
  } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1)) {
    return new RsaesPkcs1Task(aCx, aAlgorithm, aKey, aData, aEncrypt);
  }

  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::SignVerifyTask(JSContext* aCx,
                              const ObjectOrString& aAlgorithm,
                              CryptoKey& aKey,
                              const CryptoOperationData& aSignature,
                              const CryptoOperationData& aData,
                              bool aSign)
{
  TelemetryMethod method = (aSign)? TM_SIGN : TM_VERIFY;
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, method);
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_EXTRACTABLE_SIG, aKey.Extractable());

  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  // Ensure key is usable for this operation
  if ((aSign  && !aKey.HasUsage(CryptoKey::SIGN)) ||
      (!aSign && !aKey.HasUsage(CryptoKey::VERIFY))) {
    return new FailureTask(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  }

  if (algName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
    return new HmacTask(aCx, aAlgorithm, aKey, aSignature, aData, aSign);
  } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
    return new RsassaPkcs1Task(aCx, aAlgorithm, aKey, aSignature, aData, aSign);
  }

  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::DigestTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          const CryptoOperationData& aData)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_DIGEST);
  return new SimpleDigestTask(aCx, aAlgorithm, aData);
}

WebCryptoTask*
WebCryptoTask::ImportKeyTask(JSContext* aCx,
                             const nsAString& aFormat,
                             const KeyData& aKeyData,
                             const ObjectOrString& aAlgorithm,
                             bool aExtractable,
                             const Sequence<nsString>& aKeyUsages)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_IMPORTKEY);
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_EXTRACTABLE_IMPORT, aExtractable);

  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
    return new ImportSymmetricKeyTask(aCx, aFormat, aKeyData, aAlgorithm,
                                      aExtractable, aKeyUsages);
  } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1) ||
             algName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
    return new ImportRsaKeyTask(aCx, aFormat, aKeyData, aAlgorithm,
                                aExtractable, aKeyUsages);
  } else {
    return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  }
}

WebCryptoTask*
WebCryptoTask::ExportKeyTask(const nsAString& aFormat,
                             CryptoKey& aKey)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_EXPORTKEY);

  if (aFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK)) {
    return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  } else {
    return new UnifiedExportKeyTask(aFormat, aKey);
  }
}

WebCryptoTask*
WebCryptoTask::GenerateKeyTask(JSContext* aCx,
                               const ObjectOrString& aAlgorithm,
                               bool aExtractable,
                               const Sequence<nsString>& aKeyUsages)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_GENERATEKEY);
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_EXTRACTABLE_GENERATE, aExtractable);

  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  if (algName.EqualsASCII(WEBCRYPTO_ALG_AES_CBC) ||
      algName.EqualsASCII(WEBCRYPTO_ALG_AES_CTR) ||
      algName.EqualsASCII(WEBCRYPTO_ALG_AES_GCM) ||
      algName.EqualsASCII(WEBCRYPTO_ALG_HMAC)) {
    return new GenerateSymmetricKeyTask(aCx, aAlgorithm, aExtractable, aKeyUsages);
  } else if (algName.EqualsASCII(WEBCRYPTO_ALG_RSAES_PKCS1) ||
             algName.EqualsASCII(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
    return new GenerateAsymmetricKeyTask(aCx, aAlgorithm, aExtractable, aKeyUsages);
  } else {
    return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  }
}

WebCryptoTask*
WebCryptoTask::DeriveKeyTask(JSContext* aCx,
                             const ObjectOrString& aAlgorithm,
                             CryptoKey& aBaseKey,
                             const ObjectOrString& aDerivedKeyType,
                             bool aExtractable,
                             const Sequence<nsString>& aKeyUsages)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_DERIVEKEY);
  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::DeriveBitsTask(JSContext* aCx,
                              const ObjectOrString& aAlgorithm,
                              CryptoKey& aKey,
                              uint32_t aLength)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_DERIVEBITS);
  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}


} // namespace dom
} // namespace mozilla
