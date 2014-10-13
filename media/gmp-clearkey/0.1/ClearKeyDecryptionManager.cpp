/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <sstream>
#include <stdint.h>
#include <stdio.h>

#include "ClearKeyDecryptionManager.h"
#include "ClearKeyUtils.h"
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


ClearKeyDecryptionManager::ClearKeyDecryptionManager(GMPDecryptorHost* aHost)
  : mHost(aHost)
{
  CK_LOGD("ClearKeyDecryptionManager ctor");
}

ClearKeyDecryptionManager::~ClearKeyDecryptionManager()
{
  CK_LOGD("ClearKeyDecryptionManager dtor");
}

void
ClearKeyDecryptionManager::Init(GMPDecryptorCallback* aCallback)
{
  CK_LOGD("ClearKeyDecryptionManager::Init");
  mCallback = aCallback;
  mCallback->SetCapabilities(GMP_EME_CAP_DECRYPT_AUDIO |
                             GMP_EME_CAP_DECRYPT_VIDEO);
}

static string
GetNewSessionId()
{
  static uint32_t sNextSessionId = 0;

  string sessionId;
  stringstream ss;
  ss << ++sNextSessionId;
  ss >> sessionId;

  return sessionId;
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

  string sessionId = GetNewSessionId();
  assert(mSessions.find(sessionId) == mSessions.end());

  ClearKeySession* session = new ClearKeySession(sessionId, mHost, mCallback);
  session->Init(aPromiseId, aInitData, aInitDataSize);
  mSessions[sessionId] = session;

  const vector<KeyId>& sessionKeys = session->GetKeyIds();
  vector<KeyId> neededKeys;
  for (auto it = sessionKeys.begin(); it != sessionKeys.end(); it++) {
    if (mDecryptors.find(*it) == mDecryptors.end()) {
      // Need to request this key ID from the client.
      neededKeys.push_back(*it);
      mDecryptors[*it] = nullptr;
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
  ClearKeyUtils::MakeKeyRequest(neededKeys, request);
  mCallback->SessionMessage(&sessionId[0], sessionId.length(),
                            (uint8_t*)&request[0], request.length(),
                            "" /* destination url */, 0);
}

void
ClearKeyDecryptionManager::LoadSession(uint32_t aPromiseId,
                                       const char* aSessionId,
                                       uint32_t aSessionIdLength)
{
  // TODO implement "persistent" sessions.
  mCallback->ResolveLoadSessionPromise(aPromiseId, false);

  CK_LOGD("ClearKeyDecryptionManager::LoadSession");
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

  if (mSessions.find(sessionId) == mSessions.end() || !mSessions[sessionId]) {
    CK_LOGW("ClearKey CDM couldn't resolve session ID in UpdateSession.");
    mCallback->RejectPromise(aPromiseId, kGMPNotFoundError, nullptr, 0);
    return;
  }

  // Parse the response for any (key ID, key) pairs.
  vector<KeyIdPair> keyPairs;
  if (!ClearKeyUtils::ParseJWK(aResponse, aResponseSize, keyPairs)) {
    CK_LOGW("ClearKey CDM failed to parse JSON Web Key.");
    mCallback->RejectPromise(aPromiseId, kGMPAbortError, nullptr, 0);
    return;
  }
  mCallback->ResolvePromise(aPromiseId);

  for (auto it = keyPairs.begin(); it != keyPairs.end(); it++) {
    KeyId& keyId = it->mKeyId;

    if (mDecryptors.find(keyId) != mDecryptors.end()) {
      mDecryptors[keyId] = new ClearKeyDecryptor(mCallback, it->mKey);
      mCallback->KeyIdUsable(aSessionId, aSessionIdLength,
                             &keyId[0], keyId.size());
    }

    mDecryptors[keyId]->AddRef();
  }
}

void
ClearKeyDecryptionManager::CloseSession(uint32_t aPromiseId,
                                        const char* aSessionId,
                                        uint32_t aSessionIdLength)
{
  CK_LOGD("ClearKeyDecryptionManager::CloseSession");

  string sessionId(aSessionId, aSessionId + aSessionIdLength);
  ClearKeySession* session = mSessions[sessionId];

  assert(session);

  const vector<KeyId>& keyIds = session->GetKeyIds();
  for (auto it = keyIds.begin(); it != keyIds.end(); it++) {
    assert(mDecryptors.find(*it) != mDecryptors.end());

    if (!mDecryptors[*it]->Release()) {
      mDecryptors.erase(*it);
      mCallback->KeyIdNotUsable(aSessionId, aSessionIdLength,
                                &(*it)[0], it->size());
    }
  }

  mSessions.erase(sessionId);
  delete session;

  mCallback->ResolvePromise(aPromiseId);
}

void
ClearKeyDecryptionManager::RemoveSession(uint32_t aPromiseId,
                                         const char* aSessionId,
                                         uint32_t aSessionIdLength)
{
  // TODO implement "persistent" sessions.
  CK_LOGD("ClearKeyDecryptionManager::RemoveSession");
  mCallback->RejectPromise(aPromiseId, kGMPInvalidAccessError,
                           nullptr /* message */, 0 /* messageLen */);
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

  if (mDecryptors.find(keyId) == mDecryptors.end() || !mDecryptors[keyId]) {
    mCallback->Decrypted(aBuffer, GMPNoKeyErr);
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

  for (auto it = mDecryptors.begin(); it != mDecryptors.end(); it++) {
    delete it->second;
  }

  delete this;
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

  assert(aMetadata->IVSize() == 8 || aMetadata->IVSize() == 16);
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
  if (!--mRefCnt) {
    if (mThread) {
      mThread->Post(new DestroyTask(this));
      mThread->Join();
    } else {
      delete this;
    }
  }

  return mRefCnt;
}
