/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <vector>

#include "ClearKeyDecryptionManager.h"
#include "gmp-api/gmp-decryption.h"
#include <assert.h>

class ClearKeyDecryptor : public RefCounted
{
public:
  ClearKeyDecryptor();

  void InitKey(const Key& aKey);
  bool HasKey() const { return !!mKey.size(); }

  GMPErr Decrypt(uint8_t* aBuffer, uint32_t aBufferSize,
                 const GMPEncryptedBufferMetadata* aMetadata);

  const Key& DecryptionKey() const { return mKey; }

private:
  ~ClearKeyDecryptor();

  Key mKey;
};


/* static */ ClearKeyDecryptionManager* ClearKeyDecryptionManager::sInstance = nullptr;

/* static */ ClearKeyDecryptionManager*
ClearKeyDecryptionManager::Get()
{
  if (!sInstance) {
    sInstance = new ClearKeyDecryptionManager();
  }
  return sInstance;
}

ClearKeyDecryptionManager::ClearKeyDecryptionManager()
{
  CK_LOGD("ClearKeyDecryptionManager::ClearKeyDecryptionManager");
}

ClearKeyDecryptionManager::~ClearKeyDecryptionManager()
{
  CK_LOGD("ClearKeyDecryptionManager::~ClearKeyDecryptionManager");

  sInstance = nullptr;

  for (auto it = mDecryptors.begin(); it != mDecryptors.end(); it++) {
    it->second->Release();
  }
  mDecryptors.clear();
}

bool
ClearKeyDecryptionManager::HasSeenKeyId(const KeyId& aKeyId) const
{
  CK_LOGD("ClearKeyDecryptionManager::SeenKeyId %s", mDecryptors.find(aKeyId) != mDecryptors.end() ? "t" : "f");
  return mDecryptors.find(aKeyId) != mDecryptors.end();
}

bool
ClearKeyDecryptionManager::IsExpectingKeyForKeyId(const KeyId& aKeyId) const
{
  CK_LOGD("ClearKeyDecryptionManager::IsExpectingKeyForId %08x...", *(uint32_t*)&aKeyId[0]);
  const auto& decryptor = mDecryptors.find(aKeyId);
  return decryptor != mDecryptors.end() && !decryptor->second->HasKey();
}

bool
ClearKeyDecryptionManager::HasKeyForKeyId(const KeyId& aKeyId) const
{
  CK_LOGD("ClearKeyDecryptionManager::HasKeyForKeyId");
  const auto& decryptor = mDecryptors.find(aKeyId);
  return decryptor != mDecryptors.end() && decryptor->second->HasKey();
}

const Key&
ClearKeyDecryptionManager::GetDecryptionKey(const KeyId& aKeyId)
{
  assert(HasKeyForKeyId(aKeyId));
  return mDecryptors[aKeyId]->DecryptionKey();
}

void
ClearKeyDecryptionManager::InitKey(KeyId aKeyId, Key aKey)
{
  CK_LOGD("ClearKeyDecryptionManager::InitKey %08x...", *(uint32_t*)&aKeyId[0]);
  if (IsExpectingKeyForKeyId(aKeyId)) {
    mDecryptors[aKeyId]->InitKey(aKey);
  }
}

void
ClearKeyDecryptionManager::ExpectKeyId(KeyId aKeyId)
{
  CK_LOGD("ClearKeyDecryptionManager::ExpectKeyId %08x...", *(uint32_t*)&aKeyId[0]);
  if (!HasSeenKeyId(aKeyId)) {
    mDecryptors[aKeyId] = new ClearKeyDecryptor();
  }
  mDecryptors[aKeyId]->AddRef();
}

void
ClearKeyDecryptionManager::ReleaseKeyId(KeyId aKeyId)
{
  CK_LOGD("ClearKeyDecryptionManager::ReleaseKeyId");
  assert(HasKeyForKeyId(aKeyId));

  ClearKeyDecryptor* decryptor = mDecryptors[aKeyId];
  if (!decryptor->Release()) {
    mDecryptors.erase(aKeyId);
  }
}

GMPErr
ClearKeyDecryptionManager::Decrypt(uint8_t* aBuffer, uint32_t aBufferSize,
                                   const GMPEncryptedBufferMetadata* aMetadata)
{
  CK_LOGD("ClearKeyDecryptionManager::Decrypt");
  KeyId keyId(aMetadata->KeyId(), aMetadata->KeyId() + aMetadata->KeyIdSize());

  if (!HasKeyForKeyId(keyId)) {
    return GMPNoKeyErr;
  }

  return mDecryptors[keyId]->Decrypt(aBuffer, aBufferSize, aMetadata);
}

ClearKeyDecryptor::ClearKeyDecryptor()
{
  CK_LOGD("ClearKeyDecryptor ctor");
}

ClearKeyDecryptor::~ClearKeyDecryptor()
{
  CK_LOGD("ClearKeyDecryptor dtor; key = %08x...", *(uint32_t*)&mKey[0]);
}

void
ClearKeyDecryptor::InitKey(const Key& aKey)
{
  mKey = aKey;
}

GMPErr
ClearKeyDecryptor::Decrypt(uint8_t* aBuffer, uint32_t aBufferSize,
                           const GMPEncryptedBufferMetadata* aMetadata)
{
  CK_LOGD("ClearKeyDecryptor::Decrypt");
  // If the sample is split up into multiple encrypted subsamples, we need to
  // stitch them into one continuous buffer for decryption.
  std::vector<uint8_t> tmp(aBufferSize);

  if (aMetadata->NumSubsamples()) {
    // Take all encrypted parts of subsamples and stitch them into one
    // continuous encrypted buffer.
    unsigned char* data = aBuffer;
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
    memcpy(&tmp[0], aBuffer, aBufferSize);
  }

  assert(aMetadata->IVSize() == 8 || aMetadata->IVSize() == 16);
  std::vector<uint8_t> iv(aMetadata->IV(), aMetadata->IV() + aMetadata->IVSize());
  iv.insert(iv.end(), CLEARKEY_KEY_LEN - aMetadata->IVSize(), 0);

  ClearKeyUtils::DecryptAES(mKey, tmp, iv);

  if (aMetadata->NumSubsamples()) {
    // Take the decrypted buffer, split up into subsamples, and insert those
    // subsamples back into their original position in the original buffer.
    unsigned char* data = aBuffer;
    unsigned char* iter = &tmp[0];
    for (size_t i = 0; i < aMetadata->NumSubsamples(); i++) {
      data += aMetadata->ClearBytes()[i];
      uint32_t cipherBytes = aMetadata->CipherBytes()[i];

      memcpy(data, iter, cipherBytes);

      data += cipherBytes;
      iter += cipherBytes;
    }
  } else {
    memcpy(aBuffer, &tmp[0], aBufferSize);
  }

  return GMPNoErr;
}
