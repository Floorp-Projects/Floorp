/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CryptoKey_h
#define mozilla_dom_CryptoKey_h

#include <cstdint>
#include "ErrorList.h"
#include "ScopedNSSTypes.h"
#include "js/RootingAPI.h"
#include "keythi.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/KeyAlgorithmProxy.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"
#include "nsWrapperCache.h"

#define CRYPTOKEY_SC_VERSION 0x00000001

class JSObject;
class nsIGlobalObject;
struct JSContext;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;

namespace mozilla {

class ErrorResult;

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

class CryptoKey final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(CryptoKey)

  static const uint32_t CLEAR_EXTRACTABLE = 0xFFFFFFE;
  static const uint32_t EXTRACTABLE = 0x00000001;

  static const uint32_t CLEAR_TYPE = 0xFFFF00FF;
  static const uint32_t TYPE_MASK = 0x0000FF00;
  enum KeyType {
    UNKNOWN = 0x00000000,
    SECRET = 0x00000100,
    PUBLIC = 0x00000200,
    PRIVATE = 0x00000300
  };

  static const uint32_t CLEAR_USAGES = 0xFF00FFFF;
  static const uint32_t USAGES_MASK = 0x00FF0000;
  enum KeyUsage {
    ENCRYPT = 0x00010000,
    DECRYPT = 0x00020000,
    SIGN = 0x00040000,
    VERIFY = 0x00080000,
    DERIVEKEY = 0x00100000,
    DERIVEBITS = 0x00200000,
    WRAPKEY = 0x00400000,
    UNWRAPKEY = 0x00800000
  };

  explicit CryptoKey(nsIGlobalObject* aWindow);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL methods
  void GetType(nsString& aRetVal) const;
  bool Extractable() const;
  void GetAlgorithm(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal,
                    ErrorResult& aRv) const;
  void GetUsages(nsTArray<nsString>& aRetVal) const;

  // The below methods are not exposed to JS, but C++ can use
  // them to manipulate the object

  KeyAlgorithmProxy& Algorithm();
  const KeyAlgorithmProxy& Algorithm() const;
  KeyType GetKeyType() const;
  nsresult SetType(const nsString& aType);
  void SetType(KeyType aType);
  void SetExtractable(bool aExtractable);
  nsresult AddPublicKeyData(SECKEYPublicKey* point);
  void ClearUsages();
  nsresult AddUsage(const nsString& aUsage);
  nsresult AddAllowedUsage(const nsString& aUsage, const nsString& aAlgorithm);
  nsresult AddAllowedUsageIntersecting(const nsString& aUsage,
                                       const nsString& aAlgorithm,
                                       uint32_t aUsageMask = USAGES_MASK);
  void AddUsage(KeyUsage aUsage);
  bool HasAnyUsage();
  bool HasUsage(KeyUsage aUsage);
  bool HasUsageOtherThan(uint32_t aUsages);
  static bool IsRecognizedUsage(const nsString& aUsage);
  static bool AllUsagesRecognized(const Sequence<nsString>& aUsages);
  static uint32_t GetAllowedUsagesForAlgorithm(const nsString& aAlgorithm);

  nsresult SetSymKey(const CryptoBuffer& aSymKey);
  nsresult SetPrivateKey(SECKEYPrivateKey* aPrivateKey);
  nsresult SetPublicKey(SECKEYPublicKey* aPublicKey);

  // Accessors for the keys themselves
  const CryptoBuffer& GetSymKey() const;
  UniqueSECKEYPrivateKey GetPrivateKey() const;
  UniqueSECKEYPublicKey GetPublicKey() const;

  // Serialization and deserialization convenience methods
  // Note:
  // 1. The inputs aKeyData are non-const only because the NSS import
  //    functions lack the const modifier.  They should not be modified.
  // 2. All of the NSS key objects returned need to be freed by the caller.
  static UniqueSECKEYPrivateKey PrivateKeyFromPkcs8(CryptoBuffer& aKeyData);
  static nsresult PrivateKeyToPkcs8(SECKEYPrivateKey* aPrivKey,
                                    CryptoBuffer& aRetVal);

  static UniqueSECKEYPublicKey PublicKeyFromSpki(CryptoBuffer& aKeyData);
  static nsresult PublicKeyToSpki(SECKEYPublicKey* aPubKey,
                                  CryptoBuffer& aRetVal);

  static UniqueSECKEYPrivateKey PrivateKeyFromJwk(const JsonWebKey& aJwk);
  static nsresult PrivateKeyToJwk(SECKEYPrivateKey* aPrivKey,
                                  JsonWebKey& aRetVal);

  static UniqueSECKEYPublicKey PublicKeyFromJwk(const JsonWebKey& aKeyData);
  static nsresult PublicKeyToJwk(SECKEYPublicKey* aPubKey, JsonWebKey& aRetVal);

  static UniqueSECKEYPublicKey PublicECKeyFromRaw(CryptoBuffer& aKeyData,
                                                  const nsString& aNamedCurve);
  static nsresult PublicECKeyToRaw(SECKEYPublicKey* aPubKey,
                                   CryptoBuffer& aRetVal);

  static bool PublicKeyValid(SECKEYPublicKey* aPubKey);

  // Structured clone methods use these to clone keys
  bool WriteStructuredClone(JSContext* aCx,
                            JSStructuredCloneWriter* aWriter) const;
  static already_AddRefed<CryptoKey> ReadStructuredClone(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);

 private:
  ~CryptoKey() = default;

  RefPtr<nsIGlobalObject> mGlobal;
  uint32_t mAttributes;  // see above
  KeyAlgorithmProxy mAlgorithm;

  // Only one key handle should be set, according to the KeyType
  CryptoBuffer mSymKey;
  UniqueSECKEYPrivateKey mPrivateKey;
  UniqueSECKEYPublicKey mPublicKey;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CryptoKey_h
