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

#include <assert.h>
#include <string.h>

#include "BigEndian.h"
#include "ClearKeyDecryptionManager.h"
#include "ClearKeySession.h"
#include "ClearKeyStorage.h"
#include "ClearKeyUtils.h"
#include "psshparser/PsshParser.h"

using namespace mozilla;
using namespace cdm;

ClearKeySession::ClearKeySession(const std::string& aSessionId,
                                 SessionType aSessionType)
    : mSessionId(aSessionId), mSessionType(aSessionType) {
  CK_LOGD("ClearKeySession ctor %p", this);
}

ClearKeySession::~ClearKeySession() {
  CK_LOGD("ClearKeySession dtor %p", this);
}

bool ClearKeySession::Init(InitDataType aInitDataType, const uint8_t* aInitData,
                           uint32_t aInitDataSize) {
  CK_LOGD("ClearKeySession::Init");

  if (aInitDataType == InitDataType::kCenc) {
    ParseCENCInitData(aInitData, aInitDataSize, mKeyIds);
  } else if (aInitDataType == InitDataType::kKeyIds) {
    ClearKeyUtils::ParseKeyIdsInitData(aInitData, aInitDataSize, mKeyIds);
  } else if (aInitDataType == InitDataType::kWebM &&
             aInitDataSize <= kMaxWebmInitDataSize) {
    // "webm" initData format is simply the raw bytes of the keyId.
    std::vector<uint8_t> keyId;
    keyId.assign(aInitData, aInitData + aInitDataSize);
    mKeyIds.push_back(keyId);
  }

  if (mKeyIds.empty()) {
    return false;
  }

  return true;
}

SessionType ClearKeySession::Type() const { return mSessionType; }

void ClearKeySession::AddKeyId(const KeyId& aKeyId) {
  mKeyIds.push_back(aKeyId);
}
