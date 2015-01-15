/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
