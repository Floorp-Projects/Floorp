/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CryptoKey_h
#define mozilla_dom_CryptoKey_h

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsIGlobalObject.h"
#include "nsNSSShutDown.h"
#include "pk11pub.h"
#include "keyhi.h"
#include "ScopedNSSTypes.h"
#include "mozilla/dom/KeyAlgorithm.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "js/StructuredClone.h"
#include "js/TypeDecls.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

/*

The internal structure of keys is dictated by the need for cloning.
We store everything besides the key data itself in a 32-bit bitmask,
with the following structure (byte-aligned for simplicity, in order
from least to most significant):

Bits  Usage
0     Extractable
1-7   [reserved]
8-15  KeyType
16-23 KeyUsage
24-31 [reserved]

In the order of a hex value for a uint32_t

   3                   2                   1                   0
 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|~~~~~~~~~~~~~~~|     Usage     |     Type      |~~~~~~~~~~~~~|E|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Thus, internally, a key has the following fields:
* uint32_t - flags for extractable, usage, type
* KeyAlgorithm - the algorithm (which must serialize/deserialize itself)
* The actual keys (which the CryptoKey must serialize)

*/

struct JsonWebKey;

class CryptoKey MOZ_FINAL : public nsISupports,
                            public nsWrapperCache,
                            public nsNSSShutDownObject
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CryptoKey)

  static const uint32_t CLEAR_EXTRACTABLE = 0xFFFFFFE;
  static const uint32_t EXTRACTABLE = 0x00000001;

  static const uint32_t CLEAR_TYPE = 0xFFFF00FF;
  static const uint32_t TYPE_MASK = 0x0000FF00;
  enum KeyType {
    UNKNOWN = 0x00000000,
    SECRET  = 0x00000100,
    PUBLIC  = 0x00000200,
    PRIVATE = 0x00000300
  };

  static const uint32_t CLEAR_USAGES = 0xFF00FFFF;
  static const uint32_t USAGES_MASK = 0x00FF0000;
  enum KeyUsage {
    ENCRYPT    = 0x00010000,
    DECRYPT    = 0x00020000,
    SIGN       = 0x00040000,
    VERIFY     = 0x00080000,
    DERIVEKEY  = 0x00100000,
    DERIVEBITS = 0x00200000,
    WRAPKEY    = 0x00400000,
    UNWRAPKEY  = 0x00800000
  };

  explicit CryptoKey(nsIGlobalObject* aWindow);

  nsIGlobalObject* GetParentObject() const
  {
    return mGlobal;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL methods
  void GetType(nsString& aRetVal) const;
  bool Extractable() const;
  KeyAlgorithm* Algorithm() const;
  void GetUsages(nsTArray<nsString>& aRetVal) const;

  // The below methods are not exposed to JS, but C++ can use
  // them to manipulate the object

  KeyType GetKeyType() const;
  nsresult SetType(const nsString& aType);
  void SetType(KeyType aType);
  void SetExtractable(bool aExtractable);
  void SetAlgorithm(KeyAlgorithm* aAlgorithm);
  void ClearUsages();
  nsresult AddUsage(const nsString& aUsage);
  nsresult AddUsageIntersecting(const nsString& aUsage, uint32_t aUsageMask);
  void AddUsage(KeyUsage aUsage);
  bool HasUsage(KeyUsage aUsage);
  bool HasUsageOtherThan(uint32_t aUsages);

  void SetSymKey(const CryptoBuffer& aSymKey);
  void SetPrivateKey(SECKEYPrivateKey* aPrivateKey);
  void SetPublicKey(SECKEYPublicKey* aPublicKey);

  // Accessors for the keys themselves
  // Note: GetPrivateKey and GetPublicKey return copies of the internal
  // key handles, which the caller must free with SECKEY_DestroyPrivateKey
  // or SECKEY_DestroyPublicKey.
  const CryptoBuffer& GetSymKey() const;
  SECKEYPrivateKey* GetPrivateKey() const;
  SECKEYPublicKey* GetPublicKey() const;

  // For nsNSSShutDownObject
  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();

  // Serialization and deserialization convenience methods
  // Note:
  // 1. The inputs aKeyData are non-const only because the NSS import
  //    functions lack the const modifier.  They should not be modified.
  // 2. All of the NSS key objects returned need to be freed by the caller.
  static SECKEYPrivateKey* PrivateKeyFromPkcs8(CryptoBuffer& aKeyData,
                                               const nsNSSShutDownPreventionLock& /*proofOfLock*/);
  static nsresult PrivateKeyToPkcs8(SECKEYPrivateKey* aPrivKey,
                                    CryptoBuffer& aRetVal,
                                    const nsNSSShutDownPreventionLock& /*proofOfLock*/);

  static SECKEYPublicKey* PublicKeyFromSpki(CryptoBuffer& aKeyData,
                                            const nsNSSShutDownPreventionLock& /*proofOfLock*/);
  static nsresult PublicKeyToSpki(SECKEYPublicKey* aPrivKey,
                                  CryptoBuffer& aRetVal,
                                  const nsNSSShutDownPreventionLock& /*proofOfLock*/);

  static SECKEYPrivateKey* PrivateKeyFromJwk(const JsonWebKey& aJwk,
                                             const nsNSSShutDownPreventionLock& /*proofOfLock*/);
  static nsresult PrivateKeyToJwk(SECKEYPrivateKey* aPrivKey,
                                  JsonWebKey& aRetVal,
                                  const nsNSSShutDownPreventionLock& /*proofOfLock*/);

  static SECKEYPublicKey* PublicKeyFromJwk(const JsonWebKey& aKeyData,
                                           const nsNSSShutDownPreventionLock& /*proofOfLock*/);
  static nsresult PublicKeyToJwk(SECKEYPublicKey* aPrivKey,
                                 JsonWebKey& aRetVal,
                                 const nsNSSShutDownPreventionLock& /*proofOfLock*/);

  // Structured clone methods use these to clone keys
  bool WriteStructuredClone(JSStructuredCloneWriter* aWriter) const;
  bool ReadStructuredClone(JSStructuredCloneReader* aReader);

private:
  ~CryptoKey();

  nsRefPtr<nsIGlobalObject> mGlobal;
  uint32_t mAttributes; // see above
  nsRefPtr<KeyAlgorithm> mAlgorithm;

  // Only one key handle should be set, according to the KeyType
  CryptoBuffer mSymKey;
  ScopedSECKEYPrivateKey mPrivateKey;
  ScopedSECKEYPublicKey mPublicKey;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CryptoKey_h
