/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp: tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define KEYSTORE_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define KEYSTORE_LOG(args...)  printf(args);
#endif

#include "KeyStore.h"
#include "jsfriendapi.h"
#include "KeyStoreConnector.h"
#include "MainThreadUtils.h" // For NS_IsMainThread.
#include "nsICryptoHash.h"

#include "plbase64.h"
#include "certdb.h"
#include "ScopedNSSTypes.h"

using namespace mozilla::ipc;
#if ANDROID_VERSION >= 18
// After Android 4.3, it uses binder to access keystore instead of unix socket.
#include <android/log.h>
#include <binder/BinderService.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <security/keystore/include/keystore/IKeystoreService.h>
#include <security/keystore/include/keystore/keystore.h>

using namespace android;

namespace android {
// This class is used to make compiler happy.
class BpKeystoreService : public BpInterface<IKeystoreService>
{
public:
  BpKeystoreService(const sp<IBinder>& impl)
    : BpInterface<IKeystoreService>(impl)
  {
  }

  virtual int32_t get(const String16& name, uint8_t** item, size_t* itemLength) {return 0;}
  virtual int32_t test() {return 0;}
  virtual int32_t insert(const String16& name, const uint8_t* item, size_t itemLength, int uid, int32_t flags) {return 0;}
  virtual int32_t del(const String16& name, int uid) {return 0;}
  virtual int32_t exist(const String16& name, int uid) {return 0;}
  virtual int32_t saw(const String16& name, int uid, Vector<String16>* matches) {return 0;}
  virtual int32_t reset() {return 0;}
  virtual int32_t password(const String16& password) {return 0;}
  virtual int32_t lock() {return 0;}
  virtual int32_t unlock(const String16& password) {return 0;}
  virtual int32_t zero() {return 0;}
  virtual int32_t import(const String16& name, const uint8_t* data, size_t length, int uid, int32_t flags) {return 0;}
  virtual int32_t sign(const String16& name, const uint8_t* data, size_t length, uint8_t** out, size_t* outLength) {return 0;}
  virtual int32_t verify(const String16& name, const uint8_t* data, size_t dataLength, const uint8_t* signature, size_t signatureLength) {return 0;}
  virtual int32_t get_pubkey(const String16& name, uint8_t** pubkey, size_t* pubkeyLength) {return 0;}
  virtual int32_t del_key(const String16& name, int uid) {return 0;}
  virtual int32_t grant(const String16& name, int32_t granteeUid) {return 0;}
  virtual int32_t ungrant(const String16& name, int32_t granteeUid) {return 0;}
  virtual int64_t getmtime(const String16& name) {return 0;}
  virtual int32_t duplicate(const String16& srcKey, int32_t srcUid, const String16& destKey, int32_t destUid) {return 0;}
  virtual int32_t clear_uid(int64_t uid) {return 0;}
#if ANDROID_VERSION >= 21
  virtual int32_t generate(const String16& name, int32_t uid, int32_t keyType, int32_t keySize, int32_t flags, Vector<sp<KeystoreArg> >* args) {return 0;}
  virtual int32_t is_hardware_backed(const String16& keyType) {return 0;}
  virtual int32_t reset_uid(int32_t uid) {return 0;}
  virtual int32_t sync_uid(int32_t sourceUid, int32_t targetUid) {return 0;}
  virtual int32_t password_uid(const String16& password, int32_t uid) {return 0;}
#elif ANDROID_VERSION == 18
  virtual int32_t generate(const String16& name, int uid, int32_t flags) {return 0;}
  virtual int32_t is_hardware_backed() {return 0;}
#else
  virtual int32_t generate(const String16& name, int32_t uid, int32_t keyType, int32_t keySize, int32_t flags, Vector<sp<KeystoreArg> >* args) {return 0;}
  virtual int32_t is_hardware_backed(const String16& keyType) {return 0;}
#endif
};

IMPLEMENT_META_INTERFACE(KeystoreService, "android.security.keystore");

// Here comes binder requests.
status_t BnKeystoreService::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
  switch(code) {
    case TEST: {
      CHECK_INTERFACE(IKeystoreService, data, reply);
      reply->writeNoException();
      reply->writeInt32(test());
      return NO_ERROR;
    } break;
    case GET: {
      CHECK_INTERFACE(IKeystoreService, data, reply);
      String16 name = data.readString16();
      String8 tmp(name);
      uint8_t* data = NULL;
      size_t dataLength = 0;
      int32_t ret = get(name, &data, &dataLength);

      reply->writeNoException();
      if (ret == 1) {
        reply->writeInt32(dataLength);
        void* buf = reply->writeInplace(dataLength);
        memcpy(buf, data, dataLength);
        free(data);
      } else {
        reply->writeInt32(-1);
      }
      return NO_ERROR;
    } break;
    case GET_PUBKEY: {
      CHECK_INTERFACE(IKeystoreService, data, reply);
      String16 name = data.readString16();
      uint8_t* data = nullptr;
      size_t dataLength = 0;
      int32_t ret = get_pubkey(name, &data, &dataLength);

      reply->writeNoException();
      if (dataLength > 0 && data != nullptr) {
        reply->writeInt32(dataLength);
        void* buf = reply->writeInplace(dataLength);
        memcpy(buf, data, dataLength);
        free(data);
      } else {
        reply->writeInt32(-1);
      }
      reply->writeInt32(ret);
      return NO_ERROR;
    } break;
    case SIGN: {
      CHECK_INTERFACE(IKeystoreService, data, reply);
      String16 name = data.readString16();
      ssize_t signDataSize = data.readInt32();
      const uint8_t *signData = nullptr;
      if (signDataSize >= 0 && (size_t)signDataSize <= data.dataAvail()) {
        signData = (const uint8_t *)data.readInplace(signDataSize);
      }

      uint8_t *signResult = nullptr;
      size_t signResultSize;
      int32_t ret = sign(name, signData, (size_t)signDataSize, &signResult,
                         &signResultSize);

      reply->writeNoException();
      if (signResultSize > 0 && signResult != nullptr) {
        reply->writeInt32(signResultSize);
        void* buf = reply->writeInplace(signResultSize);
        memcpy(buf, signResult, signResultSize);
        free(signResult);
      } else {
        reply->writeInt32(-1);
      }
      reply->writeInt32(ret);
      return NO_ERROR;
    } break;
    default:
      return NO_ERROR;
  }
}

