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

#ifndef __ClearKeyUtils_h__
#define __ClearKeyUtils_h__

#include <stdint.h>
#include <string>
#include <vector>
#include <assert.h>
#include "gmp-api/gmp-decryption.h"

#define CLEARKEY_KEY_LEN ((size_t)16)

#if 0
void CK_Log(const char* aFmt, ...);
#define CK_LOGE(...) CK_Log(__VA_ARGS__)
#define CK_LOGD(...) CK_Log(__VA_ARGS__)
#define CK_LOGW(...) CK_Log(__VA_ARGS__)
#else
#define CK_LOGE(...)
#define CK_LOGD(...)
#define CK_LOGW(...)
#endif

struct GMPPlatformAPI;
extern GMPPlatformAPI* GetPlatform();

typedef std::vector<uint8_t> KeyId;
typedef std::vector<uint8_t> Key;

struct KeyIdPair
{
  KeyId mKeyId;
  Key mKey;
};

class ClearKeyUtils
{
public:
  static void DecryptAES(const std::vector<uint8_t>& aKey,
                         std::vector<uint8_t>& aData, std::vector<uint8_t>& aIV);

  static void ParseInitData(const uint8_t* aInitData, uint32_t aInitDataSize,
                            std::vector<Key>& aOutKeys);

  static void MakeKeyRequest(const std::vector<KeyId>& aKeyIds,
                             std::string& aOutRequest,
                             GMPSessionType aSessionType);

  static bool ParseJWK(const uint8_t* aKeyData, uint32_t aKeyDataSize,
                       std::vector<KeyIdPair>& aOutKeys,
                       GMPSessionType aSessionType);
  static const char* SessionTypeToString(GMPSessionType aSessionType);

  static bool IsValidSessionId(const char* aBuff, uint32_t aLength);
};

template<class Container, class Element>
inline bool
Contains(const Container& aContainer, const Element& aElement)
{
  return aContainer.find(aElement) != aContainer.end();
}

class AutoLock {
public:
  explicit AutoLock(GMPMutex* aMutex)
    : mMutex(aMutex)
  {
    assert(aMutex);
    if (mMutex) {
      mMutex->Acquire();
    }
  }
  ~AutoLock() {
    if (mMutex) {
      mMutex->Release();
    }
  }
private:
  GMPMutex* mMutex;
};

GMPMutex* GMPCreateMutex();

#endif // __ClearKeyUtils_h__
