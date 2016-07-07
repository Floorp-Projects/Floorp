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

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ClearKeyDecryptionManager.h"
#include "ClearKeySession.h"
#include "ClearKeyUtils.h"
#include "gmp-api/gmp-decryption.h"
#include "RefCounted.h"

class ClearKeySessionManager final : public GMPDecryptor
                                   , public RefCounted
{
public:
  ClearKeySessionManager();

  virtual void Init(GMPDecryptorCallback* aCallback,
                    bool aDistinctiveIdentifierAllowed,
                    bool aPersistentStateAllowed) override;

  virtual void CreateSession(uint32_t aCreateSessionToken,
                             uint32_t aPromiseId,
                             const char* aInitDataType,
                             uint32_t aInitDataTypeSize,
                             const uint8_t* aInitData,
                             uint32_t aInitDataSize,
                             GMPSessionType aSessionType) override;

  virtual void LoadSession(uint32_t aPromiseId,
                           const char* aSessionId,
                           uint32_t aSessionIdLength) override;

  virtual void UpdateSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength,
                             const uint8_t* aResponse,
                             uint32_t aResponseSize) override;

  virtual void CloseSession(uint32_t aPromiseId,
                            const char* aSessionId,
                            uint32_t aSessionIdLength) override;

  virtual void RemoveSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength) override;

  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const uint8_t* aServerCert,
                                    uint32_t aServerCertSize) override;

  virtual void Decrypt(GMPBuffer* aBuffer,
                       GMPEncryptedBufferMetadata* aMetadata) override;

  virtual void DecryptingComplete() override;

  void PersistentSessionDataLoaded(GMPErr aStatus,
                                   uint32_t aPromiseId,
                                   const std::string& aSessionId,
                                   const uint8_t* aKeyData,
                                   uint32_t aKeyDataSize);

private:
  ~ClearKeySessionManager();

  void DoDecrypt(GMPBuffer* aBuffer, GMPEncryptedBufferMetadata* aMetadata);
  void Shutdown();

  void ClearInMemorySessionData(ClearKeySession* aSession);
  void Serialize(const ClearKeySession* aSession, std::vector<uint8_t>& aOutKeyData);

  RefPtr<ClearKeyDecryptionManager> mDecryptionManager;

  GMPDecryptorCallback* mCallback;
  GMPThread* mThread;

  std::set<KeyId> mKeyIds;
  std::map<std::string, ClearKeySession*> mSessions;
};

#endif // __ClearKeyDecryptor_h__