// Provide service for binder.
class KeyStoreService : public BnKeystoreService
                      , public nsNSSShutDownObject
{
public:
  int32_t test() {
    uid_t callingUid = IPCThreadState::self()->getCallingUid();
    if (!mozilla::ipc::checkPermission(callingUid)) {
      return ::PERMISSION_DENIED;
    }

    return ::NO_ERROR;
  }

  int32_t get(const String16& name, uint8_t** item, size_t* itemLength) {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      return ::SYSTEM_ERROR;
    }

    uid_t callingUid = IPCThreadState::self()->getCallingUid();
    if (!mozilla::ipc::checkPermission(callingUid)) {
      return ::PERMISSION_DENIED;
    }

    String8 certName(name);
    if (!strncmp(certName.string(), "WIFI_USERKEY_", 13)) {
      return getPrivateKey(certName.string(), (const uint8_t**)item, itemLength);
    }

    return getCertificate(certName.string(), (const uint8_t**)item, itemLength);
  }

  int32_t insert(const String16& name, const uint8_t* item, size_t itemLength, int uid, int32_t flags) {return ::UNDEFINED_ACTION;}
  int32_t del(const String16& name, int uid) {return ::UNDEFINED_ACTION;}
  int32_t exist(const String16& name, int uid) {return ::UNDEFINED_ACTION;}
  int32_t saw(const String16& name, int uid, Vector<String16>* matches) {return ::UNDEFINED_ACTION;}
  int32_t reset() {return ::UNDEFINED_ACTION;}
  int32_t password(const String16& password) {return ::UNDEFINED_ACTION;}
  int32_t lock() {return ::UNDEFINED_ACTION;}
  int32_t unlock(const String16& password) {return ::UNDEFINED_ACTION;}
  int32_t zero() {return ::UNDEFINED_ACTION;}
  int32_t import(const String16& name, const uint8_t* data, size_t length, int uid, int32_t flags) {return ::UNDEFINED_ACTION;}
  int32_t sign(const String16& name, const uint8_t* data, size_t length, uint8_t** out, size_t* outLength)
  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      return ::SYSTEM_ERROR;
    }

    uid_t callingUid = IPCThreadState::self()->getCallingUid();
    if (!mozilla::ipc::checkPermission(callingUid)) {
      return ::PERMISSION_DENIED;
    }

    if (data == nullptr) {
      return ::SYSTEM_ERROR;
    }

    String8 keyName(name);
    if (!strncmp(keyName.string(), "WIFI_USERKEY_", 13)) {
      return signData(keyName.string(), data, length, out, outLength);
    }

    return ::UNDEFINED_ACTION;
  }

  int32_t verify(const String16& name, const uint8_t* data, size_t dataLength, const uint8_t* signature, size_t signatureLength) {return ::UNDEFINED_ACTION;}
  int32_t get_pubkey(const String16& name, uint8_t** pubkey, size_t* pubkeyLength) {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      return ::SYSTEM_ERROR;
    }

    uid_t callingUid = IPCThreadState::self()->getCallingUid();
    if (!mozilla::ipc::checkPermission(callingUid)) {
      return ::PERMISSION_DENIED;
    }

    String8 keyName(name);
    if (!strncmp(keyName.string(), "WIFI_USERKEY_", 13)) {
      return getPublicKey(keyName.string(), (const uint8_t**)pubkey, pubkeyLength);
    }

    return ::UNDEFINED_ACTION;
  }

  int32_t del_key(const String16& name, int uid) {return ::UNDEFINED_ACTION;}
  int32_t grant(const String16& name, int32_t granteeUid) {return ::UNDEFINED_ACTION;}
  int32_t ungrant(const String16& name, int32_t granteeUid) {return ::UNDEFINED_ACTION;}
  int64_t getmtime(const String16& name) {return ::UNDEFINED_ACTION;}
  int32_t duplicate(const String16& srcKey, int32_t srcUid, const String16& destKey, int32_t destUid) {return ::UNDEFINED_ACTION;}
  int32_t clear_uid(int64_t uid) {return ::UNDEFINED_ACTION;}
