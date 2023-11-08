/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CryptoKey.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <utility>
#include "blapit.h"
#include "certt.h"
#include "js/StructuredClone.h"
#include "js/TypeDecls.h"
#include "keyhi.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/dom/KeyAlgorithmBinding.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsLiteralString.h"
#include "nsNSSComponent.h"
#include "nsStringFlags.h"
#include "nsTArray.h"
#include "pk11pub.h"
#include "pkcs11t.h"
#include "plarena.h"
#include "prtypes.h"
#include "secasn1.h"
#include "secasn1t.h"
#include "seccomon.h"
#include "secdert.h"
#include "secitem.h"
#include "secmodt.h"
#include "secoid.h"
#include "secoidt.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CryptoKey, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(CryptoKey)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CryptoKey)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CryptoKey)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsresult StringToUsage(const nsString& aUsage, CryptoKey::KeyUsage& aUsageOut) {
  if (aUsage.EqualsLiteral(WEBCRYPTO_KEY_USAGE_ENCRYPT)) {
    aUsageOut = CryptoKey::ENCRYPT;
  } else if (aUsage.EqualsLiteral(WEBCRYPTO_KEY_USAGE_DECRYPT)) {
    aUsageOut = CryptoKey::DECRYPT;
  } else if (aUsage.EqualsLiteral(WEBCRYPTO_KEY_USAGE_SIGN)) {
    aUsageOut = CryptoKey::SIGN;
  } else if (aUsage.EqualsLiteral(WEBCRYPTO_KEY_USAGE_VERIFY)) {
    aUsageOut = CryptoKey::VERIFY;
  } else if (aUsage.EqualsLiteral(WEBCRYPTO_KEY_USAGE_DERIVEKEY)) {
    aUsageOut = CryptoKey::DERIVEKEY;
  } else if (aUsage.EqualsLiteral(WEBCRYPTO_KEY_USAGE_DERIVEBITS)) {
    aUsageOut = CryptoKey::DERIVEBITS;
  } else if (aUsage.EqualsLiteral(WEBCRYPTO_KEY_USAGE_WRAPKEY)) {
    aUsageOut = CryptoKey::WRAPKEY;
  } else if (aUsage.EqualsLiteral(WEBCRYPTO_KEY_USAGE_UNWRAPKEY)) {
    aUsageOut = CryptoKey::UNWRAPKEY;
  } else {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  return NS_OK;
}

// This helper function will release the memory backing a SECKEYPrivateKey and
// any resources acquired in its creation. It will leave the backing PKCS#11
// object untouched, however. This should only be called from
// PrivateKeyFromPrivateKeyTemplate.
static void DestroyPrivateKeyWithoutDestroyingPKCS11Object(
    SECKEYPrivateKey* key) {
  PK11_FreeSlot(key->pkcs11Slot);
  PORT_FreeArena(key->arena, PR_TRUE);
}

// To protect against key ID collisions, PrivateKeyFromPrivateKeyTemplate
// generates a random ID for each key. The given template must contain an
// attribute slot for a key ID, but it must consist of a null pointer and have a
// length of 0.
UniqueSECKEYPrivateKey PrivateKeyFromPrivateKeyTemplate(
    CK_ATTRIBUTE* aTemplate, CK_ULONG aTemplateSize) {
  // Create a generic object with the contents of the key
  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return nullptr;
  }

  // Generate a random 160-bit object ID. This ID must be unique.
  UniqueSECItem objID(::SECITEM_AllocItem(nullptr, nullptr, 20));
  SECStatus rv = PK11_GenerateRandomOnSlot(slot.get(), objID->data, objID->len);
  if (rv != SECSuccess) {
    return nullptr;
  }
  // Check if something is already using this ID.
  SECKEYPrivateKey* preexistingKey =
      PK11_FindKeyByKeyID(slot.get(), objID.get(), nullptr);
  if (preexistingKey) {
    // Note that we can't just call SECKEY_DestroyPrivateKey here because that
    // will destroy the PKCS#11 object that is backing a preexisting key (that
    // we still have a handle on somewhere else in memory). If that object were
    // destroyed, cryptographic operations performed by that other key would
    // fail.
    DestroyPrivateKeyWithoutDestroyingPKCS11Object(preexistingKey);
    // Try again with a new ID (but only once - collisions are very unlikely).
    rv = PK11_GenerateRandomOnSlot(slot.get(), objID->data, objID->len);
    if (rv != SECSuccess) {
      return nullptr;
    }
    preexistingKey = PK11_FindKeyByKeyID(slot.get(), objID.get(), nullptr);
    if (preexistingKey) {
      DestroyPrivateKeyWithoutDestroyingPKCS11Object(preexistingKey);
      return nullptr;
    }
  }

  CK_ATTRIBUTE* idAttributeSlot = nullptr;
  for (CK_ULONG i = 0; i < aTemplateSize; i++) {
    if (aTemplate[i].type == CKA_ID) {
      if (aTemplate[i].pValue != nullptr || aTemplate[i].ulValueLen != 0) {
        return nullptr;
      }
      idAttributeSlot = aTemplate + i;
      break;
    }
  }
  if (!idAttributeSlot) {
    return nullptr;
  }

  idAttributeSlot->pValue = objID->data;
  idAttributeSlot->ulValueLen = objID->len;
  UniquePK11GenericObject obj(
      PK11_CreateGenericObject(slot.get(), aTemplate, aTemplateSize, PR_FALSE));
  // Unset the ID attribute slot's pointer and length so that data that only
  // lives for the scope of this function doesn't escape.
  idAttributeSlot->pValue = nullptr;
  idAttributeSlot->ulValueLen = 0;
  if (!obj) {
    return nullptr;
  }

  // Have NSS translate the object to a private key.
  return UniqueSECKEYPrivateKey(
      PK11_FindKeyByKeyID(slot.get(), objID.get(), nullptr));
}

