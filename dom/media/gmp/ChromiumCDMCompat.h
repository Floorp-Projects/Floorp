/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumCDMCompat_h_
#define ChromiumCDMCompat_h_

#include "content_decryption_module.h"

namespace mozilla::gmp {

class ChromiumCDMCompat final : public cdm::ContentDecryptionModule_11 {
 public:
  explicit ChromiumCDMCompat(cdm::ContentDecryptionModule_10* aCDM)
      : mCDM(aCDM) {}

  void Initialize(bool aAllowDistinctiveIdentifier, bool aAllowPersistentState,
                  bool aUseHwSecureCodecs) override {
    mCDM->Initialize(aAllowDistinctiveIdentifier, aAllowPersistentState,
                     aUseHwSecureCodecs);
  }

  void GetStatusForPolicy(uint32_t aPromiseId,
                          const cdm::Policy& aPolicy) override {
    mCDM->GetStatusForPolicy(aPromiseId, aPolicy);
  }

  void SetServerCertificate(uint32_t aPromiseId,
                            const uint8_t* aServerCertificateData,
                            uint32_t aServerCertificateDataSize) override {
    mCDM->SetServerCertificate(aPromiseId, aServerCertificateData,
                               aServerCertificateDataSize);
  }

  void CreateSessionAndGenerateRequest(uint32_t aPromiseId,
                                       cdm::SessionType aSessionType,
                                       cdm::InitDataType aInitDataType,
                                       const uint8_t* aInitData,
                                       uint32_t aInitDataSize) override {
    mCDM->CreateSessionAndGenerateRequest(aPromiseId, aSessionType,
                                          aInitDataType, aInitData, aInitDataSize);
  };

  void LoadSession(uint32_t aPromiseId, cdm::SessionType aSessionType,
                   const char* aSessionId, uint32_t aSessionIdSize) override {
    mCDM->LoadSession(aPromiseId, aSessionType, aSessionId, aSessionIdSize);
  };

  void UpdateSession(uint32_t aPromiseId, const char* aSessionId,
                     uint32_t aSessionIdSize, const uint8_t* aResponse,
                     uint32_t aResponseSize) override {
    mCDM->UpdateSession(aPromiseId, aSessionId, aSessionIdSize, aResponse,
                        aResponseSize);
  };

  void CloseSession(uint32_t aPromiseId, const char* aSessionId,
                    uint32_t aSessionIdSize) override {
    mCDM->CloseSession(aPromiseId, aSessionId, aSessionIdSize);
  };

  void RemoveSession(uint32_t aPromiseId, const char* aSessionId,
                     uint32_t aSessionIdSize) override {
    mCDM->RemoveSession(aPromiseId, aSessionId, aSessionIdSize);
  };

  void TimerExpired(void* aContext) override { mCDM->TimerExpired(aContext); };

  cdm::Status Decrypt(const cdm::InputBuffer_2& aEncryptedBuffer,
                      cdm::DecryptedBlock* aDecryptedBuffer) override {
    return mCDM->Decrypt(aEncryptedBuffer, aDecryptedBuffer);
  };

  cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig_2& aAudioDecoderConfig) override {
    return mCDM->InitializeAudioDecoder(aAudioDecoderConfig);
  };

  cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig_2& aVideoDecoderConfig) override {
    return mCDM->InitializeVideoDecoder(aVideoDecoderConfig);
  };

  void DeinitializeDecoder(cdm::StreamType aDecoderType) override {
    mCDM->DeinitializeDecoder(aDecoderType);
  };

  void ResetDecoder(cdm::StreamType aDecoderType) override {
    mCDM->ResetDecoder(aDecoderType);
  };

  cdm::Status DecryptAndDecodeFrame(const cdm::InputBuffer_2& aEncryptedBuffer,
                                    cdm::VideoFrame* aVideoFrame) override {
    return mCDM->DecryptAndDecodeFrame(aEncryptedBuffer, aVideoFrame);
  };

  cdm::Status DecryptAndDecodeSamples(
      const cdm::InputBuffer_2& aEncryptedBuffer,
      cdm::AudioFrames* aAudioFrames) override {
    return mCDM->DecryptAndDecodeSamples(aEncryptedBuffer, aAudioFrames);
  };

  void OnPlatformChallengeResponse(
      const cdm::PlatformChallengeResponse& aResponse) override {
    mCDM->OnPlatformChallengeResponse(aResponse);
  };

  void OnQueryOutputProtectionStatus(cdm::QueryResult aResult,
                                     uint32_t aLinkMask,
                                     uint32_t aOutputProtectionMask) override {
    mCDM->OnQueryOutputProtectionStatus(aResult, aLinkMask,
                                        aOutputProtectionMask);
  };

  void OnStorageId(uint32_t aVersion, const uint8_t* aStorageId,
                   uint32_t aStorageIdSize) override {
    mCDM->OnStorageId(aVersion, aStorageId, aStorageIdSize);
  }

  // Destroys the object in the same aContext as it was created.
  void Destroy() override {
    mCDM->Destroy();
    delete this;
  }

 protected:
  virtual ~ChromiumCDMCompat() = default;

  cdm::ContentDecryptionModule_10* mCDM;
};

}  // namespace mozilla::gmp

#endif  // ChromiumCDMCompat_h_
