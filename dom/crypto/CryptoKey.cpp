/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pk11pub.h"
#include "cryptohi.h"
#include "ScopedNSSTypes.h"
#include "mozilla/dom/CryptoKey.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "mozilla/dom/SubtleCryptoBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CryptoKey, mGlobal, mAlgorithm)
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

CryptoKey::CryptoKey(nsIGlobalObject* aGlobal)
  : mGlobal(aGlobal)
  , mAttributes(0)
  , mSymKey()
  , mPrivateKey(nullptr)
  , mPublicKey(nullptr)
{
  SetIsDOMBinding();
}

CryptoKey::~CryptoKey()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

JSObject*
CryptoKey::WrapObject(JSContext* aCx)
{
  return CryptoKeyBinding::Wrap(aCx, this);
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

KeyAlgorithm*
CryptoKey::Algorithm() const
{
  return mAlgorithm;
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

void
CryptoKey::SetAlgorithm(KeyAlgorithm* aAlgorithm)
{
  mAlgorithm = aAlgorithm;
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
CryptoKey::HasUsage(CryptoKey::KeyUsage aUsage)
{
  return !!(mAttributes & aUsage);
}

bool
CryptoKey::HasUsageOtherThan(uint32_t aUsages)
{
  return !!(mAttributes & USAGES_MASK & ~aUsages);
}

void CryptoKey::SetSymKey(const CryptoBuffer& aSymKey)
{
  mSymKey = aSymKey;
}

void
CryptoKey::SetPrivateKey(SECKEYPrivateKey* aPrivateKey)
{
  nsNSSShutDownPreventionLock locker;
  if (!aPrivateKey || isAlreadyShutDown()) {
    mPrivateKey = nullptr;
    return;
  }
  mPrivateKey = SECKEY_CopyPrivateKey(aPrivateKey);
}

void
CryptoKey::SetPublicKey(SECKEYPublicKey* aPublicKey)
{
  nsNSSShutDownPreventionLock locker;
  if (!aPublicKey || isAlreadyShutDown()) {
    mPublicKey = nullptr;
    return;
  }
  mPublicKey = SECKEY_CopyPublicKey(aPublicKey);
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
  ScopedSECItem pkcs8Item(aKeyData.ToSECItem());
  if (!pkcs8Item) {
    return nullptr;
  }

  // Allow everything, we enforce usage ourselves
  unsigned int usage = KU_ALL;

  nsresult rv = MapSECStatus(PK11_ImportDERPrivateKeyInfoAndReturnKey(
                slot.get(), pkcs8Item.get(), nullptr, nullptr, false, false,
                usage, &privKey, nullptr));

  if (NS_FAILED(rv)) {
    return nullptr;
  }
  return privKey;
}

SECKEYPublicKey*
CryptoKey::PublicKeyFromSpki(CryptoBuffer& aKeyData,
                       const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  ScopedSECItem spkiItem(aKeyData.ToSECItem());
  if (!spkiItem) {
    return nullptr;
  }

  ScopedCERTSubjectPublicKeyInfo spki(SECKEY_DecodeDERSubjectPublicKeyInfo(spkiItem.get()));
  if (!spki) {
    return nullptr;
  }

  return SECKEY_ExtractPublicKey(spki.get());
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
  aRetVal.Assign(pkcs8Item.get());
  return NS_OK;
}

nsresult
CryptoKey::PublicKeyToSpki(SECKEYPublicKey* aPubKey,
                     CryptoBuffer& aRetVal,
                     const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  ScopedSECItem spkiItem(PK11_DEREncodePublicKey(aPubKey));
  if (!spkiItem.get()) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  aRetVal.Assign(spkiItem.get());
  return NS_OK;
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
    CryptoKey::PrivateKeyToPkcs8(mPrivateKey, priv, locker);
  }

  if (mPublicKey) {
    CryptoKey::PublicKeyToSpki(mPublicKey, pub, locker);
  }

  return JS_WriteUint32Pair(aWriter, mAttributes, 0) &&
         WriteBuffer(aWriter, mSymKey) &&
         WriteBuffer(aWriter, priv) &&
         WriteBuffer(aWriter, pub) &&
         mAlgorithm->WriteStructuredClone(aWriter);
}

bool
CryptoKey::ReadStructuredClone(JSStructuredCloneReader* aReader)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return false;
  }

  uint32_t zero;
  CryptoBuffer sym, priv, pub;
  nsRefPtr<KeyAlgorithm> algorithm;

  bool read = JS_ReadUint32Pair(aReader, &mAttributes, &zero) &&
              ReadBuffer(aReader, sym) &&
              ReadBuffer(aReader, priv) &&
              ReadBuffer(aReader, pub) &&
              (algorithm = KeyAlgorithm::Create(mGlobal, aReader));
  if (!read) {
    return false;
  }

  if (sym.Length() > 0)  {
    mSymKey = sym;
  }
  if (priv.Length() > 0) {
    mPrivateKey = CryptoKey::PrivateKeyFromPkcs8(priv, locker);
  }
  if (pub.Length() > 0)  {
    mPublicKey = CryptoKey::PublicKeyFromSpki(pub, locker);
  }
  mAlgorithm = algorithm;

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
