/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <stdio.h>

#include "ClearKeyDecryptionManager.h"
#include "ClearKeyUtils.h"
#include "ClearKeyStorage.h"
#include "ClearKeyPersistence.h"
#include "gmp-task-utils.h"

#include "mozilla/Assertions.h"
#include "mozilla/NullPtr.h"

using namespace mozilla;
using namespace std;

class ClearKeyDecryptor
{
public:
  ClearKeyDecryptor(GMPDecryptorCallback* aCallback, const Key& aKey);
  ~ClearKeyDecryptor();

  void InitKey();

  void QueueDecrypt(GMPBuffer* aBuffer, GMPEncryptedBufferMetadata* aMetadata);

  uint32_t AddRef();
  uint32_t Release();

  const Key& DecryptionKey() const { return mKey; }

private:
  struct DecryptTask : public GMPTask
  {
    DecryptTask(ClearKeyDecryptor* aTarget, GMPBuffer* aBuffer,
                GMPEncryptedBufferMetadata* aMetadata)
      : mTarget(aTarget), mBuffer(aBuffer), mMetadata(aMetadata) { }

    virtual void Run() MOZ_OVERRIDE
    {
      mTarget->Decrypt(mBuffer, mMetadata);
    }

    virtual void Destroy() MOZ_OVERRIDE {
      delete this;
    }

    virtual ~DecryptTask() { }

    ClearKeyDecryptor* mTarget;
    GMPBuffer* mBuffer;
    GMPEncryptedBufferMetadata* mMetadata;
  };

  struct DestroyTask : public GMPTask
  {
    explicit DestroyTask(ClearKeyDecryptor* aTarget) : mTarget(aTarget) { }

    virtual void Run() MOZ_OVERRIDE {
      delete mTarget;
    }

    virtual void Destroy() MOZ_OVERRIDE {
      delete this;
    }

    virtual ~DestroyTask() { }

    ClearKeyDecryptor* mTarget;
  };

  void Decrypt(GMPBuffer* aBuffer, GMPEncryptedBufferMetadata* aMetadata);

  uint32_t mRefCnt;

  GMPDecryptorCallback* mCallback;
  GMPThread* mThread;

  Key mKey;
};

ClearKeyDecryptionManager::ClearKeyDecryptionManager()
{
  CK_LOGD("ClearKeyDecryptionManager ctor");

  // The ClearKeyDecryptionManager maintains a self reference which is
  // removed when the host is finished with the interface and calls
  // DecryptingComplete(). We make ClearKeyDecryptionManager refcounted so
  // that the tasks to that we dispatch to call functions on it won't end up
  // derefing a null reference after DecryptingComplete() is called.
  AddRef();
}

ClearKeyDecryptionManager::~ClearKeyDecryptionManager()
{
  CK_LOGD("ClearKeyDecryptionManager dtor");
  MOZ_ASSERT(mRefCount == 1);
}

void
ClearKeyDecryptionManager::Init(GMPDecryptorCallback* aCallback)
{
  CK_LOGD("ClearKeyDecryptionManager::Init");
  mCallback = aCallback;
  mCallback->SetCapabilities(GMP_EME_CAP_DECRYPT_AUDIO |
                             GMP_EME_CAP_DECRYPT_VIDEO);
  ClearKeyPersistence::EnsureInitialized();
}

