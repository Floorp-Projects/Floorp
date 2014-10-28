/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ClearKeySession_h__
#define __ClearKeySession_h__

#include "ClearKeyUtils.h"

class GMPBuffer;
class GMPDecryptorCallback;
class GMPDecryptorHost;
class GMPEncryptedBufferMetadata;

/**
 * Currently useless; will be fleshed out later with support for persistent
 * key sessions.
 */

class ClearKeySession
{
public:
  ClearKeySession(const std::string& aSessionId,
                  GMPDecryptorCallback* aCallback);

  ~ClearKeySession();

  const std::vector<KeyId>& GetKeyIds() { return mKeyIds; }

  void Init(uint32_t aPromiseId,
            const uint8_t* aInitData, uint32_t aInitDataSize);
private:
  std::string mSessionId;
  std::vector<KeyId> mKeyIds;

  GMPDecryptorCallback* mCallback;
};

#endif // __ClearKeySession_h__