#if ANDROID_VERSION >= 21
  virtual int32_t generate(const String16& name, int32_t uid, int32_t keyType, int32_t keySize, int32_t flags, Vector<sp<KeystoreArg> >* args) {return ::UNDEFINED_ACTION;}
  virtual int32_t is_hardware_backed(const String16& keyType) {return ::UNDEFINED_ACTION;}
  virtual int32_t reset_uid(int32_t uid) {return ::UNDEFINED_ACTION;;}
  virtual int32_t sync_uid(int32_t sourceUid, int32_t targetUid) {return ::UNDEFINED_ACTION;}
  virtual int32_t password_uid(const String16& password, int32_t uid) {return ::UNDEFINED_ACTION;}
#elif ANDROID_VERSION == 18
  virtual int32_t generate(const String16& name, int uid, int32_t flags) {return ::UNDEFINED_ACTION;}
  virtual int32_t is_hardware_backed() {return ::UNDEFINED_ACTION;}
#else
  virtual int32_t generate(const String16& name, int32_t uid, int32_t keyType, int32_t keySize, int32_t flags, Vector<sp<KeystoreArg> >* args) {return ::UNDEFINED_ACTION;}
  virtual int32_t is_hardware_backed(const String16& keyType) {return ::UNDEFINED_ACTION;}
#endif

protected:
  virtual void virtualDestroyNSSReference() {}

private:
  ~KeyStoreService() {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      return;
    }
    shutdown(calledFromObject);
  }
};

} // namespace android

void startKeyStoreService()
{
  android::sp<android::IServiceManager> sm = android::defaultServiceManager();
  android::sp<android::KeyStoreService> keyStoreService = new android::KeyStoreService();
  sm->addService(String16("android.security.keystore"), keyStoreService);
}
#else
void startKeyStoreService() { return; }
#endif

static const char *CA_BEGIN = "-----BEGIN ",
                  *CA_END   = "-----END ",
                  *CA_TAILER = "-----\n";

