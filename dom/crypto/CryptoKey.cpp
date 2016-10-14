/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CryptoKey.h"

#include "ScopedNSSTypes.h"
#include "cryptohi.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsNSSComponent.h"
#include "pk11pub.h"

// Templates taken from security/nss/lib/cryptohi/seckey.c
// These would ideally be exported by NSS and until that
// happens we have to keep our own copies.
const SEC_ASN1Template SECKEY_DHPublicKeyTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(SECKEYPublicKey,u.dh.publicValue), },
    { 0, }
};
const SEC_ASN1Template SECKEY_DHParamKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYPublicKey) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPublicKey,u.dh.prime), },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPublicKey,u.dh.base), },
    { SEC_ASN1_SKIP_REST },
    { 0, }
};

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CryptoKey, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(CryptoKey)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CryptoKey)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CryptoKey)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsresult
StringToUsage(const nsString& aUsage, CryptoKey::KeyUsage& aUsageOut)
{
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
static void
DestroyPrivateKeyWithoutDestroyingPKCS11Object(SECKEYPrivateKey* key)
{
  PK11_FreeSlot(key->pkcs11Slot);
  PORT_FreeArena(key->arena, PR_TRUE);
}

// To protect against key ID collisions, PrivateKeyFromPrivateKeyTemplate
// generates a random ID for each key. The given template must contain an
// attribute slot for a key ID, but it must consist of a null pointer and have a
// length of 0.
SECKEYPrivateKey*
PrivateKeyFromPrivateKeyTemplate(CK_ATTRIBUTE* aTemplate,
                                 CK_ULONG aTemplateSize)
{
  // Create a generic object with the contents of the key
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return nullptr;
  }

  // Generate a random 160-bit object ID. This ID must be unique.
  ScopedSECItem objID(::SECITEM_AllocItem(nullptr, nullptr, 20));
  SECStatus rv = PK11_GenerateRandomOnSlot(slot, objID->data, objID->len);
  if (rv != SECSuccess) {
    return nullptr;
  }
  // Check if something is already using this ID.
  SECKEYPrivateKey* preexistingKey = PK11_FindKeyByKeyID(slot, objID, nullptr);
  if (preexistingKey) {
    // Note that we can't just call SECKEY_DestroyPrivateKey here because that
    // will destroy the PKCS#11 object that is backing a preexisting key (that
    // we still have a handle on somewhere else in memory). If that object were
    // destroyed, cryptographic operations performed by that other key would
    // fail.
    DestroyPrivateKeyWithoutDestroyingPKCS11Object(preexistingKey);
    // Try again with a new ID (but only once - collisions are very unlikely).
    rv = PK11_GenerateRandomOnSlot(slot, objID->data, objID->len);
    if (rv != SECSuccess) {
      return nullptr;
    }
    preexistingKey = PK11_FindKeyByKeyID(slot, objID, nullptr);
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
  ScopedPK11GenericObject obj(PK11_CreateGenericObject(slot,
                                                       aTemplate,
                                                       aTemplateSize,
                                                       PR_FALSE));
  // Unset the ID attribute slot's pointer and length so that data that only
  // lives for the scope of this function doesn't escape.
  idAttributeSlot->pValue = nullptr;
  idAttributeSlot->ulValueLen = 0;
  if (!obj) {
    return nullptr;
  }

  // Have NSS translate the object to a private key.
  return PK11_FindKeyByKeyID(slot, objID, nullptr);
}

CryptoKey::CryptoKey(nsIGlobalObject* aGlobal)
  : mGlobal(aGlobal)
  , mAttributes(0)
  , mSymKey()
  , mPrivateKey(nullptr)
  , mPublicKey(nullptr)
{
}

CryptoKey::~CryptoKey()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(ShutdownCalledFrom::Object);
}

JSObject*
CryptoKey::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CryptoKeyBinding::Wrap(aCx, this, aGivenProto);
}

void
CryptoKey::GetType(nsString& aRetVal) const
{
  uint32_t type = mAttributes & TYPE_MASK;
  switch (type) {
    case PUBLIC:  aRetVal.AssignLiteral(WEBCRYPTO_KEY_TYPE_PUBLIC); break;
    case PRIVATE: aRetVal.AssignLiteral(WEBCRYPTO_KEY_TYPE_PRIVATE); break;
    case SECRET:  aRetVal.AssignLiteral(WEBCRYPTO_KEY_TYPE_SECRET); break;
  }
}

bool
CryptoKey::Extractable() const
{
  return (mAttributes & EXTRACTABLE);
}

