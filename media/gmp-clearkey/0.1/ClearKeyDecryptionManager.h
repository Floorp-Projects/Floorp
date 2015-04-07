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

#include <map>

#include "ClearKeyUtils.h"
#include "RefCounted.h"

class ClearKeyDecryptor;

class ClearKeyDecryptionManager : public RefCounted
{
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

  GMPErr Decrypt(uint8_t* aBuffer, uint32_t aBufferSize,
                 const GMPEncryptedBufferMetadata* aMetadata);

  void Shutdown();

private:
  bool IsExpectingKeyForKeyId(const KeyId& aKeyId) const;

  std::map<KeyId, ClearKeyDecryptor*> mDecryptors;
};

#endif // __ClearKeyDecryptionManager_h__
