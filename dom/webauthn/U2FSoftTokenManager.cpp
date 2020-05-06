/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebAuthnCoseIdentifiers.h"
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
const uint32_t kWrappingKeyByteLen = 128 / 8;
const uint32_t kSaltByteLen = 64 / 8;
const uint32_t kVersion1KeyHandleLen = 162;
NS_NAMED_LITERAL_STRING(kEcAlgorithm, WEBCRYPTO_NAMED_CURVE_P256);

const PRTime kOneDay = PRTime(PR_USEC_PER_SEC) * PRTime(60)  // sec
                       * PRTime(60)                          // min
                       * PRTime(24);                         // hours
const PRTime kExpirationSlack = kOneDay;  // Pre-date for clock skew
const PRTime kExpirationLife = kOneDay;

static mozilla::LazyLogModule gNSSTokenLog("webauth_u2f");

enum SoftTokenHandle {
  Version1 = 0,
};

}  // namespace

U2FSoftTokenManager::U2FSoftTokenManager(uint32_t aCounter)
    : mInitialized(false), mCounter(aCounter) {}

/**
 * Gets the first key with the given nickname from the given slot. Any other
 * keys found are not returned.
 * PK11_GetNextSymKey() should not be called on the returned key.
 *
 * @param aSlot Slot to search.
 * @param aNickname Nickname the key should have.
 * @return The first key found. nullptr if no key could be found.
 */
static UniquePK11SymKey GetSymKeyByNickname(const UniquePK11SlotInfo& aSlot,
                                            const nsCString& aNickname) {
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

static nsresult GenEcKeypair(const UniquePK11SlotInfo& aSlot,
                             /*out*/ UniqueSECKEYPrivateKey& aPrivKey,
                             /*out*/ UniqueSECKEYPublicKey& aPubKey) {
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

nsresult U2FSoftTokenManager::GetOrCreateWrappingKey(
    const UniquePK11SlotInfo& aSlot) {
  MOZ_ASSERT(aSlot);
  if (NS_WARN_IF(!aSlot)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Search for an existing wrapping key. If we find it,
  // store it for later and mark ourselves initialized.
  mWrappingKey = GetSymKeyByNickname(aSlot, mSecretNickname);
  if (mWrappingKey) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Debug, ("U2F Soft Token Key found."));
    mInitialized = true;
    return NS_OK;
  }

  MOZ_LOG(gNSSTokenLog, LogLevel::Info,
          ("No keys found. Generating new U2F Soft Token wrapping key."));

  // We did not find an existing wrapping key, so we generate one in the
  // persistent database (e.g, Token).
  mWrappingKey = UniquePK11SymKey(PK11_TokenKeyGenWithFlags(
      aSlot.get(), CKM_AES_KEY_GEN,
      /* default params */ nullptr, kWrappingKeyByteLen,
      /* empty keyid */ nullptr,
      /* flags */ CKF_WRAP | CKF_UNWRAP,
      /* attributes */ PK11_ATTR_TOKEN | PK11_ATTR_PRIVATE,
      /* wincx */ nullptr));

  if (NS_WARN_IF(!mWrappingKey)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to store wrapping key, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  SECStatus srv =
      PK11_SetSymKeyNickname(mWrappingKey.get(), mSecretNickname.get());
  if (NS_WARN_IF(srv != SECSuccess)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to set nickname, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
          ("Key stored, nickname set to %s.", mSecretNickname.get()));

  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
      "dom::U2FSoftTokenManager::GetOrCreateWrappingKey", []() {
        MOZ_ASSERT(NS_IsMainThread());
        Preferences::SetUint(PREF_U2F_NSSTOKEN_COUNTER, 0);
      }));

  return NS_OK;
}