void
CryptoKey::GetAlgorithm(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal,
                        ErrorResult& aRv) const
{
  bool converted = false;
  JS::RootedValue val(cx);
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
    case KeyAlgorithmProxy::DH: {
      RootedDictionary<DhKeyAlgorithm> dh(cx);
      converted = mAlgorithm.mDh.ToKeyAlgorithm(cx, dh);
      if (converted) {
        converted = ToJSValue(cx, dh, &val);
      }
      break;
    }
  }
  if (!converted) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  aRetVal.set(&val.toObject());
}

void
CryptoKey::GetUsages(nsTArray<nsString>& aRetVal) const
{
  if (mAttributes & ENCRYPT) {
    aRetVal.AppendElement(NS_LITERAL_STRING(WEBCRYPTO_KEY_USAGE_ENCRYPT));
  }
  if (mAttributes & DECRYPT) {
    aRetVal.AppendElement(NS_LITERAL_STRING(WEBCRYPTO_KEY_USAGE_DECRYPT));
  }
  if (mAttributes & SIGN) {
    aRetVal.AppendElement(NS_LITERAL_STRING(WEBCRYPTO_KEY_USAGE_SIGN));
  }
  if (mAttributes & VERIFY) {
    aRetVal.AppendElement(NS_LITERAL_STRING(WEBCRYPTO_KEY_USAGE_VERIFY));
  }
  if (mAttributes & DERIVEKEY) {
    aRetVal.AppendElement(NS_LITERAL_STRING(WEBCRYPTO_KEY_USAGE_DERIVEKEY));
  }
  if (mAttributes & DERIVEBITS) {
    aRetVal.AppendElement(NS_LITERAL_STRING(WEBCRYPTO_KEY_USAGE_DERIVEBITS));
  }
  if (mAttributes & WRAPKEY) {
    aRetVal.AppendElement(NS_LITERAL_STRING(WEBCRYPTO_KEY_USAGE_WRAPKEY));
  }
  if (mAttributes & UNWRAPKEY) {
    aRetVal.AppendElement(NS_LITERAL_STRING(WEBCRYPTO_KEY_USAGE_UNWRAPKEY));
  }
}

KeyAlgorithmProxy&
CryptoKey::Algorithm()
{
  return mAlgorithm;
}

const KeyAlgorithmProxy&
CryptoKey::Algorithm() const
{
  return mAlgorithm;
}

CryptoKey::KeyType
CryptoKey::GetKeyType() const
{
  return static_cast<CryptoKey::KeyType>(mAttributes & TYPE_MASK);
}

