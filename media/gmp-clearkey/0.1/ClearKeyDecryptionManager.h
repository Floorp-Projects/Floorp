/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ClearKeyDecryptor_h__
#define __ClearKeyDecryptor_h__

#include <map>
#include <string>
#include <vector>

#include "ClearKeySession.h"
#include "ClearKeyUtils.h"
#include "gmp-api/gmp-decryption.h"
#include "ScopedNSSTypes.h"
#include "RefCounted.h"

class ClearKeyDecryptor;
class ClearKeyDecryptionManager MOZ_FINAL : public GMPDecryptor
                                          , public RefCounted
{
public:
  ClearKeyDecryptionManager();

  virtual void Init(GMPDecryptorCallback* aCallback) MOZ_OVERRIDE;

  virtual void CreateSession(uint32_t aPromiseId,
                             const char* aInitDataType,
                             uint32_t aInitDataTypeSize,
                             const uint8_t* aInitData,
                             uint32_t aInitDataSize,
                             GMPSessionType aSessionType) MOZ_OVERRIDE;

  virtual void LoadSession(uint32_t aPromiseId,
                           const char* aSessionId,
                           uint32_t aSessionIdLength) MOZ_OVERRIDE;

  virtual void UpdateSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength,
                             const uint8_t* aResponse,
                             uint32_t aResponseSize) MOZ_OVERRIDE;

  virtual void CloseSession(uint32_t aPromiseId,
                            const char* aSessionId,
                            uint32_t aSessionIdLength) MOZ_OVERRIDE;

  virtual void RemoveSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength) MOZ_OVERRIDE;

  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const uint8_t* aServerCert,
                                    uint32_t aServerCertSize) MOZ_OVERRIDE;

  virtual void Decrypt(GMPBuffer* aBuffer,
                       GMPEncryptedBufferMetadata* aMetadata) MOZ_OVERRIDE;

  virtual void DecryptingComplete() MOZ_OVERRIDE;

  void PersistentSessionDataLoaded(GMPErr aStatus,
                                   uint32_t aPromiseId,
                                   const std::string& aSessionId,
                                   const uint8_t* aKeyData,
                                   uint32_t aKeyDataSize);

private:
  ~ClearKeyDecryptionManager();

  void ClearInMemorySessionData(ClearKeySession* aSession);
  void Serialize(const ClearKeySession* aSession, std::vector<uint8_t>& aOutKeyData);

  GMPDecryptorCallback* mCallback;

  std::map<KeyId, ClearKeyDecryptor*> mDecryptors;
  std::map<std::string, ClearKeySession*> mSessions;
};

#endif // __ClearKeyDecryptor_h__
