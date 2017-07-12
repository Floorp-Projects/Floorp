/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/U2FSoftTokenManager.h"
#include "CryptoBuffer.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "nsNSSComponent.h"
#include "nsThreadUtils.h"
#include "pk11pub.h"
#include "prerror.h"
#include "secerr.h"
#include "WebCryptoCommon.h"

#define PREF_U2F_NSSTOKEN_COUNTER "security.webauth.softtoken_counter"

namespace mozilla {
namespace dom {

using namespace mozilla;
using mozilla::dom::CreateECParamsForCurve;

const nsCString U2FSoftTokenManager::mSecretNickname =
  NS_LITERAL_CSTRING("U2F_NSSTOKEN");
const nsString U2FSoftTokenManager::mVersion =
  NS_LITERAL_STRING("U2F_V2");

namespace {
NS_NAMED_LITERAL_CSTRING(kAttestCertSubjectName, "CN=Firefox U2F Soft Token");

// This U2F-compatible soft token uses FIDO U2F-compatible ECDSA keypairs
// on the SEC_OID_SECG_EC_SECP256R1 curve. When asked to Register, it will
// generate and return a new keypair KP, where the private component is wrapped
// using AES-KW with the 128-bit mWrappingKey to make an opaque "key handle".
// In other words, Register yields { KP_pub, AES-KW(KP_priv, key=mWrappingKey) }
//
// The value mWrappingKey is long-lived; it is persisted as part of the NSS DB
// for the current profile. The attestation certificates that are produced are
// ephemeral to counteract profiling. They have little use for a soft-token
// at any rate, but are required by the specification.

const uint32_t kParamLen = 32;
const uint32_t kPublicKeyLen = 65;
const uint32_t kWrappedKeyBufLen = 256;
const uint32_t kWrappingKeyByteLen = 128/8;
const uint32_t kSaltByteLen = 64/8;
const uint32_t kVersion1KeyHandleLen = 162;
NS_NAMED_LITERAL_STRING(kEcAlgorithm, WEBCRYPTO_NAMED_CURVE_P256);

const PRTime kOneDay = PRTime(PR_USEC_PER_SEC)
  * PRTime(60)  // sec
  * PRTime(60)  // min
  * PRTime(24); // hours
const PRTime kExpirationSlack = kOneDay; // Pre-date for clock skew
const PRTime kExpirationLife = kOneDay;

static mozilla::LazyLogModule gNSSTokenLog("webauth_u2f");

enum SoftTokenHandle {
  Version1 = 0,
};

}

U2FSoftTokenManager::U2FSoftTokenManager(uint32_t aCounter)
  : mInitialized(false),
    mCounter(aCounter)
{}

U2FSoftTokenManager::~U2FSoftTokenManager()
{
  nsNSSShutDownPreventionLock locker;

  if (isAlreadyShutDown()) {
    return;
  }

  destructorSafeDestroyNSSReference();
  shutdown(ShutdownCalledFrom::Object);
}

void
U2FSoftTokenManager::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void
U2FSoftTokenManager::destructorSafeDestroyNSSReference()
{
  mWrappingKey = nullptr;
}

/**
 * Gets the first key with the given nickname from the given slot. Any other
 * keys found are not returned.
 * PK11_GetNextSymKey() should not be called on the returned key.
 *
 * @param aSlot Slot to search.
 * @param aNickname Nickname the key should have.
 * @return The first key found. nullptr if no key could be found.
 */
static UniquePK11SymKey
GetSymKeyByNickname(const UniquePK11SlotInfo& aSlot,
                    const nsCString& aNickname,
                    const nsNSSShutDownPreventionLock&)
{
  MOZ_ASSERT(aSlot);
  if (NS_WARN_IF(!aSlot)) {
    return nullptr;
  }

  MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
          ("Searching for a symmetric key named %s", aNickname.get()));

  UniquePK11SymKey keyListHead(
    PK11_ListFixedKeysInSlot(aSlot.get(), const_cast<char*>(aNickname.get()),
                             /* wincx */ nullptr));
  if (NS_WARN_IF(!keyListHead)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Debug, ("Symmetric key not found."));
    return nullptr;
  }