nsresult
CryptoKey::SetType(const nsString& aType)
{
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

void
CryptoKey::SetType(CryptoKey::KeyType aType)
{
  mAttributes &= CLEAR_TYPE;
  mAttributes |= aType;
}

void
CryptoKey::SetExtractable(bool aExtractable)
{
  mAttributes &= CLEAR_EXTRACTABLE;
  if (aExtractable) {
    mAttributes |= EXTRACTABLE;
  }
}

// NSS exports private EC keys without the CKA_EC_POINT attribute, i.e. the
// public value. To properly export the private key to JWK or PKCS #8 we need
// the public key data though and so we use this method to augment a private
// key with data from the given public key.
nsresult
CryptoKey::AddPublicKeyData(SECKEYPublicKey* aPublicKey)
{
  // This should be a private key.
  MOZ_ASSERT(GetKeyType() == PRIVATE);
  // There should be a private NSS key with type 'EC'.
  MOZ_ASSERT(mPrivateKey && mPrivateKey->keyType == ecKey);
  // The given public key should have the same key type.
  MOZ_ASSERT(aPublicKey->keyType == mPrivateKey->keyType);

  nsNSSShutDownPreventionLock locker;

  // Read EC params.
  ScopedAutoSECItem params;
  SECStatus rv = PK11_ReadRawAttribute(PK11_TypePrivKey, mPrivateKey,
                                       CKA_EC_PARAMS, &params);
  if (rv != SECSuccess) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  // Read private value.
  ScopedAutoSECItem value;
  rv = PK11_ReadRawAttribute(PK11_TypePrivKey, mPrivateKey, CKA_VALUE, &value);
  if (rv != SECSuccess) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  SECItem* point = &aPublicKey->u.ec.publicValue;
  CK_OBJECT_CLASS privateKeyValue = CKO_PRIVATE_KEY;
  CK_BBOOL falseValue = CK_FALSE;
  CK_KEY_TYPE ecValue = CKK_EC;

  CK_ATTRIBUTE keyTemplate[9] = {
    { CKA_CLASS,            &privateKeyValue,     sizeof(privateKeyValue) },
    { CKA_KEY_TYPE,         &ecValue,             sizeof(ecValue) },
    { CKA_TOKEN,            &falseValue,          sizeof(falseValue) },
    { CKA_SENSITIVE,        &falseValue,          sizeof(falseValue) },
    { CKA_PRIVATE,          &falseValue,          sizeof(falseValue) },
    // PrivateKeyFromPrivateKeyTemplate sets the ID.
    { CKA_ID,               nullptr,              0 },
    { CKA_EC_PARAMS,        params.data,          params.len },
    { CKA_EC_POINT,         point->data,          point->len },
    { CKA_VALUE,            value.data,           value.len },
  };

  mPrivateKey = PrivateKeyFromPrivateKeyTemplate(keyTemplate,
                                                 ArrayLength(keyTemplate));
  NS_ENSURE_TRUE(mPrivateKey, NS_ERROR_DOM_OPERATION_ERR);

  return NS_OK;
}

void
CryptoKey::ClearUsages()
{
  mAttributes &= CLEAR_USAGES;
}

nsresult
CryptoKey::AddUsage(const nsString& aUsage)
{
  return AddUsageIntersecting(aUsage, USAGES_MASK);
}

nsresult
CryptoKey::AddUsageIntersecting(const nsString& aUsage, uint32_t aUsageMask)
{
  KeyUsage usage;
  if (NS_FAILED(StringToUsage(aUsage, usage))) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if (usage & aUsageMask) {
    AddUsage(usage);
    return NS_OK;
  }

  return NS_OK;
}

void
CryptoKey::AddUsage(CryptoKey::KeyUsage aUsage)
{
  mAttributes |= aUsage;
}

bool
CryptoKey::HasAnyUsage()
{
  return !!(mAttributes & USAGES_MASK);
}

bool
CryptoKey::HasUsage(CryptoKey::KeyUsage aUsage)
{
  return !!(mAttributes & aUsage);
}

bool
CryptoKey::HasUsageOtherThan(uint32_t aUsages)
{
  return !!(mAttributes & USAGES_MASK & ~aUsages);
}

bool
CryptoKey::IsRecognizedUsage(const nsString& aUsage)
{
  KeyUsage dummy;
  nsresult rv = StringToUsage(aUsage, dummy);
  return NS_SUCCEEDED(rv);
}

bool
CryptoKey::AllUsagesRecognized(const Sequence<nsString>& aUsages)
{
  for (uint32_t i = 0; i < aUsages.Length(); ++i) {
    if (!IsRecognizedUsage(aUsages[i])) {
      return false;
    }
  }
  return true;
}

nsresult CryptoKey::SetSymKey(const CryptoBuffer& aSymKey)
{
  if (!mSymKey.Assign(aSymKey)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
CryptoKey::SetPrivateKey(SECKEYPrivateKey* aPrivateKey)
{
  nsNSSShutDownPreventionLock locker;

  if (!aPrivateKey || isAlreadyShutDown()) {
    mPrivateKey = nullptr;
    return NS_OK;
  }

  mPrivateKey = SECKEY_CopyPrivateKey(aPrivateKey);
  return mPrivateKey ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
CryptoKey::SetPublicKey(SECKEYPublicKey* aPublicKey)
{
  nsNSSShutDownPreventionLock locker;

  if (!aPublicKey || isAlreadyShutDown()) {
    mPublicKey = nullptr;
    return NS_OK;
  }

  mPublicKey = SECKEY_CopyPublicKey(aPublicKey);
  return mPublicKey ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

const CryptoBuffer&
CryptoKey::GetSymKey() const
{
  return mSymKey;
}

SECKEYPrivateKey*
CryptoKey::GetPrivateKey() const
{
  nsNSSShutDownPreventionLock locker;
  if (!mPrivateKey || isAlreadyShutDown()) {
    return nullptr;
  }
  return SECKEY_CopyPrivateKey(mPrivateKey.get());
}

SECKEYPublicKey*
CryptoKey::GetPublicKey() const
{
  nsNSSShutDownPreventionLock locker;
  if (!mPublicKey || isAlreadyShutDown()) {
    return nullptr;
  }
  return SECKEY_CopyPublicKey(mPublicKey.get());
}

void CryptoKey::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void CryptoKey::destructorSafeDestroyNSSReference()
{
  mPrivateKey.dispose();
  mPublicKey.dispose();
}


// Serialization and deserialization convenience methods

SECKEYPrivateKey*
CryptoKey::PrivateKeyFromPkcs8(CryptoBuffer& aKeyData,
                         const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  SECKEYPrivateKey* privKey;
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return nullptr;
  }

  ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return nullptr;
  }

  SECItem pkcs8Item = { siBuffer, nullptr, 0 };
  if (!aKeyData.ToSECItem(arena, &pkcs8Item)) {
    return nullptr;
  }

  // Allow everything, we enforce usage ourselves
  unsigned int usage = KU_ALL;

  SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
                 slot.get(), &pkcs8Item, nullptr, nullptr, false, false,
                 usage, &privKey, nullptr);

  if (rv == SECFailure) {
    return nullptr;
  }
  return privKey;
}

SECKEYPublicKey*
CryptoKey::PublicKeyFromSpki(CryptoBuffer& aKeyData,
                       const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return nullptr;
  }

  SECItem spkiItem = { siBuffer, nullptr, 0 };
  if (!aKeyData.ToSECItem(arena, &spkiItem)) {
    return nullptr;
  }

  ScopedCERTSubjectPublicKeyInfo spki(SECKEY_DecodeDERSubjectPublicKeyInfo(&spkiItem));
  if (!spki) {
    return nullptr;
  }

  bool isECDHAlgorithm = SECITEM_ItemsAreEqual(&SEC_OID_DATA_EC_DH,
                                               &spki->algorithm.algorithm);
  bool isDHAlgorithm = SECITEM_ItemsAreEqual(&SEC_OID_DATA_DH_KEY_AGREEMENT,
                                             &spki->algorithm.algorithm);

  // Check for |id-ecDH| and |dhKeyAgreement|. Per the WebCrypto spec we must
  // support these OIDs but NSS does unfortunately not know about them. Let's
  // change the algorithm to |id-ecPublicKey| or |dhPublicKey| to make NSS happy.
  if (isECDHAlgorithm || isDHAlgorithm) {
    SECOidTag oid = SEC_OID_UNKNOWN;
    if (isECDHAlgorithm) {
      oid = SEC_OID_ANSIX962_EC_PUBLIC_KEY;
    } else if (isDHAlgorithm) {
      oid = SEC_OID_X942_DIFFIE_HELMAN_KEY;
    } else {
      MOZ_ASSERT(false);
    }

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

  ScopedSECKEYPublicKey tmp(SECKEY_ExtractPublicKey(spki.get()));
  if (!tmp.get() || !PublicKeyValid(tmp.get())) {
    return nullptr;
  }

  return SECKEY_CopyPublicKey(tmp);
}

nsresult
CryptoKey::PrivateKeyToPkcs8(SECKEYPrivateKey* aPrivKey,
                       CryptoBuffer& aRetVal,
                       const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  ScopedSECItem pkcs8Item(PK11_ExportDERPrivateKeyInfo(aPrivKey, nullptr));
  if (!pkcs8Item.get()) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }
  if (!aRetVal.Assign(pkcs8Item.get())) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }
  return NS_OK;
}

