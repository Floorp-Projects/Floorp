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

#include "ClearKeyDecryptionManager.h"

#include "psshparser/PsshParser.h"

#include <assert.h>
#include <string.h>
#include <vector>
#include <algorithm>

#include "mozilla/CheckedInt.h"
#include "mozilla/Span.h"

using namespace cdm;

bool AllZero(const std::vector<uint32_t>& aBytes) {
  return all_of(aBytes.begin(), aBytes.end(),
                [](uint32_t b) { return b == 0; });
}

class ClearKeyDecryptor : public RefCounted {
 public:
  ClearKeyDecryptor();

  void InitKey(const Key& aKey);
  bool HasKey() const { return !mKey.empty(); }

  Status Decrypt(uint8_t* aBuffer, uint32_t aBufferSize,
                 const CryptoMetaData& aMetadata);

  const Key& DecryptionKey() const { return mKey; }

 private:
  ~ClearKeyDecryptor();

  Key mKey;
};

/* static */
ClearKeyDecryptionManager* ClearKeyDecryptionManager::sInstance = nullptr;

/* static */
ClearKeyDecryptionManager* ClearKeyDecryptionManager::Get() {
  if (!sInstance) {
    sInstance = new ClearKeyDecryptionManager();
  }
  return sInstance;
}

ClearKeyDecryptionManager::ClearKeyDecryptionManager() {
  CK_LOGD("ClearKeyDecryptionManager::ClearKeyDecryptionManager");
}

ClearKeyDecryptionManager::~ClearKeyDecryptionManager() {
  CK_LOGD("ClearKeyDecryptionManager::~ClearKeyDecryptionManager");

  sInstance = nullptr;

  for (auto it = mDecryptors.begin(); it != mDecryptors.end(); it++) {
    it->second->Release();
  }
  mDecryptors.clear();
}

bool ClearKeyDecryptionManager::HasSeenKeyId(const KeyId& aKeyId) const {
  CK_LOGD("ClearKeyDecryptionManager::SeenKeyId %s",
          mDecryptors.find(aKeyId) != mDecryptors.end() ? "t" : "f");
  return mDecryptors.find(aKeyId) != mDecryptors.end();
}

bool ClearKeyDecryptionManager::IsExpectingKeyForKeyId(
    const KeyId& aKeyId) const {
  CK_LOGARRAY("ClearKeyDecryptionManager::IsExpectingKeyForId ", aKeyId.data(),
              aKeyId.size());
  const auto& decryptor = mDecryptors.find(aKeyId);
  return decryptor != mDecryptors.end() && !decryptor->second->HasKey();
}

bool ClearKeyDecryptionManager::HasKeyForKeyId(const KeyId& aKeyId) const {
  CK_LOGD("ClearKeyDecryptionManager::HasKeyForKeyId");
  const auto& decryptor = mDecryptors.find(aKeyId);
  return decryptor != mDecryptors.end() && decryptor->second->HasKey();
}

const Key& ClearKeyDecryptionManager::GetDecryptionKey(const KeyId& aKeyId) {
  assert(HasKeyForKeyId(aKeyId));
  return mDecryptors[aKeyId]->DecryptionKey();
}

void ClearKeyDecryptionManager::InitKey(KeyId aKeyId, Key aKey) {
  CK_LOGD("ClearKeyDecryptionManager::InitKey ", aKeyId.data(), aKeyId.size());
  if (IsExpectingKeyForKeyId(aKeyId)) {
    CK_LOGARRAY("Initialized Key ", aKeyId.data(), aKeyId.size());
    mDecryptors[aKeyId]->InitKey(aKey);
  } else {
    CK_LOGARRAY("Failed to initialize key ", aKeyId.data(), aKeyId.size());
  }
}

