/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_KeyAlgorithmProxy_h
#define mozilla_dom_KeyAlgorithmProxy_h

#include "pk11pub.h"
#include "js/StructuredClone.h"
#include "mozilla/dom/KeyAlgorithmBinding.h"
#include "mozilla/dom/WebCryptoCommon.h"

#define KEY_ALGORITHM_SC_VERSION 0x00000001

namespace mozilla {
namespace dom {

// A heap-safe variant of RsaHashedKeyAlgorithm
// The only difference is that it uses CryptoBuffer instead of Uint8Array
struct RsaHashedKeyAlgorithmStorage {
  nsString mName;
  KeyAlgorithm mHash;
  uint16_t mModulusLength;
  CryptoBuffer mPublicExponent;

  bool
  ToKeyAlgorithm(JSContext* aCx, RsaHashedKeyAlgorithm& aRsa) const
  {
    JS::Rooted<JSObject*> exponent(aCx, mPublicExponent.ToUint8Array(aCx));
    if (!exponent) {
      return false;
    }

    aRsa.mName = mName;
    aRsa.mModulusLength = mModulusLength;
    aRsa.mHash.mName = mHash.mName;
    aRsa.mPublicExponent.Init(exponent);
    aRsa.mPublicExponent.ComputeLengthAndData();

    return true;
  }
};

// A heap-safe variant of DhKeyAlgorithm
// The only difference is that it uses CryptoBuffers instead of Uint8Arrays
struct DhKeyAlgorithmStorage {
  nsString mName;
  CryptoBuffer mPrime;
  CryptoBuffer mGenerator;

  bool
  ToKeyAlgorithm(JSContext* aCx, DhKeyAlgorithm& aDh) const
  {
    JS::Rooted<JSObject*> prime(aCx, mPrime.ToUint8Array(aCx));
    if (!prime) {
      return false;
    }

    JS::Rooted<JSObject*> generator(aCx, mGenerator.ToUint8Array(aCx));
    if (!generator) {
      return false;
    }

    aDh.mName = mName;
    aDh.mPrime.Init(prime);
    aDh.mPrime.ComputeLengthAndData();
    aDh.mGenerator.Init(generator);
    aDh.mGenerator.ComputeLengthAndData();

    return true;
  }
};

// This class encapuslates a KeyAlgorithm object, and adds several
// methods that make WebCrypto operations simpler.
struct KeyAlgorithmProxy
{
  enum KeyAlgorithmType {
    AES,
    HMAC,
    RSA,
    EC,
    DH,
  };
  KeyAlgorithmType mType;

  // Plain is always populated with the algorithm name
  // Others are only populated for the corresponding key type
  nsString mName;
  AesKeyAlgorithm mAes;
  HmacKeyAlgorithm mHmac;
  RsaHashedKeyAlgorithmStorage mRsa;
  EcKeyAlgorithm mEc;
  DhKeyAlgorithmStorage mDh;

  // Structured clone
  bool WriteStructuredClone(JSStructuredCloneWriter* aWriter) const;
  bool ReadStructuredClone(JSStructuredCloneReader* aReader);

  // Extract various forms of derived information
  CK_MECHANISM_TYPE Mechanism() const;
  nsString JwkAlg() const;

  // And in static form for calling on raw KeyAlgorithm dictionaries
  static CK_MECHANISM_TYPE GetMechanism(const KeyAlgorithm& aAlgorithm);
  static CK_MECHANISM_TYPE GetMechanism(const HmacKeyAlgorithm& aAlgorithm);
  static nsString GetJwkAlg(const KeyAlgorithm& aAlgorithm);

  // Construction of the various algorithm types
  void
  MakeAes(const nsString& aName, uint32_t aLength)
  {
    mType = AES;
    mName = aName;
    mAes.mName = aName;
    mAes.mLength = aLength;
  }

  void
  MakeHmac(uint32_t aLength, const nsString& aHashName)
  {
    mType = HMAC;
    mName = NS_LITERAL_STRING(WEBCRYPTO_ALG_HMAC);
    mHmac.mName = NS_LITERAL_STRING(WEBCRYPTO_ALG_HMAC);
    mHmac.mLength = aLength;
    mHmac.mHash.mName = aHashName;
  }

  bool
  MakeRsa(const nsString& aName, uint32_t aModulusLength,
         const CryptoBuffer& aPublicExponent, const nsString& aHashName)
  {
    mType = RSA;
    mName = aName;
    mRsa.mName = aName;
    mRsa.mModulusLength = aModulusLength;
    mRsa.mHash.mName = aHashName;
    if (!mRsa.mPublicExponent.Assign(aPublicExponent)) {
      return false;
    }
    return true;
  }

  void
  MakeEc(const nsString& aName, const nsString& aNamedCurve)
  {
    mType = EC;
    mName = aName;
    mEc.mName = aName;
    mEc.mNamedCurve = aNamedCurve;
  }

  bool
  MakeDh(const nsString& aName, const CryptoBuffer& aPrime,
         const CryptoBuffer& aGenerator)
  {
    mType = DH;
    mName = aName;
    mDh.mName = aName;
    if (!mDh.mPrime.Assign(aPrime)) {
      return false;
    }
    if (!mDh.mGenerator.Assign(aGenerator)) {
      return false;
    }
    return true;
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_KeyAlgorithmProxy_h