CryptoKey::CryptoKey(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal),
      mAttributes(0),
      mSymKey(),
      mPrivateKey(nullptr),
      mPublicKey(nullptr) {}

JSObject* CryptoKey::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return CryptoKey_Binding::Wrap(aCx, this, aGivenProto);
}

void CryptoKey::GetType(nsString& aRetVal) const {
  uint32_t type = mAttributes & TYPE_MASK;
  switch (type) {
    case PUBLIC:
      aRetVal.AssignLiteral(WEBCRYPTO_KEY_TYPE_PUBLIC);
      break;
    case PRIVATE:
      aRetVal.AssignLiteral(WEBCRYPTO_KEY_TYPE_PRIVATE);
      break;
    case SECRET:
      aRetVal.AssignLiteral(WEBCRYPTO_KEY_TYPE_SECRET);
      break;
  }
}

bool CryptoKey::Extractable() const { return (mAttributes & EXTRACTABLE); }

void CryptoKey::GetAlgorithm(JSContext* cx,
                             JS::MutableHandle<JSObject*> aRetVal,
                             ErrorResult& aRv) const {
  bool converted = false;
  JS::Rooted<JS::Value> val(cx);
  switch (mAlgorithm.mType) {
    case KeyAlgorithmProxy::AES:
      converted = ToJSValue(cx, mAlgorithm.mAes, &val);
      break;
    case KeyAlgorithmProxy::HMAC:
      converted = ToJSValue(cx, mAlgorithm.mHmac, &val);
      break;
    case KeyAlgorithmProxy::RSA: {
      RootedDictionary<RsaHashedKeyAlgorithm> rsa(cx);
      converted = mAlgorithm.mRsa.ToKeyAlgorithm(cx, rsa);
      if (converted) {
        converted = ToJSValue(cx, rsa, &val);
      }
      break;
    }
    case KeyAlgorithmProxy::EC:
      converted = ToJSValue(cx, mAlgorithm.mEc, &val);
      break;
  }
  if (!converted) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  aRetVal.set(&val.toObject());
}

void CryptoKey::GetUsages(nsTArray<nsString>& aRetVal) const {
  if (mAttributes & ENCRYPT) {
    aRetVal.AppendElement(
        NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_KEY_USAGE_ENCRYPT));
  }
  if (mAttributes & DECRYPT) {
    aRetVal.AppendElement(
        NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_KEY_USAGE_DECRYPT));
  }
  if (mAttributes & SIGN) {
    aRetVal.AppendElement(
        NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_KEY_USAGE_SIGN));
  }
  if (mAttributes & VERIFY) {
    aRetVal.AppendElement(
        NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_KEY_USAGE_VERIFY));
  }
  if (mAttributes & DERIVEKEY) {
    aRetVal.AppendElement(
        NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_KEY_USAGE_DERIVEKEY));
  }
  if (mAttributes & DERIVEBITS) {
    aRetVal.AppendElement(
        NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_KEY_USAGE_DERIVEBITS));
  }
  if (mAttributes & WRAPKEY) {
    aRetVal.AppendElement(
        NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_KEY_USAGE_WRAPKEY));
  }
  if (mAttributes & UNWRAPKEY) {
    aRetVal.AppendElement(
        NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_KEY_USAGE_UNWRAPKEY));
  }
}

KeyAlgorithmProxy& CryptoKey::Algorithm() { return mAlgorithm; }

const KeyAlgorithmProxy& CryptoKey::Algorithm() const { return mAlgorithm; }

CryptoKey::KeyType CryptoKey::GetKeyType() const {
  return static_cast<CryptoKey::KeyType>(mAttributes & TYPE_MASK);
}

