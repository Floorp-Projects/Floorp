#ifndef ClearKeyCDM_h_
#define ClearKeyCDM_h_

#include "ClearKeySessionManager.h"

// This include is required in order for content_decryption_module to work
// on Unix systems.
#include "stddef.h"
#include "content_decryption_module.h"

#ifdef ENABLE_WMF
#include "WMFUtils.h"
#include "VideoDecoder.h"
#endif

class ClearKeyCDM : public cdm::ContentDecryptionModule_8
{
private:
  RefPtr<ClearKeySessionManager> mSessionManager;
#ifdef ENABLE_WMF
  RefPtr<VideoDecoder> mVideoDecoder;
#endif

protected:
  cdm::Host_8* mHost;

public:
  explicit ClearKeyCDM(cdm::Host_8* mHost);

  void Initialize(bool aAllowDistinctiveIdentifier,
                  bool aAllowPersistentState) override;

  void SetServerCertificate(uint32_t aPromiseId,
                            const uint8_t* aServerCertificateData,
                            uint32_t aServerCertificateDataSize)
                            override;

  void CreateSessionAndGenerateRequest(uint32_t aPromiseId,
                                       cdm::SessionType aSessionType,
                                       cdm::InitDataType aInitDataType,
                                       const uint8_t* aInitData,
                                       uint32_t aInitDataSize)
                                       override;

  void LoadSession(uint32_t aPromiseId,
                   cdm::SessionType aSessionType,
                   const char* aSessionId,
                   uint32_t aSessionIdSize) override;

  void UpdateSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdSize,
                     const uint8_t* aResponse,
                     uint32_t aResponseSize) override;

  void CloseSession(uint32_t aPromiseId,
                    const char* aSessionId,
                    uint32_t aSessionIdSize) override;

  void RemoveSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdSize) override;

  void TimerExpired(void* aContext) override;

  cdm::Status Decrypt(const cdm::InputBuffer& aEncryptedBuffer,
                      cdm::DecryptedBlock* aDecryptedBuffer) override;

  cdm::Status InitializeAudioDecoder(
    const cdm::AudioDecoderConfig& aAudioDecoderConfig) override;

  cdm::Status InitializeVideoDecoder(
    const cdm::VideoDecoderConfig& aVideoDecoderConfig) override;

  void DeinitializeDecoder(cdm::StreamType aDecoderType) override;

  void ResetDecoder(cdm::StreamType aDecoderType) override;

  cdm::Status DecryptAndDecodeFrame(
    const cdm::InputBuffer& aEncryptedBuffer,
    cdm::VideoFrame* aVideoFrame) override;

  cdm::Status DecryptAndDecodeSamples(
    const cdm::InputBuffer& aEncryptedBuffer,
    cdm::AudioFrames* aAudioFrame) override;

  void OnPlatformChallengeResponse(
    const cdm::PlatformChallengeResponse& aResponse) override;

  void
    OnQueryOutputProtectionStatus(cdm::QueryResult aResult,
                                  uint32_t aLinkMask,
                                  uint32_t aOutputProtectionMask) override;

  void Destroy() override;
};

#endif