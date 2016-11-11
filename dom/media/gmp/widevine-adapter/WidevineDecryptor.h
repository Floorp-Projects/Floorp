/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WidevineDecryptor_h_
#define WidevineDecryptor_h_

#include "stddef.h"
#include "content_decryption_module.h"
#include "gmp-api/gmp-decryption.h"
#include "mozilla/RefPtr.h"
#include "WidevineUtils.h"
#include <map>

namespace mozilla {

class WidevineDecryptor : public GMPDecryptor
                        , public cdm::Host_8
{
public:

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WidevineDecryptor)

  WidevineDecryptor();

  void SetCDM(RefPtr<CDMWrapper> aCDM, uint32_t aDecryptorId);

  static RefPtr<CDMWrapper> GetInstance(uint32_t aDecryptorId);

  // GMPDecryptor
  void Init(GMPDecryptorCallback* aCallback,
            bool aDistinctiveIdentifierRequired,
            bool aPersistentStateRequired) override;

  void CreateSession(uint32_t aCreateSessionToken,
                     uint32_t aPromiseId,
                     const char* aInitDataType,
                     uint32_t aInitDataTypeSize,
                     const uint8_t* aInitData,
                     uint32_t aInitDataSize,
                     GMPSessionType aSessionType) override;

  void LoadSession(uint32_t aPromiseId,
                   const char* aSessionId,
                   uint32_t aSessionIdLength) override;

  void UpdateSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdLength,
                     const uint8_t* aResponse,
                     uint32_t aResponseSize) override;

  void CloseSession(uint32_t aPromiseId,
                    const char* aSessionId,
                    uint32_t aSessionIdLength) override;

  void RemoveSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdLength) override;

  void SetServerCertificate(uint32_t aPromiseId,
                            const uint8_t* aServerCert,
                            uint32_t aServerCertSize) override;

  void Decrypt(GMPBuffer* aBuffer,
               GMPEncryptedBufferMetadata* aMetadata) override;

  void DecryptingComplete() override;


  // cdm::Host_8
  cdm::Buffer* Allocate(uint32_t aCapacity) override;
  void SetTimer(int64_t aDelayMs, void* aContext) override;
  cdm::Time GetCurrentWallTime() override;
  void OnResolveNewSessionPromise(uint32_t aPromiseId,
                                  const char* aSessionId,
                                  uint32_t aSessionIdSize) override;
  void OnResolvePromise(uint32_t aPromiseId) override;
  void OnRejectPromise(uint32_t aPromiseId,
                       cdm::Error aError,
                       uint32_t aSystemCode,
                       const char* aErrorMessage,
                       uint32_t aErrorMessageSize) override;
  void OnSessionMessage(const char* aSessionId,
                        uint32_t aSessionIdSize,
                        cdm::MessageType aMessageType,
                        const char* aMessage,
                        uint32_t aMessageSize,
                        const char* aLegacyDestinationUrl,
                        uint32_t aLegacyDestinationUrlLength) override;
  void OnSessionKeysChange(const char* aSessionId,
                           uint32_t aSessionIdSize,
                           bool aHasAdditionalUsableKey,
                           const cdm::KeyInformation* aKeysInfo,
                           uint32_t aKeysInfoCount) override;
  void OnExpirationChange(const char* aSessionId,
                          uint32_t aSessionIdSize,
                          cdm::Time aNewExpiryTime) override;
  void OnSessionClosed(const char* aSessionId,
                       uint32_t aSessionIdSize) override;
  void OnLegacySessionError(const char* aSessionId,
                            uint32_t aSessionId_length,
                            cdm::Error aError,
                            uint32_t aSystemCode,
                            const char* aErrorMessage,
                            uint32_t aErrorMessageLength) override;
  void SendPlatformChallenge(const char* aServiceId,
                             uint32_t aServiceIdSize,
                             const char* aChallenge,
                             uint32_t aChallengeSize) override;
  void EnableOutputProtection(uint32_t aDesiredProtectionMask) override;
  void QueryOutputProtectionStatus() override;
  void OnDeferredInitializationDone(cdm::StreamType aStreamType,
                                    cdm::Status aDecoderStatus) override;
  cdm::FileIO* CreateFileIO(cdm::FileIOClient* aClient) override;

  GMPDecryptorCallback* Callback() const { return mCallback; }
  RefPtr<CDMWrapper> GetCDMWrapper() const { return mCDM; }
private:
  ~WidevineDecryptor();
  RefPtr<CDMWrapper> mCDM;
  cdm::ContentDecryptionModule_8* CDM() { return mCDM->GetCDM(); }

  GMPDecryptorCallback* mCallback;
  std::map<uint32_t, uint32_t> mPromiseIdToNewSessionTokens;
  bool mDistinctiveIdentifierRequired = false;
  bool mPersistentStateRequired = false;
  uint32_t mInstanceId = 0;
};

} // namespace mozilla

#endif // WidevineDecryptor_h_