nsresult
PublicDhKeyToSpki(SECKEYPublicKey* aPubKey,
                  CERTSubjectPublicKeyInfo* aSpki)
{
  SECItem* params = ::SECITEM_AllocItem(aSpki->arena, nullptr, 0);
  if (!params) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  SECItem* rvItem = SEC_ASN1EncodeItem(aSpki->arena, params, aPubKey,
                                       SECKEY_DHParamKeyTemplate);
  if (!rvItem) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  SECStatus rv = SECOID_SetAlgorithmID(aSpki->arena, &aSpki->algorithm,
                                       SEC_OID_X942_DIFFIE_HELMAN_KEY, params);
  if (rv != SECSuccess) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  rvItem = SEC_ASN1EncodeItem(aSpki->arena, &aSpki->subjectPublicKey, aPubKey,
                              SECKEY_DHPublicKeyTemplate);
  if (!rvItem) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  // The public value is a BIT_STRING encoded as an INTEGER. After encoding
  // an INT we need to adjust the length to reflect the number of bits.
  aSpki->subjectPublicKey.len <<= 3;

  return NS_OK;
}

nsresult
CryptoKey::PublicKeyToSpki(SECKEYPublicKey* aPubKey,
                           CryptoBuffer& aRetVal,
                           const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  ScopedCERTSubjectPublicKeyInfo spki;

  // NSS doesn't support exporting DH public keys.
  if (aPubKey->keyType == dhKey) {
    // Mimic the behavior of SECKEY_CreateSubjectPublicKeyInfo() and create
    // a new arena for the SPKI object.
    ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
    if (!arena) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }

    spki = PORT_ArenaZNew(arena, CERTSubjectPublicKeyInfo);
    if (!spki) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }

    // Assign |arena| to |spki| and null the variable afterwards so that the
    // arena created above that holds the SPKI object is free'd when |spki|
    // goes out of scope, not when |arena| does.
    spki->arena = arena.forget();

    nsresult rv = PublicDhKeyToSpki(aPubKey, spki);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    spki = SECKEY_CreateSubjectPublicKeyInfo(aPubKey);
    if (!spki) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }
  }

  // Per WebCrypto spec we must export ECDH SPKIs with the algorithm OID
  // id-ecDH (1.3.132.112) and DH SPKIs with OID dhKeyAgreement
  // (1.2.840.113549.1.3.1). NSS doesn't know about these OIDs and there is
  // no way to specify the algorithm to use when exporting a public key.
  if (aPubKey->keyType == ecKey || aPubKey->keyType == dhKey) {
    const SECItem* oidData = nullptr;
    if (aPubKey->keyType == ecKey) {
      oidData = &SEC_OID_DATA_EC_DH;
    } else if (aPubKey->keyType == dhKey) {
      oidData = &SEC_OID_DATA_DH_KEY_AGREEMENT;
    } else {
      MOZ_ASSERT(false);
    }

    SECStatus rv = SECITEM_CopyItem(spki->arena, &spki->algorithm.algorithm,
                                    oidData);
    if (rv != SECSuccess) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }
  }

  const SEC_ASN1Template* tpl = SEC_ASN1_GET(CERT_SubjectPublicKeyInfoTemplate);
  ScopedSECItem spkiItem(SEC_ASN1EncodeItem(nullptr, nullptr, spki, tpl));

  if (!aRetVal.Assign(spkiItem.get())) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }
  return NS_OK;
}