void
ClearKeyDecryptionManager::CreateSession(uint32_t aPromiseId,
                                         const char* aInitDataType,
                                         uint32_t aInitDataTypeSize,
                                         const uint8_t* aInitData,
                                         uint32_t aInitDataSize,
                                         GMPSessionType aSessionType)
{
  CK_LOGD("ClearKeyDecryptionManager::CreateSession type:%s", aInitDataType);

  // initDataType must be "cenc".
  if (strcmp("cenc", aInitDataType)) {
    mCallback->RejectPromise(aPromiseId, kGMPNotSupportedError,
                             nullptr /* message */, 0 /* messageLen */);
    return;
  }

  if (ClearKeyPersistence::DeferCreateSessionIfNotReady(this,
                                                        aPromiseId,
                                                        aInitData,
                                                        aInitDataSize,
                                                        aSessionType)) {
    return;
  }

  string sessionId = ClearKeyPersistence::GetNewSessionId(aSessionType);
  MOZ_ASSERT(mSessions.find(sessionId) == mSessions.end());

  ClearKeySession* session = new ClearKeySession(sessionId, mCallback, aSessionType);
  session->Init(aPromiseId, aInitData, aInitDataSize);
  mSessions[sessionId] = session;

  const vector<KeyId>& sessionKeys = session->GetKeyIds();
  vector<KeyId> neededKeys;
  for (auto it = sessionKeys.begin(); it != sessionKeys.end(); it++) {
    if (!Contains(mDecryptors, *it)) {
      // Need to request this key ID from the client.
      neededKeys.push_back(*it);
    } else {
      // We already have a key for this key ID. Mark as usable.
      mCallback->KeyIdUsable(sessionId.c_str(), sessionId.length(),
                             &(*it)[0], it->size());
    }
  }

  if (neededKeys.empty()) {
    CK_LOGD("No keys needed from client.");
    return;
  }

  // Send a request for needed key data.
  string request;
  ClearKeyUtils::MakeKeyRequest(neededKeys, request, aSessionType);
  mCallback->SessionMessage(&sessionId[0], sessionId.length(),
                            (uint8_t*)&request[0], request.length(),
                            "" /* destination url */, 0);
}

void
ClearKeyDecryptionManager::LoadSession(uint32_t aPromiseId,
                                       const char* aSessionId,
                                       uint32_t aSessionIdLength)
{
  CK_LOGD("ClearKeyDecryptionManager::LoadSession");

  if (!ClearKeyUtils::IsValidSessionId(aSessionId, aSessionIdLength)) {
    mCallback->ResolveLoadSessionPromise(aPromiseId, false);
    return;
  }

  if (ClearKeyPersistence::DeferLoadSessionIfNotReady(this,
                                                      aPromiseId,
                                                      aSessionId,
                                                      aSessionIdLength)) {
    return;
  }

  string sid(aSessionId, aSessionId + aSessionIdLength);
  if (!ClearKeyPersistence::IsPersistentSessionId(sid)) {
    mCallback->ResolveLoadSessionPromise(aPromiseId, false);
    return;
  }

  // Callsback PersistentSessionDataLoaded with results...
  ClearKeyPersistence::LoadSessionData(this, sid, aPromiseId);
}

void
ClearKeyDecryptionManager::PersistentSessionDataLoaded(GMPErr aStatus,
                                                       uint32_t aPromiseId,
                                                       const string& aSessionId,
                                                       const uint8_t* aKeyData,
                                                       uint32_t aKeyDataSize)
{
  if (GMP_FAILED(aStatus) ||
      Contains(mSessions, aSessionId) ||
      (aKeyDataSize % (2 * CLEARKEY_KEY_LEN)) != 0) {
    mCallback->ResolveLoadSessionPromise(aPromiseId, false);
    return;
  }

  ClearKeySession* session = new ClearKeySession(aSessionId,
                                                 mCallback,
                                                 kGMPPersistentSession);
  mSessions[aSessionId] = session;

  // TODO: currently we have to resolve the load-session promise before we
  // can mark the keys as usable. We should really do this before marking
  // the keys usable, but we need to fix Gecko first.
  mCallback->ResolveLoadSessionPromise(aPromiseId, true);

  uint32_t numKeys = aKeyDataSize / (2 * CLEARKEY_KEY_LEN);
  for (uint32_t i = 0; i < numKeys; i ++) {
    const uint8_t* base = aKeyData + 2 * CLEARKEY_KEY_LEN * i;

    KeyId keyId(base, base + CLEARKEY_KEY_LEN);
    MOZ_ASSERT(keyId.size() == CLEARKEY_KEY_LEN);

    Key key(base + CLEARKEY_KEY_LEN, base + 2 * CLEARKEY_KEY_LEN);
    MOZ_ASSERT(key.size() == CLEARKEY_KEY_LEN);

    session->AddKeyId(keyId);

    if (!Contains(mDecryptors, keyId)) {
      mDecryptors[keyId] = new ClearKeyDecryptor(mCallback, key);
    }
    mDecryptors[keyId]->AddRef();
    mCallback->KeyIdUsable(aSessionId.c_str(), aSessionId.size(),
                           &keyId[0], keyId.size());
  }
}