void ClearKeyDecryptionManager::ExpectKeyId(KeyId aKeyId) {
  CK_LOGD("ClearKeyDecryptionManager::ExpectKeyId ", aKeyId.data(),
          aKeyId.size());
  if (!HasSeenKeyId(aKeyId)) {
    mDecryptors[aKeyId] = new ClearKeyDecryptor();
  }
  mDecryptors[aKeyId]->AddRef();
}

void ClearKeyDecryptionManager::ReleaseKeyId(KeyId aKeyId) {
  CK_LOGD("ClearKeyDecryptionManager::ReleaseKeyId");
  assert(HasSeenKeyId(aKeyId));

  ClearKeyDecryptor* decryptor = mDecryptors[aKeyId];
  if (!decryptor->Release()) {
    mDecryptors.erase(aKeyId);
  }
}

Status ClearKeyDecryptionManager::Decrypt(std::vector<uint8_t>& aBuffer,
                                          const CryptoMetaData& aMetadata) {
  return Decrypt(&aBuffer[0], aBuffer.size(), aMetadata);
}

Status ClearKeyDecryptionManager::Decrypt(uint8_t* aBuffer,
                                          uint32_t aBufferSize,
                                          const CryptoMetaData& aMetadata) {
  CK_LOGD("ClearKeyDecryptionManager::Decrypt");
  if (!HasKeyForKeyId(aMetadata.mKeyId)) {
    CK_LOGARRAY("Unable to find decryptor for keyId: ", aMetadata.mKeyId.data(),
                aMetadata.mKeyId.size());
    return Status::kNoKey;
  }

  CK_LOGARRAY("Found decryptor for keyId: ", aMetadata.mKeyId.data(),
              aMetadata.mKeyId.size());
  return mDecryptors[aMetadata.mKeyId]->Decrypt(aBuffer, aBufferSize,
                                                aMetadata);
}

ClearKeyDecryptor::ClearKeyDecryptor() { CK_LOGD("ClearKeyDecryptor ctor"); }

ClearKeyDecryptor::~ClearKeyDecryptor() {
  if (HasKey()) {
    CK_LOGARRAY("ClearKeyDecryptor dtor; key = ", mKey.data(), mKey.size());
  } else {
    CK_LOGD("ClearKeyDecryptor dtor");
  }
}

void ClearKeyDecryptor::InitKey(const Key& aKey) { mKey = aKey; }

