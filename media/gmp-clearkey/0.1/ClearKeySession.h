/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ClearKeySession_h__
#define __ClearKeySession_h__

#include "ClearKeyUtils.h"
#include "gmp-decryption.h"

class GMPBuffer;
class GMPDecryptorCallback;
class GMPDecryptorHost;
class GMPEncryptedBufferMetadata;

class ClearKeySession
{
public:
  explicit ClearKeySession(const std::string& aSessionId,
                           GMPDecryptorCallback* aCallback,
                           GMPSessionType aSessionType);

  ~ClearKeySession();

  const std::vector<KeyId>& GetKeyIds() const { return mKeyIds; }

  void Init(uint32_t aPromiseId,
            const uint8_t* aInitData, uint32_t aInitDataSize);

  GMPSessionType Type() const;

  void AddKeyId(const KeyId& aKeyId);

  const std::string Id() const { return mSessionId; }

private:
  const std::string mSessionId;
  std::vector<KeyId> mKeyIds;

  GMPDecryptorCallback* mCallback;
  const GMPSessionType mSessionType;
};

#endif // __ClearKeySession_h__
