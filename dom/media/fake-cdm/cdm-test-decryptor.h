/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FAKE_DECRYPTOR_H__
#define FAKE_DECRYPTOR_H__

#include "content_decryption_module.h"
#include <string>
#include "mozilla/Attributes.h"

class FakeDecryptor : public cdm::ContentDecryptionModule_9 {
public:
  explicit FakeDecryptor(cdm::Host_9* aHost);

  void Initialize(bool aAllowDistinctiveIdentifier,
                  bool aAllowPersistentState) override
  {
  }

  void GetStatusForPolicy(uint32_t aPromiseId,
                          const cdm::Policy& aPolicy) override
  {
  }

  void SetServerCertificate(uint32_t aPromiseId,
                            const uint8_t* aServerCertificateData,
                            uint32_t aServerCertificateDataSize)
                            override
  {
  }

  void CreateSessionAndGenerateRequest(uint32_t aPromiseId,
                                       cdm::SessionType aSessionType,
                                       cdm::InitDataType aInitDataType,
                                       const uint8_t* aInitData,
                                       uint32_t aInitDataSize)
                                       override
  {
  }

  void LoadSession(uint32_t aPromiseId,
                   cdm::SessionType aSessionType,
                   const char* aSessionId,
                   uint32_t aSessionIdSize) override
  {
  }

  void UpdateSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdSize,
                     const uint8_t* aResponse,
                     uint32_t aResponseSize) override;

  void CloseSession(uint32_t aPromiseId,
                    const char* aSessionId,
                    uint32_t aSessionIdSize) override
  {
  }

  void RemoveSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdSize) override
  {
  }

  void TimerExpired(void* aContext) override
  {
  }

  cdm::Status Decrypt(const cdm::InputBuffer_1& aEncryptedBuffer,
                      cdm::DecryptedBlock* aDecryptedBuffer) override
  {
    return cdm::Status::kDecodeError;
  }

  cdm::Status InitializeAudioDecoder(
    const cdm::AudioDecoderConfig_1& aAudioDecoderConfig) override
  {
    return cdm::Status::kDecodeError;
  }

  cdm::Status InitializeVideoDecoder(
    const cdm::VideoDecoderConfig_1& aVideoDecoderConfig) override
  {
    return cdm::Status::kDecodeError;
  }

  void DeinitializeDecoder(cdm::StreamType aDecoderType) override
  {
  }

  void ResetDecoder(cdm::StreamType aDecoderType) override
  {
  }

  cdm::Status DecryptAndDecodeFrame(const cdm::InputBuffer_1& aEncryptedBuffer,
                                    cdm::VideoFrame* aVideoFrame) override
  {
    return cdm::Status::kDecodeError;
  }

  cdm::Status DecryptAndDecodeSamples(
    const cdm::InputBuffer_1& aEncryptedBuffer,
    cdm::AudioFrames* aAudioFrame) override
  {
    return cdm::Status::kDecodeError;
  }

  void OnPlatformChallengeResponse(
    const cdm::PlatformChallengeResponse& aResponse) override
  {
  }

  void OnQueryOutputProtectionStatus(cdm::QueryResult aResult,
                                     uint32_t aLinkMask,
                                     uint32_t aOutputProtectionMask) override
  {
  }

  void OnStorageId(uint32_t aVersion,
                   const uint8_t* aStorageId,
                   uint32_t aStorageIdSize) override
  {
  }

  void Destroy() override
  {
    delete this;
    sInstance = nullptr;
  }

  static void Message(const std::string& aMessage);

  cdm::Host_9* mHost;

  static FakeDecryptor* sInstance;

private:

  virtual ~FakeDecryptor() {}

  void TestStorage();

};

#endif