  // Sanity check PK11_ListFixedKeysInSlot() only returns keys with the correct
  // nickname.
  MOZ_ASSERT(aNickname ==
             UniquePORTString(PK11_GetSymKeyNickname(keyListHead.get())).get());
  MOZ_LOG(gNSSTokenLog, LogLevel::Debug, ("Symmetric key found!"));

  // Free any remaining keys in the key list.
  UniquePK11SymKey freeKey(PK11_GetNextSymKey(keyListHead.get()));
  while (freeKey) {
    freeKey = UniquePK11SymKey(PK11_GetNextSymKey(freeKey.get()));
  }

  return keyListHead;
}

static nsresult
GenEcKeypair(const UniquePK11SlotInfo& aSlot,
             /*out*/ UniqueSECKEYPrivateKey& aPrivKey,
             /*out*/ UniqueSECKEYPublicKey& aPubKey,
             const nsNSSShutDownPreventionLock&)
{
  MOZ_ASSERT(aSlot);
  if (NS_WARN_IF(!aSlot)) {
    return NS_ERROR_INVALID_ARG;
  }

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (NS_WARN_IF(!arena)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Set the curve parameters; keyParams belongs to the arena memory space
  SECItem* keyParams = CreateECParamsForCurve(kEcAlgorithm, arena.get());
  if (NS_WARN_IF(!keyParams)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Generate a key pair
  CK_MECHANISM_TYPE mechanism = CKM_EC_KEY_PAIR_GEN;

  SECKEYPublicKey* pubKeyRaw;
  aPrivKey = UniqueSECKEYPrivateKey(
    PK11_GenerateKeyPair(aSlot.get(), mechanism, keyParams, &pubKeyRaw,
                         /* ephemeral */ false, false,
                         /* wincx */ nullptr));
  aPubKey = UniqueSECKEYPublicKey(pubKeyRaw);
  pubKeyRaw = nullptr;
  if (NS_WARN_IF(!aPrivKey.get() || !aPubKey.get())) {
    return NS_ERROR_FAILURE;
  }

  // Check that the public key has the correct length
  if (NS_WARN_IF(aPubKey->u.ec.publicValue.len != kPublicKeyLen)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
U2FSoftTokenManager::GetOrCreateWrappingKey(const UniquePK11SlotInfo& aSlot,
                                            const nsNSSShutDownPreventionLock& locker)
{
  MOZ_ASSERT(aSlot);
  if (NS_WARN_IF(!aSlot)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Search for an existing wrapping key. If we find it,
  // store it for later and mark ourselves initialized.
  mWrappingKey = GetSymKeyByNickname(aSlot, mSecretNickname, locker);
  if (mWrappingKey) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Debug, ("U2F Soft Token Key found."));
    mInitialized = true;
    return NS_OK;
  }

  MOZ_LOG(gNSSTokenLog, LogLevel::Info,
          ("No keys found. Generating new U2F Soft Token wrapping key."));

  // We did not find an existing wrapping key, so we generate one in the
  // persistent database (e.g, Token).
  mWrappingKey = UniquePK11SymKey(
    PK11_TokenKeyGenWithFlags(aSlot.get(), CKM_AES_KEY_GEN,
                              /* default params */ nullptr,
                              kWrappingKeyByteLen,
                              /* empty keyid */ nullptr,
                              /* flags */ CKF_WRAP | CKF_UNWRAP,
                              /* attributes */ PK11_ATTR_TOKEN |
                              PK11_ATTR_PRIVATE,
                              /* wincx */ nullptr));

  if (NS_WARN_IF(!mWrappingKey)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to store wrapping key, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  SECStatus srv = PK11_SetSymKeyNickname(mWrappingKey.get(),
                                         mSecretNickname.get());
  if (NS_WARN_IF(srv != SECSuccess)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to set nickname, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
          ("Key stored, nickname set to %s.", mSecretNickname.get()));

  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
                                         "dom::U2FSoftTokenManager::GetOrCreateWrappingKey",
                                           [] () {
                                             MOZ_ASSERT(NS_IsMainThread());
                                             Preferences::SetUint(PREF_U2F_NSSTOKEN_COUNTER, 0);
                                           }));

  return NS_OK;
}

static nsresult
GetAttestationCertificate(const UniquePK11SlotInfo& aSlot,
                          /*out*/ UniqueSECKEYPrivateKey& aAttestPrivKey,
                          /*out*/ UniqueCERTCertificate& aAttestCert,
                          const nsNSSShutDownPreventionLock& locker)
{
  MOZ_ASSERT(aSlot);
  if (NS_WARN_IF(!aSlot)) {
    return NS_ERROR_INVALID_ARG;
  }

  UniqueSECKEYPublicKey pubKey;

  // Construct an ephemeral keypair for this Attestation Certificate
  nsresult rv = GenEcKeypair(aSlot, aAttestPrivKey, pubKey, locker);
  if (NS_WARN_IF(NS_FAILED(rv) || !aAttestPrivKey || !pubKey)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen keypair, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  // Construct the Attestation Certificate itself
  UniqueCERTName subjectName(CERT_AsciiToName(kAttestCertSubjectName.get()));
  if (NS_WARN_IF(!subjectName)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to set subject name, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  UniqueCERTSubjectPublicKeyInfo spki(
    SECKEY_CreateSubjectPublicKeyInfo(pubKey.get()));
  if (NS_WARN_IF(!spki)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to set SPKI, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  UniqueCERTCertificateRequest certreq(
    CERT_CreateCertificateRequest(subjectName.get(), spki.get(), nullptr));
  if (NS_WARN_IF(!certreq)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen CSR, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  PRTime now = PR_Now();
  PRTime notBefore = now - kExpirationSlack;
  PRTime notAfter = now + kExpirationLife;

  UniqueCERTValidity validity(CERT_CreateValidity(notBefore, notAfter));
  if (NS_WARN_IF(!validity)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen validity, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  unsigned long serial;
  unsigned char* serialBytes =
    mozilla::BitwiseCast<unsigned char*, unsigned long*>(&serial);
  SECStatus srv = PK11_GenerateRandomOnSlot(aSlot.get(), serialBytes,
                                            sizeof(serial));
  if (NS_WARN_IF(srv != SECSuccess)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen serial, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }
  // Ensure that the most significant bit isn't set (which would
  // indicate a negative number, which isn't valid for serial
  // numbers).
  serialBytes[0] &= 0x7f;
  // Also ensure that the least significant bit on the most
  // significant byte is set (to prevent a leading zero byte,
  // which also wouldn't be valid).
  serialBytes[0] |= 0x01;

  aAttestCert = UniqueCERTCertificate(
    CERT_CreateCertificate(serial, subjectName.get(), validity.get(),
                           certreq.get()));
  if (NS_WARN_IF(!aAttestCert)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen certificate, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  PLArenaPool* arena = aAttestCert->arena;

  srv = SECOID_SetAlgorithmID(arena, &aAttestCert->signature,
                              SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE,
                              /* wincx */ nullptr);
  if (NS_WARN_IF(srv != SECSuccess)) {
    return NS_ERROR_FAILURE;
  }

  // Set version to X509v3.
  *(aAttestCert->version.data) = SEC_CERTIFICATE_VERSION_3;
  aAttestCert->version.len = 1;

  SECItem innerDER = { siBuffer, nullptr, 0 };
  if (NS_WARN_IF(!SEC_ASN1EncodeItem(arena, &innerDER, aAttestCert.get(),
                                     SEC_ASN1_GET(CERT_CertificateTemplate)))) {
    return NS_ERROR_FAILURE;
  }

  SECItem* signedCert = PORT_ArenaZNew(arena, SECItem);
  if (NS_WARN_IF(!signedCert)) {
    return NS_ERROR_FAILURE;
  }

  srv = SEC_DerSignData(arena, signedCert, innerDER.data, innerDER.len,
                        aAttestPrivKey.get(),
                        SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE);
  if (NS_WARN_IF(srv != SECSuccess)) {
    return NS_ERROR_FAILURE;
  }
  aAttestCert->derCert = *signedCert;

  MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
          ("U2F Soft Token attestation certificate generated."));
  return NS_OK;
}

// Set up the context for the soft U2F Token. This is called by NSS
// initialization.
nsresult
U2FSoftTokenManager::Init()
{
  // If we've already initialized, just return.
  if (mInitialized) {
    return NS_OK;
  }

  nsNSSShutDownPreventionLock locker;
  if (NS_WARN_IF(isAlreadyShutDown())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  MOZ_ASSERT(slot.get());

  // Search for an existing wrapping key, or create one.
  nsresult rv = GetOrCreateWrappingKey(slot, locker);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInitialized = true;
  MOZ_LOG(gNSSTokenLog, LogLevel::Debug, ("U2F Soft Token initialized."));
  return NS_OK;
}

// Convert a Private Key object into an opaque key handle, using AES Key Wrap
// with the long-lived aPersistentKey mixed with aAppParam to convert aPrivKey.
// The key handle's format is version || saltLen || salt || wrappedPrivateKey
static UniqueSECItem
KeyHandleFromPrivateKey(const UniquePK11SlotInfo& aSlot,
                        const UniquePK11SymKey& aPersistentKey,
                        uint8_t* aAppParam, uint32_t aAppParamLen,
                        const UniqueSECKEYPrivateKey& aPrivKey,
                        const nsNSSShutDownPreventionLock&)
{
  MOZ_ASSERT(aSlot);
  MOZ_ASSERT(aPersistentKey);
  MOZ_ASSERT(aAppParam);
  MOZ_ASSERT(aPrivKey);
  if (NS_WARN_IF(!aSlot || !aPersistentKey || !aPrivKey || !aAppParam)) {
    return nullptr;
  }

  // Generate a random salt
  uint8_t saltParam[kSaltByteLen];
  SECStatus srv = PK11_GenerateRandomOnSlot(aSlot.get(), saltParam,
                                            sizeof(saltParam));
  if (NS_WARN_IF(srv != SECSuccess)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to generate a salt, NSS error #%d", PORT_GetError()));
    return nullptr;
  }

  // Prepare the HKDF (https://tools.ietf.org/html/rfc5869)
  CK_NSS_HKDFParams hkdfParams = { true, saltParam, sizeof(saltParam),
                                   true, aAppParam, aAppParamLen };
  SECItem kdfParams = { siBuffer, (unsigned char*)&hkdfParams,
                        sizeof(hkdfParams) };

  // Derive a wrapping key from aPersistentKey, the salt, and the aAppParam.
  // CKM_AES_KEY_GEN and CKA_WRAP are key type and usage attributes of the
  // derived symmetric key and don't matter because we ignore them anyway.
  UniquePK11SymKey wrapKey(PK11_Derive(aPersistentKey.get(), CKM_NSS_HKDF_SHA256,
                                       &kdfParams, CKM_AES_KEY_GEN, CKA_WRAP,
                                       kWrappingKeyByteLen));
  if (NS_WARN_IF(!wrapKey.get())) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to derive a wrapping key, NSS error #%d", PORT_GetError()));
    return nullptr;
  }

  UniqueSECItem wrappedKey(::SECITEM_AllocItem(/* default arena */ nullptr,
                                               /* no buffer */ nullptr,
                                               kWrappedKeyBufLen));
  if (NS_WARN_IF(!wrappedKey)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Failed to allocate memory"));
    return nullptr;
  }

  UniqueSECItem param(PK11_ParamFromIV(CKM_NSS_AES_KEY_WRAP_PAD,
                                       /* default IV */ nullptr ));

  srv = PK11_WrapPrivKey(aSlot.get(), wrapKey.get(), aPrivKey.get(),
                         CKM_NSS_AES_KEY_WRAP_PAD, param.get(), wrappedKey.get(),
                         /* wincx */ nullptr);
  if (NS_WARN_IF(srv != SECSuccess)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to wrap U2F key, NSS error #%d", PORT_GetError()));
    return nullptr;
  }

  // Concatenate the salt and the wrapped Private Key together
  mozilla::dom::CryptoBuffer keyHandleBuf;
  if (NS_WARN_IF(!keyHandleBuf.SetCapacity(wrappedKey.get()->len + sizeof(saltParam) + 2,
                                           mozilla::fallible))) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Failed to allocate memory"));
    return nullptr;
  }

  // It's OK to ignore the return values here because we're writing into
  // pre-allocated space
  keyHandleBuf.AppendElement(SoftTokenHandle::Version1, mozilla::fallible);
  keyHandleBuf.AppendElement(sizeof(saltParam), mozilla::fallible);
  keyHandleBuf.AppendElements(saltParam, sizeof(saltParam), mozilla::fallible);
  keyHandleBuf.AppendSECItem(wrappedKey.get());

  UniqueSECItem keyHandle(::SECITEM_AllocItem(nullptr, nullptr, 0));
  if (NS_WARN_IF(!keyHandle)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Failed to allocate memory"));
    return nullptr;
  }

  if (NS_WARN_IF(!keyHandleBuf.ToSECItem(/* default arena */ nullptr, keyHandle.get()))) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Failed to allocate memory"));
    return nullptr;
  }
  return keyHandle;
}

// Convert an opaque key handle aKeyHandle back into a Private Key object, using
// the long-lived aPersistentKey mixed with aAppParam and the AES Key Wrap
// algorithm.
static UniqueSECKEYPrivateKey
PrivateKeyFromKeyHandle(const UniquePK11SlotInfo& aSlot,
                        const UniquePK11SymKey& aPersistentKey,
                        uint8_t* aKeyHandle, uint32_t aKeyHandleLen,
                        uint8_t* aAppParam, uint32_t aAppParamLen,
                        const nsNSSShutDownPreventionLock&)
{
  MOZ_ASSERT(aSlot);
  MOZ_ASSERT(aPersistentKey);
  MOZ_ASSERT(aKeyHandle);
  MOZ_ASSERT(aAppParam);
  MOZ_ASSERT(aAppParamLen == SHA256_LENGTH);
  if (NS_WARN_IF(!aSlot || !aPersistentKey || !aKeyHandle || !aAppParam ||
                 aAppParamLen != SHA256_LENGTH)) {
    return nullptr;
  }

  // As we only support one key format ourselves (right now), fail early if
  // we aren't that length
  if (NS_WARN_IF(aKeyHandleLen != kVersion1KeyHandleLen)) {
    return nullptr;
  }

  if (NS_WARN_IF(aKeyHandle[0] != SoftTokenHandle::Version1)) {
    // Unrecognized version
    return nullptr;
  }

  uint8_t saltLen = aKeyHandle[1];
  uint8_t* saltPtr = aKeyHandle + 2;
  if (NS_WARN_IF(saltLen != kSaltByteLen)) {
    return nullptr;
  }

  // Prepare the HKDF (https://tools.ietf.org/html/rfc5869)
  CK_NSS_HKDFParams hkdfParams = { true, saltPtr, saltLen,
                                   true, aAppParam, aAppParamLen };
  SECItem kdfParams = { siBuffer, (unsigned char*)&hkdfParams,
                        sizeof(hkdfParams) };

  // Derive a wrapping key from aPersistentKey, the salt, and the aAppParam.
  // CKM_AES_KEY_GEN and CKA_WRAP are key type and usage attributes of the
  // derived symmetric key and don't matter because we ignore them anyway.
  UniquePK11SymKey wrapKey(PK11_Derive(aPersistentKey.get(), CKM_NSS_HKDF_SHA256,
                                       &kdfParams, CKM_AES_KEY_GEN, CKA_WRAP,
                                       kWrappingKeyByteLen));
  if (NS_WARN_IF(!wrapKey.get())) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to derive a wrapping key, NSS error #%d", PORT_GetError()));
    return nullptr;
  }

  uint8_t wrappedLen = aKeyHandleLen - saltLen - 2;
  uint8_t* wrappedPtr = aKeyHandle + saltLen + 2;

  ScopedAutoSECItem wrappedKeyItem(wrappedLen);
  memcpy(wrappedKeyItem.data, wrappedPtr, wrappedKeyItem.len);

  ScopedAutoSECItem pubKey(kPublicKeyLen);

  UniqueSECItem param(PK11_ParamFromIV(CKM_NSS_AES_KEY_WRAP_PAD,
                                       /* default IV */ nullptr ));

  CK_ATTRIBUTE_TYPE usages[] = { CKA_SIGN };
  int usageCount = 1;

  UniqueSECKEYPrivateKey unwrappedKey(
    PK11_UnwrapPrivKey(aSlot.get(), wrapKey.get(), CKM_NSS_AES_KEY_WRAP_PAD,
                       param.get(), &wrappedKeyItem,
                       /* no nickname */ nullptr,
                       /* discard pubkey */ &pubKey,
                       /* not permanent */ false,
                       /* non-exportable */ true,
                       CKK_EC, usages, usageCount,
                       /* wincx */ nullptr));
  if (NS_WARN_IF(!unwrappedKey)) {
    // Not our key.
    MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
            ("Could not unwrap key handle, NSS Error #%d", PORT_GetError()));
    return nullptr;
  }

  return unwrappedKey;
}

// Return whether the provided version is supported by this token.
bool
U2FSoftTokenManager::IsCompatibleVersion(const nsAString& aVersion)
{
  return mVersion == aVersion;
}

// IsRegistered determines if the provided key handle is usable by this token.
nsresult
U2FSoftTokenManager::IsRegistered(const nsTArray<uint8_t>& aKeyHandle,
                                  const nsTArray<uint8_t>& aAppParam,
                                  bool& aResult)
{
  nsNSSShutDownPreventionLock locker;
  if (NS_WARN_IF(isAlreadyShutDown())) {
    return NS_ERROR_FAILURE;
  }

  if (!mInitialized) {
    nsresult rv = Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  MOZ_ASSERT(slot.get());

  // Decode the key handle
  UniqueSECKEYPrivateKey privKey = PrivateKeyFromKeyHandle(slot, mWrappingKey,
                                                           const_cast<uint8_t*>(aKeyHandle.Elements()),
                                                           aKeyHandle.Length(),
                                                           const_cast<uint8_t*>(aAppParam.Elements()),
                                                           aAppParam.Length(),
                                                           locker);
  aResult = privKey.get() != nullptr;
  return NS_OK;
}

// A U2F Register operation causes a new key pair to be generated by the token.
// The token then returns the public key of the key pair, and a handle to the
// private key, which is a fancy way of saying "key wrapped private key", as
// well as the generated attestation certificate and a signature using that
// certificate's private key.
//
// The KeyHandleFromPrivateKey and PrivateKeyFromKeyHandle methods perform
// the actual key wrap/unwrap operations.
//
// The format of the return registration data is as follows:
//
// Bytes  Value
// 1      0x05
// 65     public key
// 1      key handle length
// *      key handle
// ASN.1  attestation certificate
// *      attestation signature
//
RefPtr<U2FRegisterPromise>
U2FSoftTokenManager::Register(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors,
                              const nsTArray<uint8_t>& aApplication,
                              const nsTArray<uint8_t>& aChallenge)
{
  nsNSSShutDownPreventionLock locker;
  if (NS_WARN_IF(isAlreadyShutDown())) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  if (!mInitialized) {
    nsresult rv = Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return U2FRegisterPromise::CreateAndReject(rv, __func__);
    }
  }

  // Optional exclusion list.
  for (auto desc: aDescriptors) {
    bool isRegistered = false;
    nsresult rv = IsRegistered(desc.id(), aApplication, isRegistered);
    if (NS_FAILED(rv)) {
      return U2FRegisterPromise::CreateAndReject(rv, __func__);
    }
    if (isRegistered) {
      return U2FRegisterPromise::CreateAndReject(NS_ERROR_DOM_NOT_ALLOWED_ERR, __func__);
    }
  }

  // We should already have a wrapping key
  MOZ_ASSERT(mWrappingKey);

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  MOZ_ASSERT(slot.get());

  // Construct a one-time-use Attestation Certificate
  UniqueSECKEYPrivateKey attestPrivKey;
  UniqueCERTCertificate attestCert;
  nsresult rv = GetAttestationCertificate(slot, attestPrivKey, attestCert,
                                          locker);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  MOZ_ASSERT(attestCert);
  MOZ_ASSERT(attestPrivKey);

  // Generate a new keypair; the private will be wrapped into a Key Handle
  UniqueSECKEYPrivateKey privKey;
  UniqueSECKEYPublicKey pubKey;
  rv = GenEcKeypair(slot, privKey, pubKey, locker);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // The key handle will be the result of keywrap(privKey, key=mWrappingKey)
  UniqueSECItem keyHandleItem = KeyHandleFromPrivateKey(slot, mWrappingKey,
                                                        const_cast<uint8_t*>(aApplication.Elements()),
                                                        aApplication.Length(),
                                                        privKey, locker);
  if (NS_WARN_IF(!keyHandleItem.get())) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Sign the challenge using the Attestation privkey (from attestCert)
  mozilla::dom::CryptoBuffer signedDataBuf;
  if (NS_WARN_IF(!signedDataBuf.SetCapacity(1 + aApplication.Length() + aChallenge.Length() +
                                            keyHandleItem->len + kPublicKeyLen,
                                            mozilla::fallible))) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  // // It's OK to ignore the return values here because we're writing into
  // // pre-allocated space
  signedDataBuf.AppendElement(0x00, mozilla::fallible);
  signedDataBuf.AppendElements(aApplication, mozilla::fallible);
  signedDataBuf.AppendElements(aChallenge, mozilla::fallible);
  signedDataBuf.AppendSECItem(keyHandleItem.get());
  signedDataBuf.AppendSECItem(pubKey->u.ec.publicValue);

  ScopedAutoSECItem signatureItem;
  SECStatus srv = SEC_SignData(&signatureItem, signedDataBuf.Elements(),
                               signedDataBuf.Length(), attestPrivKey.get(),
                               SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE);
  if (NS_WARN_IF(srv != SECSuccess)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Signature failure: %d", PORT_GetError()));
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Serialize the registration data
  mozilla::dom::CryptoBuffer registrationBuf;
  if (NS_WARN_IF(!registrationBuf.SetCapacity(1 + kPublicKeyLen + 1 + keyHandleItem->len +
                                              attestCert.get()->derCert.len +
                                              signatureItem.len, mozilla::fallible))) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }
  registrationBuf.AppendElement(0x05, mozilla::fallible);
  registrationBuf.AppendSECItem(pubKey->u.ec.publicValue);
  registrationBuf.AppendElement(keyHandleItem->len, mozilla::fallible);
  registrationBuf.AppendSECItem(keyHandleItem.get());
  registrationBuf.AppendSECItem(attestCert.get()->derCert);
  registrationBuf.AppendSECItem(signatureItem);

  U2FRegisterResult result((nsTArray<uint8_t>(registrationBuf)));
  return U2FRegisterPromise::CreateAndResolve(Move(result), __func__);
}

// A U2F Sign operation creates a signature over the "param" arguments (plus
// some other stuff) using the private key indicated in the key handle argument.
//
// The format of the signed data is as follows:
//
//  32    Application parameter
//  1     User presence (0x01)
//  4     Counter
//  32    Challenge parameter
//
// The format of the signature data is as follows:
//
//  1     User presence
//  4     Counter
//  *     Signature
//
RefPtr<U2FSignPromise>
U2FSoftTokenManager::Sign(const nsTArray<WebAuthnScopedCredentialDescriptor>& aDescriptors,
                          const nsTArray<uint8_t>& aApplication,
                          const nsTArray<uint8_t>& aChallenge)
{
  nsNSSShutDownPreventionLock locker;
  if (NS_WARN_IF(isAlreadyShutDown())) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  nsTArray<uint8_t> keyHandle;
  for (auto desc: aDescriptors) {
    bool isRegistered = false;
    nsresult rv = IsRegistered(desc.id(), aApplication, isRegistered);
    if (NS_SUCCEEDED(rv) && isRegistered) {
      keyHandle.Assign(desc.id());
      break;
    }
  }

  // Fail if we didn't recognize a key id.
  if (keyHandle.IsEmpty()) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_DOM_NOT_ALLOWED_ERR, __func__);
  }

  MOZ_ASSERT(mWrappingKey);

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  MOZ_ASSERT(slot.get());

  if (NS_WARN_IF((aChallenge.Length() != kParamLen) || (aApplication.Length() != kParamLen))) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Parameter lengths are wrong! challenge=%d app=%d expected=%d",
             (uint32_t)aChallenge.Length(), (uint32_t)aApplication.Length(), kParamLen));

    return U2FSignPromise::CreateAndReject(NS_ERROR_ILLEGAL_VALUE, __func__);
  }

  // Decode the key handle
  UniqueSECKEYPrivateKey privKey = PrivateKeyFromKeyHandle(slot, mWrappingKey,
                                                           const_cast<uint8_t*>(keyHandle.Elements()),
                                                           keyHandle.Length(),
                                                           const_cast<uint8_t*>(aApplication.Elements()),
                                                           aApplication.Length(),
                                                           locker);
  if (NS_WARN_IF(!privKey.get())) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Couldn't get the priv key!"));
    return U2FSignPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Increment the counter and turn it into a SECItem
  mCounter += 1;
  ScopedAutoSECItem counterItem(4);
  counterItem.data[0] = (mCounter >> 24) & 0xFF;
  counterItem.data[1] = (mCounter >> 16) & 0xFF;
  counterItem.data[2] = (mCounter >>  8) & 0xFF;
  counterItem.data[3] = (mCounter >>  0) & 0xFF;
  uint32_t counter = mCounter;
  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
                                         "dom::U2FSoftTokenManager::Sign",
                                           [counter] () {
                                             MOZ_ASSERT(NS_IsMainThread());
                                             Preferences::SetUint(PREF_U2F_NSSTOKEN_COUNTER, counter);
                                           }));

  // Compute the signature
  mozilla::dom::CryptoBuffer signedDataBuf;
  if (NS_WARN_IF(!signedDataBuf.SetCapacity(1 + 4 + (2 * kParamLen), mozilla::fallible))) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  // It's OK to ignore the return values here because we're writing into
  // pre-allocated space
  signedDataBuf.AppendElements(aApplication.Elements(), aApplication.Length(),
                               mozilla::fallible);
  signedDataBuf.AppendElement(0x01, mozilla::fallible);
  signedDataBuf.AppendSECItem(counterItem);
  signedDataBuf.AppendElements(aChallenge.Elements(), aChallenge.Length(),
                               mozilla::fallible);

  if (MOZ_LOG_TEST(gNSSTokenLog, LogLevel::Debug)) {
    nsAutoCString base64;
    nsresult rv = Base64URLEncode(signedDataBuf.Length(), signedDataBuf.Elements(),
                                  Base64URLEncodePaddingPolicy::Omit, base64);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return U2FSignPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }

    MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
            ("U2F Token signing bytes (base64): %s", base64.get()));
  }

  ScopedAutoSECItem signatureItem;
  SECStatus srv = SEC_SignData(&signatureItem, signedDataBuf.Elements(),
                               signedDataBuf.Length(), privKey.get(),
                               SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE);
  if (NS_WARN_IF(srv != SECSuccess)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Signature failure: %d", PORT_GetError()));
    return U2FSignPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Assemble the signature data into a buffer for return
  mozilla::dom::CryptoBuffer signatureBuf;
  if (NS_WARN_IF(!signatureBuf.SetCapacity(1 + counterItem.len + signatureItem.len,
                                           mozilla::fallible))) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  // It's OK to ignore the return values here because we're writing into
  // pre-allocated space
  signatureBuf.AppendElement(0x01, mozilla::fallible);
  signatureBuf.AppendSECItem(counterItem);
  signatureBuf.AppendSECItem(signatureItem);

  U2FSignResult result(Move(keyHandle), nsTArray<uint8_t>(signatureBuf));
  return U2FSignPromise::CreateAndResolve(Move(result), __func__);
}

void
U2FSoftTokenManager::Cancel()
{
  // This implementation is sync, requests can't be aborted.
}

}
}
