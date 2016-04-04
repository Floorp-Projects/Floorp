/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentVerifier_h
#define mozilla_dom_ContentVerifier_h

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIStreamListener.h"
#include "nsNSSShutDown.h"
#include "nsString.h"
#include "nsTArray.h"
#include "ScopedNSSTypes.h"

/**
 * Mediator intercepting OnStartRequest in nsHttpChannel, blocks until all
 * data is read from the input stream, verifies the content signature and
 * releases the request to the next listener if the verification is successful.
 * If the verification fails or anything else goes wrong, a
 * NS_ERROR_INVALID_SIGNATURE is thrown.
 */
class ContentVerifier : public nsIStreamListener
                      , public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  explicit ContentVerifier(nsIStreamListener* aMediatedListener,
                           nsISupports* aMediatedContext)
    : mNextListener(aMediatedListener)
    , mContext(aMediatedContext)
    , mCx(nullptr) {}

  nsresult Init(const nsAString& aContentSignatureHeader);

  // nsNSSShutDownObject
  virtual void virtualDestroyNSSReference() override
  {
    destructorSafeDestroyNSSReference();
  }

protected:
  virtual ~ContentVerifier()
  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      return;
    }
    destructorSafeDestroyNSSReference();
    shutdown(calledFromObject);
  }

  void destructorSafeDestroyNSSReference()
  {
    mCx = nullptr;
  }

private:
  nsresult ParseContentSignatureHeader(const nsAString& aContentSignatureHeader);
  nsresult GetVerificationKey(const nsAString& aKeyId);

  // utility function to parse input before put into verification functions
  nsresult ParseInput(mozilla::ScopedSECKEYPublicKey& aPublicKeyOut,
                      mozilla::ScopedSECItem& aSignatureItemOut,
                      SECOidTag& aOidOut,
                      const nsNSSShutDownPreventionLock&);

  // create a verifier context and store it in mCx
  nsresult CreateContext();

  // Adds data to the context that was used to generate the signature.
  nsresult Update(const nsACString& aData);

  // Finalises the signature and returns the result of the signature
  // verification.
  nsresult End(bool* _retval);

  // content and next listener for nsIStreamListener
  nsCOMPtr<nsIStreamListener> mNextListener;
  nsCOMPtr<nsISupports> mContext;

  // verifier context for incrementel verifications
  mozilla::UniqueVFYContext mCx;
  // buffered content to verify
  FallibleTArray<nsCString> mContent;
  // signature to verify
  nsCString mSignature;
  // verification key
  nsCString mKey;
  // verification key preference
  nsString mVks;
};

#endif /* mozilla_dom_ContentVerifier_h */