Status ClearKeyDecryptor::Decrypt(uint8_t* aBuffer, uint32_t aBufferSize,
                                  const CryptoMetaData& aMetadata) {
  CK_LOGD("ClearKeyDecryptor::Decrypt");
  // If the sample is split up into multiple encrypted subsamples, we need to
  // stitch them into one continuous buffer for decryption.
  std::vector<uint8_t> tmp(aBufferSize);
  static_assert(sizeof(uintptr_t) == sizeof(uint8_t*),
                "We need uintptr_t to be exactly the same size as a pointer");

  // Decrypt CBCS case:
  if (aMetadata.mEncryptionScheme == EncryptionScheme::kCbcs) {
    mozilla::CheckedInt<uintptr_t> data = reinterpret_cast<uintptr_t>(aBuffer);
    if (!data.isValid()) {
      return Status::kDecryptError;
    }
    const uintptr_t endBuffer =
        reinterpret_cast<uintptr_t>(aBuffer + aBufferSize);

    if (aMetadata.NumSubsamples() == 0) {
      if (data.value() > endBuffer) {
        return Status::kDecryptError;
      }
      mozilla::Span<uint8_t> encryptedSpan = mozilla::MakeSpan(
          reinterpret_cast<uint8_t*>(data.value()), aBufferSize);
      if (!ClearKeyUtils::DecryptCbcs(mKey, aMetadata.mIV, encryptedSpan,
                                      aMetadata.mCryptByteBlock,
                                      aMetadata.mSkipByteBlock)) {
        return Status::kDecryptError;
      }
      return Status::kSuccess;
    }

    for (size_t i = 0; i < aMetadata.NumSubsamples(); i++) {
      data += aMetadata.mClearBytes[i];
      if (!data.isValid() || data.value() > endBuffer) {
        return Status::kDecryptError;
      }
      mozilla::CheckedInt<uintptr_t> dataAfterCipher =
          data + aMetadata.mCipherBytes[i];
      if (!dataAfterCipher.isValid() || dataAfterCipher.value() > endBuffer) {
        // Trying to read past the end of the buffer!
        return Status::kDecryptError;
      }
      mozilla::Span<uint8_t> encryptedSpan = mozilla::MakeSpan(
          reinterpret_cast<uint8_t*>(data.value()), aMetadata.mCipherBytes[i]);
      if (!ClearKeyUtils::DecryptCbcs(mKey, aMetadata.mIV, encryptedSpan,
                                      aMetadata.mCryptByteBlock,
                                      aMetadata.mSkipByteBlock)) {
        return Status::kDecryptError;
      }
      data += aMetadata.mCipherBytes[i];
      if (!data.isValid()) {
        return Status::kDecryptError;
      }
      return Status::kSuccess;
    }
  }

  // Decrypt CENC case:
  if (aMetadata.NumSubsamples()) {
    // Take all encrypted parts of subsamples and stitch them into one
    // continuous encrypted buffer.
    mozilla::CheckedInt<uintptr_t> data = reinterpret_cast<uintptr_t>(aBuffer);
    const uintptr_t endBuffer =
        reinterpret_cast<uintptr_t>(aBuffer + aBufferSize);
    uint8_t* iter = &tmp[0];
    for (size_t i = 0; i < aMetadata.NumSubsamples(); i++) {
      data += aMetadata.mClearBytes[i];
      if (!data.isValid() || data.value() > endBuffer) {
        // Trying to read past the end of the buffer!
        return Status::kDecryptError;
      }
      const uint32_t& cipherBytes = aMetadata.mCipherBytes[i];
      mozilla::CheckedInt<uintptr_t> dataAfterCipher = data + cipherBytes;
      if (!dataAfterCipher.isValid() || dataAfterCipher.value() > endBuffer) {
        // Trying to read past the end of the buffer!
        return Status::kDecryptError;
      }

      memcpy(iter, reinterpret_cast<uint8_t*>(data.value()), cipherBytes);

      data = dataAfterCipher;
      iter += cipherBytes;
    }

    tmp.resize((size_t)(iter - &tmp[0]));
  } else {
    memcpy(&tmp[0], aBuffer, aBufferSize);
  }

  // It is possible that we could be passed an unencrypted sample, if all
  // encrypted sample lengths are zero, and in this case, a zero length
  // IV is allowed.
  assert(aMetadata.mIV.size() == 8 || aMetadata.mIV.size() == 16 ||
         (aMetadata.mIV.size() == 0 && AllZero(aMetadata.mCipherBytes)));

  std::vector<uint8_t> iv(aMetadata.mIV);
  iv.insert(iv.end(), CENC_KEY_LEN - aMetadata.mIV.size(), 0);

  if (!ClearKeyUtils::DecryptAES(mKey, tmp, iv)) {
    return Status::kDecryptError;
  }

  if (aMetadata.NumSubsamples()) {
    // Take the decrypted buffer, split up into subsamples, and insert those
    // subsamples back into their original position in the original buffer.
    uint8_t* data = aBuffer;
    uint8_t* iter = &tmp[0];
    for (size_t i = 0; i < aMetadata.NumSubsamples(); i++) {
      data += aMetadata.mClearBytes[i];
      uint32_t cipherBytes = aMetadata.mCipherBytes[i];

      memcpy(data, iter, cipherBytes);

      data += cipherBytes;
      iter += cipherBytes;
    }
  } else {
    memcpy(aBuffer, &tmp[0], aBufferSize);
  }

  return Status::kSuccess;
}