nsresult CryptoKey::SetType(const nsString& aType) {
  mAttributes &= CLEAR_TYPE;
  if (aType.EqualsLiteral(WEBCRYPTO_KEY_TYPE_SECRET)) {
    mAttributes |= SECRET;
  } else if (aType.EqualsLiteral(WEBCRYPTO_KEY_TYPE_PUBLIC)) {
    mAttributes |= PUBLIC;
  } else if (aType.EqualsLiteral(WEBCRYPTO_KEY_TYPE_PRIVATE)) {
    mAttributes |= PRIVATE;
  } else {
    mAttributes |= UNKNOWN;
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  return NS_OK;
}

void CryptoKey::SetType(CryptoKey::KeyType aType) {
  mAttributes &= CLEAR_TYPE;
  mAttributes |= aType;
}

void CryptoKey::SetExtractable(bool aExtractable) {
  mAttributes &= CLEAR_EXTRACTABLE;
  if (aExtractable) {
    mAttributes |= EXTRACTABLE;
  }
}

// NSS exports private EC keys without the CKA_EC_POINT attribute, i.e. the
// public value. To properly export the private key to JWK or PKCS #8 we need
// the public key data though and so we use this method to augment a private
// key with data from the given public key.
nsresult CryptoKey::AddPublicKeyData(SECKEYPublicKey* aPublicKey) {
  // This should be a private key.
  MOZ_ASSERT(GetKeyType() == PRIVATE);
  // There should be a private NSS key with type 'EC'.
  MOZ_ASSERT(mPrivateKey && mPrivateKey->keyType == ecKey);
  // The given public key should have the same key type.
  MOZ_ASSERT(aPublicKey->keyType == mPrivateKey->keyType);

  // Read EC params.
  ScopedAutoSECItem params;
  SECStatus rv = PK11_ReadRawAttribute(PK11_TypePrivKey, mPrivateKey.get(),
                                       CKA_EC_PARAMS, &params);
  if (rv != SECSuccess) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  // Read private value.
  ScopedAutoSECItem value;
  rv = PK11_ReadRawAttribute(PK11_TypePrivKey, mPrivateKey.get(), CKA_VALUE,
                             &value);
  if (rv != SECSuccess) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  SECItem* point = &aPublicKey->u.ec.publicValue;
  CK_OBJECT_CLASS privateKeyValue = CKO_PRIVATE_KEY;
  CK_BBOOL falseValue = CK_FALSE;
  CK_KEY_TYPE ecValue = CKK_EC;

  CK_ATTRIBUTE keyTemplate[9] = {
      {CKA_CLASS, &privateKeyValue, sizeof(privateKeyValue)},
      {CKA_KEY_TYPE, &ecValue, sizeof(ecValue)},
      {CKA_TOKEN, &falseValue, sizeof(falseValue)},
      {CKA_SENSITIVE, &falseValue, sizeof(falseValue)},
      {CKA_PRIVATE, &falseValue, sizeof(falseValue)},
      // PrivateKeyFromPrivateKeyTemplate sets the ID.
      {CKA_ID, nullptr, 0},
      {CKA_EC_PARAMS, params.data, params.len},
      {CKA_EC_POINT, point->data, point->len},
      {CKA_VALUE, value.data, value.len},
  };

  mPrivateKey =
      PrivateKeyFromPrivateKeyTemplate(keyTemplate, ArrayLength(keyTemplate));
  NS_ENSURE_TRUE(mPrivateKey, NS_ERROR_DOM_OPERATION_ERR);

  return NS_OK;
}

void CryptoKey::ClearUsages() { mAttributes &= CLEAR_USAGES; }

nsresult CryptoKey::AddUsage(const nsString& aUsage) {
  KeyUsage usage;
  if (NS_FAILED(StringToUsage(aUsage, usage))) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  MOZ_ASSERT(usage & USAGES_MASK, "Usages should be valid");

  // This is harmless if usage is 0, so we don't repeat the assertion check
  AddUsage(usage);
  return NS_OK;
}

nsresult CryptoKey::AddAllowedUsage(const nsString& aUsage,
                                    const nsString& aAlgorithm) {
  return AddAllowedUsageIntersecting(aUsage, aAlgorithm, USAGES_MASK);
}

nsresult CryptoKey::AddAllowedUsageIntersecting(const nsString& aUsage,
                                                const nsString& aAlgorithm,
                                                uint32_t aUsageMask) {
  uint32_t allowedUsages = GetAllowedUsagesForAlgorithm(aAlgorithm);
  KeyUsage usage;
  if (NS_FAILED(StringToUsage(aUsage, usage))) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if ((usage & allowedUsages) != usage) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  MOZ_ASSERT(usage & USAGES_MASK, "Usages should be valid");

  // This is harmless if usage is 0, so we don't repeat the assertion check
  if (usage & aUsageMask) {
    AddUsage(usage);
    return NS_OK;
  }

  return NS_OK;
}

void CryptoKey::AddUsage(CryptoKey::KeyUsage aUsage) { mAttributes |= aUsage; }

bool CryptoKey::HasAnyUsage() { return !!(mAttributes & USAGES_MASK); }

bool CryptoKey::HasUsage(CryptoKey::KeyUsage aUsage) {
  return !!(mAttributes & aUsage);
}

bool CryptoKey::HasUsageOtherThan(uint32_t aUsages) {
  return !!(mAttributes & USAGES_MASK & ~aUsages);
}

bool CryptoKey::IsRecognizedUsage(const nsString& aUsage) {
  KeyUsage dummy;
  nsresult rv = StringToUsage(aUsage, dummy);
  return NS_SUCCEEDED(rv);
}

bool CryptoKey::AllUsagesRecognized(const Sequence<nsString>& aUsages) {
  for (uint32_t i = 0; i < aUsages.Length(); ++i) {
    if (!IsRecognizedUsage(aUsages[i])) {
      return false;
    }
  }
  return true;
}

uint32_t CryptoKey::GetAllowedUsagesForAlgorithm(const nsString& aAlgorithm) {
  uint32_t allowedUsages = 0;
  if (aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_AES_CTR) ||
      aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_AES_CBC) ||
      aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_AES_GCM) ||
      aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_RSA_OAEP)) {
    allowedUsages = ENCRYPT | DECRYPT | WRAPKEY | UNWRAPKEY;
  } else if (aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_AES_KW)) {
    allowedUsages = WRAPKEY | UNWRAPKEY;
  } else if (aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_HMAC) ||
             aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_RSASSA_PKCS1) ||
             aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_RSA_PSS) ||
             aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_ECDSA)) {
    allowedUsages = SIGN | VERIFY;
  } else if (aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_ECDH) ||
             aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_HKDF) ||
             aAlgorithm.EqualsASCII(WEBCRYPTO_ALG_PBKDF2)) {
    allowedUsages = DERIVEBITS | DERIVEKEY;
  }
  return allowedUsages;
}