namespace mozilla {
namespace ipc {

static const char* KEYSTORE_ALLOWED_USERS[] = {
  "root",
  "wifi",
  NULL
};
static const char* KEYSTORE_ALLOWED_PREFIXES[] = {
  "WIFI_SERVERCERT_",
  "WIFI_USERCERT_",
  "WIFI_USERKEY_",
  NULL
};

// Transform base64 certification data into DER format
void
FormatCaData(const char *aCaData, int aCaDataLength,
             const char *aName, const uint8_t **aFormatData,
             size_t *aFormatDataLength)
{
  size_t bufSize = strlen(CA_BEGIN) + strlen(CA_END) + strlen(CA_TAILER) * 2 +
                   strlen(aName) * 2 + aCaDataLength + aCaDataLength/CA_LINE_SIZE
                   + 2;
  char *buf = (char *)malloc(bufSize);
  if (!buf) {
    *aFormatData = nullptr;
    return;
  }

  *aFormatDataLength = bufSize;
  *aFormatData = (const uint8_t *)buf;

  char *ptr = buf;
  size_t len;

  // Create DER header.
  len = snprintf(ptr, bufSize, "%s%s%s", CA_BEGIN, aName, CA_TAILER);
  ptr += len;
  bufSize -= len;

  // Split base64 data in lines.
  int copySize;
  while (aCaDataLength > 0) {
    copySize = (aCaDataLength > CA_LINE_SIZE) ? CA_LINE_SIZE : aCaDataLength;

    memcpy(ptr, aCaData, copySize);
    ptr += copySize;
    aCaData += copySize;
    aCaDataLength -= copySize;
    bufSize -= copySize;

    *ptr = '\n';
    ptr++;
    bufSize--;
  }

  // Create DEA tailer.
  snprintf(ptr, bufSize, "%s%s%s", CA_END, aName, CA_TAILER);
}

ResponseCode
getCertificate(const char *aCertName, const uint8_t **aCertData,
               size_t *aCertDataLength)
{
  // certificate name prefix check.
  if (!aCertName) {
    return KEY_NOT_FOUND;
  }

  const char **prefix = KEYSTORE_ALLOWED_PREFIXES;
  for (; *prefix; prefix++ ) {
    if (!strncmp(*prefix, aCertName, strlen(*prefix))) {
      break;
    }
  }
  if (!(*prefix)) {
    return KEY_NOT_FOUND;
  }

  // Get cert from NSS by name
  ScopedCERTCertificate cert(CERT_FindCertByNickname(CERT_GetDefaultCertDB(),
                                                     aCertName));

  if (!cert) {
    return KEY_NOT_FOUND;
  }

  char *certDER = PL_Base64Encode((const char *)cert->derCert.data,
                                  cert->derCert.len, nullptr);
  if (!certDER) {
    return SYSTEM_ERROR;
  }

  FormatCaData(certDER, strlen(certDER), "CERTIFICATE", aCertData,
               aCertDataLength);
  PL_strfree(certDER);

  if (!(*aCertData)) {
    return SYSTEM_ERROR;
  }

  return SUCCESS;
}

ResponseCode getPrivateKey(const char *aKeyName, const uint8_t **aKeyData,
                           size_t *aKeyDataLength)
{
  *aKeyData = nullptr;
  // Get corresponding user certificate nickname
  char userCertName[128] = {0};
  snprintf(userCertName, sizeof(userCertName) - 1, "WIFI_USERCERT_%s", aKeyName + 13);

  // Get private key from user certificate.
  ScopedCERTCertificate userCert(
    CERT_FindCertByNickname(CERT_GetDefaultCertDB(), userCertName));
  if (!userCert) {
    return KEY_NOT_FOUND;
  }

  ScopedSECKEYPrivateKey privateKey(
    PK11_FindKeyByAnyCert(userCert.get(), nullptr));
  if (!privateKey) {
    return KEY_NOT_FOUND;
  }

  // Export private key in PKCS#12 encrypted format, no password.
  unsigned char pwstr[] = {0, 0};
  SECItem password = {siBuffer, pwstr, sizeof(pwstr)};
  ScopedSECKEYEncryptedPrivateKeyInfo encryptedPrivateKey(
    PK11_ExportEncryptedPrivKeyInfo(privateKey->pkcs11Slot,
      SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4, &password, privateKey, 1,
      privateKey->wincx));

  if (!encryptedPrivateKey) {
    return KEY_NOT_FOUND;
  }

  // Decrypt into RSA private key.
  //
  // Generate key for PKCS#12 encryption, we use SHA1 with 1 iteration, as the
  // parameters used in PK11_ExportEncryptedPrivKeyInfo() above.
  // see: PKCS#12 v1.0, B.2.
  //
  uint8_t DSP[192] = {0};
  memset(DSP, 0x01, 64);        // Diversifier part, ID = 1 for decryption.
  memset(DSP + 128, 0x00, 64);  // Password part, no password.

  uint8_t *S = &DSP[64];        // Salt part.
  uint8_t *salt = encryptedPrivateKey->algorithm.parameters.data + 4;
  int saltLength = (int)encryptedPrivateKey->algorithm.parameters.data[3];
  if (saltLength <= 0) {
    return SYSTEM_ERROR;
  }
  for (int i = 0; i < 64; i++) {
    S[i] = salt[i % saltLength];
  }

  // Generate key by SHA-1
  nsresult rv;
  nsCOMPtr<nsICryptoHash> hash =
    do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  if (NS_FAILED(rv)) {
    return SYSTEM_ERROR;
  }

  rv = hash->Init(nsICryptoHash::SHA1);
  if (NS_FAILED(rv)) {
    return SYSTEM_ERROR;
  }

  rv = hash->Update(DSP, sizeof(DSP));
  if (NS_FAILED(rv)) {
    return SYSTEM_ERROR;
  }

  nsCString hashResult;
  rv = hash->Finish(false, hashResult);
  if (NS_FAILED(rv)) {
    return SYSTEM_ERROR;
  }

  // First 40-bit as key for RC4.
  uint8_t key[5];
  memcpy(key, hashResult.get(), sizeof(key));

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return SYSTEM_ERROR;
  }