void
ClearKeyDecryptionManager::UpdateSession(uint32_t aPromiseId,
                                         const char* aSessionId,
                                         uint32_t aSessionIdLength,
                                         const uint8_t* aResponse,
                                         uint32_t aResponseSize)
{
  CK_LOGD("ClearKeyDecryptionManager::UpdateSession");
  string sessionId(aSessionId, aSessionId + aSessionIdLength);

  auto itr = mSessions.find(sessionId);
  if (itr == mSessions.end() || !(itr->second)) {
    CK_LOGW("ClearKey CDM couldn't resolve session ID in UpdateSession.");
    mCallback->RejectPromise(aPromiseId, kGMPNotFoundError, nullptr, 0);
    return;
  }
  ClearKeySession* session = itr->second;

  // Parse the response for any (key ID, key) pairs.
  vector<KeyIdPair> keyPairs;
  if (!ClearKeyUtils::ParseJWK(aResponse, aResponseSize, keyPairs, session->Type())) {
    CK_LOGW("ClearKey CDM failed to parse JSON Web Key.");
    mCallback->RejectPromise(aPromiseId, kGMPInvalidAccessError, nullptr, 0);
    return;
  }

  for (auto it = keyPairs.begin(); it != keyPairs.end(); it++) {
    KeyId& keyId = it->mKeyId;

    if (!Contains(mDecryptors, keyId)) {
      mDecryptors[keyId] = new ClearKeyDecryptor(mCallback, it->mKey);
      mCallback->KeyIdUsable(aSessionId, aSessionIdLength,
                             &keyId[0], keyId.size());
    }

    mDecryptors[keyId]->AddRef();
  }

  if (session->Type() != kGMPPersistentSession) {
    mCallback->ResolvePromise(aPromiseId);
    return;
  }

  // Store the keys on disk. We store a record whose name is the sessionId,
  // and simply append each keyId followed by its key.
  vector<uint8_t> keydata;
  Serialize(session, keydata);
  GMPTask* resolve = WrapTask(mCallback, &GMPDecryptorCallback::ResolvePromise, aPromiseId);
  static const char* message = "Couldn't store cenc key init data";
  GMPTask* reject = WrapTask(mCallback,
                             &GMPDecryptorCallback::RejectPromise,
                             aPromiseId,
                             kGMPInvalidStateError,
                             message,
                             strlen(message));
  StoreData(sessionId, keydata, resolve, reject);
}

void
ClearKeyDecryptionManager::Serialize(const ClearKeySession* aSession,
                                     std::vector<uint8_t>& aOutKeyData)
{
  const std::vector<KeyId>& keyIds = aSession->GetKeyIds();
  for (size_t i = 0; i < keyIds.size(); i++) {
    const KeyId& keyId = keyIds[i];
    if (!Contains(mDecryptors, keyId)) {
      continue;
    }
    MOZ_ASSERT(keyId.size() == CLEARKEY_KEY_LEN);
    aOutKeyData.insert(aOutKeyData.end(), keyId.begin(), keyId.end());
    const Key& key = mDecryptors[keyId]->DecryptionKey();
    MOZ_ASSERT(key.size() == CLEARKEY_KEY_LEN);
    aOutKeyData.insert(aOutKeyData.end(), key.begin(), key.end());
  }
}