SECItem*
CreateECPointForCoordinates(const CryptoBuffer& aX,
                            const CryptoBuffer& aY,
                            PLArenaPool* aArena)
{
  // Check that both points have the same length.
  if (aX.Length() != aY.Length()) {
    return nullptr;
  }

  // Create point.
  SECItem* point = ::SECITEM_AllocItem(aArena, nullptr, aX.Length() + aY.Length() + 1);
  if (!point) {
    return nullptr;
  }

  // Set point data.
  point->data[0] = EC_POINT_FORM_UNCOMPRESSED;
  memcpy(point->data + 1, aX.Elements(), aX.Length());
  memcpy(point->data + 1 + aX.Length(), aY.Elements(), aY.Length());

  return point;
}

SECKEYPrivateKey*
CryptoKey::PrivateKeyFromJwk(const JsonWebKey& aJwk,
                             const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  CK_OBJECT_CLASS privateKeyValue = CKO_PRIVATE_KEY;
  CK_BBOOL falseValue = CK_FALSE;

  if (aJwk.mKty.EqualsLiteral(JWK_TYPE_EC)) {
    // Verify that all of the required parameters are present
    CryptoBuffer x, y, d;
    if (!aJwk.mCrv.WasPassed() ||
        !aJwk.mX.WasPassed() || NS_FAILED(x.FromJwkBase64(aJwk.mX.Value())) ||
        !aJwk.mY.WasPassed() || NS_FAILED(y.FromJwkBase64(aJwk.mY.Value())) ||
        !aJwk.mD.WasPassed() || NS_FAILED(d.FromJwkBase64(aJwk.mD.Value()))) {
      return nullptr;
    }

    nsString namedCurve;
    if (!NormalizeToken(aJwk.mCrv.Value(), namedCurve)) {
      return nullptr;
    }

    ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
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
      { CKA_CLASS,            &privateKeyValue,     sizeof(privateKeyValue) },
      { CKA_KEY_TYPE,         &ecValue,             sizeof(ecValue) },
      { CKA_TOKEN,            &falseValue,          sizeof(falseValue) },
      { CKA_SENSITIVE,        &falseValue,          sizeof(falseValue) },
      { CKA_PRIVATE,          &falseValue,          sizeof(falseValue) },
      // PrivateKeyFromPrivateKeyTemplate sets the ID.
      { CKA_ID,               nullptr,              0 },
      { CKA_EC_PARAMS,        params->data,         params->len },
      { CKA_EC_POINT,         ecPoint->data,        ecPoint->len },
      { CKA_VALUE,            (void*) d.Elements(), (CK_ULONG) d.Length() },
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
        !aJwk.mDp.WasPassed() || NS_FAILED(dp.FromJwkBase64(aJwk.mDp.Value())) ||
        !aJwk.mDq.WasPassed() || NS_FAILED(dq.FromJwkBase64(aJwk.mDq.Value())) ||
        !aJwk.mQi.WasPassed() || NS_FAILED(qi.FromJwkBase64(aJwk.mQi.Value()))) {
      return nullptr;
    }

    ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
    if (!arena) {
      return nullptr;
    }

    // Populate template from parameters
    CK_KEY_TYPE rsaValue = CKK_RSA;
    CK_ATTRIBUTE keyTemplate[14] = {
      { CKA_CLASS,            &privateKeyValue,      sizeof(privateKeyValue) },
      { CKA_KEY_TYPE,         &rsaValue,             sizeof(rsaValue) },
      { CKA_TOKEN,            &falseValue,           sizeof(falseValue) },
      { CKA_SENSITIVE,        &falseValue,           sizeof(falseValue) },
      { CKA_PRIVATE,          &falseValue,           sizeof(falseValue) },
      // PrivateKeyFromPrivateKeyTemplate sets the ID.
      { CKA_ID,               nullptr,               0 },
      { CKA_MODULUS,          (void*) n.Elements(),  (CK_ULONG) n.Length() },
      { CKA_PUBLIC_EXPONENT,  (void*) e.Elements(),  (CK_ULONG) e.Length() },
      { CKA_PRIVATE_EXPONENT, (void*) d.Elements(),  (CK_ULONG) d.Length() },
      { CKA_PRIME_1,          (void*) p.Elements(),  (CK_ULONG) p.Length() },
      { CKA_PRIME_2,          (void*) q.Elements(),  (CK_ULONG) q.Length() },
      { CKA_EXPONENT_1,       (void*) dp.Elements(), (CK_ULONG) dp.Length() },
      { CKA_EXPONENT_2,       (void*) dq.Elements(), (CK_ULONG) dq.Length() },
      { CKA_COEFFICIENT,      (void*) qi.Elements(), (CK_ULONG) qi.Length() },
    };

    return PrivateKeyFromPrivateKeyTemplate(keyTemplate,
                                            ArrayLength(keyTemplate));
  }

  return nullptr;
}

