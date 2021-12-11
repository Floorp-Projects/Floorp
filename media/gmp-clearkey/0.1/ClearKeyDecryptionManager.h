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

#ifndef __ClearKeyDecryptionManager_h__
#define __ClearKeyDecryptionManager_h__

// This include is required in order for content_decryption_module to work
// on Unix systems.
#include <stddef.h>

#include <map>

#include "content_decryption_module.h"

#include "ClearKeyUtils.h"
#include "RefCounted.h"

class ClearKeyDecryptor;

class CryptoMetaData {
 public:
  CryptoMetaData() = default;

  explicit CryptoMetaData(const cdm::InputBuffer_2* aInputBuffer) {
    Init(aInputBuffer);
  }

  void Init(const cdm::InputBuffer_2* aInputBuffer) {
    if (!aInputBuffer) {
      assert(!IsValid());
      return;
    }

    mEncryptionScheme = aInputBuffer->encryption_scheme;
    Assign(mKeyId, aInputBuffer->key_id, aInputBuffer->key_id_size);
    Assign(mIV, aInputBuffer->iv, aInputBuffer->iv_size);
    mCryptByteBlock = aInputBuffer->pattern.crypt_byte_block;
    mSkipByteBlock = aInputBuffer->pattern.skip_byte_block;

    for (uint32_t i = 0; i < aInputBuffer->num_subsamples; ++i) {
      const cdm::SubsampleEntry& subsample = aInputBuffer->subsamples[i];
      mClearBytes.push_back(subsample.clear_bytes);
      mCipherBytes.push_back(subsample.cipher_bytes);
    }
  }

  bool IsValid() const {
    return !mKeyId.empty() && !mIV.empty() && !mCipherBytes.empty() &&
           !mClearBytes.empty();
  }

  size_t NumSubsamples() const {
    assert(mClearBytes.size() == mCipherBytes.size());
    return mClearBytes.size();
  }

  cdm::EncryptionScheme mEncryptionScheme;
  std::vector<uint8_t> mKeyId;
  std::vector<uint8_t> mIV;
  uint32_t mCryptByteBlock;
  uint32_t mSkipByteBlock;
  std::vector<uint32_t> mClearBytes;
  std::vector<uint32_t> mCipherBytes;
};

class ClearKeyDecryptionManager : public RefCounted {
 private:
  ClearKeyDecryptionManager();
  ~ClearKeyDecryptionManager();

  static ClearKeyDecryptionManager* sInstance;

 public:
  static ClearKeyDecryptionManager* Get();

  bool HasSeenKeyId(const KeyId& aKeyId) const;
  bool HasKeyForKeyId(const KeyId& aKeyId) const;

  const Key& GetDecryptionKey(const KeyId& aKeyId);

  // Create a decryptor for the given KeyId if one does not already exist.
  void InitKey(KeyId aKeyId, Key aKey);
  void ExpectKeyId(KeyId aKeyId);
  void ReleaseKeyId(KeyId aKeyId);

  // Decrypts buffer *in place*.
  cdm::Status Decrypt(uint8_t* aBuffer, uint32_t aBufferSize,
                      const CryptoMetaData& aMetadata);
  cdm::Status Decrypt(std::vector<uint8_t>& aBuffer,
                      const CryptoMetaData& aMetadata);

 private:
  bool IsExpectingKeyForKeyId(const KeyId& aKeyId) const;

  std::map<KeyId, ClearKeyDecryptor*> mDecryptors;
};

#endif  // __ClearKeyDecryptionManager_h__