static nsresult GetAttestationCertificate(
    const UniquePK11SlotInfo& aSlot,
    /*out*/ UniqueSECKEYPrivateKey& aAttestPrivKey,
    /*out*/ UniqueCERTCertificate& aAttestCert) {
  MOZ_ASSERT(aSlot);
  if (NS_WARN_IF(!aSlot)) {
    return NS_ERROR_INVALID_ARG;
  }

  UniqueSECKEYPublicKey pubKey;

  // Construct an ephemeral keypair for this Attestation Certificate
  nsresult rv = GenEcKeypair(aSlot, aAttestPrivKey, pubKey);
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
  SECStatus srv =
      PK11_GenerateRandomOnSlot(aSlot.get(), serialBytes, sizeof(serial));
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

  aAttestCert = UniqueCERTCertificate(CERT_CreateCertificate(
      serial, subjectName.get(), validity.get(), certreq.get()));
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

  SECItem innerDER = {siBuffer, nullptr, 0};
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
nsresult U2FSoftTokenManager::Init() {
  // If we've already initialized, just return.
  if (mInitialized) {
    return NS_OK;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  MOZ_ASSERT(slot.get());

  // Search for an existing wrapping key, or create one.
  nsresult rv = GetOrCreateWrappingKey(slot);
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
static UniqueSECItem KeyHandleFromPrivateKey(
    const UniquePK11SlotInfo& aSlot, const UniquePK11SymKey& aPersistentKey,
    uint8_t* aAppParam, uint32_t aAppParamLen,
    const UniqueSECKEYPrivateKey& aPrivKey) {
  MOZ_ASSERT(aSlot);
  MOZ_ASSERT(aPersistentKey);
  MOZ_ASSERT(aAppParam);
  MOZ_ASSERT(aPrivKey);
  if (NS_WARN_IF(!aSlot || !aPersistentKey || !aPrivKey || !aAppParam)) {
    return nullptr;
  }

  // Generate a random salt
  uint8_t saltParam[kSaltByteLen];
  SECStatus srv =
      PK11_GenerateRandomOnSlot(aSlot.get(), saltParam, sizeof(saltParam));
  if (NS_WARN_IF(srv != SECSuccess)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to generate a salt, NSS error #%d", PORT_GetError()));
    return nullptr;
  }

  // Prepare the HKDF (https://tools.ietf.org/html/rfc5869)
  CK_NSS_HKDFParams hkdfParams = {true, saltParam, sizeof(saltParam),
                                  true, aAppParam, aAppParamLen};
  SECItem kdfParams = {siBuffer, (unsigned char*)&hkdfParams,
                       sizeof(hkdfParams)};

  // Derive a wrapping key from aPersistentKey, the salt, and the aAppParam.
  // CKM_AES_KEY_GEN and CKA_WRAP are key type and usage attributes of the
  // derived symmetric key and don't matter because we ignore them anyway.
  UniquePK11SymKey wrapKey(
      PK11_Derive(aPersistentKey.get(), CKM_NSS_HKDF_SHA256, &kdfParams,
                  CKM_AES_KEY_GEN, CKA_WRAP, kWrappingKeyByteLen));
  if (NS_WARN_IF(!wrapKey.get())) {
    MOZ_LOG(
        gNSSTokenLog, LogLevel::Warning,
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
                                       /* default IV */ nullptr));

  srv =
      PK11_WrapPrivKey(aSlot.get(), wrapKey.get(), aPrivKey.get(),
                       CKM_NSS_AES_KEY_WRAP_PAD, param.get(), wrappedKey.get(),
                       /* wincx */ nullptr);
  if (NS_WARN_IF(srv != SECSuccess)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to wrap U2F key, NSS error #%d", PORT_GetError()));
    return nullptr;
  }

  // Concatenate the salt and the wrapped Private Key together
  mozilla::dom::CryptoBuffer keyHandleBuf;
  if (NS_WARN_IF(!keyHandleBuf.SetCapacity(
          wrappedKey.get()->len + sizeof(saltParam) + 2, mozilla::fallible))) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Failed to allocate memory"));
    return nullptr;
  }

  // It's OK to ignore the return values here because we're writing into
  // pre-allocated space
  (void)keyHandleBuf.AppendElement(SoftTokenHandle::Version1,
                                   mozilla::fallible);
  (void)keyHandleBuf.AppendElement(sizeof(saltParam), mozilla::fallible);
  (void)keyHandleBuf.AppendElements(saltParam, sizeof(saltParam),
                                    mozilla::fallible);
  keyHandleBuf.AppendSECItem(wrappedKey.get());

  UniqueSECItem keyHandle(::SECITEM_AllocItem(nullptr, nullptr, 0));
  if (NS_WARN_IF(!keyHandle)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Failed to allocate memory"));
    return nullptr;
  }

  if (NS_WARN_IF(!keyHandleBuf.ToSECItem(/* default arena */ nullptr,
                                         keyHandle.get()))) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Failed to allocate memory"));
    return nullptr;
  }
  return keyHandle;
}

// Convert an opaque key handle aKeyHandle back into a Private Key object, using
// the long-lived aPersistentKey mixed with aAppParam and the AES Key Wrap
// algorithm.
static UniqueSECKEYPrivateKey PrivateKeyFromKeyHandle(
    const UniquePK11SlotInfo& aSlot, const UniquePK11SymKey& aPersistentKey,
    uint8_t* aKeyHandle, uint32_t aKeyHandleLen, uint8_t* aAppParam,
    uint32_t aAppParamLen) {
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
  CK_NSS_HKDFParams hkdfParams = {true, saltPtr,   saltLen,
                                  true, aAppParam, aAppParamLen};
  SECItem kdfParams = {siBuffer, (unsigned char*)&hkdfParams,
                       sizeof(hkdfParams)};

  // Derive a wrapping key from aPersistentKey, the salt, and the aAppParam.
  // CKM_AES_KEY_GEN and CKA_WRAP are key type and usage attributes of the
  // derived symmetric key and don't matter because we ignore them anyway.
  UniquePK11SymKey wrapKey(
      PK11_Derive(aPersistentKey.get(), CKM_NSS_HKDF_SHA256, &kdfParams,
                  CKM_AES_KEY_GEN, CKA_WRAP, kWrappingKeyByteLen));
  if (NS_WARN_IF(!wrapKey.get())) {
    MOZ_LOG(
        gNSSTokenLog, LogLevel::Warning,
        ("Failed to derive a wrapping key, NSS error #%d", PORT_GetError()));
    return nullptr;
  }

  uint8_t wrappedLen = aKeyHandleLen - saltLen - 2;
  uint8_t* wrappedPtr = aKeyHandle + saltLen + 2;

  ScopedAutoSECItem wrappedKeyItem(wrappedLen);
  memcpy(wrappedKeyItem.data, wrappedPtr, wrappedKeyItem.len);

  ScopedAutoSECItem pubKey(kPublicKeyLen);

  UniqueSECItem param(PK11_ParamFromIV(CKM_NSS_AES_KEY_WRAP_PAD,
                                       /* default IV */ nullptr));

  CK_ATTRIBUTE_TYPE usages[] = {CKA_SIGN};
  int usageCount = 1;

  UniqueSECKEYPrivateKey unwrappedKey(
      PK11_UnwrapPrivKey(aSlot.get(), wrapKey.get(), CKM_NSS_AES_KEY_WRAP_PAD,
                         param.get(), &wrappedKeyItem,
                         /* no nickname */ nullptr,
                         /* discard pubkey */ &pubKey,
                         /* not permanent */ false,
                         /* non-exportable */ true, CKK_EC, usages, usageCount,
                         /* wincx */ nullptr));
  if (NS_WARN_IF(!unwrappedKey)) {
    // Not our key.
    MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
            ("Could not unwrap key handle, NSS Error #%d", PORT_GetError()));
    return nullptr;
  }

  return unwrappedKey;
}