nsresult CryptoKey::SetSymKey(const CryptoBuffer& aSymKey) {
  if (!mSymKey.Assign(aSymKey)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult CryptoKey::SetPrivateKey(SECKEYPrivateKey* aPrivateKey) {
  if (!aPrivateKey) {
    mPrivateKey = nullptr;
    return NS_OK;
  }

  mPrivateKey = UniqueSECKEYPrivateKey(SECKEY_CopyPrivateKey(aPrivateKey));
  return mPrivateKey ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult CryptoKey::SetPublicKey(SECKEYPublicKey* aPublicKey) {
  if (!aPublicKey) {
    mPublicKey = nullptr;
    return NS_OK;
  }

  mPublicKey = UniqueSECKEYPublicKey(SECKEY_CopyPublicKey(aPublicKey));
  return mPublicKey ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

const CryptoBuffer& CryptoKey::GetSymKey() const { return mSymKey; }

UniqueSECKEYPrivateKey CryptoKey::GetPrivateKey() const {
  if (!mPrivateKey) {
    return nullptr;
  }
  return UniqueSECKEYPrivateKey(SECKEY_CopyPrivateKey(mPrivateKey.get()));
}

UniqueSECKEYPublicKey CryptoKey::GetPublicKey() const {
  if (!mPublicKey) {
    return nullptr;
  }
  return UniqueSECKEYPublicKey(SECKEY_CopyPublicKey(mPublicKey.get()));
}

// Serialization and deserialization convenience methods

UniqueSECKEYPrivateKey CryptoKey::PrivateKeyFromPkcs8(CryptoBuffer& aKeyData) {
  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return nullptr;
  }

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return nullptr;
  }

  SECItem pkcs8Item = {siBuffer, nullptr, 0};
  if (!aKeyData.ToSECItem(arena.get(), &pkcs8Item)) {
    return nullptr;
  }

  // Allow everything, we enforce usage ourselves
  unsigned int usage = KU_ALL;

  SECKEYPrivateKey* privKey;
  SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
      slot.get(), &pkcs8Item, nullptr, nullptr, false, false, usage, &privKey,
      nullptr);

  if (rv == SECFailure) {
    return nullptr;
  }

  return UniqueSECKEYPrivateKey(privKey);
}

UniqueSECKEYPublicKey CryptoKey::PublicKeyFromSpki(CryptoBuffer& aKeyData) {
  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return nullptr;
  }

  SECItem spkiItem = {siBuffer, nullptr, 0};
  if (!aKeyData.ToSECItem(arena.get(), &spkiItem)) {
    return nullptr;
  }

  UniqueCERTSubjectPublicKeyInfo spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&spkiItem));
  if (!spki) {
    return nullptr;
  }

  bool isECDHAlgorithm =
      SECITEM_ItemsAreEqual(&SEC_OID_DATA_EC_DH, &spki->algorithm.algorithm);

  // Check for |id-ecDH|. Per old versions of the WebCrypto spec we must
  // support this OID but NSS does unfortunately not know it. Let's
  // change the algorithm to |id-ecPublicKey| to make NSS happy.
  if (isECDHAlgorithm) {
    SECOidTag oid = SEC_OID_ANSIX962_EC_PUBLIC_KEY;

    SECOidData* oidData = SECOID_FindOIDByTag(oid);
    if (!oidData) {
      return nullptr;
    }

    SECStatus rv = SECITEM_CopyItem(spki->arena, &spki->algorithm.algorithm,
                                    &oidData->oid);
    if (rv != SECSuccess) {
      return nullptr;
    }
  }

  UniqueSECKEYPublicKey tmp(SECKEY_ExtractPublicKey(spki.get()));
  if (!tmp.get() || !PublicKeyValid(tmp.get())) {
    return nullptr;
  }

  return UniqueSECKEYPublicKey(SECKEY_CopyPublicKey(tmp.get()));
}

nsresult CryptoKey::PrivateKeyToPkcs8(SECKEYPrivateKey* aPrivKey,
                                      CryptoBuffer& aRetVal) {
  UniqueSECItem pkcs8Item(PK11_ExportDERPrivateKeyInfo(aPrivKey, nullptr));
  if (!pkcs8Item.get()) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }
  if (!aRetVal.Assign(pkcs8Item.get())) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }
  return NS_OK;
}

nsresult CryptoKey::PublicKeyToSpki(SECKEYPublicKey* aPubKey,
                                    CryptoBuffer& aRetVal) {
  UniqueCERTSubjectPublicKeyInfo spki;

  spki.reset(SECKEY_CreateSubjectPublicKeyInfo(aPubKey));
  if (!spki) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  const SEC_ASN1Template* tpl = SEC_ASN1_GET(CERT_SubjectPublicKeyInfoTemplate);
  UniqueSECItem spkiItem(SEC_ASN1EncodeItem(nullptr, nullptr, spki.get(), tpl));

  if (!aRetVal.Assign(spkiItem.get())) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }
  return NS_OK;
}

