/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_KeyAlgorithmProxy_h
#define mozilla_dom_KeyAlgorithmProxy_h

#include <cstdint>
#include <utility>
#include "js/RootingAPI.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/KeyAlgorithmBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "nsLiteralString.h"
#include "nsStringFwd.h"
#include "pkcs11t.h"

class JSObject;
struct JSContext;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;

#define KEY_ALGORITHM_SC_VERSION 0x00000001

namespace mozilla::dom {

// A heap-safe variant of RsaHashedKeyAlgorithm
// The only difference is that it uses CryptoBuffer instead of Uint8Array
struct RsaHashedKeyAlgorithmStorage {
  nsString mName;
  KeyAlgorithm mHash;
  uint16_t mModulusLength;
  CryptoBuffer mPublicExponent;

  bool ToKeyAlgorithm(JSContext* aCx, RsaHashedKeyAlgorithm& aRsa,
                      ErrorResult& aError) const {
    JS::Rooted<JSObject*> exponent(aCx,
                                   mPublicExponent.ToUint8Array(aCx, aError));
    if (aError.Failed()) {
      return false;
    }

    aRsa.mName = mName;
    aRsa.mModulusLength = mModulusLength;
    aRsa.mHash.mName = mHash.mName;
    aRsa.mPublicExponent.Init(exponent);

    return true;
  }
};

// This class encapuslates a KeyAlgorithm object, and adds several
// methods that make WebCrypto operations simpler.
struct KeyAlgorithmProxy {
  enum KeyAlgorithmType {
    AES,
    HMAC,
    RSA,
    EC,
  };
  KeyAlgorithmType mType;

  // Plain is always populated with the algorithm name
  // Others are only populated for the corresponding key type
  nsString mName;
  AesKeyAlgorithm mAes;
  HmacKeyAlgorithm mHmac;
  RsaHashedKeyAlgorithmStorage mRsa;
  EcKeyAlgorithm mEc;

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
  void MakeAes(const nsString& aName, uint32_t aLength) {
    mType = AES;
    mName = aName;
    mAes.mName = aName;
    mAes.mLength = aLength;
  }

  void MakeHmac(uint32_t aLength, const nsString& aHashName) {
    mType = HMAC;
    mName = NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_ALG_HMAC);
    mHmac.mName = NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_ALG_HMAC);
    mHmac.mLength = aLength;
    mHmac.mHash.mName = aHashName;
  }

  bool MakeRsa(const nsString& aName, uint32_t aModulusLength,
               const CryptoBuffer& aPublicExponent, const nsString& aHashName) {
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

  void MakeEc(const nsString& aName, const nsString& aNamedCurve) {
    mType = EC;
    mName = aName;
    mEc.mName = aName;
    mEc.mNamedCurve = aNamedCurve;
  }
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_KeyAlgorithmProxy_h