// IsRegistered determines if the provided key handle is usable by this token.
nsresult U2FSoftTokenManager::IsRegistered(const nsTArray<uint8_t>& aKeyHandle,
                                           const nsTArray<uint8_t>& aAppParam,
                                           bool& aResult) {
  if (!mInitialized) {
    nsresult rv = Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  MOZ_ASSERT(slot.get());

  // Decode the key handle
  UniqueSECKEYPrivateKey privKey = PrivateKeyFromKeyHandle(
      slot, mWrappingKey, const_cast<uint8_t*>(aKeyHandle.Elements()),
      aKeyHandle.Length(), const_cast<uint8_t*>(aAppParam.Elements()),
      aAppParam.Length());
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
RefPtr<U2FRegisterPromise> U2FSoftTokenManager::Register(
    const WebAuthnMakeCredentialInfo& aInfo, bool aForceNoneAttestation) {
  if (!mInitialized) {
    nsresult rv = Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return U2FRegisterPromise::CreateAndReject(rv, __func__);
    }
  }

  if (aInfo.Extra().isSome()) {
    const auto& extra = aInfo.Extra().ref();
    const WebAuthnAuthenticatorSelection& sel = extra.AuthenticatorSelection();

    UserVerificationRequirement userVerificaitonRequirement =
        sel.userVerificationRequirement();

    bool requireUserVerification =
        userVerificaitonRequirement == UserVerificationRequirement::Required;

    bool requirePlatformAttachment = false;
    if (sel.authenticatorAttachment().isSome()) {
      const AuthenticatorAttachment authenticatorAttachment =
          sel.authenticatorAttachment().value();
      if (authenticatorAttachment == AuthenticatorAttachment::Platform) {
        requirePlatformAttachment = true;
      }
    }

    // The U2F softtoken neither supports resident keys or
    // user verification, nor is it a platform authenticator.
    if (sel.requireResidentKey() || requireUserVerification ||
        requirePlatformAttachment) {
      return U2FRegisterPromise::CreateAndReject(NS_ERROR_DOM_NOT_ALLOWED_ERR,
                                                 __func__);
    }

    nsTArray<CoseAlg> coseAlgos;
    for (const auto& coseAlg : extra.coseAlgs()) {
      switch (static_cast<CoseAlgorithmIdentifier>(coseAlg.alg())) {
        case CoseAlgorithmIdentifier::ES256:
          coseAlgos.AppendElement(coseAlg);
          break;
        default:
          continue;
      }
    }

    // Only if no algorithms were specified, default to the one the soft token
    // supports.
    if (extra.coseAlgs().IsEmpty()) {
      coseAlgos.AppendElement(
          static_cast<int32_t>(CoseAlgorithmIdentifier::ES256));
    }

    // If there are no acceptable/supported algorithms, reject the promise.
    if (coseAlgos.IsEmpty()) {
      return U2FRegisterPromise::CreateAndReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                                                 __func__);
    }
  }

  CryptoBuffer rpIdHash, clientDataHash;
  NS_ConvertUTF16toUTF8 rpId(aInfo.RpId());
  nsresult rv = BuildTransactionHashes(rpId, aInfo.ClientDataJSON(), rpIdHash,
                                       clientDataHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_DOM_UNKNOWN_ERR,
                                               __func__);
  }

  // Optional exclusion list.
  for (const WebAuthnScopedCredential& cred : aInfo.ExcludeList()) {
    bool isRegistered = false;
    nsresult rv = IsRegistered(cred.id(), rpIdHash, isRegistered);
    if (NS_FAILED(rv)) {
      return U2FRegisterPromise::CreateAndReject(rv, __func__);
    }
    if (isRegistered) {
      return U2FRegisterPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                                 __func__);
    }
  }

  // We should already have a wrapping key
  MOZ_ASSERT(mWrappingKey);

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  MOZ_ASSERT(slot.get());

  // Construct a one-time-use Attestation Certificate
  UniqueSECKEYPrivateKey attestPrivKey;
  UniqueCERTCertificate attestCert;
  rv = GetAttestationCertificate(slot, attestPrivKey, attestCert);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  MOZ_ASSERT(attestCert);
  MOZ_ASSERT(attestPrivKey);

  // Generate a new keypair; the private will be wrapped into a Key Handle
  UniqueSECKEYPrivateKey privKey;
  UniqueSECKEYPublicKey pubKey;
  rv = GenEcKeypair(slot, privKey, pubKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // The key handle will be the result of keywrap(privKey, key=mWrappingKey)
  UniqueSECItem keyHandleItem = KeyHandleFromPrivateKey(
      slot, mWrappingKey, const_cast<uint8_t*>(rpIdHash.Elements()),
      rpIdHash.Length(), privKey);
  if (NS_WARN_IF(!keyHandleItem.get())) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Sign the challenge using the Attestation privkey (from attestCert)
  mozilla::dom::CryptoBuffer signedDataBuf;
  if (NS_WARN_IF(!signedDataBuf.SetCapacity(
          1 + rpIdHash.Length() + clientDataHash.Length() + keyHandleItem->len +
              kPublicKeyLen,
          mozilla::fallible))) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY,
                                               __func__);
  }

  // // It's OK to ignore the return values here because we're writing into
  // // pre-allocated space
  (void)signedDataBuf.AppendElement(0x00, mozilla::fallible);
  (void)signedDataBuf.AppendElements(rpIdHash, mozilla::fallible);
  (void)signedDataBuf.AppendElements(clientDataHash, mozilla::fallible);
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
  if (NS_WARN_IF(!registrationBuf.SetCapacity(
          1 + kPublicKeyLen + 1 + keyHandleItem->len +
              attestCert.get()->derCert.len + signatureItem.len,
          mozilla::fallible))) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY,
                                               __func__);
  }
  (void)registrationBuf.AppendElement(0x05, mozilla::fallible);
  registrationBuf.AppendSECItem(pubKey->u.ec.publicValue);
  (void)registrationBuf.AppendElement(keyHandleItem->len, mozilla::fallible);
  registrationBuf.AppendSECItem(keyHandleItem.get());
  registrationBuf.AppendSECItem(attestCert.get()->derCert);
  registrationBuf.AppendSECItem(signatureItem);

  CryptoBuffer keyHandleBuf;
  if (!keyHandleBuf.AppendSECItem(keyHandleItem.get())) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  CryptoBuffer attestCertBuf;
  if (!attestCertBuf.AppendSECItem(attestCert.get()->derCert)) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  CryptoBuffer signatureBuf;
  if (!signatureBuf.AppendSECItem(signatureItem)) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  CryptoBuffer pubKeyBuf;
  if (!pubKeyBuf.AppendSECItem(pubKey->u.ec.publicValue)) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  CryptoBuffer attObj;
  rv = AssembleAttestationObject(rpIdHash, pubKeyBuf, keyHandleBuf,
                                 attestCertBuf, signatureBuf,
                                 aForceNoneAttestation, attObj);
  if (NS_FAILED(rv)) {
    return U2FRegisterPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  nsTArray<WebAuthnExtensionResult> extensions;
  WebAuthnMakeCredentialResult result(aInfo.ClientDataJSON(), attObj,
                                      keyHandleBuf, registrationBuf,
                                      extensions);
  return U2FRegisterPromise::CreateAndResolve(std::move(result), __func__);
}

