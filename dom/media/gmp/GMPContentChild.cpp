/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPContentChild.h"
#include "GMPChild.h"
#include "GMPVideoDecoderChild.h"
#include "GMPVideoEncoderChild.h"
#include "ChromiumCDMChild.h"
#include "base/task.h"
#include "GMPUtils.h"

namespace mozilla {
namespace gmp {

GMPContentChild::GMPContentChild(GMPChild* aChild) : mGMPChild(aChild) {
  MOZ_COUNT_CTOR(GMPContentChild);
}

GMPContentChild::~GMPContentChild() { MOZ_COUNT_DTOR(GMPContentChild); }

MessageLoop* GMPContentChild::GMPMessageLoop() {
  return mGMPChild->GMPMessageLoop();
}

void GMPContentChild::CheckThread() {
  MOZ_ASSERT(mGMPChild->mGMPMessageLoop == MessageLoop::current());
}

void GMPContentChild::ActorDestroy(ActorDestroyReason aWhy) {
  mGMPChild->GMPContentChildActorDestroy(this);
}

void GMPContentChild::ProcessingError(Result aCode, const char* aReason) {
  mGMPChild->ProcessingError(aCode, aReason);
}

already_AddRefed<PGMPVideoDecoderChild>
GMPContentChild::AllocPGMPVideoDecoderChild(const uint32_t& aDecryptorId) {
  return MakeAndAddRef<GMPVideoDecoderChild>(this);
}

already_AddRefed<PGMPVideoEncoderChild>
GMPContentChild::AllocPGMPVideoEncoderChild() {
  return MakeAndAddRef<GMPVideoEncoderChild>(this);
}

already_AddRefed<PChromiumCDMChild> GMPContentChild::AllocPChromiumCDMChild() {
  return MakeAndAddRef<ChromiumCDMChild>(this);
}

mozilla::ipc::IPCResult GMPContentChild::RecvPGMPVideoDecoderConstructor(
    PGMPVideoDecoderChild* aActor, const uint32_t& aDecryptorId) {
  auto vdc = static_cast<GMPVideoDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err =
      mGMPChild->GetAPI(GMP_API_VIDEO_DECODER, &vdc->Host(), &vd, aDecryptorId);
  if (err != GMPNoErr || !vd) {
    NS_WARNING("GMPGetAPI call failed trying to construct decoder.");
    return IPC_FAIL_NO_REASON(this);
  }

  vdc->Init(static_cast<GMPVideoDecoder*>(vd));

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPContentChild::RecvPGMPVideoEncoderConstructor(
    PGMPVideoEncoderChild* aActor) {
  auto vec = static_cast<GMPVideoEncoderChild*>(aActor);

  void* ve = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_VIDEO_ENCODER, &vec->Host(), &ve);
  if (err != GMPNoErr || !ve) {
    NS_WARNING("GMPGetAPI call failed trying to construct encoder.");
    return IPC_FAIL_NO_REASON(this);
  }

  vec->Init(static_cast<GMPVideoEncoder*>(ve));

  return IPC_OK();
}

// Convert CDM10 calls to CDM9 calls, massage args where needed
class ChromiumCDM9BackwardsCompat : public cdm::ContentDecryptionModule_10 {
 public:
  explicit ChromiumCDM9BackwardsCompat(cdm::Host_10* aHost,
                                       cdm::ContentDecryptionModule_9* aCDM)
      : mCDM(aCDM), mHost(aHost) {}

  void Initialize(bool aAllowDistinctiveIdentifier, bool aAllowPersistentState,
                  bool /* aUseHardwareSecureCodec */) override {
    // aUseHardwareSecureCodec is not used by CDM9
    mCDM->Initialize(aAllowDistinctiveIdentifier, aAllowPersistentState);
    // CDM9 should init synchronously and does not call an OnInit callback, so
    // we make sure it's called here.
    mHost->OnInitialized(true);
  }

  void GetStatusForPolicy(uint32_t aPromiseId,
                          const cdm::Policy& policy) override {
    mCDM->GetStatusForPolicy(aPromiseId, policy);
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
    mCDM->CreateSessionAndGenerateRequest(
        aPromiseId, aSessionType, aInitDataType, aInitData, aInitDataSize);
  }

  void LoadSession(uint32_t aPromiseId, cdm::SessionType aSessionType,
                   const char* aSessionId, uint32_t aSessionIdSize) override {
    mCDM->LoadSession(aPromiseId, aSessionType, aSessionId, aSessionIdSize);
  }

