/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebCryptoTask_h
#define mozilla_dom_WebCryptoTask_h

#include "CryptoTask.h"

#include "nsIGlobalObject.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/CryptoKey.h"

namespace mozilla {
namespace dom {

typedef ArrayBufferViewOrArrayBuffer CryptoOperationData;
typedef ArrayBufferViewOrArrayBuffer KeyData;

/*

The execution of a WebCryptoTask happens in several phases

1. Constructor
2. BeforeCrypto
3. CalculateResult -> DoCrypto
4. AfterCrypto
5. Resolve or FailWithError
6. Cleanup

If any of these steps produces an error (setting mEarlyRv), then
subsequent steps will not proceed.  If the constructor or BeforeCrypto
sets mEarlyComplete to true, then we will skip step 3, saving the
thread overhead.

In general, the constructor should handle any parsing steps that
require JS context, and otherwise just cache information for later
steps to use.

All steps besides step 3 occur on the main thread, so they should
avoid blocking operations.

Only step 3 is guarded to ensure that NSS has not been shutdown,
so all NSS interactions should occur in DoCrypto

Cleanup should execute regardless of what else happens.

*/

#define MAYBE_EARLY_FAIL(rv) \
if (NS_FAILED(rv)) { \
  FailWithError(rv); \
  Skip(); \
  return; \
}

class WebCryptoTask : public CryptoTask
{
public:
  virtual void DispatchWithPromise(Promise* aResultPromise)
  {
    MOZ_ASSERT(NS_IsMainThread());
    mResultPromise = aResultPromise;

    // Fail if an error was set during the constructor
    MAYBE_EARLY_FAIL(mEarlyRv)

    // Perform pre-NSS operations, and fail if they fail
    mEarlyRv = BeforeCrypto();
    MAYBE_EARLY_FAIL(mEarlyRv)

    // Skip NSS if we're already done, or launch a CryptoTask
    if (mEarlyComplete) {
      CallCallback(mEarlyRv);
      Skip();
      return;
    }

     mEarlyRv = Dispatch("SubtleCrypto");
     MAYBE_EARLY_FAIL(mEarlyRv)
  }

protected:
  static WebCryptoTask* EncryptDecryptTask(JSContext* aCx,
                           const ObjectOrString& aAlgorithm,
                           CryptoKey& aKey,
                           const CryptoOperationData& aData,
                           bool aEncrypt);

  static WebCryptoTask* SignVerifyTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          CryptoKey& aKey,
                          const CryptoOperationData& aSignature,
                          const CryptoOperationData& aData,
                          bool aSign);

public:
  static WebCryptoTask* EncryptTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          CryptoKey& aKey,
                          const CryptoOperationData& aData)
  {
    return EncryptDecryptTask(aCx, aAlgorithm, aKey, aData, true);
  }

  static WebCryptoTask* DecryptTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          CryptoKey& aKey,
                          const CryptoOperationData& aData)
  {
    return EncryptDecryptTask(aCx, aAlgorithm, aKey, aData, false);
  }

  static WebCryptoTask* SignTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          CryptoKey& aKey,
                          const CryptoOperationData& aData)
  {
    CryptoOperationData dummy;
    dummy.SetAsArrayBuffer(aCx);
    return SignVerifyTask(aCx, aAlgorithm, aKey, dummy, aData, true);
  }

  static WebCryptoTask* VerifyTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          CryptoKey& aKey,
                          const CryptoOperationData& aSignature,
                          const CryptoOperationData& aData)
  {
    return SignVerifyTask(aCx, aAlgorithm, aKey, aSignature, aData, false);
  }

  static WebCryptoTask* DigestTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          const CryptoOperationData& aData);

  static WebCryptoTask* ImportKeyTask(JSContext* aCx,
                          const nsAString& aFormat,
                          const KeyData& aKeyData,
                          const ObjectOrString& aAlgorithm,
                          bool aExtractable,
                          const Sequence<nsString>& aKeyUsages);
  static WebCryptoTask* ExportKeyTask(const nsAString& aFormat,
                          CryptoKey& aKey);
  static WebCryptoTask* GenerateKeyTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          bool aExtractable,
                          const Sequence<nsString>& aKeyUsages);

  static WebCryptoTask* DeriveKeyTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          CryptoKey& aBaseKey,
                          const ObjectOrString& aDerivedKeyType,
                          bool extractable,
                          const Sequence<nsString>& aKeyUsages);
  static WebCryptoTask* DeriveBitsTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          CryptoKey& aKey,
                          uint32_t aLength);

protected:
  nsRefPtr<Promise> mResultPromise;
  nsresult mEarlyRv;
  bool mEarlyComplete;

  WebCryptoTask()
    : mEarlyRv(NS_OK)
    , mEarlyComplete(false)
  {}

  // For things that need to happen on the main thread
  // either before or after CalculateResult
  virtual nsresult BeforeCrypto() { return NS_OK; }
  virtual nsresult DoCrypto() { return NS_OK; }
  virtual nsresult AfterCrypto() { return NS_OK; }
  virtual void Resolve() {}
  virtual void Cleanup() {}

  void FailWithError(nsresult aRv);

  // Subclasses should override this method if they keep references to
  // any NSS objects, e.g., SECKEYPrivateKey or PK11SymKey.
  virtual void ReleaseNSSResources() MOZ_OVERRIDE {}

  virtual nsresult CalculateResult() MOZ_OVERRIDE MOZ_FINAL;

  virtual void CallCallback(nsresult rv) MOZ_OVERRIDE MOZ_FINAL;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebCryptoTask_h
