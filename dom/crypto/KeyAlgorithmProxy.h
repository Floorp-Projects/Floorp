/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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

  void
  ToKeyAlgorithm(JSContext* aCx, RsaHashedKeyAlgorithm& aRsa) const
  {
    aRsa.mName = mName;
    aRsa.mModulusLength = mModulusLength;
    aRsa.mHash.mName = mHash.mName;
    aRsa.mPublicExponent.Init(mPublicExponent.ToUint8Array(aCx));
    aRsa.mPublicExponent.ComputeLengthAndData();
  }
};

// A heap-safe variant of DhKeyAlgorithm
// The only difference is that it uses CryptoBuffers instead of Uint8Arrays
struct DhKeyAlgorithmStorage {
  nsString mName;
  CryptoBuffer mPrime;
  CryptoBuffer mGenerator;

  void
  ToKeyAlgorithm(JSContext* aCx, DhKeyAlgorithm& aDh) const
  {
    aDh.mName = mName;
    aDh.mPrime.Init(mPrime.ToUint8Array(aCx));
    aDh.mPrime.ComputeLengthAndData();
    aDh.mGenerator.Init(mGenerator.ToUint8Array(aCx));
    aDh.mGenerator.ComputeLengthAndData();
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

  void
  MakeRsa(const nsString& aName, uint32_t aModulusLength,
         const CryptoBuffer& aPublicExponent, const nsString& aHashName)
  {
    mType = RSA;
    mName = aName;
    mRsa.mName = aName;
    mRsa.mModulusLength = aModulusLength;
    mRsa.mHash.mName = aHashName;
    mRsa.mPublicExponent.Assign(aPublicExponent);
  }

  void
  MakeEc(const nsString& aName, const nsString& aNamedCurve)
  {
    mType = EC;
    mName = aName;
    mEc.mName = aName;
    mEc.mNamedCurve = aNamedCurve;
  }

  void
  MakeDh(const nsString& aName, const CryptoBuffer& aPrime,
         const CryptoBuffer& aGenerator)
  {
    mType = DH;
    mName = aName;
    mDh.mName = aName;
    mDh.mPrime.Assign(aPrime);
    mDh.mGenerator.Assign(aGenerator);
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_KeyAlgorithmProxy_h