  void UpdateSession(uint32_t aPromiseId, const char* aSessionId,
                     uint32_t aSessionIdSize, const uint8_t* aResponse,
                     uint32_t aResponseSize) override {
    mCDM->UpdateSession(aPromiseId, aSessionId, aSessionIdSize, aResponse,
                        aResponseSize);
  }

  void CloseSession(uint32_t aPromiseId, const char* aSessionId,
                    uint32_t aSessionIdSize) override {
    mCDM->CloseSession(aPromiseId, aSessionId, aSessionIdSize);
  }

  void RemoveSession(uint32_t aPromiseId, const char* aSessionId,
                     uint32_t aSessionIdSize) override {
    mCDM->RemoveSession(aPromiseId, aSessionId, aSessionIdSize);
  }

  void TimerExpired(void* aContext) override { mCDM->TimerExpired(aContext); }

  cdm::Status Decrypt(const cdm::InputBuffer_2& aEncryptedBuffer,
                      cdm::DecryptedBlock* aDecryptedBuffer) override {
    // Handle possible encryption mismatch
    if (!IsEncryptionSchemeSupported(aEncryptedBuffer.encryption_scheme)) {
      return cdm::Status::kDecryptError;
    }

    return mCDM->Decrypt(ConvertToInputBuffer_1(aEncryptedBuffer),
                         aDecryptedBuffer);
  }

  cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig_2& aAudioDecoderConfig) override {
    // Handle possible encryption mismatch
    if (!IsEncryptionSchemeSupported(aAudioDecoderConfig.encryption_scheme)) {
      return cdm::Status::kInitializationError;
    }

    return mCDM->InitializeAudioDecoder(
        ConverToAudioDecoderConfig_1(aAudioDecoderConfig));
  }

  cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig_2& aVideoDecoderConfig) override {
    // Handle possible encryption mismatch
    if (!IsEncryptionSchemeSupported(aVideoDecoderConfig.encryption_scheme)) {
      return cdm::Status::kInitializationError;
    }

    return mCDM->InitializeVideoDecoder(
        ConvertToVideoDecoderConfig_1(aVideoDecoderConfig));
  }

  void DeinitializeDecoder(cdm::StreamType aDecoderType) override {
    mCDM->DeinitializeDecoder(aDecoderType);
  }

  void ResetDecoder(cdm::StreamType aDecoderType) override {
    mCDM->ResetDecoder(aDecoderType);
  }

  cdm::Status DecryptAndDecodeFrame(const cdm::InputBuffer_2& aEncryptedBuffer,
                                    cdm::VideoFrame* aVideoFrame) override {
    // Handle possible encryption mismatch
    if (!IsEncryptionSchemeSupported(aEncryptedBuffer.encryption_scheme)) {
      return cdm::Status::kDecryptError;
    }

    return mCDM->DecryptAndDecodeFrame(ConvertToInputBuffer_1(aEncryptedBuffer),
                                       aVideoFrame);
  }

  cdm::Status DecryptAndDecodeSamples(
      const cdm::InputBuffer_2& aEncryptedBuffer,
      cdm::AudioFrames* aAudioFrames) override {
    // Handle possible encryption mismatch
    if (!IsEncryptionSchemeSupported(aEncryptedBuffer.encryption_scheme)) {
      return cdm::Status::kDecryptError;
    }

    return mCDM->DecryptAndDecodeSamples(
        ConvertToInputBuffer_1(aEncryptedBuffer), aAudioFrames);
  }

  void OnPlatformChallengeResponse(
      const cdm::PlatformChallengeResponse& aResponse) override {
    mCDM->OnPlatformChallengeResponse(aResponse);
  }

  void OnQueryOutputProtectionStatus(cdm::QueryResult aResult,
                                     uint32_t aLinkMask,
                                     uint32_t aOutputProtectionMask) override {
    mCDM->OnQueryOutputProtectionStatus(aResult, aLinkMask,
                                        aOutputProtectionMask);
  }

  void OnStorageId(uint32_t aVersion, const uint8_t* aStorageId,
                   uint32_t aStorageIdSize) override {
    mCDM->OnStorageId(aVersion, aStorageId, aStorageIdSize);
  }

  void Destroy() override {
    mCDM->Destroy();
    delete this;
  }

  cdm::ContentDecryptionModule_9* mCDM;
  cdm::Host_10* mHost;