  SECItem keyItem = {siBuffer, key, sizeof(key)};
  ScopedPK11SymKey symKey(PK11_ImportSymKey(slot, CKM_RC4, PK11_OriginUnwrap,
                                            CKA_DECRYPT, &keyItem, nullptr));
  if (!symKey) {
    return SYSTEM_ERROR;
  }

  // Get expected decrypted data size then allocate memory.
  uint8_t *encryptedData = (uint8_t *)encryptedPrivateKey->encryptedData.data;
  unsigned int encryptedDataLen = encryptedPrivateKey->encryptedData.len;
  unsigned int decryptedDataLen = encryptedDataLen;
  SECStatus srv = PK11_Decrypt(symKey, CKM_RC4, &keyItem, nullptr,
                               &decryptedDataLen, encryptedDataLen,
                               encryptedData, encryptedDataLen);
  if (srv != SECSuccess) {
    return SYSTEM_ERROR;
  }

  ScopedSECItem decryptedData(::SECITEM_AllocItem(nullptr, nullptr,
                                                  decryptedDataLen));
  if (!decryptedData) {
    return SYSTEM_ERROR;
  }

  // Decrypt by RC4.
  srv = PK11_Decrypt(symKey, CKM_RC4, &keyItem, decryptedData->data,
                     &decryptedDataLen, decryptedData->len, encryptedData,
                     encryptedDataLen);
  if (srv != SECSuccess) {
    return SYSTEM_ERROR;
  }

  // Export key in PEM format.
  char *keyPEM = PL_Base64Encode((const char *)decryptedData->data,
                                 decryptedDataLen, nullptr);

  if (!keyPEM) {
    return SYSTEM_ERROR;
  }

  FormatCaData(keyPEM, strlen(keyPEM), "PRIVATE KEY", aKeyData, aKeyDataLength);
  PL_strfree(keyPEM);

  if (!(*aKeyData)) {
    return SYSTEM_ERROR;
  }