bool U2FSoftTokenManager::FindRegisteredKeyHandle(
    const nsTArray<nsTArray<uint8_t>>& aAppIds,
    const nsTArray<WebAuthnScopedCredential>& aCredentials,
    /*out*/ nsTArray<uint8_t>& aKeyHandle,
    /*out*/ nsTArray<uint8_t>& aAppId) {
  for (const nsTArray<uint8_t>& app_id : aAppIds) {
    for (const WebAuthnScopedCredential& cred : aCredentials) {
      bool isRegistered = false;
      nsresult rv = IsRegistered(cred.id(), app_id, isRegistered);
      if (NS_SUCCEEDED(rv) && isRegistered) {
        aKeyHandle.Assign(cred.id());
        aAppId.Assign(app_id);
        return true;
      }
    }
  }

  return false;
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
RefPtr<U2FSignPromise> U2FSoftTokenManager::Sign(
    const WebAuthnGetAssertionInfo& aInfo) {
  if (!mInitialized) {
    nsresult rv = Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return U2FSignPromise::CreateAndReject(rv, __func__);
    }
  }

  CryptoBuffer rpIdHash, clientDataHash;
  NS_ConvertUTF16toUTF8 rpId(aInfo.RpId());
  nsresult rv = BuildTransactionHashes(rpId, aInfo.ClientDataJSON(), rpIdHash,
                                       clientDataHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
  }

  nsTArray<nsTArray<uint8_t>> appIds;
  appIds.AppendElement(std::move(rpIdHash));

  if (aInfo.Extra().isSome()) {
    const auto& extra = aInfo.Extra().ref();

    UserVerificationRequirement userVerificaitonReq =
        extra.userVerificationRequirement();

    // The U2F softtoken doesn't support user verification.
    if (userVerificaitonReq == UserVerificationRequirement::Required) {
      return U2FSignPromise::CreateAndReject(NS_ERROR_DOM_NOT_ALLOWED_ERR,
                                             __func__);
    }

    // Process extensions.
    for (const WebAuthnExtension& ext : extra.Extensions()) {
      if (ext.type() == WebAuthnExtension::TWebAuthnExtensionAppId) {
        appIds.AppendElement(ext.get_WebAuthnExtensionAppId().AppId().Clone());
      }
    }
  }

  nsTArray<uint8_t> chosenAppId;
  nsTArray<uint8_t> keyHandle;

  // Fail if we can't find a valid key handle.
  if (!FindRegisteredKeyHandle(appIds, aInfo.AllowList(), keyHandle,
                               chosenAppId)) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                           __func__);
  }

  MOZ_ASSERT(mWrappingKey);

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  MOZ_ASSERT(slot.get());

  if (NS_WARN_IF((clientDataHash.Length() != kParamLen) ||
                 (chosenAppId.Length() != kParamLen))) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Parameter lengths are wrong! challenge=%d app=%d expected=%d",
             (uint32_t)clientDataHash.Length(), (uint32_t)chosenAppId.Length(),
             kParamLen));

    return U2FSignPromise::CreateAndReject(NS_ERROR_ILLEGAL_VALUE, __func__);
  }

  // Decode the key handle
  UniqueSECKEYPrivateKey privKey = PrivateKeyFromKeyHandle(
      slot, mWrappingKey, const_cast<uint8_t*>(keyHandle.Elements()),
      keyHandle.Length(), const_cast<uint8_t*>(chosenAppId.Elements()),
      chosenAppId.Length());
  if (NS_WARN_IF(!privKey.get())) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Couldn't get the priv key!"));
    return U2FSignPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Increment the counter and turn it into a SECItem
  mCounter += 1;
  ScopedAutoSECItem counterItem(4);
  counterItem.data[0] = (mCounter >> 24) & 0xFF;
  counterItem.data[1] = (mCounter >> 16) & 0xFF;
  counterItem.data[2] = (mCounter >> 8) & 0xFF;
  counterItem.data[3] = (mCounter >> 0) & 0xFF;
  uint32_t counter = mCounter;
  GetMainThreadEventTarget()->Dispatch(
      NS_NewRunnableFunction("dom::U2FSoftTokenManager::Sign", [counter]() {
        MOZ_ASSERT(NS_IsMainThread());
        Preferences::SetUint(PREF_U2F_NSSTOKEN_COUNTER, counter);
      }));

  // Compute the signature
  mozilla::dom::CryptoBuffer signedDataBuf;
  if (NS_WARN_IF(!signedDataBuf.SetCapacity(1 + 4 + (2 * kParamLen),
                                            mozilla::fallible))) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  // It's OK to ignore the return values here because we're writing into
  // pre-allocated space
  (void)signedDataBuf.AppendElements(chosenAppId.Elements(),
                                     chosenAppId.Length(), mozilla::fallible);
  (void)signedDataBuf.AppendElement(0x01, mozilla::fallible);
  signedDataBuf.AppendSECItem(counterItem);
  (void)signedDataBuf.AppendElements(
      clientDataHash.Elements(), clientDataHash.Length(), mozilla::fallible);

  if (MOZ_LOG_TEST(gNSSTokenLog, LogLevel::Debug)) {
    nsAutoCString base64;
    nsresult rv =
        Base64URLEncode(signedDataBuf.Length(), signedDataBuf.Elements(),
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
  mozilla::dom::CryptoBuffer signatureDataBuf;
  if (NS_WARN_IF(!signatureDataBuf.SetCapacity(
          1 + counterItem.len + signatureItem.len, mozilla::fallible))) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  // It's OK to ignore the return values here because we're writing into
  // pre-allocated space
  (void)signatureDataBuf.AppendElement(0x01, mozilla::fallible);
  signatureDataBuf.AppendSECItem(counterItem);
  signatureDataBuf.AppendSECItem(signatureItem);

  nsTArray<WebAuthnExtensionResult> extensions;

  if (chosenAppId != rpIdHash) {
    // Indicate to the RP that we used the FIDO appId.
    extensions.AppendElement(WebAuthnExtensionResultAppId(true));
  }

  CryptoBuffer counterBuf;
  if (!counterBuf.AppendSECItem(counterItem)) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  CryptoBuffer signatureBuf;
  if (!signatureBuf.AppendSECItem(signatureItem)) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  CryptoBuffer chosenAppIdBuf;
  if (!chosenAppIdBuf.Assign(chosenAppId)) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  CryptoBuffer authenticatorData;
  CryptoBuffer emptyAttestationData;
  rv = AssembleAuthenticatorData(chosenAppIdBuf, 0x01, counterBuf,
                                 emptyAttestationData, authenticatorData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return U2FSignPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  nsTArray<uint8_t> userHandle;

  WebAuthnGetAssertionResult result(aInfo.ClientDataJSON(), keyHandle,
                                    signatureBuf, authenticatorData, extensions,
                                    signatureDataBuf, userHandle);
  return U2FSignPromise::CreateAndResolve(std::move(result), __func__);
}

void U2FSoftTokenManager::Cancel() {
  // This implementation is sync, requests can't be aborted.
}

}  // namespace dom
}  // namespace mozilla