SECItem* CreateECPointForCoordinates(const CryptoBuffer& aX,
                                     const CryptoBuffer& aY,
                                     PLArenaPool* aArena) {
  // Check that both points have the same length.
  if (aX.Length() != aY.Length()) {
    return nullptr;
  }

  // Create point.
  SECItem* point =
      ::SECITEM_AllocItem(aArena, nullptr, aX.Length() + aY.Length() + 1);
  if (!point) {
    return nullptr;
  }

  // Set point data.
  point->data[0] = EC_POINT_FORM_UNCOMPRESSED;
  memcpy(point->data + 1, aX.Elements(), aX.Length());
  memcpy(point->data + 1 + aX.Length(), aY.Elements(), aY.Length());

  return point;
}

UniqueSECKEYPrivateKey CryptoKey::PrivateKeyFromJwk(const JsonWebKey& aJwk) {
  CK_OBJECT_CLASS privateKeyValue = CKO_PRIVATE_KEY;
  CK_BBOOL falseValue = CK_FALSE;

  if (aJwk.mKty.EqualsLiteral(JWK_TYPE_EC)) {
    // Verify that all of the required parameters are present
    CryptoBuffer x, y, d;
    if (!aJwk.mCrv.WasPassed() || !aJwk.mX.WasPassed() ||
        NS_FAILED(x.FromJwkBase64(aJwk.mX.Value())) || !aJwk.mY.WasPassed() ||
        NS_FAILED(y.FromJwkBase64(aJwk.mY.Value())) || !aJwk.mD.WasPassed() ||
        NS_FAILED(d.FromJwkBase64(aJwk.mD.Value()))) {
      return nullptr;
    }

    nsString namedCurve;
    if (!NormalizeToken(aJwk.mCrv.Value(), namedCurve)) {
      return nullptr;
    }

    UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
    if (!arena) {
      return nullptr;
    }

    // Create parameters.
    SECItem* params = CreateECParamsForCurve(namedCurve, arena.get());
    if (!params) {
      return nullptr;
    }

    SECItem* ecPoint = CreateECPointForCoordinates(x, y, arena.get());
    if (!ecPoint) {
      return nullptr;
    }

    // Populate template from parameters
    CK_KEY_TYPE ecValue = CKK_EC;
    CK_ATTRIBUTE keyTemplate[9] = {
        {CKA_CLASS, &privateKeyValue, sizeof(privateKeyValue)},
        {CKA_KEY_TYPE, &ecValue, sizeof(ecValue)},
        {CKA_TOKEN, &falseValue, sizeof(falseValue)},
        {CKA_SENSITIVE, &falseValue, sizeof(falseValue)},
        {CKA_PRIVATE, &falseValue, sizeof(falseValue)},
        // PrivateKeyFromPrivateKeyTemplate sets the ID.
        {CKA_ID, nullptr, 0},
        {CKA_EC_PARAMS, params->data, params->len},
        {CKA_EC_POINT, ecPoint->data, ecPoint->len},
        {CKA_VALUE, (void*)d.Elements(), (CK_ULONG)d.Length()},
    };

    return PrivateKeyFromPrivateKeyTemplate(keyTemplate,
                                            ArrayLength(keyTemplate));
  }

  if (aJwk.mKty.EqualsLiteral(JWK_TYPE_RSA)) {
    // Verify that all of the required parameters are present
    CryptoBuffer n, e, d, p, q, dp, dq, qi;
    if (!aJwk.mN.WasPassed() || NS_FAILED(n.FromJwkBase64(aJwk.mN.Value())) ||
        !aJwk.mE.WasPassed() || NS_FAILED(e.FromJwkBase64(aJwk.mE.Value())) ||
        !aJwk.mD.WasPassed() || NS_FAILED(d.FromJwkBase64(aJwk.mD.Value())) ||
        !aJwk.mP.WasPassed() || NS_FAILED(p.FromJwkBase64(aJwk.mP.Value())) ||
        !aJwk.mQ.WasPassed() || NS_FAILED(q.FromJwkBase64(aJwk.mQ.Value())) ||
        !aJwk.mDp.WasPassed() ||
        NS_FAILED(dp.FromJwkBase64(aJwk.mDp.Value())) ||
        !aJwk.mDq.WasPassed() ||
        NS_FAILED(dq.FromJwkBase64(aJwk.mDq.Value())) ||
        !aJwk.mQi.WasPassed() ||
        NS_FAILED(qi.FromJwkBase64(aJwk.mQi.Value()))) {
      return nullptr;
    }

    // Populate template from parameters
    CK_KEY_TYPE rsaValue = CKK_RSA;
    CK_ATTRIBUTE keyTemplate[14] = {
        {CKA_CLASS, &privateKeyValue, sizeof(privateKeyValue)},
        {CKA_KEY_TYPE, &rsaValue, sizeof(rsaValue)},
        {CKA_TOKEN, &falseValue, sizeof(falseValue)},
        {CKA_SENSITIVE, &falseValue, sizeof(falseValue)},
        {CKA_PRIVATE, &falseValue, sizeof(falseValue)},
        // PrivateKeyFromPrivateKeyTemplate sets the ID.
        {CKA_ID, nullptr, 0},
        {CKA_MODULUS, (void*)n.Elements(), (CK_ULONG)n.Length()},
        {CKA_PUBLIC_EXPONENT, (void*)e.Elements(), (CK_ULONG)e.Length()},
        {CKA_PRIVATE_EXPONENT, (void*)d.Elements(), (CK_ULONG)d.Length()},
        {CKA_PRIME_1, (void*)p.Elements(), (CK_ULONG)p.Length()},
        {CKA_PRIME_2, (void*)q.Elements(), (CK_ULONG)q.Length()},
        {CKA_EXPONENT_1, (void*)dp.Elements(), (CK_ULONG)dp.Length()},
        {CKA_EXPONENT_2, (void*)dq.Elements(), (CK_ULONG)dq.Length()},
        {CKA_COEFFICIENT, (void*)qi.Elements(), (CK_ULONG)qi.Length()},
    };

    return PrivateKeyFromPrivateKeyTemplate(keyTemplate,
                                            ArrayLength(keyTemplate));
  }

  return nullptr;
}

