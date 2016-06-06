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

#include "BigEndian.h"
#include "ClearKeyDecryptionManager.h"
#include "ClearKeySession.h"
#include "ClearKeyUtils.h"
#include "ClearKeyStorage.h"
#include "gmp-task-utils.h"
#include "gmp-api/gmp-decryption.h"
#include <assert.h>
#include <string.h>

using namespace mozilla;

ClearKeySession::ClearKeySession(const std::string& aSessionId,
                                 GMPDecryptorCallback* aCallback,
                                 GMPSessionType aSessionType)
  : mSessionId(aSessionId)
  , mCallback(aCallback)
  , mSessionType(aSessionType)
{
  CK_LOGD("ClearKeySession ctor %p", this);
}

ClearKeySession::~ClearKeySession()
{
  CK_LOGD("ClearKeySession dtor %p", this);

  auto& keyIds = GetKeyIds();
  for (auto it = keyIds.begin(); it != keyIds.end(); it++) {
    assert(ClearKeyDecryptionManager::Get()->HasSeenKeyId(*it));

    ClearKeyDecryptionManager::Get()->ReleaseKeyId(*it);
    mCallback->KeyStatusChanged(&mSessionId[0], mSessionId.size(),
                                &(*it)[0], it->size(),
                                kGMPUnknown);
  }
}

void
ClearKeySession::Init(uint32_t aCreateSessionToken,
                      uint32_t aPromiseId,
                      const std::string& aInitDataType,
                      const uint8_t* aInitData, uint32_t aInitDataSize)
{
  CK_LOGD("ClearKeySession::Init");

  if (aInitDataType == "cenc") {
    ClearKeyUtils::ParseCENCInitData(aInitData, aInitDataSize, mKeyIds);
  } else if (aInitDataType == "keyids") {
    std::string sessionType;
    ClearKeyUtils::ParseKeyIdsInitData(aInitData, aInitDataSize, mKeyIds, sessionType);
    if (sessionType != ClearKeyUtils::SessionTypeToString(mSessionType)) {
      const char message[] = "Session type specified in keyids init data doesn't match session type.";
      mCallback->RejectPromise(aPromiseId, kGMPAbortError, message, strlen(message));
      return;
    }
  } else if (aInitDataType == "webm" && aInitDataSize == 16) {
    // "webm" initData format is simply the raw bytes of the keyId.
    vector<uint8_t> keyId;
    keyId.assign(aInitData, aInitData+aInitDataSize);
    mKeyIds.push_back(keyId);
  }

  if (!mKeyIds.size()) {
    const char message[] = "Couldn't parse init data";
    mCallback->RejectPromise(aPromiseId, kGMPAbortError, message, strlen(message));
    return;
  }

  mCallback->SetSessionId(aCreateSessionToken, &mSessionId[0], mSessionId.length());

  mCallback->ResolvePromise(aPromiseId);
}

GMPSessionType
ClearKeySession::Type() const
{
  return mSessionType;
}

void
ClearKeySession::AddKeyId(const KeyId& aKeyId)
{
  mKeyIds.push_back(aKeyId);
}
