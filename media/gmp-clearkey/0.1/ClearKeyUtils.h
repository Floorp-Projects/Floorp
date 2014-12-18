/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ClearKeyUtils_h__
#define __ClearKeyUtils_h__

#include <stdint.h>
#include <string>
#include <vector>
#include "gmp-decryption.h"

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

#endif // __ClearKeyUtils_h__