 private:
  // CDM9 supports non-encrypted or cenc encrypted media, anything else should
  // be rejected.
  static bool IsEncryptionSchemeSupported(
      const cdm::EncryptionScheme& aEncryptionScheme) {
    return aEncryptionScheme == cdm::EncryptionScheme::kUnencrypted ||
           aEncryptionScheme == cdm::EncryptionScheme::kCenc;
  }

  // Conversion functions that drop the encryption scheme member. CDMs prior to
  // 10 assumed no encryption or cenc encryption (if encryption is present). So
  // we can drop the scheme member if we check to make sure it was one of these
  // two options.
  static cdm::InputBuffer_1 ConvertToInputBuffer_1(
      const cdm::InputBuffer_2& aInputBuffer) {
    MOZ_ASSERT(
        IsEncryptionSchemeSupported(aInputBuffer.encryption_scheme),
        "Encryption scheme should be checked before attempting conversion!");
    return {aInputBuffer.data,       aInputBuffer.data_size,
            aInputBuffer.key_id,     aInputBuffer.key_id_size,
            aInputBuffer.iv,         aInputBuffer.iv_size,
            aInputBuffer.subsamples, aInputBuffer.num_subsamples,
            aInputBuffer.timestamp};
  }

  static cdm::AudioDecoderConfig_1 ConverToAudioDecoderConfig_1(
      const cdm::AudioDecoderConfig_2& aAudioConfig) {
    MOZ_ASSERT(
        IsEncryptionSchemeSupported(aAudioConfig.encryption_scheme),
        "Encryption scheme should be checked before attempting conversion!");
    return {aAudioConfig.codec,
            aAudioConfig.channel_count,
            aAudioConfig.bits_per_channel,
            aAudioConfig.samples_per_second,
            aAudioConfig.extra_data,
            aAudioConfig.extra_data_size};
  }

  static cdm::VideoDecoderConfig_1 ConvertToVideoDecoderConfig_1(
      const cdm::VideoDecoderConfig_2& aVideoConfig) {
    MOZ_ASSERT(
        IsEncryptionSchemeSupported(aVideoConfig.encryption_scheme),
        "Encryption scheme should be checked before attempting conversion!");
    return {aVideoConfig.codec,      aVideoConfig.profile,
            aVideoConfig.format,     aVideoConfig.coded_size,
            aVideoConfig.extra_data, aVideoConfig.extra_data_size};
  }
};  // class ChromiumCDM9BackwardsCompat

mozilla::ipc::IPCResult GMPContentChild::RecvPChromiumCDMConstructor(
    PChromiumCDMChild* aActor) {
  ChromiumCDMChild* child = static_cast<ChromiumCDMChild*>(aActor);
  cdm::Host_10* host10 = child;

  void* cdm = nullptr;
  GMPErr err = mGMPChild->GetAPI(CHROMIUM_CDM_API, host10, &cdm);
  if (err != GMPNoErr || !cdm) {
    // Try to create older version 9 CDM.
    cdm::Host_9* host9 = child;
    GMPErr err =
        mGMPChild->GetAPI(CHROMIUM_CDM_API_BACKWARD_COMPAT, host9, &cdm);
    if (err != GMPNoErr || !cdm) {
      NS_WARNING("GMPGetAPI call failed trying to get CDM.");
      return IPC_FAIL_NO_REASON(this);
    }
    cdm = new ChromiumCDM9BackwardsCompat(
        host10, static_cast<cdm::ContentDecryptionModule_9*>(cdm));
  }

  child->Init(static_cast<cdm::ContentDecryptionModule_10*>(cdm),
              mGMPChild->mStorageId);

  return IPC_OK();
}

void GMPContentChild::CloseActive() {
  // Invalidate and remove any remaining API objects.
  const ManagedContainer<PGMPVideoDecoderChild>& videoDecoders =
      ManagedPGMPVideoDecoderChild();
  for (auto iter = videoDecoders.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }

  const ManagedContainer<PGMPVideoEncoderChild>& videoEncoders =
      ManagedPGMPVideoEncoderChild();
  for (auto iter = videoEncoders.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }

  const ManagedContainer<PChromiumCDMChild>& cdms = ManagedPChromiumCDMChild();
  for (auto iter = cdms.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }
}

bool GMPContentChild::IsUsed() {
  return !ManagedPGMPVideoDecoderChild().IsEmpty() ||
         !ManagedPGMPVideoEncoderChild().IsEmpty() ||
         !ManagedPChromiumCDMChild().IsEmpty();
}

}  // namespace gmp
}  // namespace mozilla
