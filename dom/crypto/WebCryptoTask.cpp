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

class SimpleDigestTask : public ReturnArrayBufferViewTask
{
public:
  SimpleDigestTask(JSContext* aCx,
                   const ObjectOrString& aAlgorithm,
                   const CryptoOperationData& aData)
  {
    if (!mData.Assign(aData)) {
      mEarlyRv = NS_ERROR_DOM_UNKNOWN_ERR;
      return;
    }

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