  return SUCCESS;
}

ResponseCode getPublicKey(const char *aKeyName, const uint8_t **aKeyData,
                          size_t *aKeyDataLength)
{
  *aKeyData = nullptr;

  // Get corresponding user certificate nickname
  char userCertName[128] = {0};
  snprintf(userCertName, sizeof(userCertName) - 1, "WIFI_USERCERT_%s", aKeyName + 13);

  // Get public key from user certificate.
  ScopedCERTCertificate userCert(
    CERT_FindCertByNickname(CERT_GetDefaultCertDB(), userCertName));
  if (!userCert) {
    return KEY_NOT_FOUND;
  }

  // Get public key.
  ScopedSECKEYPublicKey publicKey(CERT_ExtractPublicKey(userCert));
  if (!publicKey) {
    return KEY_NOT_FOUND;
  }

  ScopedSECItem keyItem(PK11_DEREncodePublicKey(publicKey));
  if (!keyItem) {
    return KEY_NOT_FOUND;
  }

  size_t bufSize = keyItem->len;
  char *buf = (char *)malloc(bufSize);
  if (!buf) {
    return SYSTEM_ERROR;
  }

  memcpy(buf, keyItem->data, bufSize);
  *aKeyData = (const uint8_t *)buf;
  *aKeyDataLength = bufSize;

  return SUCCESS;
}

ResponseCode signData(const char *aKeyName, const uint8_t *data, size_t length,
                      uint8_t **out, size_t *outLength)
{
  *out = nullptr;
  // Get corresponding user certificate nickname
  char userCertName[128] = {0};
  snprintf(userCertName, sizeof(userCertName) - 1, "WIFI_USERCERT_%s", aKeyName + 13);

  // Get private key from user certificate.
  ScopedCERTCertificate userCert(
    CERT_FindCertByNickname(CERT_GetDefaultCertDB(), userCertName));
  if (!userCert) {
    return KEY_NOT_FOUND;
  }

  ScopedSECKEYPrivateKey privateKey(
    PK11_FindKeyByAnyCert(userCert.get(), nullptr));
  if (!privateKey) {
    return KEY_NOT_FOUND;
  }

  //
  // Find hash data from incoming data.
  //
  // Incoming data might be padded by PKCS-1 format:
  // 00 01 FF FF ... FF 00 || Hash of length 36
  // If the padding part exists, we have to ignore them.
  //
  uint8_t *hash = (uint8_t *)data;
  const size_t HASH_LENGTH = 36;
  if (length < HASH_LENGTH) {
    return VALUE_CORRUPTED;
  }
  if (hash[0] == 0x00 && hash[1] == 0x01 && hash[2] == 0xFF && hash[3] == 0xFF) {
    hash += 4;
    while (*hash == 0xFF) {
      if (hash + HASH_LENGTH > data + length) {
        return VALUE_CORRUPTED;
      }
      hash++;
    }
    if (*hash != 0x00) {
      return VALUE_CORRUPTED;
    }
    hash++;
  }
  if (hash + HASH_LENGTH != data + length) {
    return VALUE_CORRUPTED;
  }
  SECItem hashItem = {siBuffer, hash, HASH_LENGTH};

  // Sign hash.
  ScopedSECItem signItem(::SECITEM_AllocItem(nullptr, nullptr,
                                             PK11_SignatureLen(privateKey)));
  if (!signItem) {
    return SYSTEM_ERROR;
  }

  SECStatus srv;
  srv = PK11_Sign(privateKey, signItem.get(), &hashItem);
  if (srv != SECSuccess) {
    return SYSTEM_ERROR;
  }

  uint8_t *buf = (uint8_t *)malloc(signItem->len);
  if (!buf) {
    return SYSTEM_ERROR;
  }

  memcpy(buf, signItem->data, signItem->len);
  *out = buf;
  *outLength = signItem->len;

  return SUCCESS;
}

bool
checkPermission(uid_t uid)
{
  struct passwd *userInfo = getpwuid(uid);
  for (const char **user = KEYSTORE_ALLOWED_USERS; *user; user++ ) {
    if (!strcmp(*user, userInfo->pw_name)) {
      return true;
    }
  }

  return false;
}

//
// KeyStore
//

KeyStore::KeyStore()
: mShutdown(false)
{
  MOZ_COUNT_CTOR(KeyStore);
  ::startKeyStoreService();
  Listen();
}

KeyStore::~KeyStore()
{
  nsNSSShutDownPreventionLock locker;
  MOZ_COUNT_DTOR(KeyStore);

  if (isAlreadyShutDown()) {
    return;
  }

  shutdown(calledFromObject);

  MOZ_ASSERT(!mListenSocket);
  MOZ_ASSERT(!mStreamSocket);
}

void
KeyStore::Shutdown()
{
  // We set mShutdown first, so that |OnDisconnect| won't try to reconnect.
  mShutdown = true;

  if (mStreamSocket) {
    mStreamSocket->Close();
    mStreamSocket = nullptr;
  }
  if (mListenSocket) {
    mListenSocket->Close();
    mListenSocket = nullptr;
  }
}

void
KeyStore::Listen()
{
  // We only allocate one |StreamSocket|, but re-use it for every connection.
  if (mStreamSocket) {
    mStreamSocket->Close();
  } else {
    mStreamSocket = new StreamSocket(this, STREAM_SOCKET);
  }

  if (!mListenSocket) {
    // We only ever allocate one |ListenSocket|...
    mListenSocket = new ListenSocket(this, LISTEN_SOCKET);
    mListenSocket->Listen(new KeyStoreConnector(KEYSTORE_ALLOWED_USERS),
                          mStreamSocket);
  } else {
    // ... but keep it open.
    mListenSocket->Listen(mStreamSocket);
  }

  ResetHandlerInfo();
}

void
KeyStore::ResetHandlerInfo()
{
  mHandlerInfo.state = STATE_IDLE;
  mHandlerInfo.command = 0;
  mHandlerInfo.paramCount = 0;
  mHandlerInfo.commandPattern = nullptr;
  for (int i = 0; i < MAX_PARAM; i++) {
    mHandlerInfo.param[i].length = 0;
    memset(mHandlerInfo.param[i].data, 0, VALUE_SIZE);
  }
}

bool
KeyStore::CheckSize(UnixSocketBuffer *aMessage, size_t aExpectSize)
{
  return (aMessage->GetSize() >= aExpectSize);
}

ResponseCode
KeyStore::ReadCommand(UnixSocketBuffer *aMessage)
{
  if (mHandlerInfo.state != STATE_IDLE) {
    NS_WARNING("Wrong state in ReadCommand()!");
    return SYSTEM_ERROR;
  }

  if (!CheckSize(aMessage, 1)) {
    NS_WARNING("Data size error in ReadCommand()!");
    return PROTOCOL_ERROR;
  }

  mHandlerInfo.command = *aMessage->GetData();
  aMessage->Consume(1);

  // Find corrsponding command pattern
  const struct ProtocolCommand *command = commands;
  while (command->command && command->command != mHandlerInfo.command) {
    command++;
  }

  if (!command->command) {
    NS_WARNING("Unsupported command!");
    return PROTOCOL_ERROR;
  }

  // Get command pattern.
  mHandlerInfo.commandPattern = command;
  if (command->paramNum) {
    // Read command parameter if needed.
    mHandlerInfo.state = STATE_READ_PARAM_LEN;
  } else {
    mHandlerInfo.state = STATE_PROCESSING;
  }

  return SUCCESS;
}

ResponseCode
KeyStore::ReadLength(UnixSocketBuffer *aMessage)
{
  if (mHandlerInfo.state != STATE_READ_PARAM_LEN) {
    NS_WARNING("Wrong state in ReadLength()!");
    return SYSTEM_ERROR;
  }

  if (!CheckSize(aMessage, 2)) {
    NS_WARNING("Data size error in ReadLength()!");
    return PROTOCOL_ERROR;
  }

  // Read length of command parameter.
  // FIXME: Depends on endianess and (sizeof(unsigned short) == 2)
  unsigned short dataLength;
  memcpy(&dataLength, aMessage->GetData(), 2);
  aMessage->Consume(2);
  mHandlerInfo.param[mHandlerInfo.paramCount].length = ntohs(dataLength);

  mHandlerInfo.state = STATE_READ_PARAM_DATA;

  return SUCCESS;
}

ResponseCode
KeyStore::ReadData(UnixSocketBuffer *aMessage)
{
  if (mHandlerInfo.state != STATE_READ_PARAM_DATA) {
    NS_WARNING("Wrong state in ReadData()!");
    return SYSTEM_ERROR;
  }

  if (!CheckSize(aMessage, mHandlerInfo.param[mHandlerInfo.paramCount].length)) {
    NS_WARNING("Data size error in ReadData()!");
    return PROTOCOL_ERROR;
  }

  // Read command parameter.
  memcpy(mHandlerInfo.param[mHandlerInfo.paramCount].data,
         aMessage->GetData(),
         mHandlerInfo.param[mHandlerInfo.paramCount].length);
  aMessage->Consume(mHandlerInfo.param[mHandlerInfo.paramCount].length);
  mHandlerInfo.paramCount++;

  if (mHandlerInfo.paramCount == mHandlerInfo.commandPattern->paramNum) {
    mHandlerInfo.state = STATE_PROCESSING;
  } else {
    mHandlerInfo.state = STATE_READ_PARAM_LEN;
  }

  return SUCCESS;
}

// Status response
void
KeyStore::SendResponse(ResponseCode aResponse)
{
  MOZ_ASSERT(mStreamSocket);

  if (aResponse == NO_RESPONSE)
    return;

  uint8_t response = (uint8_t)aResponse;
  UnixSocketRawData* data = new UnixSocketRawData((const void *)&response, 1);
  mStreamSocket->SendSocketData(data);
}

// Data response
void
KeyStore::SendData(const uint8_t *aData, int aLength)
{
  MOZ_ASSERT(mStreamSocket);

  unsigned short dataLength = htons(aLength);

  UnixSocketRawData* length = new UnixSocketRawData((const void *)&dataLength, 2);
  mStreamSocket->SendSocketData(length);

  UnixSocketRawData* data = new UnixSocketRawData((const void *)aData, aLength);
  mStreamSocket->SendSocketData(data);
}

// |StreamSocketConsumer|, |ListenSocketConsumer|

void
KeyStore::ReceiveSocketData(int aIndex, nsAutoPtr<UnixSocketBuffer>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Handle request.
  ResponseCode result = SUCCESS;
  while (aMessage->GetSize() ||
         mHandlerInfo.state == STATE_PROCESSING) {
    switch (mHandlerInfo.state) {
      case STATE_IDLE:
        result = ReadCommand(aMessage);
        break;
      case STATE_READ_PARAM_LEN:
        result = ReadLength(aMessage);
        break;
      case STATE_READ_PARAM_DATA:
        result = ReadData(aMessage);
        break;
      case STATE_PROCESSING:
        if (mHandlerInfo.command == 'g') {
          result = SYSTEM_ERROR;

          nsNSSShutDownPreventionLock locker;
          if (isAlreadyShutDown()) {
            break;
          }

          // Get CA
          const uint8_t *data;
          size_t dataLength;
          const char *name = (const char *)mHandlerInfo.param[0].data;

          if (!strncmp(name, "WIFI_USERKEY_", 13)) {
            result = getPrivateKey(name, &data, &dataLength);
          } else {
            result = getCertificate(name, &data, &dataLength);
          }
          if (result != SUCCESS) {
            break;
          }

          SendResponse(SUCCESS);
          SendData(data, (int)dataLength);

          free((void *)data);
        }

        ResetHandlerInfo();
        break;
    }

    if (result != SUCCESS) {
      SendResponse(result);
      ResetHandlerInfo();
      return;
    }
  }
}

void
KeyStore::OnConnectSuccess(int aIndex)
{
  if (aIndex == STREAM_SOCKET) {
    mShutdown = false;
  }
}

void
KeyStore::OnConnectError(int aIndex)
{
  if (mShutdown) {
    return;
  }

  if (aIndex == STREAM_SOCKET) {
    // Stream socket error; start listening again
    Listen();
  }
}

void
KeyStore::OnDisconnect(int aIndex)
{
  if (mShutdown) {
    return;
  }

  switch (aIndex) {
    case LISTEN_SOCKET:
      // Listen socket disconnected; start anew.
      mListenSocket = nullptr;
      Listen();
      break;
    case STREAM_SOCKET:
      // Stream socket disconnected; start listening again.
      Listen();
      break;
  }
}

} // namespace ipc
} // namespace mozilla