bool ReadAndEncodeAttribute(SECKEYPrivateKey* aKey,
                            CK_ATTRIBUTE_TYPE aAttribute,
                            Optional<nsString>& aDst)
{
  ScopedAutoSECItem item;
  if (PK11_ReadRawAttribute(PK11_TypePrivKey, aKey, aAttribute, &item)
        != SECSuccess) {
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

bool
ECKeyToJwk(const PK11ObjectType aKeyType, void* aKey, const SECItem* aEcParams,
           const SECItem* aPublicValue, JsonWebKey& aRetVal)
{
  aRetVal.mX.Construct();
  aRetVal.mY.Construct();

  // Check that the given EC parameters are valid.
  if (!CheckEncodedECParameters(aEcParams)) {
    return false;
  }

  // Construct the OID tag.
  SECItem oid = { siBuffer, nullptr, 0 };
  oid.len = aEcParams->data[1];
  oid.data = aEcParams->data + 2;

  uint32_t flen;
  switch (SECOID_FindOIDTag(&oid)) {
    case SEC_OID_SECG_EC_SECP256R1:
      flen = 32; // bytes
      aRetVal.mCrv.Construct(NS_LITERAL_STRING(WEBCRYPTO_NAMED_CURVE_P256));
      break;
    case SEC_OID_SECG_EC_SECP384R1:
      flen = 48; // bytes
      aRetVal.mCrv.Construct(NS_LITERAL_STRING(WEBCRYPTO_NAMED_CURVE_P384));
      break;
    case SEC_OID_SECG_EC_SECP521R1:
      flen = 66; // bytes
      aRetVal.mCrv.Construct(NS_LITERAL_STRING(WEBCRYPTO_NAMED_CURVE_P521));
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

  ScopedSECItem ecPointX(::SECITEM_AllocItem(nullptr, nullptr, flen));
  ScopedSECItem ecPointY(::SECITEM_AllocItem(nullptr, nullptr, flen));
  if (!ecPointX || !ecPointY) {
    return false;
  }

  // Extract point data.
  memcpy(ecPointX->data, aPublicValue->data + 1, flen);
  memcpy(ecPointY->data, aPublicValue->data + 1 + flen, flen);

  CryptoBuffer x, y;
  if (!x.Assign(ecPointX) || NS_FAILED(x.ToJwkBase64(aRetVal.mX.Value())) ||
      !y.Assign(ecPointY) || NS_FAILED(y.ToJwkBase64(aRetVal.mY.Value()))) {
    return false;
  }

  aRetVal.mKty = NS_LITERAL_STRING(JWK_TYPE_EC);
  return true;
}

nsresult
CryptoKey::PrivateKeyToJwk(SECKEYPrivateKey* aPrivKey,
                           JsonWebKey& aRetVal,
                           const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
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

      aRetVal.mKty = NS_LITERAL_STRING(JWK_TYPE_RSA);
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

SECKEYPublicKey*
CreateECPublicKey(const SECItem* aKeyData, const nsString& aNamedCurve)
{
  ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return nullptr;
  }

  // It's important that this be a ScopedSECKEYPublicKey, as this ensures that
  // SECKEY_DestroyPublicKey will be called on it. If this doesn't happen, when
  // CryptoKey::PublicKeyValid is called on it and it gets moved to the internal
  // PKCS#11 slot, it will leak a reference to the slot.
  ScopedSECKEYPublicKey key(PORT_ArenaZNew(arena, SECKEYPublicKey));
  if (!key) {
    return nullptr;
  }

  key->arena = nullptr; // key doesn't own the arena; it won't get double-freed
  key->keyType = ecKey;
  key->pkcs11Slot = nullptr;
  key->pkcs11ID = CK_INVALID_HANDLE;

  // Create curve parameters.
  SECItem* params = CreateECParamsForCurve(aNamedCurve, arena);
  if (!params) {
    return nullptr;
  }
  key->u.ec.DEREncodedParams = *params;

  // Set public point.
  key->u.ec.publicValue = *aKeyData;

  // Ensure the given point is on the curve.
  if (!CryptoKey::PublicKeyValid(key)) {
    return nullptr;
  }

  return SECKEY_CopyPublicKey(key);
}

SECKEYPublicKey*
CryptoKey::PublicKeyFromJwk(const JsonWebKey& aJwk,
                            const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
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
      { siUnsignedInteger, n.Elements(), (unsigned int) n.Length() },
      { siUnsignedInteger, e.Elements(), (unsigned int) e.Length() }
    };
    const SEC_ASN1Template rsaPublicKeyTemplate[] = {
      {SEC_ASN1_SEQUENCE, 0, nullptr, sizeof(RSAPublicKeyData)},
      {SEC_ASN1_INTEGER, offsetof(RSAPublicKeyData, n),},
      {SEC_ASN1_INTEGER, offsetof(RSAPublicKeyData, e),},
      {0,}
    };

    ScopedSECItem pkDer(SEC_ASN1EncodeItem(nullptr, nullptr, &input,
                                           rsaPublicKeyTemplate));
    if (!pkDer.get()) {
      return nullptr;
    }

    return SECKEY_ImportDERPublicKey(pkDer.get(), CKK_RSA);
  }

  if (aJwk.mKty.EqualsLiteral(JWK_TYPE_EC)) {
    // Verify that all of the required parameters are present
    CryptoBuffer x, y;
    if (!aJwk.mCrv.WasPassed() ||
        !aJwk.mX.WasPassed() || NS_FAILED(x.FromJwkBase64(aJwk.mX.Value())) ||
        !aJwk.mY.WasPassed() || NS_FAILED(y.FromJwkBase64(aJwk.mY.Value()))) {
      return nullptr;
    }

    ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
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

nsresult
CryptoKey::PublicKeyToJwk(SECKEYPublicKey* aPubKey,
                          JsonWebKey& aRetVal,
                          const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
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

      aRetVal.mKty = NS_LITERAL_STRING(JWK_TYPE_RSA);
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

SECKEYPublicKey*
CryptoKey::PublicDhKeyFromRaw(CryptoBuffer& aKeyData,
                              const CryptoBuffer& aPrime,
                              const CryptoBuffer& aGenerator,
                              const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return nullptr;
  }

  SECKEYPublicKey* key = PORT_ArenaZNew(arena, SECKEYPublicKey);
  if (!key) {
    return nullptr;
  }

  key->keyType = dhKey;
  key->pkcs11Slot = nullptr;
  key->pkcs11ID = CK_INVALID_HANDLE;

  // Set DH public key params.
  if (!aPrime.ToSECItem(arena, &key->u.dh.prime) ||
      !aGenerator.ToSECItem(arena, &key->u.dh.base) ||
      !aKeyData.ToSECItem(arena, &key->u.dh.publicValue)) {
    return nullptr;
  }

  key->u.dh.prime.type = siUnsignedInteger;
  key->u.dh.base.type = siUnsignedInteger;
  key->u.dh.publicValue.type = siUnsignedInteger;

  return SECKEY_CopyPublicKey(key);
}

nsresult
CryptoKey::PublicDhKeyToRaw(SECKEYPublicKey* aPubKey,
                            CryptoBuffer& aRetVal,
                            const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  if (!aRetVal.Assign(&aPubKey->u.dh.publicValue)) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }
  return NS_OK;
}

SECKEYPublicKey*
CryptoKey::PublicECKeyFromRaw(CryptoBuffer& aKeyData,
                              const nsString& aNamedCurve,
                              const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return nullptr;
  }

  SECItem rawItem = { siBuffer, nullptr, 0 };
  if (!aKeyData.ToSECItem(arena, &rawItem)) {
    return nullptr;
  }

  uint32_t flen;
  if (aNamedCurve.EqualsLiteral(WEBCRYPTO_NAMED_CURVE_P256)) {
    flen = 32; // bytes
  } else if (aNamedCurve.EqualsLiteral(WEBCRYPTO_NAMED_CURVE_P384)) {
    flen = 48; // bytes
  } else if (aNamedCurve.EqualsLiteral(WEBCRYPTO_NAMED_CURVE_P521)) {
    flen = 66; // bytes
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

nsresult
CryptoKey::PublicECKeyToRaw(SECKEYPublicKey* aPubKey,
                            CryptoBuffer& aRetVal,
                            const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  if (!aRetVal.Assign(&aPubKey->u.ec.publicValue)) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }
  return NS_OK;
}

bool
CryptoKey::PublicKeyValid(SECKEYPublicKey* aPubKey)
{
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot.get()) {
    return false;
  }

  // This assumes that NSS checks the validity of a public key when
  // it is imported into a PKCS#11 module, and returns CK_INVALID_HANDLE
  // if it is invalid.
  CK_OBJECT_HANDLE id = PK11_ImportPublicKey(slot, aPubKey, PR_FALSE);
  if (id == CK_INVALID_HANDLE) {
    return false;
  }

  SECStatus rv = PK11_DestroyObject(slot, id);
  return (rv == SECSuccess);
}

bool
CryptoKey::WriteStructuredClone(JSStructuredCloneWriter* aWriter) const
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return false;
  }

  // Write in five pieces
  // 1. Attributes
  // 2. Symmetric key as raw (if present)
  // 3. Private key as pkcs8 (if present)
  // 4. Public key as spki (if present)
  // 5. Algorithm in whatever form it chooses
  CryptoBuffer priv, pub;

  if (mPrivateKey) {
    if (NS_FAILED(CryptoKey::PrivateKeyToPkcs8(mPrivateKey, priv, locker))) {
      return false;
    }
  }

  if (mPublicKey) {
    if (NS_FAILED(CryptoKey::PublicKeyToSpki(mPublicKey, pub, locker))) {
      return false;
    }
  }

  return JS_WriteUint32Pair(aWriter, mAttributes, CRYPTOKEY_SC_VERSION) &&
         WriteBuffer(aWriter, mSymKey) &&
         WriteBuffer(aWriter, priv) &&
         WriteBuffer(aWriter, pub) &&
         mAlgorithm.WriteStructuredClone(aWriter);
}

