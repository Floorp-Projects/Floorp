/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pk11pub.h"
#include "cryptohi.h"
#include "ScopedNSSTypes.h"

#include "mozilla/dom/WebCryptoTask.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/Key.h"
#include "mozilla/dom/KeyAlgorithm.h"
#include "mozilla/dom/AesKeyAlgorithm.h"
#include "mozilla/dom/HmacKeyAlgorithm.h"
#include "mozilla/dom/RsaKeyAlgorithm.h"
#include "mozilla/dom/RsaHashedKeyAlgorithm.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/WebCryptoCommon.h"

namespace mozilla {
namespace dom {

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
          mozilla::dom::Key& aKey, const CryptoOperationData& aData,
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
    if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC)) {
      mMechanism = CKM_AES_CBC_PAD;
      AesCbcParams params;
      nsresult rv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(rv) || !params.mIv.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }

      ATTEMPT_BUFFER_INIT(mIv, params.mIv.Value())
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR)) {
      mMechanism = CKM_AES_CTR;
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

class HmacTask : public WebCryptoTask
{
public:
  HmacTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
           mozilla::dom::Key& aKey,
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
      int cmp = NSS_SecureMemcmp(mSignature.Elements(),
                                 mResult.Elements(),
                                 mSignature.Length());
      equal = equal && (cmp == 0);
      mResultPromise->MaybeResolve(equal);
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
      return;
    }

    if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA1))   {
      mOidTag = SEC_OID_SHA1;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA224)) {
      mOidTag = SEC_OID_SHA224;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA256)) {
      mOidTag = SEC_OID_SHA256;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA384)) {
      mOidTag = SEC_OID_SHA384;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA512)) {
      mOidTag = SEC_OID_SHA512;
    } else {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }
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
    mKey = new Key(global);
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
  nsRefPtr<Key> mKey;
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
      if (mKey->HasUsageOtherThan(Key::ENCRYPT | Key::DECRYPT)) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      if ( (length != 128) && (length != 192) && (length != 256) ) {
        return NS_ERROR_DOM_DATA_ERR;
      }
      algorithm = new AesKeyAlgorithm(global, mAlgName, length);
    } else if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
      if (mKey->HasUsageOtherThan(Key::SIGN | Key::VERIFY)) {
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
    mKey->SetType(Key::SECRET);
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
      ScopedSECKEYPrivateKey privKey(Key::PrivateKeyFromPkcs8(mKeyData, locker));
      if (!privKey.get()) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      mKey->SetPrivateKey(privKey.get());
      mKey->SetType(Key::PRIVATE);
      pubKey = SECKEY_ConvertToPublicKey(privKey.get());
      if (!pubKey) {
        return NS_ERROR_DOM_UNKNOWN_ERR;
      }
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_SPKI)) {
      pubKey = Key::PublicKeyFromSpki(mKeyData, locker);
      if (!pubKey.get()) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      if (pubKey->keyType != rsaKey) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      mKey->SetPublicKey(pubKey.get());
      mKey->SetType(Key::PUBLIC);
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
      if ((mKey->GetKeyType() == Key::PUBLIC &&
           mKey->HasUsageOtherThan(Key::ENCRYPT)) ||
          (mKey->GetKeyType() == Key::PRIVATE &&
           mKey->HasUsageOtherThan(Key::DECRYPT))) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      mKey->SetAlgorithm(new RsaKeyAlgorithm(global, mAlgName, mModulusLength, mPublicExponent));
    } else if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
      if ((mKey->GetKeyType() == Key::PUBLIC &&
           mKey->HasUsageOtherThan(Key::VERIFY)) ||
          (mKey->GetKeyType() == Key::PRIVATE &&
           mKey->HasUsageOtherThan(Key::SIGN))) {
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
  UnifiedExportKeyTask(const nsAString& aFormat, Key& aKey)
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
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_PKCS8)) {
      if (!mPrivateKey) {
        mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }

      switch (mPrivateKey->keyType) {
        case rsaKey:
          Key::PrivateKeyToPkcs8(mPrivateKey.get(), mResult, locker);
          mEarlyRv = NS_OK;
          break;
        default:
          mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_SPKI)) {
      if (!mPublicKey) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }

      mEarlyRv = Key::PublicKeyToSpki(mPublicKey.get(), mResult, locker);
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK)) {
      mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    } else {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
    }

    return NS_OK;
  }
};


// Task creation methods for WebCryptoTask

WebCryptoTask*
WebCryptoTask::EncryptDecryptTask(JSContext* aCx,
                                  const ObjectOrString& aAlgorithm,
                                  Key& aKey,
                                  const CryptoOperationData& aData,
                                  bool aEncrypt)
{
  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  // Ensure key is usable for this operation
  if ((aEncrypt  && !aKey.HasUsage(Key::ENCRYPT)) ||
      (!aEncrypt && !aKey.HasUsage(Key::DECRYPT))) {
    return new FailureTask(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  }

  if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
    return new AesTask(aCx, aAlgorithm, aKey, aData, aEncrypt);
  }

  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::SignVerifyTask(JSContext* aCx,
                              const ObjectOrString& aAlgorithm,
                              Key& aKey,
                              const CryptoOperationData& aSignature,
                              const CryptoOperationData& aData,
                              bool aSign)
{
  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  // Ensure key is usable for this operation
  if ((aSign  && !aKey.HasUsage(Key::SIGN)) ||
      (!aSign && !aKey.HasUsage(Key::VERIFY))) {
    return new FailureTask(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  }

  if (algName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
    return new HmacTask(aCx, aAlgorithm, aKey, aSignature, aData, aSign);
  }

  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::DigestTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          const CryptoOperationData& aData)
{
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
                             Key& aKey)
{
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
  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::DeriveKeyTask(JSContext* aCx,
                             const ObjectOrString& aAlgorithm,
                             Key& aBaseKey,
                             const ObjectOrString& aDerivedKeyType,
                             bool aExtractable,
                             const Sequence<nsString>& aKeyUsages)
{
  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::DeriveBitsTask(JSContext* aCx,
                              const ObjectOrString& aAlgorithm,
                              Key& aKey,
                              uint32_t aLength)
{
  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}


} // namespace dom
} // namespace mozilla
