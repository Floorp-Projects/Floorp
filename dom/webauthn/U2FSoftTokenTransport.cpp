/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CtapResults.h"
#include "WebAuthnCoseIdentifiers.h"
#include "WebAuthnEnumStrings.h"
#include "U2FSoftTokenTransport.h"
#include "mozilla/dom/WebAuthnUtil.h"
#include "CryptoBuffer.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "mozilla/Preferences.h"
#include "nsNSSComponent.h"
#include "nsThreadUtils.h"
#include "pk11pub.h"
#include "prerror.h"
#include "secerr.h"
#include "WebCryptoCommon.h"

#define PREF_U2F_NSSTOKEN_COUNTER "security.webauth.softtoken_counter"

namespace mozilla::dom {

using namespace mozilla;
using mozilla::dom::CreateECParamsForCurve;

const nsCString U2FSoftTokenTransport::mSecretNickname = "U2F_NSSTOKEN"_ns;

namespace {
constexpr auto kAttestCertSubjectName = "CN=Firefox U2F Soft Token"_ns;

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
constexpr auto kEcAlgorithm =
    NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_NAMED_CURVE_P256);

const PRTime kOneDay = PRTime(PR_USEC_PER_SEC) * PRTime(60)  // sec
                       * PRTime(60)                          // min
                       * PRTime(24);                         // hours
const PRTime kExpirationSlack = kOneDay;  // Pre-date for clock skew
const PRTime kExpirationLife = kOneDay;

static mozilla::LazyLogModule gNSSTokenLog("webauthn_softtoken");

enum SoftTokenHandle {
  Version1 = 0,
};

}  // namespace

NS_IMPL_ISUPPORTS(U2FSoftTokenTransport, nsIWebAuthnTransport)

U2FSoftTokenTransport::U2FSoftTokenTransport(uint32_t aCounter)
    : mInitialized(false), mCounter(aCounter), mController(nullptr) {}