bool
CryptoKey::ReadStructuredClone(JSStructuredCloneReader* aReader)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return false;
  }

  // Ensure that NSS is initialized.
  if (!EnsureNSSInitializedChromeOrContent()) {
    return false;
  }

  uint32_t version;
  CryptoBuffer sym, priv, pub;

  bool read = JS_ReadUint32Pair(aReader, &mAttributes, &version) &&
              (version == CRYPTOKEY_SC_VERSION) &&
              ReadBuffer(aReader, sym) &&
              ReadBuffer(aReader, priv) &&
              ReadBuffer(aReader, pub) &&
              mAlgorithm.ReadStructuredClone(aReader);
  if (!read) {
    return false;
  }

  if (sym.Length() > 0 && !mSymKey.Assign(sym))  {
    return false;
  }
  if (priv.Length() > 0) {
    mPrivateKey = CryptoKey::PrivateKeyFromPkcs8(priv, locker);
  }
  if (pub.Length() > 0)  {
    mPublicKey = CryptoKey::PublicKeyFromSpki(pub, locker);
  }

  // Ensure that what we've read is consistent
  // If the attributes indicate a key type, should have a key of that type
  if (!((GetKeyType() == SECRET  && mSymKey.Length() > 0) ||
        (GetKeyType() == PRIVATE && mPrivateKey) ||
        (GetKeyType() == PUBLIC  && mPublicKey))) {
    return false;
  }

  return true;
}

} // namespace dom
} // namespace mozilla
