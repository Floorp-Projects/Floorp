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

#include <assert.h>
// stdef.h is required for content_decryption_module to work on Unix systems.
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "content_decryption_module.h"
#include "pk11pub.h"

#include "mozilla/Span.h"

#if 0
void CK_Log(const char* aFmt, ...);
#  define CK_LOGE(...) CK_Log(__VA_ARGS__)
#  define CK_LOGD(...) CK_Log(__VA_ARGS__)
#  define CK_LOGW(...) CK_Log(__VA_ARGS__)
#  define CK_LOGARRAY(APREPEND, ADATA, ADATA_SIZE) \
    CK_LogArray(APREPEND, ADATA, ADATA_SIZE)
#else
// Note: Enabling logging slows things down a LOT, especially when logging to
// a file.
#  define CK_LOGE(...)
#  define CK_LOGD(...)
#  define CK_LOGW(...)
#  define CK_LOGARRAY(APREPEND, ADATA, ADATA_SIZE)
#endif

typedef std::vector<uint8_t> KeyId;
typedef std::vector<uint8_t> Key;

// The session response size should be within a reasonable limit.
// The size 64 KB is referenced from web-platform-test.
static const uint32_t kMaxSessionResponseLength = 65536;

// Provide limitation for KeyIds length and webm initData size.
static const uint32_t kMaxWebmInitDataSize = 65536;
static const uint32_t kMaxKeyIdsLength = 512;

void CK_LogArray(const char* aPrepend, const uint8_t* aData,
                 const uint32_t aDataSize);

struct KeyIdPair {
  KeyId mKeyId;
  Key mKey;
};

class ClearKeyUtils {
 public:
  static bool DecryptCbcs(const std::vector<uint8_t>& aKey,
                          const std::vector<uint8_t>& aIV,
                          mozilla::Span<uint8_t> aSubsampleEncryptedRange,
                          uint32_t aCryptByteBlocks, uint32_t aSkipByteBlocks);

  static bool DecryptAES(const std::vector<uint8_t>& aKey,
                         std::vector<uint8_t>& aData,
                         std::vector<uint8_t>& aIV);

  static bool ParseKeyIdsInitData(const uint8_t* aInitData,
                                  uint32_t aInitDataSize,
                                  std::vector<KeyId>& aOutKeyIds);

  static void MakeKeyRequest(const std::vector<KeyId>& aKeyIds,
                             std::string& aOutRequest,
                             cdm::SessionType aSessionType);

  static bool ParseJWK(const uint8_t* aKeyData, uint32_t aKeyDataSize,
                       std::vector<KeyIdPair>& aOutKeys,
                       cdm::SessionType aSessionType);
  static const char* SessionTypeToString(cdm::SessionType aSessionType);

  static bool IsValidSessionId(const char* aBuff, uint32_t aLength);

  static std::string ToHexString(const uint8_t* aBytes, uint32_t aLength);
};

template <class Container, class Element>
inline bool Contains(const Container& aContainer, const Element& aElement) {
  return aContainer.find(aElement) != aContainer.end();
}

template <typename T>
inline void Assign(std::vector<T>& aVec, const T* aData, size_t aLength) {
  aVec.assign(aData, aData + aLength);
}

#endif  // __ClearKeyUtils_h__
