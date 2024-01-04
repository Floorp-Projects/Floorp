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

#ifndef __ClearKeySession_h__
#define __ClearKeySession_h__

// This include is required in order for content_decryption_module to work
// on Unix systems.
#include <stddef.h>

#include <string>
#include <vector>

#include "content_decryption_module.h"

#include "ClearKeyUtils.h"

class ClearKeySession {
 public:
  explicit ClearKeySession(const std::string& aSessionId,
                           cdm::SessionType aSessionType);

  ~ClearKeySession();

  const std::vector<KeyId>& GetKeyIds() const { return mKeyIds; }

  bool Init(cdm::InitDataType aInitDataType, const uint8_t* aInitData,
            uint32_t aInitDataSize);

  cdm::SessionType Type() const;

  void AddKeyId(const KeyId& aKeyId);

  const std::string& Id() const { return mSessionId; }

 private:
  const std::string mSessionId;
  std::vector<KeyId> mKeyIds;

  const cdm::SessionType mSessionType;
};

#endif  // __ClearKeySession_h__