bool ReadAndEncodeAttribute(SECKEYPrivateKey* aKey,
                            CK_ATTRIBUTE_TYPE aAttribute,
                            Optional<nsString>& aDst) {
  ScopedAutoSECItem item;
  if (PK11_ReadRawAttribute(PK11_TypePrivKey, aKey, aAttribute, &item) !=
      SECSuccess) {
    return false;
  }

  CryptoBuffer buffer;
  if (!buffer.Assign(&item)) {
    return false;
  }

  if (NS_FAILED(buffer.ToJwkBase64(aDst.Value()))) {
    return false;
  }

  return true;
}

bool ECKeyToJwk(const PK11ObjectType aKeyType, void* aKey,
                const SECItem* aEcParams, const SECItem* aPublicValue,
                JsonWebKey& aRetVal) {
  aRetVal.mX.Construct();
  aRetVal.mY.Construct();

  // Check that the given EC parameters are valid.
  if (!CheckEncodedECParameters(aEcParams)) {
    return false;
  }

  // Construct the OID tag.
  SECItem oid = {siBuffer, nullptr, 0};
  oid.len = aEcParams->data[1];
  oid.data = aEcParams->data + 2;

  uint32_t flen;
  switch (SECOID_FindOIDTag(&oid)) {
    case SEC_OID_SECG_EC_SECP256R1:
      flen = 32;  // bytes
      aRetVal.mCrv.Construct(
          NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_NAMED_CURVE_P256));
      break;
    case SEC_OID_SECG_EC_SECP384R1:
      flen = 48;  // bytes
      aRetVal.mCrv.Construct(
          NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_NAMED_CURVE_P384));
      break;
    case SEC_OID_SECG_EC_SECP521R1:
      flen = 66;  // bytes
      aRetVal.mCrv.Construct(
          NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_NAMED_CURVE_P521));
      break;
    default:
      return false;
  }

  // No support for compressed points.
  if (aPublicValue->data[0] != EC_POINT_FORM_UNCOMPRESSED) {
    return false;
  }

  // Check length of uncompressed point coordinates.
  if (aPublicValue->len != (2 * flen + 1)) {
    return false;
  }

  UniqueSECItem ecPointX(::SECITEM_AllocItem(nullptr, nullptr, flen));
  UniqueSECItem ecPointY(::SECITEM_AllocItem(nullptr, nullptr, flen));
  if (!ecPointX || !ecPointY) {
    return false;
  }

  // Extract point data.
  memcpy(ecPointX->data, aPublicValue->data + 1, flen);
  memcpy(ecPointY->data, aPublicValue->data + 1 + flen, flen);

  CryptoBuffer x, y;
  if (!x.Assign(ecPointX.get()) ||
      NS_FAILED(x.ToJwkBase64(aRetVal.mX.Value())) ||
      !y.Assign(ecPointY.get()) ||
      NS_FAILED(y.ToJwkBase64(aRetVal.mY.Value()))) {
    return false;
  }

  aRetVal.mKty = NS_LITERAL_STRING_FROM_CSTRING(JWK_TYPE_EC);
  return true;
}

nsresult CryptoKey::PrivateKeyToJwk(SECKEYPrivateKey* aPrivKey,
                                    JsonWebKey& aRetVal) {
  switch (aPrivKey->keyType) {
    case rsaKey: {
      aRetVal.mN.Construct();
      aRetVal.mE.Construct();
      aRetVal.mD.Construct();
      aRetVal.mP.Construct();
      aRetVal.mQ.Construct();
      aRetVal.mDp.Construct();
      aRetVal.mDq.Construct();
      aRetVal.mQi.Construct();

      if (!ReadAndEncodeAttribute(aPrivKey, CKA_MODULUS, aRetVal.mN) ||
          !ReadAndEncodeAttribute(aPrivKey, CKA_PUBLIC_EXPONENT, aRetVal.mE) ||
          !ReadAndEncodeAttribute(aPrivKey, CKA_PRIVATE_EXPONENT, aRetVal.mD) ||
          !ReadAndEncodeAttribute(aPrivKey, CKA_PRIME_1, aRetVal.mP) ||
          !ReadAndEncodeAttribute(aPrivKey, CKA_PRIME_2, aRetVal.mQ) ||
          !ReadAndEncodeAttribute(aPrivKey, CKA_EXPONENT_1, aRetVal.mDp) ||
          !ReadAndEncodeAttribute(aPrivKey, CKA_EXPONENT_2, aRetVal.mDq) ||
          !ReadAndEncodeAttribute(aPrivKey, CKA_COEFFICIENT, aRetVal.mQi)) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      aRetVal.mKty = NS_LITERAL_STRING_FROM_CSTRING(JWK_TYPE_RSA);
      return NS_OK;
    }
    case ecKey: {
      // Read EC params.
      ScopedAutoSECItem params;
      SECStatus rv = PK11_ReadRawAttribute(PK11_TypePrivKey, aPrivKey,
                                           CKA_EC_PARAMS, &params);
      if (rv != SECSuccess) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      // Read public point Q.
      ScopedAutoSECItem ecPoint;
      rv = PK11_ReadRawAttribute(PK11_TypePrivKey, aPrivKey, CKA_EC_POINT,
                                 &ecPoint);
      if (rv != SECSuccess) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      if (!ECKeyToJwk(PK11_TypePrivKey, aPrivKey, &params, &ecPoint, aRetVal)) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      aRetVal.mD.Construct();

      // Read private value.
      if (!ReadAndEncodeAttribute(aPrivKey, CKA_VALUE, aRetVal.mD)) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      return NS_OK;
    }
    default:
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }
}