NS_IMETHODIMP
U2FSoftTokenTransport::GetController(nsIWebAuthnController** aController) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
U2FSoftTokenTransport::SetController(nsIWebAuthnController* aController) {
  mController = aController;
  return NS_OK;
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

nsresult U2FSoftTokenTransport::GetOrCreateWrappingKey(
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

  GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      "dom::U2FSoftTokenTransport::GetOrCreateWrappingKey", []() {
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

  // NB: CERTCertificates created with CERT_CreateCertificate are not safe to
  // use with other NSS functions like CERT_DupCertificate.
  // The strategy here is to create a tbsCertificate ("to-be-signed
  // certificate"), encode it, and sign it, resulting in a signed DER
  // certificate that can be decoded into a CERTCertificate.
  UniqueCERTCertificate tbsCertificate(CERT_CreateCertificate(
      serial, subjectName.get(), validity.get(), certreq.get()));
  if (NS_WARN_IF(!tbsCertificate)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen certificate, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  PLArenaPool* arena = tbsCertificate->arena;

  srv = SECOID_SetAlgorithmID(arena, &tbsCertificate->signature,
                              SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE,
                              /* wincx */ nullptr);
  if (NS_WARN_IF(srv != SECSuccess)) {
    return NS_ERROR_FAILURE;
  }

  // Set version to X509v3.
  *(tbsCertificate->version.data) = SEC_CERTIFICATE_VERSION_3;
  tbsCertificate->version.len = 1;

  SECItem innerDER = {siBuffer, nullptr, 0};
  if (NS_WARN_IF(!SEC_ASN1EncodeItem(arena, &innerDER, tbsCertificate.get(),
                                     SEC_ASN1_GET(CERT_CertificateTemplate)))) {
    return NS_ERROR_FAILURE;
  }

  SECItem* certDer = PORT_ArenaZNew(arena, SECItem);
  if (NS_WARN_IF(!certDer)) {
    return NS_ERROR_FAILURE;
  }

  srv = SEC_DerSignData(arena, certDer, innerDER.data, innerDER.len,
                        aAttestPrivKey.get(),
                        SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE);
  if (NS_WARN_IF(srv != SECSuccess)) {
    return NS_ERROR_FAILURE;
  }
  aAttestCert = UniqueCERTCertificate(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), certDer, nullptr, false, true));
  if (NS_WARN_IF(!aAttestCert)) {
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
          ("U2F Soft Token attestation certificate generated."));
  return NS_OK;
}

// Set up the context for the soft U2F Token. This is called by NSS
// initialization.
nsresult U2FSoftTokenTransport::Init() {
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
nsresult U2FSoftTokenTransport::IsRegistered(
    const nsTArray<uint8_t>& aKeyHandle, const nsTArray<uint8_t>& aAppParam,
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
NS_IMETHODIMP
U2FSoftTokenTransport::MakeCredential(uint64_t aTransactionId,
                                      uint64_t _aBrowsingContextId,
                                      nsICtapRegisterArgs* args) {
  nsresult rv;

  if (!mInitialized) {
    rv = Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (NS_WARN_IF(!args)) {
    return NS_ERROR_FAILURE;
  }

  bool requireResidentKey = false;
  nsString residentKey;
  rv = args->GetResidentKey(residentKey);
  if (NS_SUCCEEDED(rv) && residentKey.EqualsLiteral(
                              MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_REQUIRED)) {
    requireResidentKey = true;
  }

  bool requireUserVerification = false;
  nsString userVerification;
  // Bug 1737205 will make this infallible
  rv = args->GetUserVerification(userVerification);
  if (NS_SUCCEEDED(rv)) {
    requireUserVerification = userVerification.EqualsLiteral(
        MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED);
  }

  bool requirePlatformAttachment = false;
  nsString authenticatorAttachment;
  // Bug 1737205 will make this infallible
  rv = args->GetAuthenticatorAttachment(authenticatorAttachment);
  if (NS_SUCCEEDED(rv) && authenticatorAttachment.EqualsLiteral(
                              MOZ_WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM)) {
    requirePlatformAttachment = true;
  }

  // The U2F softtoken neither supports resident keys or
  // user verification, nor is it a platform authenticator.
  if (requireResidentKey || requireUserVerification ||
      requirePlatformAttachment) {
    return NS_ERROR_DOM_NOT_ALLOWED_ERR;
  }

  bool noneAttestationRequested = false;
  nsString attestationConveyancePreference;
  rv =
      args->GetAttestationConveyancePreference(attestationConveyancePreference);
  if (NS_SUCCEEDED(rv) &&
      attestationConveyancePreference.EqualsLiteral(
          MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE)) {
    noneAttestationRequested = true;
  }

  nsTArray<int32_t> coseAlgs;
  // Bug 1737205 will make this infallible
  rv = args->GetCoseAlgs(coseAlgs);
  // If the request does not list algs, assume ES256.
  if (NS_FAILED(rv) || coseAlgs.IsEmpty()) {
    coseAlgs.AppendElement(
        static_cast<int32_t>(CoseAlgorithmIdentifier::ES256));
  }
  // This token only supports ES256.
  coseAlgs.RemoveElementsBy([](auto& alg) {
    return alg != static_cast<int32_t>(CoseAlgorithmIdentifier::ES256);
  });
  // If there are no acceptable/supported algorithms, exit
  if (coseAlgs.IsEmpty()) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  nsString rpId;
  args->GetRpId(rpId);

  nsTArray<uint8_t> clientDataHash;
  rv = args->GetClientDataHash(clientDataHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  CryptoBuffer rpIdHash;
  rv = HashCString(NS_ConvertUTF16toUTF8(rpId), rpIdHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  // Optional exclusion list.
  nsTArray<nsTArray<uint8_t>> excludeList;
  args->GetExcludeList(excludeList);
  for (const auto& credId : excludeList) {
    bool isRegistered = false;
    nsresult rv = IsRegistered(credId, rpIdHash, isRegistered);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (isRegistered) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
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
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(attestCert);
  MOZ_ASSERT(attestPrivKey);

  // Generate a new keypair; the private will be wrapped into a Key Handle
  UniqueSECKEYPrivateKey privKey;
  UniqueSECKEYPublicKey pubKey;
  rv = GenEcKeypair(slot, privKey, pubKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  // The key handle will be the result of keywrap(privKey, key=mWrappingKey)
  UniqueSECItem keyHandleItem = KeyHandleFromPrivateKey(
      slot, mWrappingKey, const_cast<uint8_t*>(rpIdHash.Elements()),
      rpIdHash.Length(), privKey);
  if (NS_WARN_IF(!keyHandleItem.get())) {
    return NS_ERROR_FAILURE;
  }

  // Sign the challenge using the Attestation privkey (from attestCert)
  mozilla::dom::CryptoBuffer signedDataBuf;
  if (NS_WARN_IF(!signedDataBuf.SetCapacity(
          1 + rpIdHash.Length() + clientDataHash.Length() + keyHandleItem->len +
              kPublicKeyLen,
          mozilla::fallible))) {
    return NS_ERROR_FAILURE;
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
    return NS_ERROR_FAILURE;
  }

  CryptoBuffer keyHandleBuf;
  if (!keyHandleBuf.AppendSECItem(keyHandleItem.get())) {
    return NS_ERROR_FAILURE;
  }

  CryptoBuffer attestCertBuf;
  if (!attestCertBuf.AppendSECItem(attestCert.get()->derCert)) {
    return NS_ERROR_FAILURE;
  }

  CryptoBuffer signatureBuf;
  if (!signatureBuf.AppendSECItem(signatureItem)) {
    return NS_ERROR_FAILURE;
  }

  CryptoBuffer pubKeyBuf;
  if (!pubKeyBuf.AppendSECItem(pubKey->u.ec.publicValue)) {
    return NS_ERROR_FAILURE;
  }

  CryptoBuffer attObj;
  rv = AssembleAttestationObject(rpIdHash, pubKeyBuf, keyHandleBuf,
                                 attestCertBuf, signatureBuf,
                                 noneAttestationRequested, attObj);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<uint8_t> outAttObj(std::move(attObj));
  nsTArray<uint8_t> outKeyHandleBuf(std::move(keyHandleBuf));
  mController->FinishRegister(
      aTransactionId, new CtapRegisterResult(NS_OK, std::move(outAttObj),
                                             std::move(outKeyHandleBuf)));
  return NS_OK;
}

bool U2FSoftTokenTransport::FindRegisteredKeyHandle(
    const nsTArray<nsTArray<uint8_t>>& aAppIds,
    const nsTArray<nsTArray<uint8_t>>& aCredentialIds,
    /*out*/ nsTArray<uint8_t>& aKeyHandle,
    /*out*/ nsTArray<uint8_t>& aAppId) {
  for (const nsTArray<uint8_t>& app_id : aAppIds) {
    for (const nsTArray<uint8_t>& credId : aCredentialIds) {
      bool isRegistered = false;
      nsresult rv = IsRegistered(credId, app_id, isRegistered);
      if (NS_SUCCEEDED(rv) && isRegistered) {
        aKeyHandle.Clear();
        aKeyHandle.AppendElements(credId);
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
NS_IMETHODIMP
U2FSoftTokenTransport::GetAssertion(uint64_t aTransactionId,
                                    uint64_t _aBrowsingContextId,
                                    nsICtapSignArgs* args) {
  nsresult rv;
  if (!mInitialized) {
    rv = Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (NS_WARN_IF(!args)) {
    return NS_ERROR_FAILURE;
  }

  bool requireUserVerification = false;
  nsString userVerification;
  // Bug 1737205 will make this infallible
  rv = args->GetUserVerification(userVerification);
  if (NS_SUCCEEDED(rv)) {
    requireUserVerification = userVerification.EqualsLiteral(
        MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED);
  }
  if (requireUserVerification) {
    return NS_ERROR_DOM_NOT_ALLOWED_ERR;
  }

  nsString rpId;
  args->GetRpId(rpId);

  nsTArray<uint8_t> clientDataHash;
  rv = args->GetClientDataHash(clientDataHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  CryptoBuffer rpIdHash;
  rv = HashCString(NS_ConvertUTF16toUTF8(rpId), rpIdHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsTArray<uint8_t>> appIdHashes;
  appIdHashes.AppendElement(std::move(rpIdHash));

  nsTArray<uint8_t> appIdHash;
  rv = args->GetAppIdHash(appIdHash);
  if (NS_SUCCEEDED(rv)) {
    appIdHashes.AppendElement(std::move(appIdHash));
  }

  nsTArray<nsTArray<uint8_t>> allowList;
  args->GetAllowList(allowList);

  nsTArray<uint8_t> chosenAppId;
  nsTArray<uint8_t> keyHandle;

  // Fail if we can't find a valid key handle.
  if (!FindRegisteredKeyHandle(appIdHashes, allowList, keyHandle,
                               chosenAppId)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
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

    return NS_ERROR_ILLEGAL_VALUE;
  }

  // Decode the key handle
  UniqueSECKEYPrivateKey privKey = PrivateKeyFromKeyHandle(
      slot, mWrappingKey, const_cast<uint8_t*>(keyHandle.Elements()),
      keyHandle.Length(), const_cast<uint8_t*>(chosenAppId.Elements()),
      chosenAppId.Length());
  if (NS_WARN_IF(!privKey.get())) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Couldn't get the priv key!"));
    return NS_ERROR_FAILURE;
  }

  // Increment the counter and turn it into a SECItem
  mCounter += 1;
  ScopedAutoSECItem counterItem(4);
  counterItem.data[0] = (mCounter >> 24) & 0xFF;
  counterItem.data[1] = (mCounter >> 16) & 0xFF;
  counterItem.data[2] = (mCounter >> 8) & 0xFF;
  counterItem.data[3] = (mCounter >> 0) & 0xFF;
  uint32_t counter = mCounter;
  GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      "dom::U2FSoftTokenTransport::GetAssertion", [counter]() {
        MOZ_ASSERT(NS_IsMainThread());
        Preferences::SetUint(PREF_U2F_NSSTOKEN_COUNTER, counter);
      }));

  // Compute the signature
  mozilla::dom::CryptoBuffer signedDataBuf;
  if (NS_WARN_IF(!signedDataBuf.SetCapacity(1 + 4 + (2 * kParamLen),
                                            mozilla::fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
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
    rv = Base64URLEncode(signedDataBuf.Length(), signedDataBuf.Elements(),
                         Base64URLEncodePaddingPolicy::Omit, base64);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_FAILURE;
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
    return NS_ERROR_FAILURE;
  }

  nsTArray<WebAuthnExtensionResult> extensions;

  if (appIdHashes.Length() == 2) {
    bool usedAppId = (chosenAppId == appIdHashes[1]);
    extensions.AppendElement(WebAuthnExtensionResultAppId(usedAppId));
  }

  CryptoBuffer counterBuf;
  if (!counterBuf.AppendSECItem(counterItem)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CryptoBuffer signatureBuf;
  if (!signatureBuf.AppendSECItem(signatureItem)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CryptoBuffer chosenAppIdBuf;
  if (!chosenAppIdBuf.Assign(chosenAppId)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CryptoBuffer authenticatorData;
  CryptoBuffer emptyAttestationData;
  rv = AssembleAuthenticatorData(chosenAppIdBuf, 0x01, counterBuf,
                                 emptyAttestationData, authenticatorData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<uint8_t> outSignatureBuf(std::move(signatureBuf));
  nsTArray<uint8_t> outAuthenticatorData(std::move(authenticatorData));
  nsTArray<uint8_t> userHandle;  // unused because this is a CTAP1 token

  nsTArray<RefPtr<nsICtapSignResult>> results;
  results.AppendElement(new CtapSignResult(
      NS_OK, std::move(keyHandle), std::move(outSignatureBuf),
      std::move(outAuthenticatorData), std::move(userHandle),
      std::move(chosenAppId)));

  mController->FinishSign(aTransactionId, results);
  return NS_OK;
}

NS_IMETHODIMP
U2FSoftTokenTransport::Cancel(void) {
  // This implementation is sync, requests can't be aborted.
  return NS_OK;
}

NS_IMETHODIMP
U2FSoftTokenTransport::PinCallback(uint64_t aTransactionId,
                                   const nsACString& aPin) {
  // This is a U2F/CTAP1 token. It doesn't support pins.
  return NS_ERROR_FAILURE;
}

}  // namespace mozilla::dom
