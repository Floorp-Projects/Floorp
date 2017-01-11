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

#ifndef __ClearKeyDecryptor_h__
#define __ClearKeyDecryptor_h__

#include "ClearKeyDecryptionManager.h"
#include "ClearKeyPersistence.h"
#include "ClearKeySession.h"
#include "ClearKeyUtils.h"
// This include is required in order for content_decryption_module to work
// on Unix systems.
#include "stddef.h"
#include "content_decryption_module.h"
#include "RefCounted.h"

#include <functional>
#include <map>
#include <queue>
#include <set>
#include <string>

class ClearKeySessionManager final : public RefCounted
{
public:
  explicit ClearKeySessionManager(cdm::Host_8* aHost);

  void Init(bool aDistinctiveIdentifierAllowed,
            bool aPersistentStateAllowed);

  void CreateSession(uint32_t aPromiseId,
                     cdm::InitDataType aInitDataType,
                     const uint8_t* aInitData,
                     uint32_t aInitDataSize,
                     cdm::SessionType aSessionType);

  void LoadSession(uint32_t aPromiseId,
                   const char* aSessionId,
                   uint32_t aSessionIdLength);

  void UpdateSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdLength,
                     const uint8_t* aResponse,
                     uint32_t aResponseSize);

  void CloseSession(uint32_t aPromiseId,
                    const char* aSessionId,
                    uint32_t aSessionIdLength);

  void RemoveSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdLength);

  void SetServerCertificate(uint32_t aPromiseId,
                            const uint8_t* aServerCert,
                            uint32_t aServerCertSize);

  cdm::Status
  Decrypt(const cdm::InputBuffer& aBuffer,
          cdm::DecryptedBlock* aDecryptedBlock);

  void DecryptingComplete();

  void PersistentSessionDataLoaded(uint32_t aPromiseId,
                                   const std::string& aSessionId,
                                   const uint8_t* aKeyData,
                                   uint32_t aKeyDataSize);

private:
  ~ClearKeySessionManager();

  void ClearInMemorySessionData(ClearKeySession* aSession);
  bool MaybeDeferTillInitialized(std::function<void()> aMaybeDefer);
  void Serialize(const ClearKeySession* aSession,
                 std::vector<uint8_t>& aOutKeyData);

  RefPtr<ClearKeyDecryptionManager> mDecryptionManager;
  RefPtr<ClearKeyPersistence> mPersistence;

  cdm::Host_8* mHost = nullptr;

  std::set<KeyId> mKeyIds;
  std::map<std::string, ClearKeySession*> mSessions;

  std::queue<std::function<void()>> mDeferredInitialize;
};

#endif // __ClearKeyDecryptor_h__