UniqueSECKEYPublicKey CreateECPublicKey(const SECItem* aKeyData,
                                        const nsAString& aNamedCurve) {
  if (!EnsureNSSInitializedChromeOrContent()) {
    return nullptr;
  }

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return nullptr;
  }

  // It's important that this be a UniqueSECKEYPublicKey, as this ensures that
  // SECKEY_DestroyPublicKey will be called on it. If this doesn't happen, when
  // CryptoKey::PublicKeyValid is called on it and it gets moved to the internal
  // PKCS#11 slot, it will leak a reference to the slot.
  UniqueSECKEYPublicKey key(PORT_ArenaZNew(arena.get(), SECKEYPublicKey));
  if (!key) {
    return nullptr;
  }

  // Transfer arena ownership to the key.
  key->arena = arena.release();
  key->keyType = ecKey;
  key->pkcs11Slot = nullptr;
  key->pkcs11ID = CK_INVALID_HANDLE;

  // Create curve parameters.
  SECItem* params = CreateECParamsForCurve(aNamedCurve, key->arena);
  if (!params) {
    return nullptr;
  }
  key->u.ec.DEREncodedParams = *params;

  // Set public point.
  SECStatus ret =
      SECITEM_CopyItem(key->arena, &key->u.ec.publicValue, aKeyData);
  if (NS_WARN_IF(ret != SECSuccess)) {
    return nullptr;
  }

  // Ensure the given point is on the curve.
  if (!CryptoKey::PublicKeyValid(key.get())) {
    return nullptr;
  }

  return key;
}

UniqueSECKEYPublicKey CryptoKey::PublicKeyFromJwk(const JsonWebKey& aJwk) {
  if (aJwk.mKty.EqualsLiteral(JWK_TYPE_RSA)) {
    // Verify that all of the required parameters are present
    CryptoBuffer n, e;
    if (!aJwk.mN.WasPassed() || NS_FAILED(n.FromJwkBase64(aJwk.mN.Value())) ||
        !aJwk.mE.WasPassed() || NS_FAILED(e.FromJwkBase64(aJwk.mE.Value()))) {
      return nullptr;
    }

    // Transcode to a DER RSAPublicKey structure
    struct RSAPublicKeyData {
      SECItem n;
      SECItem e;
    };
    const RSAPublicKeyData input = {
        {siUnsignedInteger, n.Elements(), (unsigned int)n.Length()},
        {siUnsignedInteger, e.Elements(), (unsigned int)e.Length()}};
    const SEC_ASN1Template rsaPublicKeyTemplate[] = {
        {SEC_ASN1_SEQUENCE, 0, nullptr, sizeof(RSAPublicKeyData)},
        {
            SEC_ASN1_INTEGER,
            offsetof(RSAPublicKeyData, n),
        },
        {
            SEC_ASN1_INTEGER,
            offsetof(RSAPublicKeyData, e),
        },
        {
            0,
        }};

    UniqueSECItem pkDer(
        SEC_ASN1EncodeItem(nullptr, nullptr, &input, rsaPublicKeyTemplate));
    if (!pkDer.get()) {
      return nullptr;
    }

    return UniqueSECKEYPublicKey(
        SECKEY_ImportDERPublicKey(pkDer.get(), CKK_RSA));
  }

  if (aJwk.mKty.EqualsLiteral(JWK_TYPE_EC)) {
    // Verify that all of the required parameters are present
    CryptoBuffer x, y;
    if (!aJwk.mCrv.WasPassed() || !aJwk.mX.WasPassed() ||
        NS_FAILED(x.FromJwkBase64(aJwk.mX.Value())) || !aJwk.mY.WasPassed() ||
        NS_FAILED(y.FromJwkBase64(aJwk.mY.Value()))) {
      return nullptr;
    }

    UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
    if (!arena) {
      return nullptr;
    }

    // Create point.
    SECItem* point = CreateECPointForCoordinates(x, y, arena.get());
    if (!point) {
      return nullptr;
    }

    nsString namedCurve;
    if (!NormalizeToken(aJwk.mCrv.Value(), namedCurve)) {
      return nullptr;
    }

    return CreateECPublicKey(point, namedCurve);
  }

  return nullptr;
}