void
ClearKeyDecryptionManager::CloseSession(uint32_t aPromiseId,
                                        const char* aSessionId,
                                        uint32_t aSessionIdLength)
{
  CK_LOGD("ClearKeyDecryptionManager::CloseSession");

  string sessionId(aSessionId, aSessionId + aSessionIdLength);
  auto itr = mSessions.find(sessionId);
  if (itr == mSessions.end()) {
    CK_LOGW("ClearKey CDM couldn't close non-existent session.");
    mCallback->RejectPromise(aPromiseId, kGMPNotFoundError, nullptr, 0);
    return;
  }

  ClearKeySession* session = itr->second;
  MOZ_ASSERT(session);

  ClearInMemorySessionData(session);
  mCallback->ResolvePromise(aPromiseId);
  mCallback->SessionClosed(aSessionId, aSessionIdLength);
}

void
ClearKeyDecryptionManager::ClearInMemorySessionData(ClearKeySession* aSession)
{
  MOZ_ASSERT(aSession);

  const vector<KeyId>& keyIds = aSession->GetKeyIds();
  for (auto it = keyIds.begin(); it != keyIds.end(); it++) {
    MOZ_ASSERT(Contains(mDecryptors, *it));
    if (!mDecryptors[*it]->Release()) {
      mDecryptors.erase(*it);
      mCallback->KeyIdNotUsable(aSession->Id().c_str(), aSession->Id().size(),
                                &(*it)[0], it->size());
    }
  }

  mSessions.erase(aSession->Id());
  delete aSession;
}

void
ClearKeyDecryptionManager::RemoveSession(uint32_t aPromiseId,
                                         const char* aSessionId,
                                         uint32_t aSessionIdLength)
{
  string sessionId(aSessionId, aSessionId + aSessionIdLength);
  auto itr = mSessions.find(sessionId);
  if (itr == mSessions.end()) {
    CK_LOGW("ClearKey CDM couldn't remove non-existent session.");
    mCallback->RejectPromise(aPromiseId, kGMPNotFoundError, nullptr, 0);
    return;
  }

  ClearKeySession* session = itr->second;
  MOZ_ASSERT(session);
  string sid = session->Id();
  bool isPersistent = session->Type() == kGMPPersistentSession;
  ClearInMemorySessionData(session);

  if (!isPersistent) {
    mCallback->ResolvePromise(aPromiseId);
    return;
  }

  ClearKeyPersistence::PersistentSessionRemoved(sid);

  // Overwrite the record storing the sessionId's key data with a zero
  // length record to delete it.
  vector<uint8_t> emptyKeydata;
  GMPTask* resolve = WrapTask(mCallback, &GMPDecryptorCallback::ResolvePromise, aPromiseId);
  static const char* message = "Could not remove session";
  GMPTask* reject = WrapTask(mCallback,
                             &GMPDecryptorCallback::RejectPromise,
                             aPromiseId,
                             kGMPInvalidAccessError,
                             message,
                             strlen(message));
  StoreData(sessionId, emptyKeydata, resolve, reject);
}

void
ClearKeyDecryptionManager::SetServerCertificate(uint32_t aPromiseId,
                                                const uint8_t* aServerCert,
                                                uint32_t aServerCertSize)
{
  // ClearKey CDM doesn't support this method by spec.
  CK_LOGD("ClearKeyDecryptionManager::SetServerCertificate");
  mCallback->RejectPromise(aPromiseId, kGMPNotSupportedError,
                           nullptr /* message */, 0 /* messageLen */);
}

void
ClearKeyDecryptionManager::Decrypt(GMPBuffer* aBuffer,
                                   GMPEncryptedBufferMetadata* aMetadata)
{
  CK_LOGD("ClearKeyDecryptionManager::Decrypt");
  KeyId keyId(aMetadata->KeyId(), aMetadata->KeyId() + aMetadata->KeyIdSize());

  if (!Contains(mDecryptors, keyId)) {
    CK_LOGD("ClearKeyDecryptionManager::Decrypt GMPNoKeyErr");
    mCallback->Decrypted(aBuffer, GMPNoKeyErr);
    return;
  }

  mDecryptors[keyId]->QueueDecrypt(aBuffer, aMetadata);
}

void
ClearKeyDecryptionManager::DecryptingComplete()
{
  CK_LOGD("ClearKeyDecryptionManager::DecryptingComplete");

  for (auto it = mSessions.begin(); it != mSessions.end(); it++) {
    delete it->second;
  }
  mSessions.clear();

  for (auto it = mDecryptors.begin(); it != mDecryptors.end(); it++) {
    delete it->second;
  }
  mDecryptors.clear();

  Release();
}