nsresult CryptoKey::PublicKeyToJwk(SECKEYPublicKey* aPubKey,
                                   JsonWebKey& aRetVal) {
  switch (aPubKey->keyType) {
    case rsaKey: {
      CryptoBuffer n, e;
      aRetVal.mN.Construct();
      aRetVal.mE.Construct();

      if (!n.Assign(&aPubKey->u.rsa.modulus) ||
          !e.Assign(&aPubKey->u.rsa.publicExponent) ||
          NS_FAILED(n.ToJwkBase64(aRetVal.mN.Value())) ||
          NS_FAILED(e.ToJwkBase64(aRetVal.mE.Value()))) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      aRetVal.mKty = NS_LITERAL_STRING_FROM_CSTRING(JWK_TYPE_RSA);
      return NS_OK;
    }
    case ecKey:
      if (!ECKeyToJwk(PK11_TypePubKey, aPubKey, &aPubKey->u.ec.DEREncodedParams,
                      &aPubKey->u.ec.publicValue, aRetVal)) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }
      return NS_OK;
    default:
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }
}

UniqueSECKEYPublicKey CryptoKey::PublicECKeyFromRaw(
    CryptoBuffer& aKeyData, const nsString& aNamedCurve) {
  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return nullptr;
  }

  SECItem rawItem = {siBuffer, nullptr, 0};
  if (!aKeyData.ToSECItem(arena.get(), &rawItem)) {
    return nullptr;
  }

  uint32_t flen;
  if (aNamedCurve.EqualsLiteral(WEBCRYPTO_NAMED_CURVE_P256)) {
    flen = 32;  // bytes
  } else if (aNamedCurve.EqualsLiteral(WEBCRYPTO_NAMED_CURVE_P384)) {
    flen = 48;  // bytes
  } else if (aNamedCurve.EqualsLiteral(WEBCRYPTO_NAMED_CURVE_P521)) {
    flen = 66;  // bytes
  } else {
    return nullptr;
  }

  // Check length of uncompressed point coordinates. There are 2 field elements
  // and a leading point form octet (which must EC_POINT_FORM_UNCOMPRESSED).
  if (rawItem.len != (2 * flen + 1)) {
    return nullptr;
  }

  // No support for compressed points.
  if (rawItem.data[0] != EC_POINT_FORM_UNCOMPRESSED) {
    return nullptr;
  }

  return CreateECPublicKey(&rawItem, aNamedCurve);
}

nsresult CryptoKey::PublicECKeyToRaw(SECKEYPublicKey* aPubKey,
                                     CryptoBuffer& aRetVal) {
  if (!aRetVal.Assign(&aPubKey->u.ec.publicValue)) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }
  return NS_OK;
}

bool CryptoKey::PublicKeyValid(SECKEYPublicKey* aPubKey) {
  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot.get()) {
    return false;
  }

  // This assumes that NSS checks the validity of a public key when
  // it is imported into a PKCS#11 module, and returns CK_INVALID_HANDLE
  // if it is invalid.
  CK_OBJECT_HANDLE id = PK11_ImportPublicKey(slot.get(), aPubKey, PR_FALSE);
  return id != CK_INVALID_HANDLE;
}

bool CryptoKey::WriteStructuredClone(JSContext* aCX,
                                     JSStructuredCloneWriter* aWriter) const {
  // Write in five pieces
  // 1. Attributes
  // 2. Symmetric key as raw (if present)
  // 3. Private key as pkcs8 (if present)
  // 4. Public key as spki (if present)
  // 5. Algorithm in whatever form it chooses
  CryptoBuffer priv, pub;

  if (mPrivateKey) {
    if (NS_FAILED(CryptoKey::PrivateKeyToPkcs8(mPrivateKey.get(), priv))) {
      return false;
    }
  }

  if (mPublicKey) {
    if (NS_FAILED(CryptoKey::PublicKeyToSpki(mPublicKey.get(), pub))) {
      return false;
    }
  }

  return JS_WriteUint32Pair(aWriter, mAttributes, CRYPTOKEY_SC_VERSION) &&
         WriteBuffer(aWriter, mSymKey) && WriteBuffer(aWriter, priv) &&
         WriteBuffer(aWriter, pub) && mAlgorithm.WriteStructuredClone(aWriter);
}

// static
already_AddRefed<CryptoKey> CryptoKey::ReadStructuredClone(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  // Ensure that NSS is initialized.
  if (!EnsureNSSInitializedChromeOrContent()) {
    return nullptr;
  }

  RefPtr<CryptoKey> key = new CryptoKey(aGlobal);

  uint32_t version;
  CryptoBuffer sym, priv, pub;

  bool read = JS_ReadUint32Pair(aReader, &key->mAttributes, &version) &&
              (version == CRYPTOKEY_SC_VERSION) && ReadBuffer(aReader, sym) &&
              ReadBuffer(aReader, priv) && ReadBuffer(aReader, pub) &&
              key->mAlgorithm.ReadStructuredClone(aReader);
  if (!read) {
    return nullptr;
  }

  if (sym.Length() > 0 && !key->mSymKey.Assign(sym)) {
    return nullptr;
  }
  if (priv.Length() > 0) {
    key->mPrivateKey = CryptoKey::PrivateKeyFromPkcs8(priv);
  }
  if (pub.Length() > 0) {
    key->mPublicKey = CryptoKey::PublicKeyFromSpki(pub);
  }

  // Ensure that what we've read is consistent
  // If the attributes indicate a key type, should have a key of that type
  if (!((key->GetKeyType() == SECRET && key->mSymKey.Length() > 0) ||
        (key->GetKeyType() == PRIVATE && key->mPrivateKey) ||
        (key->GetKeyType() == PUBLIC && key->mPublicKey))) {
    return nullptr;
  }

  return key.forget();
}

}  // namespace mozilla::dom