void
ClearKeyDecryptor::QueueDecrypt(GMPBuffer* aBuffer,
                                GMPEncryptedBufferMetadata* aMetadata)
{
  CK_LOGD("ClearKeyDecryptor::QueueDecrypt");
  mThread->Post(new DecryptTask(this, aBuffer, aMetadata));
}

void
ClearKeyDecryptor::Decrypt(GMPBuffer* aBuffer,
                           GMPEncryptedBufferMetadata* aMetadata)
{
  if (!mThread) {
    mCallback->Decrypted(aBuffer, GMPGenericErr);
  }

  // If the sample is split up into multiple encrypted subsamples, we need to
  // stitch them into one continuous buffer for decryption.
  vector<uint8_t> tmp(aBuffer->Size());

  if (aMetadata->NumSubsamples()) {
    // Take all encrypted parts of subsamples and stitch them into one
    // continuous encrypted buffer.
    unsigned char* data = aBuffer->Data();
    unsigned char* iter = &tmp[0];
    for (size_t i = 0; i < aMetadata->NumSubsamples(); i++) {
      data += aMetadata->ClearBytes()[i];
      uint32_t cipherBytes = aMetadata->CipherBytes()[i];

      memcpy(iter, data, cipherBytes);

      data += cipherBytes;
      iter += cipherBytes;
    }

    tmp.resize((size_t)(iter - &tmp[0]));
  } else {
    memcpy(&tmp[0], aBuffer->Data(), aBuffer->Size());
  }

  MOZ_ASSERT(aMetadata->IVSize() == 8 || aMetadata->IVSize() == 16);
  vector<uint8_t> iv(aMetadata->IV(), aMetadata->IV() + aMetadata->IVSize());
  iv.insert(iv.end(), CLEARKEY_KEY_LEN - aMetadata->IVSize(), 0);

  ClearKeyUtils::DecryptAES(mKey, tmp, iv);

  if (aMetadata->NumSubsamples()) {
    // Take the decrypted buffer, split up into subsamples, and insert those
    // subsamples back into their original position in the original buffer.
    unsigned char* data = aBuffer->Data();
    unsigned char* iter = &tmp[0];
    for (size_t i = 0; i < aMetadata->NumSubsamples(); i++) {
      data += aMetadata->ClearBytes()[i];
      uint32_t cipherBytes = aMetadata->CipherBytes()[i];

      memcpy(data, iter, cipherBytes);

      data += cipherBytes;
      iter += cipherBytes;
    }
  } else {
    memcpy(aBuffer->Data(), &tmp[0], aBuffer->Size());
  }

  mCallback->Decrypted(aBuffer, GMPNoErr);
}

ClearKeyDecryptor::ClearKeyDecryptor(GMPDecryptorCallback* aCallback,
                                     const Key& aKey)
  : mRefCnt(0)
  , mCallback(aCallback)
  , mThread(nullptr)
  , mKey(aKey)
{
  if (GetPlatform()->createthread(&mThread) != GMPNoErr) {
    CK_LOGD("failed to create thread in clearkey cdm");
    mThread = nullptr;
    return;
  }
}

ClearKeyDecryptor::~ClearKeyDecryptor()
{
  CK_LOGD("ClearKeyDecryptor dtor; key ID = %08x...", *(uint32_t*)&mKey[0]);
}

uint32_t
ClearKeyDecryptor::AddRef()
{
  return ++mRefCnt;
}

uint32_t
ClearKeyDecryptor::Release()
{
  uint32_t newCount = --mRefCnt;
  if (!newCount) {
    if (mThread) {
      // Shutdown mThread. We cache a pointer to mThread, as the DestroyTask
      // may run and delete |this| before Post() returns.
      GMPThread* thread = mThread;
      thread->Post(new DestroyTask(this));
      thread->Join();
    } else {
      delete this;
    }
  }

  return newCount;
}
